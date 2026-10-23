# ShareKeys

Windows-native C++20 keyboard/mouse KVM and keyboard mirroring for one host and
up to five agents. It routes input; it is not remote desktop/video software.
Windows compilation and interactive input remain unverified here; see
[progress and test results](docs/PROGRESS.md).

## Use the desktop app on Windows

Install the ShareKeys release once on each PC. Start **ShareKeys Host (main PC)**
on the keyboard computer and **ShareKeys Agent (remote PC)** on each PC to
control. Start-menu shortcuts launch the GUI, so normal use needs neither a
terminal nor recompiling. Each PC needs first-time setup and pairing. The
repository configures GitHub Actions to build a downloadable installer for
`vX.Y.Z` release tags; no Windows release has yet been published from this
checkout.

1. Launch Host. It creates a P-256 identity and displays its SHA-256 certificate
   fingerprint.
2. On each remote PC, launch Agent. Copy its displayed fingerprint, select its
   private LAN address, enter the host fingerprint, and click **Save and start**.
   Compare the two fingerprints directly on the PCs or through another trusted
   channel; never trust fingerprints delivered only over the network.
3. In Host's Devices tab, enter a unique name, the agent's LAN IP, port (24870 by
   default), and verified agent fingerprint. Click **Pair and connect**. Allow
   ShareKeys through Windows Firewall on the private network if prompted. A
   stable address or DHCP reservation keeps saved devices reachable.
4. Select a device and choose **Control selected PC** for exclusive routing, or
   select mirror targets and use **Enable keyboard MIRROR** / `Ctrl+Alt+M`.
   Mirroring keeps local input active. Mouse mirroring is separate and starts off.
5. Use **EMERGENCY STOP** / `Ctrl+Alt+Esc` to return to Local. Disconnect,
   unhealthy session, or queue overflow also returns routing to Local. Reconnect
   does not automatically resume remote control or mirroring.

Keep the destination application focused and start with Notepad on disposable test
desktops. Both PCs should use the same keyboard layout. Network receipt and
SendInput acceptance do not prove an application performed the intended action.

The GUIs save identities, pairings and settings per user. Host can reconnect
saved devices on launch but starts in Local mode; Agent can listen automatically.
Both have a tray menu and optional **Start with Windows** setting. Login startup
is disabled until selected. Agent's **Unpair host** revokes its saved host.
Windows identity keys are DPAPI-protected. A one-time install avoids rebuilding
for everyday use, but certificates expire after one year and future software or
security releases may require updates. Both PCs need mutual LAN reachability.

Host's **Unpair selected** disconnects a device and removes its configuration.
Certificates last one year; create a new identity and explicitly re-pair both
endpoints after expiry. Never share private keys or automatically trust a changed
fingerprint.

## Developer build

Windows development requires Visual Studio 2022 with Desktop Development with
C++, Windows 10/11 SDK, CMake 3.25+, Git and vcpkg. Dependencies include Asio,
OpenSSL, Qt 6 Core/Widgets/Network and GoogleTest. The manifest pins a vcpkg
baseline; review dependency security updates before deployment.

From a Visual Studio developer PowerShell:

```powershell
git clone https://github.com/microsoft/vcpkg.git C:/dev/vcpkg
C:/dev/vcpkg/bootstrap-vcpkg.bat -disableMetrics
$env:VCPKG_ROOT = 'C:/dev/vcpkg'
cmake --preset windows
cmake --build --preset windows-debug --parallel 2
ctest --preset windows-debug
cmake --build --preset windows-release --parallel 2
ctest --preset windows-release
cmake --install build/windows --config Release --prefix build/stage
```

The Windows workflow tests both configurations, stages dependencies,
smoke-tests both GUIs and packages an Inno Setup installer. It does not inject
input into a CI desktop. With Inno Setup 6 installed, a local developer can build
the installer using:

```powershell
& 'C:/Program Files (x86)/Inno Setup 6/ISCC.exe' "/DStageDir=$pwd/build/stage" installer/sharekeys.iss
```

For Linux development, install a C++20 compiler, Ninja, CMake, standalone Asio,
OpenSSL development files, GoogleTest and Qt 6 Widgets/Network development files:

```sh
cmake --preset linux
cmake --build --preset linux
ctest --preset linux
QT_QPA_PLATFORM=offscreen ./build/linux/sharekeys_host --smoke-test --identity /tmp/sharekeys-smoke
```

If Asio is not installed, CMake downloads a SHA-256-pinned Asio 1.32.0 archive.
Offline builds can set `-DASIO_INCLUDE_DIR=/path/to/asio/include`. Set
`-DSHAREKEYS_BUILD_GUI=OFF` for portable backend/tests. Linux builds do not
capture/inject desktop input; TLS tests need loopback sockets. See
[architecture](docs/ARCHITECTURE.md) for ownership and protocol details.

The installer is per-user, has Start menu shortcuts for Host and Agent, and
includes an uninstaller; desktop shortcuts are optional. Uninstall removes
installed binaries and shortcuts while retaining per-user identity/configuration
for deliberate recovery or removal. The installer, Windows GUI, DPAPI, hooks,
SendInput and interactive behavior have not been validated in this environment.

See [Windows acceptance instructions](docs/WINDOWS_TESTING.md) before real use.
Secure desktops, Ctrl+Alt+Delete, login screens, protected input and bypassing
UAC/UIPI are unsupported. Equal privilege levels are required for ordinary
injection. AltGr, Pause/Break, unusual keyboards, mixed DPI and pointer
acceleration need Windows acceptance testing. `sharekeys_agent_cli.exe` remains
available for developer diagnostics; normal operation uses the desktop GUIs.
