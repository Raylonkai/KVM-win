# Build and publish a Windows release

This release pipeline produces two role-specific x64 per-user installers:

- `KVM-Win-Setup-X.Y.Z.exe`, containing `sharekeys_host.exe`.
- `KVM-Agent-Setup-X.Y.Z.exe`, containing `sharekeys_agent.exe`.

Both bundle their own Qt 6 Widgets/Network runtime, required Qt platform plugins,
OpenSSL DLLs, vcpkg runtime DLLs and licenses/notices, and app-local MSVC runtime.
The developer console Agent and test executables are excluded. SHA256SUMS hashes
the final installer bytes after optional signing. The release manifest records the
source commit/dirty flag, dependency baseline, build image/tool metadata and
per-role file hashes. Hosted-runner install/reinstall/uninstall tests are required
before an artifact is published. These checks are not a native input or five-PC
acceptance test.

## Versions and dependencies

Before release, make all three values equal:

- `project(ShareKeys VERSION ...)` in CMakeLists.txt.
- `version-string` in vcpkg.json.
- The manual dispatch version input, in `X.Y.Z` form.

The build workflow checks the vcpkg checkout against the exact 40-character
`builtin-baseline`, boots it, uses the Visual Studio 2022 x64 generator on
`windows-2022`, builds/tests Debug and Release, deploys Qt to the application root,
packages each role and records the resolved build details. The runner image itself
is maintained by GitHub, so the manifest records its image version; this is a
reproducible dependency/build recipe, not a byte-for-byte reproducibility claim.
Retain test XML, install logs, manifests and SHA-256 files with the release record.

A Windows developer PowerShell can build and stage without publishing:

```powershell
# Use a vcpkg checkout at the exact `builtin-baseline` in vcpkg.json.
$env:VCPKG_ROOT = 'C:/dev/KVM-win/.vcpkg'
& "$env:VCPKG_ROOT/bootstrap-vcpkg.bat" -disableMetrics
cmake --preset windows
cmake --build --preset windows-debug --parallel 2
ctest --preset windows-debug
./scripts/package_windows.ps1
```

The package script builds Release, runs CTest, stages Qt/dependencies, verifies
both staged GUI PE subsystems/metadata and isolated startup, signs if requested,
builds both Inno Setup installers, writes SHA256SUMS/manifest and inventories each
role. Output is `build/distribution/`. It requires fresh staging/output directories
to prevent stale files being mistaken for this release. See
[INSTALLATION.md](INSTALLATION.md) for user steps.

## Manually publish a GitHub Release

The workflow does **not** publish on pushes or tags. After its workflow file is on
the repository's default branch and the selected source commit contains the version
updates, a maintainer with Actions write access can:

1. Open **Actions → Windows Release → Run workflow** and choose the reviewed branch.
2. Enter `X.Y.Z` matching the source. Keep **Publish a GitHub Release** unchecked
   for a build/test/artifact run. Check it only to make the artifacts public.
3. Optionally check **Sign installers and binaries** after configuring
   `release-signing` as below. Choose whether the published release is a prerelease.
4. Wait for build, Release CTest, isolated payload launch, installer installation,
   same-version reinstall, settings/identity retention, shortcuts and uninstallation
   to pass. Review the validation artifact, both installer files, release manifest
   and SHA256SUMS before distributing them.
5. When Publish was selected, the separate least-privilege job verifies the source
   commit, clean source tree, exact two filenames and SHA256 hashes, then creates
   the `vX.Y.Z` release at that commit and attaches both installers, checksum file,
   manifest and installation guide. An existing version tag pointing elsewhere
   fails closed; GitHub also rejects recreating an already published release.

The manual-dispatch workflow must already exist on the default branch before it
appears in Actions. A test build without publish still uploads the same installer
artifacts for review. Follow the organization's code review and release approvals;
this workflow does not bypass protected branches or environment rules.

GitHub documents [manually dispatching workflows](https://docs.github.com/en/actions/how-tos/manage-workflow-runs/manually-run-a-workflow)
and the [GitHub CLI release command](https://cli.github.com/manual/gh_release_create).
This project uses `workflow_dispatch`, not automatic release-tag publication.

## Optional Authenticode signing

The default pipeline creates unsigned files. No certificate or signing secret is
committed. Signing uses Microsoft SignTool SHA-256 file digests plus an RFC 3161
timestamp, then verifies each signature. Inno Setup signs each Setup and generated
uninstaller through its compiler SignTool hook.

For hosted signing, add a protected GitHub environment named `release-signing`.
Restrict its reviewers/branches according to organization policy and add these
**environment secrets**:

- `KVM_SIGN_PFX_BASE64`: base64 of the authorized code-signing PFX.
- `KVM_SIGN_PFX_PASSWORD`: PFX import password.

Optionally define `KVM_SIGN_TIMESTAMP_URL` as an organization-approved RFC 3161
service URL. Do not use a self-signed certificate for public distribution. The
import step deletes the temporary PFX and removes its secret variables; the
workflow's `always()` cleanup removes the imported private-key certificate from
the ephemeral runner. Prefer an organization-approved dedicated signing service
or isolated protected runner if policy forbids exporting private keys as CI secrets.
Do not print secrets, put passwords in shell arguments/logs, grant the publish job
signing secrets, or store a PFX in the repository or an artifact. Signing runs only
on the manually dispatched build job when the opt-in is selected. The release job
has `contents: write` and no certificate access.

The certificate subject must identify the actual publisher, be trusted by the
target Windows systems, be currently valid and allow Code Signing EKU. Public
certificate provisioning and certificate purchase are organization decisions.
Microsoft's [SignTool guidance](https://learn.microsoft.com/windows/win32/seccrypto/signtool)
recommends SHA-256 file/timestamp digests; Inno documents its [SignTool hook](https://jrsoftware.org/ishelp/topic_setup_signtool.htm)
and [signed uninstaller](https://jrsoftware.org/ishelp/topic_setup_signeduninstaller.htm).
Windows reputation/SmartScreen can still warn for a new publisher. A checksum
verifies file integrity; Authenticode provides the signed publisher identity.

## Release validation and limits

The workflow performs Windows compilation/tests, root-dependency staging, x64 GUI
subsystem/version checks, and launches each installed GUI without development PATH
or Qt plugin environment paths. On an ephemeral hosted runner it also installs,
reinstalls the same version and uninstalls each package, verifying shortcuts,
per-user identity/preferences, and the role-specific startup entry. This is an
installation/upgrade-path smoke check; same-version reinstall does not demonstrate
a cross-version upgrade. Supply a previously published `-PreviousInstaller` to the
local installer test for explicit old-to-new upgrade verification.

No GitHub-hosted check here types into a native Windows foreground application,
starts five physical Windows PCs, validates edge/DPI behavior, proves logon startup
interactively or installs on a completely clean Windows VM. The GUI smoke explicitly
skips input hooks/listening. Link a dated acceptance record from
[WINDOWS_TESTING.md](WINDOWS_TESTING.md) before calling those gates passed.

The agent listens on user-selected private IPv4 TCP `24870` by default. The Host
initiates outbound TCP to that Agent. Only allow the Agent on a trusted private
Windows Firewall profile when policy permits; this pipeline does not create or
weaken firewall rules. See [INSTALLATION.md](INSTALLATION.md) for pairing and user
instructions.
