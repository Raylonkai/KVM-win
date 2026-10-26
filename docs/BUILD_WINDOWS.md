# Build and package KVM-Win for Windows

This release pipeline targets Windows 10/11 x64 and PowerShell 7. It uses the
existing CMake targets `sharekeys_host` and `sharekeys_agent`, Qt 6 Widgets,
OpenSSL, standalone Asio, MSVC, vcpkg, and Inno Setup 6. The developer-only
`sharekeys_agent_cli` is built for development but excluded from end-user packages.
This Linux checkout cannot produce or validate Windows binaries; see
[RELEASE.md](RELEASE.md) for the state and the GitHub workflow instructions.

## Prerequisites

- Windows 10/11 x64, Visual Studio 2022 with Desktop Development with C++, and a
  Windows SDK.
- CMake 3.25 or later, Git, PowerShell 7, Python 3, and Inno Setup 6.
- A vcpkg checkout at the exact `builtin-baseline` commit in `vcpkg.json`, bootstrapped
  with its `bootstrap-vcpkg.bat`.

Use a VS 2022 Developer PowerShell and set `VCPKG_ROOT` to that pinned checkout.
The wrapper checks the vcpkg Git commit and product versions before packaging.
Dependencies are configured from the existing vcpkg manifest and CMake preset.

## Reproducible local Release package

```powershell
$env:VCPKG_ROOT = 'C:/dev/vcpkg'
./scripts/package_windows.ps1 -Version 0.1.0
```

The version must match both `CMakeLists.txt` and `vcpkg.json`. Use fresh
`-StageDir` and `-OutputDir` paths if overriding defaults; existing paths are
refused to prevent stale files from entering a release. The script configures the
Windows preset, builds the Release targets, runs CTest, installs the runtime tree,
checks x64/GUI-subsystem/version metadata and runtime dependencies, launches each
GUI in isolated offscreen smoke mode, splits the Host and Agent payloads, and
compiles their installers.

Outputs in `build/distribution/`:

- `KVM-Win-Setup-0.1.0.exe` — `sharekeys_host.exe` and its Qt/plugins/runtime files.
- `KVM-Agent-Setup-0.1.0.exe` — `sharekeys_agent.exe` and its Qt/plugins/runtime files.
- `SHA256SUMS`, `release-manifest.json`, `tests-release.xml`, and `INSTALLATION.md`.

The staged role payloads are under `build/package-<id>/host` and `/agent` by
default. Qt deployment scripts copy Qt runtime DLLs and platform plugins. CMake
installs app-local vcpkg DLLs (including OpenSSL), gathers the MSVC runtime, and
collects package license files. Inspect `release-manifest.json` and each staged
payload inventory for the actual dependency list for a given build.

## Optional code signing

The release script supports signing with a certificate already imported into the
current user's Windows certificate store:

```powershell
$env:KVM_SIGN_CERT_THUMBPRINT = '<certificate SHA-1 thumbprint>'
$env:KVM_SIGN_TIMESTAMP_URL = 'http://timestamp.digicert.com'
./scripts/package_windows.ps1 -Version 0.1.0 -Sign
```

The certificate must be valid, have the Code Signing EKU, and contain an accessible
private key. The script signs staged EXEs/DLLs and the final installers, timestamps
signatures, and verifies them. Never put PFX passwords or private key material in
source or command-line arguments. The workflow imports a PFX from protected GitHub
secrets and removes the temporary certificate after the job. See [RELEASE.md](RELEASE.md)
for secret setup and release controls. Do not describe an unsigned package as signed.

## Installer checks

The CI workflow installs each package into a path containing spaces on a disposable
Windows runner, checks Start and desktop shortcut targets, launches the installed
GUI with development paths removed, repeats installation, checks settings and
identity retention, uninstalls, and checks cleanup. To test a real cross-version
upgrade, give the script a previous version installer:

```powershell
./scripts/test_windows_installer.ps1 `
  -Installer ./build/distribution/KVM-Win-Setup-0.1.0.exe `
  -PreviousInstaller ./previous/KVM-Win-Setup-0.0.9.exe `
  -Role host -Version 0.1.0 -EvidenceDir ./build/installer-evidence
```

This requires a disposable Windows account without that role already installed.
The GitHub workflow's same-version reinstall is not proof of a cross-version
upgrade. Neither this smoke test nor hosted install/uninstall checks prove physical
keyboard/mouse input, multi-PC connectivity, interactive tray behavior, or clean
Windows VM compatibility. Those require the Windows acceptance matrix in
[WINDOWS_TESTING.md](WINDOWS_TESTING.md).

## Troubleshooting

- Missing Qt plugin/runtime: keep the installed app directory intact and inspect
  `qt.conf`, `platforms/qwindows.dll`, and `release-manifest.json`.
- Installer rejects a shared install path: install Host and Agent to separate
  directories.
- Firewall prompt or unavailable agent: allow the agent only on the trusted private
  network according to local policy. TCP port 24870 is the default; no firewall rule
  is silently added.
- Windows SmartScreen may show an unknown publisher for unsigned packages. Code
  signing is required for a trusted publisher identity and should be completed
  before general distribution.
