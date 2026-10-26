# Disposable Windows VM/CI profile only: installs real AppIds and shortcuts.
param(
    [Parameter(Mandatory = $true)][string]$Installer,
    [Parameter(Mandatory = $true)][ValidateSet('host', 'agent')][string]$Role,
    [Parameter(Mandatory = $true)][string]$Version,
    [string]$PreviousInstaller = '',
    [string]$EvidenceDir = '',
    [switch]$RequireSigned
)
$ErrorActionPreference = 'Stop'
if (-not $IsWindows) { throw 'Windows PowerShell 7 is required.' }
$installerPath = (Resolve-Path $Installer).Path
$testRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("kvm-win-install-$Role-" + [guid]::NewGuid())
if (-not $EvidenceDir) { $EvidenceDir = $testRoot }
New-Item -ItemType Directory -Force -Path $EvidenceDir | Out-Null
$destination = Join-Path $testRoot 'Application With Spaces'
$appDataFolder = if ($Role -eq 'host') { 'ShareKeysHost' } else { 'ShareKeysAgent' }
$profile = Join-Path $env:LOCALAPPDATA $appDataFolder
$identityRoot = $profile
$title = if ($Role -eq 'host') { 'KVM-Win Host' } else { 'KVM-Win Agent' }
$shortcutName = if ($Role -eq 'host') { 'KVM-Win Host (main PC)' } else { 'KVM-Win Agent (remote PC)' }
$runKey = if ($Role -eq 'host') { 'ShareKeysHost' } else { 'ShareKeysAgent' }
$otherRunKey = if ($Role -eq 'host') { 'ShareKeysAgent' } else { 'ShareKeysHost' }
$guid = if ($Role -eq 'host') { '{B126C46A-D3BF-4D3C-9BD4-1F68B4F52796}' } else { '{D169AD0E-CC81-4E2B-B06D-33151D62E750}' }
if (Test-Path "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\${guid}_is1") { throw 'Use a disposable profile without this role already installed.' }
function Install-Package([string]$Package, [string]$LogName) {
    $log = Join-Path $EvidenceDir $LogName
    $setup = Start-Process -FilePath $Package -ArgumentList @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/TASKS=desktopicon', "/DIR=`"$destination`"", "/LOG=`"$log`"") -PassThru -Wait
    if ($setup.ExitCode -ne 0) { throw "Install failed ($($setup.ExitCode)); see $log" }
}
$first = if ($PreviousInstaller) { (Resolve-Path $PreviousInstaller).Path } else { $installerPath }
Install-Package $first "install-$Role.log"
& "$PSScriptRoot/verify_windows_stage.ps1" -StageDir $destination -Role $Role -IdentityRoot $identityRoot
$certificate = Join-Path $profile 'identity.crt'
$privateKey = Join-Path $profile 'identity.key'
$beforeCertificate = (Get-FileHash $certificate).Hash
$beforeKey = (Get-FileHash $privateKey).Hash
# Seed real per-user role configuration after first-run app identity creation.
if ($Role -eq 'host') {
    $config = Join-Path $profile 'host.json'
    '{"version":1,"devices":[{"name":"Test peer","address":"192.168.1.9","port":24870,"pin":"' + ('a' * 64) + '","mirror":true}],"groups":{},"positions":{},"edges":false,"autoConnect":false,"mirrorKey":77,"emergencyKey":27,"nextKey":9}' | Set-Content $config -Encoding utf8NoBOM
} else {
    $config = Join-Path $profile 'authorized-host.json'
    '{"hostPin":"' + ('a' * 64) + '","address":"192.168.1.9","port":24870,"autoListen":false}' | Set-Content $config -Encoding utf8NoBOM
}
$beforeConfig = (Get-FileHash $config).Hash
$preferences = Join-Path $profile 'desktop.json'
Set-Content $preferences '{"closeToTray":true,"minimizeToTray":true}'
$beforePreferences = (Get-FileHash $preferences).Hash
$registry = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
New-Item -Path $registry -Force | Out-Null
New-ItemProperty -Path $registry -Name $runKey -Value ('"' + (Join-Path $destination "sharekeys_$Role.exe") + '" --background') -Force | Out-Null
# The other role's startup value must survive this role's uninstall.
$otherBefore = Get-ItemPropertyValue -Path $registry -Name $otherRunKey -ErrorAction SilentlyContinue
if (-not $otherBefore) { New-ItemProperty -Path $registry -Name $otherRunKey -Value 'kvm-installer-test-marker' -Force | Out-Null }
try {
    Install-Package $installerPath "upgrade-$Role.log"
    & "$PSScriptRoot/verify_windows_stage.ps1" -StageDir $destination -Role $Role -ExpectedVersion $Version -IdentityRoot $identityRoot -RequireSigned:$RequireSigned
    if ((Get-FileHash $certificate).Hash -ne $beforeCertificate -or (Get-FileHash $privateKey).Hash -ne $beforeKey -or (Get-FileHash $config).Hash -ne $beforeConfig -or (Get-FileHash $preferences).Hash -ne $beforePreferences) { throw 'Upgrade changed identity or preferences.' }
    if (-not (Get-ItemPropertyValue -Path $registry -Name $runKey)) { throw 'Upgrade removed chosen autostart.' }
    $shell = New-Object -ComObject WScript.Shell
    $shortcuts = @(
        (Join-Path ([Environment]::GetFolderPath('Programs')) "$title/$shortcutName.lnk"),
        (Join-Path ([Environment]::GetFolderPath('DesktopDirectory')) "$shortcutName.lnk")
    )
    foreach ($shortcut in $shortcuts) {
        if (-not (Test-Path $shortcut)) { throw "Missing shortcut: $shortcut" }
        if ($shell.CreateShortcut($shortcut).TargetPath -ne (Join-Path $destination "sharekeys_$Role.exe")) { throw "Wrong shortcut target: $shortcut" }
    }
    $uninstaller = Join-Path $destination 'unins000.exe'
    if ($RequireSigned -and (Get-AuthenticodeSignature $uninstaller).Status -ne 'Valid') { throw 'Uninstaller is not validly signed.' }
    $log = Join-Path $EvidenceDir "uninstall-$Role.log"
    $uninstall = Start-Process -FilePath $uninstaller -ArgumentList @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', "/LOG=`"$log`"") -PassThru -Wait
    if ($uninstall.ExitCode -ne 0) { throw "Uninstall failed ($($uninstall.ExitCode))." }
    $deadline = [DateTime]::UtcNow.AddSeconds(20)
    while ((Test-Path (Join-Path $destination "sharekeys_$Role.exe")) -and [DateTime]::UtcNow -lt $deadline) { Start-Sleep -Milliseconds 200 }
    if (Test-Path (Join-Path $destination "sharekeys_$Role.exe")) { throw 'Uninstall retained executable.' }
    foreach ($shortcut in $shortcuts) { if (Test-Path $shortcut) { throw "Uninstall retained shortcut: $shortcut" } }
    if ((Get-FileHash $certificate).Hash -ne $beforeCertificate -or (Get-FileHash $privateKey).Hash -ne $beforeKey -or (Get-FileHash $config).Hash -ne $beforeConfig -or (Get-FileHash $preferences).Hash -ne $beforePreferences) { throw 'Uninstall changed retained user data.' }
    if (Get-ItemPropertyValue -Path $registry -Name $runKey -ErrorAction SilentlyContinue) { throw 'Uninstall retained this role startup entry.' }
    $otherAfter = Get-ItemPropertyValue -Path $registry -Name $otherRunKey
    $expectedOther = if ($otherBefore) { $otherBefore } else { 'kvm-installer-test-marker' }
    if ($otherAfter -ne $expectedOther) { throw 'Uninstall modified the other role startup entry.' }
    $kind = if ($PreviousInstaller) { 'previous-version upgrade' } else { 'same-version reinstall (not cross-version acceptance)' }
    "PASS: $Role install, isolated installed launch, $kind, retained identity/preferences, shortcuts and uninstall. No physical input was tested." | Tee-Object (Join-Path $EvidenceDir "result-$Role.txt")
} finally {
    if (-not $otherBefore) { Remove-ItemProperty -Path $registry -Name $otherRunKey -ErrorAction SilentlyContinue }
}
