param(
    [string]$InnoCompiler = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
    [string]$StageDir = '',
    [string]$OutputDir = '',
    [string]$Version = '',
    [switch]$SkipBuild,
    [switch]$Sign
)
$ErrorActionPreference = 'Stop'
if (-not $IsWindows) { throw 'Packaging requires Windows x64, MSVC and PowerShell 7.' }
$repo = Split-Path $PSScriptRoot -Parent
$manifest = Get-Content (Join-Path $repo 'vcpkg.json') -Raw | ConvertFrom-Json
$sourceVersion = $manifest.'version-string'
if (-not $Version) { $Version = $sourceVersion }
if ($Version -notmatch '^\d+\.\d+\.\d+$' -or $Version -ne $sourceVersion) { throw 'Version must match vcpkg.json (X.Y.Z).' }
foreach ($part in $Version.Split('.')) { if ([int]$part -gt 65535) { throw 'Version fields must fit Windows VERSIONINFO.' } }
$cmake = Get-Content (Join-Path $repo 'CMakeLists.txt') -Raw
if ($cmake -notmatch ('project\(ShareKeys VERSION ' + [regex]::Escape($Version) + '\s')) { throw 'CMake and manifest versions differ.' }
if (-not $env:VCPKG_ROOT) { throw 'Set VCPKG_ROOT to your bootstrapped vcpkg checkout at the manifest baseline.' }
$vcpkgCommit = & git -C $env:VCPKG_ROOT rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $vcpkgCommit -ne $manifest.'builtin-baseline') { throw 'Check out VCPKG_ROOT at the exact builtin-baseline from vcpkg.json and bootstrap it.' }
if (-not (Test-Path $InnoCompiler)) { throw 'Install Inno Setup 6 or pass -InnoCompiler.' }
if (-not $StageDir) { $StageDir = Join-Path $repo ('build/package-' + [guid]::NewGuid()) }
if (-not $OutputDir) { $OutputDir = Join-Path $repo 'build/distribution' }
$StageDir = [System.IO.Path]::GetFullPath($StageDir)
$OutputDir = [System.IO.Path]::GetFullPath($OutputDir)
if ((Test-Path $StageDir) -or (Test-Path $OutputDir)) { throw 'Choose fresh staging and output directories to avoid stale release files.' }
New-Item -ItemType Directory -Path $OutputDir | Out-Null
Push-Location $repo
try {
    if (-not $SkipBuild) {
        cmake --preset windows
        if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
        cmake --build --preset windows-release --parallel 2
        if ($LASTEXITCODE -ne 0) { throw 'Release compilation failed.' }
    }
    # Tests run even when a caller already built the binaries.
    ctest --preset windows-release --output-junit (Join-Path $OutputDir 'tests-release.xml')
    if ($LASTEXITCODE -ne 0) { throw 'Automated tests failed.' }
    $all = Join-Path $StageDir 'all'
    cmake --install build/windows --config Release --prefix $all
    if ($LASTEXITCODE -ne 0) { throw 'Runtime deployment failed.' }
    & "$PSScriptRoot/verify_windows_stage.ps1" -StageDir $all -ExpectedVersion $Version
    $inventories = @{}
    $installers = @()
    foreach ($role in @('host', 'agent')) {
        $payload = Join-Path $StageDir $role
        New-Item -ItemType Directory -Path $payload | Out-Null
        Get-ChildItem $all | Where-Object { $_.PSIsContainer -or $_.Extension -ne '.exe' } | Copy-Item -Destination $payload -Recurse
        Copy-Item (Join-Path $all "sharekeys_$role.exe") $payload
        if ($Sign) {
            $peFiles = @(Get-ChildItem $payload -Recurse -File | Where-Object { $_.Extension -in @('.exe', '.dll') } | Select-Object -ExpandProperty FullName)
            & "$PSScriptRoot/sign_windows.ps1" -Path $peFiles
        }
        & "$PSScriptRoot/verify_windows_stage.ps1" -StageDir $payload -Role $role -ExpectedVersion $Version -RequireSigned:$Sign
        $inventories[$role] = @(Get-ChildItem $payload -Recurse -File | Sort-Object FullName | ForEach-Object {
            @{ path = [System.IO.Path]::GetRelativePath($payload, $_.FullName).Replace('\', '/'); bytes = $_.Length; sha256 = (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant() }
        })
        $compilerArgs = @("/DStageDir=$payload", "/DAppVersion=$Version", "/DOutputDir=$OutputDir")
        if ($Sign) {
            # $f is Inno's quoted filename placeholder. Secrets are not arguments.
            $pwshPath = (Get-Command pwsh.exe).Source.Replace('$', '$$')
            $signScript = (Join-Path $PSScriptRoot 'sign_windows.ps1').Replace('$', '$$')
            $command = '$q' + $pwshPath + '$q -NoProfile -File $q' + $signScript + '$q -Path $f'
            $compilerArgs += '/DSignPackages=1', ('/SkvmSign=' + $command)
        }
        & $InnoCompiler @compilerArgs "installer/$role.iss"
        if ($LASTEXITCODE -ne 0) { throw "Installer compilation failed: $role" }
        $name = if ($role -eq 'host') { "KVM-Win-Setup-$Version.exe" } else { "KVM-Agent-Setup-$Version.exe" }
        $installer = Join-Path $OutputDir $name
        if (-not (Test-Path $installer)) { throw "Missing expected installer: $name" }
        if ($Sign -and (Get-AuthenticodeSignature $installer).Status -ne 'Valid') { throw "Installer signature invalid: $name" }
        $installers += $installer
    }
    $commit = & git rev-parse HEAD
    if ($LASTEXITCODE -ne 0) { throw 'Cannot determine source commit.' }
    $dirty = [bool](& git status --porcelain)
    $cache = Get-Content build/windows/CMakeCache.txt | Where-Object { $_ -match '^(CMAKE_(CXX_COMPILER|GENERATOR|VS_VERSION|SYSTEM_VERSION)|Qt6_DIR|OPENSSL_.*):' }
    $release = [ordered]@{
        version = $Version; sourceCommit = $commit; sourceDirty = $dirty; signed = [bool]$Sign
        generatedUtc = [DateTime]::UtcNow.ToString('o'); platform = 'Windows x64 Release'
        vcpkgCommit = $vcpkgCommit; runnerImage = $env:ImageOS; runnerImageVersion = $env:ImageVersion
        cmake = ((& cmake --version) | Select-Object -First 1)
        innoCompilerVersion = (Get-Item $InnoCompiler).VersionInfo.FileVersion
        buildConfiguration = $cache; payloads = $inventories
        validation = 'Automated tests and isolated GUI smoke completed. Installer round-trip is a separate workflow step; physical input and clean-VM acceptance are not implied.'
    }
    $release | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $OutputDir 'release-manifest.json') -Encoding utf8NoBOM
    $checksums = foreach ($installer in $installers) {
        (Get-FileHash $installer -Algorithm SHA256).Hash.ToLowerInvariant() + '  ' + (Split-Path $installer -Leaf)
    }
    $checksums | Set-Content (Join-Path $OutputDir 'SHA256SUMS') -Encoding ascii
    Copy-Item docs/INSTALLATION.md $OutputDir
    Write-Output "Generated installers, SHA256SUMS and payload manifest in $OutputDir"
    Write-Output "Isolated Host/Agent payloads in $StageDir"
} finally { Pop-Location }
