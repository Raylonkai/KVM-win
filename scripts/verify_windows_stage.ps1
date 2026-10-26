param(
    [Parameter(Mandatory = $true)][string]$StageDir,
    [ValidateSet('offscreen', 'windows')][string]$Platform = 'offscreen',
    [ValidateSet('all', 'host', 'agent')][string]$Role = 'all',
    [string]$ExpectedVersion = '',
    [string]$IdentityRoot = '',
    [switch]$RequireSigned
)
$ErrorActionPreference = 'Stop'
$stage = (Resolve-Path $StageDir).Path
if (-not $IsWindows) { throw 'Windows PowerShell 7 is required.' }
$roles = if ($Role -eq 'all') { @('host', 'agent') } else { @($Role) }
foreach ($file in @('Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Widgets.dll', 'Qt6Network.dll', 'qt.conf')) {
    if (-not (Test-Path (Join-Path $stage $file))) { throw "Missing staged dependency: $file" }
}
foreach ($pattern in @('qwindows.dll', 'qoffscreen.dll', 'libssl*.dll', 'libcrypto*.dll', 'vcruntime140.dll', 'msvcp140.dll')) {
    if (-not (Get-ChildItem $stage -Recurse -Filter $pattern)) { throw "Missing staged runtime: $pattern" }
}
if ($Role -ne 'all') {
    $other = if ($Role -eq 'host') { 'agent' } else { 'host' }
    if (Test-Path (Join-Path $stage "sharekeys_$other.exe")) { throw 'Role payload contains the other application.' }
    if (Test-Path (Join-Path $stage 'sharekeys_agent_cli.exe')) { throw 'Developer CLI must not be distributed to end users.' }
}
foreach ($currentRole in $roles) {
    $exe = Join-Path $stage "sharekeys_$currentRole.exe"
    $stream = [System.IO.File]::OpenRead($exe)
    $reader = [System.IO.BinaryReader]::new($stream)
    try {
        $stream.Position = 0x3c
        $peOffset = $reader.ReadInt32()
        $stream.Position = $peOffset
        if ($reader.ReadUInt32() -ne 0x4550) { throw "Invalid PE signature: $exe" }
        if ($reader.ReadUInt16() -ne 0x8664) { throw "Expected x64 executable: $exe" }
        $stream.Position = $peOffset + 24 + 68
        if ($reader.ReadUInt16() -ne 2) { throw "Expected Windows GUI subsystem: $exe" }
    } finally { $reader.Dispose(); $stream.Dispose() }
    if ($RequireSigned) {
        foreach ($binary in (Get-ChildItem $stage -Recurse -File | Where-Object { $_.Extension -in @('.exe', '.dll') })) {
            if ((Get-AuthenticodeSignature $binary.FullName).Status -ne 'Valid') { throw "Invalid PE signature: $($binary.FullName)" }
        }
    }
    $version = [System.Diagnostics.FileVersionInfo]::GetVersionInfo($exe)
    if (-not $version.ProductVersion -or $version.ProductName -ne 'KVM-Win / ShareKeys') { throw "Missing application metadata: $exe" }
    if ($ExpectedVersion -and $version.ProductVersion -ne $ExpectedVersion) { throw "Version mismatch: $exe" }
    if ($RequireSigned -and (Get-AuthenticodeSignature $exe).Status -ne 'Valid') { throw "Invalid executable signature: $exe" }
    $info = [System.Diagnostics.ProcessStartInfo]::new()
    $info.FileName = $exe
    $info.UseShellExecute = $false
    $info.WorkingDirectory = $stage
    $info.Environment['PATH'] = "$env:SystemRoot\System32;$env:SystemRoot"
    $info.Environment['QT_PLUGIN_PATH'] = ''
    $info.Environment['QT_QPA_PLATFORM_PLUGIN_PATH'] = ''
    $info.Environment['QT_QPA_PLATFORM'] = $Platform
    $identity = Join-Path ([System.IO.Path]::GetTempPath()) ("kvm-win-stage-$currentRole-" + [guid]::NewGuid())
    if ($IdentityRoot) { $identity = Join-Path $IdentityRoot $currentRole }
    $info.ArgumentList.Add('--smoke-test')
    $info.ArgumentList.Add('--identity')
    $info.ArgumentList.Add($identity)
    $process = [System.Diagnostics.Process]::Start($info)
    if (-not $process.WaitForExit(30000)) { $process.Kill(); throw "Startup/shutdown timed out: $exe" }
    if ($process.ExitCode -ne 0) { throw "Staged GUI failed: $exe (exit $($process.ExitCode))" }
    $process.Dispose()
    Write-Output "PASS: $currentRole GUI subsystem, metadata and isolated staged startup/shutdown ($Platform)."
}
Write-Output 'No input hooks, SendInput, multi-PC interaction or installation were tested by this check.'
