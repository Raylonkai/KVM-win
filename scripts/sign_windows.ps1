param([Parameter(Mandatory = $true)][string[]]$Path)
$ErrorActionPreference = 'Stop'
if (-not $IsWindows) { throw 'Authenticode signing requires Windows and PowerShell 7.' }
$thumbprint = $env:KVM_SIGN_CERT_THUMBPRINT
if ($thumbprint -notmatch '^[A-Fa-f0-9]{40}$') { throw 'Set KVM_SIGN_CERT_THUMBPRINT to an authorized CurrentUser/My code-signing certificate.' }
$certificate = Get-Item "Cert:\CurrentUser\My\$thumbprint"
if (-not $certificate.HasPrivateKey -or $certificate.NotAfter -le (Get-Date)) { throw 'Signing certificate is expired or has no private key.' }
if (-not ($certificate.EnhancedKeyUsageList | Where-Object { $_.ObjectId.Value -eq '1.3.6.1.5.5.7.3.3' })) { throw 'Certificate must permit code signing.' }
$timestamp = $env:KVM_SIGN_TIMESTAMP_URL
if (-not $timestamp) { $timestamp = 'http://timestamp.digicert.com' }
$tool = Get-Command signtool.exe -ErrorAction SilentlyContinue
if ($tool) { $signTool = $tool.Source }
else {
    $sdkTool = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin\*\x64\signtool.exe" | Sort-Object FullName | Select-Object -Last 1
    if (-not $sdkTool) { throw 'Install the Windows SDK signing tools.' }
    $signTool = $sdkTool.FullName
}
foreach ($file in $Path) {
    $target = (Resolve-Path -LiteralPath $file).Path
    & $signTool sign /s My /sha1 $thumbprint /fd SHA256 /tr $timestamp /td SHA256 $target
    if ($LASTEXITCODE -ne 0) { throw "Signing failed: $target" }
    & $signTool verify /pa /all $target
    if ($LASTEXITCODE -ne 0) { throw "Signature verification failed: $target" }
    $signature = Get-AuthenticodeSignature -LiteralPath $target
    if ($signature.Status -ne 'Valid' -or -not $signature.TimeStamperCertificate) { throw "A trusted timestamped signature is required: $target" }
}
