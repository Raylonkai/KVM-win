# KVM-Win / ShareKeys

Windows-native C++20 software KVM for one host and up to five remote PCs. Keyboard
Mirror Mode forwards physical keyboard events to selected agents while preserving
local input. This is input routing, not remote desktop/video streaming.

The repository already had a Qt host, Qt agent and secure backend; this revision
preserves them and adds desktop polish, lifecycle safeguards and packaging checks.
**Windows input and installer acceptance remain unverified.** Linux builds/tests
and offscreen GUI integration are successful; see the [audit and feature matrix](docs/PROJECT_STATUS.md)
and [executed test record](docs/PROGRESS.md). No Windows installer was built during
this Linux audit and no release was published.

## Install and open on Windows

Download `KVM-Win-Setup-X.Y.Z.exe` for the keyboard PC and `KVM-Agent-Setup-X.Y.Z.exe`
for each remote from a successful Windows Release workflow or GitHub Release. Follow
the nontechnical [installation guide](docs/INSTALLATION.md) and verify installer
hashes using the accompanying `SHA256SUMS`. The installers are unsigned unless the
release notes explicitly say a valid code-signing certificate was used.

The host and agent installers are separate, per-user installs. The host GUI is
launched from Start or its optional desktop shortcut. The remote agent runs in the
logged-in user's interactive session, with login startup opt-in. Pairing is manual
and uses certificate fingerprints exchanged through a trusted channel; automatic
device discovery is not implemented. The installer does not change Windows Firewall
policy.

## Build and package

Windows: Visual Studio 2022 C++/SDK, CMake >=3.25, vcpkg, Inno Setup 6 and PowerShell 7.
With VCPKG_ROOT pointing to a bootstrapped vcpkg checkout, run from a VS developer
PowerShell in the repository:

```powershell
./scripts/package_windows.ps1
```

This builds and tests Release, stages Qt/OpenSSL/MSVC dependencies, verifies isolated
GUI startup, and generates separate host and agent Inno installers with SHA-256
checksums. [BUILD_WINDOWS.md](docs/BUILD_WINDOWS.md) and [RELEASE.md](docs/RELEASE.md)
describe local packaging and explicitly triggered GitHub releases. End users do not
run the build script.

Linux development requires a C++20 compiler, Ninja, CMake, OpenSSL, GoogleTest,
Qt 6 Widgets/Network and standalone Asio headers:

```sh
cmake --preset linux
cmake --build --preset linux --parallel 2
ctest --preset linux
QT_QPA_PLATFORM=offscreen ./build/linux/sharekeys_host --smoke-test --identity /tmp/kvm-win-host-smoke
QT_QPA_PLATFORM=offscreen ./build/linux/sharekeys_agent --smoke-test --identity /tmp/kvm-win-agent-smoke
```

CMake downloads SHA-256-pinned Asio 1.32.0 if headers are unavailable. Offline builds
can set `-DASIO_INCLUDE_DIR=/path/to/asio/include`. `-DSHAREKEYS_BUILD_GUI=OFF` builds
portable backend/tests on Linux. TLS tests need loopback socket access. Linux is a
configuration/testing preview and does not capture or inject native desktop input.

## Understand and validate the project

- [Project status](docs/PROJECT_STATUS.md): full audit, feature evidence, code trace,
  technical debt and readiness scores.
- [Architecture](docs/ARCHITECTURE.md): startup, ownership, protocol and shutdown.
- [GUI design](docs/GUI_DESIGN.md): controls, explicit lifecycle preferences and limits.
- [Windows acceptance](docs/WINDOWS_TESTING.md): physical one-host/five-agent matrix.
- [Progress](docs/PROGRESS.md) and [ranked roadmap](docs/ROADMAP.md).

Secure desktops, Ctrl+Alt+Delete, login screens and UAC/UIPI bypass are unsupported.
Special keys, mixed DPI, high-rate mice and physical/injected key coexistence still
need Windows acceptance. SendInput success and protocol RTT do not prove an
application executed a business action. Forced termination of the agent cannot
run its cleanup. Production trading-desk deployment is not validated.
