# Optional CI helper. Secrets are supplied only to this step through environment.
$ErrorActionPreference = 'Stop'
if (-not $IsWindows -or -not $env:KVM_SIGN_PFX_BASE64 -or -not $env:KVM_SIGN_PFX_PASSWORD) { throw 'Windows and both signing secrets are required.' }
$temp = Join-Path ([System.IO.Path]::GetTempPath()) ('kvm-sign-' + [guid]::NewGuid() + '.pfx')
$password = $null
try {
    [System.IO.File]::WriteAllBytes($temp, [Convert]::FromBase64String($env:KVM_SIGN_PFX_BASE64))
    $password = ConvertTo-SecureString $env:KVM_SIGN_PFX_PASSWORD -AsPlainText -Force
    # Private key is not marked exportable. Prefer a provider-backed certificate
    # on a dedicated signing runner when organizational policy requires HSM use.
    $imported = @(Import-PfxCertificate -FilePath $temp -CertStoreLocation Cert:\CurrentUser\My -Password $password)
    $certificate = @($imported | Where-Object { $_.HasPrivateKey })
    if ($certificate.Count -ne 1) { throw 'Expected exactly one signing identity.' }
    "KVM_SIGN_CERT_THUMBPRINT=$($certificate[0].Thumbprint)" | Out-File $env:GITHUB_ENV -Append -Encoding utf8
    # Track imported certificates for the workflow's always-run cleanup step.
    "KVM_IMPORTED_CERTIFICATES=$(($imported.Thumbprint) -join ',')" | Out-File $env:GITHUB_ENV -Append -Encoding utf8
} finally {
    if ($password) { $password.Dispose() }
    if (Test-Path -LiteralPath $temp) { Remove-Item -LiteralPath $temp -Force }
    Remove-Item Env:KVM_SIGN_PFX_BASE64, Env:KVM_SIGN_PFX_PASSWORD -ErrorAction SilentlyContinue
}
