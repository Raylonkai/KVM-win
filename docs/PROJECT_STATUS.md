# KVM-Win / ShareKeys project status

Audit date: 2026-10-26. Product specification: [SPEC.md](SPEC.md).
ShareKeys is the existing product/target/configuration name in this repository;
KVM-Win is the repository and desktop display name for this revision. Existing
identity directories, executable names and installer AppId are preserved.

## A. Executive summary

**Current revision:** the existing desktop applications have been extended, not
rebuilt. Dashboard status, a real mirror toggle, explicit tray/exit preferences,
diagnostic history/export, a Qt controller adapter, icon/version resources and
Windows staging/installer verification infrastructure are now implemented.
Host worker exceptions and queued activation after emergency have safety fixes.
Both GUIs compile and run offscreen on Linux, including TLS-backed widget tests.
Windows compilation, launch, physical input and installer execution for this
revision are **not validated**; no Windows binary/installer is delivered from this
Linux environment. Readiness remains laboratory alpha, not a trading-desk release.

### Baseline found before implementation

This is a substantial C++20 prototype, not an empty scaffold. It already contains
Qt 6 Widgets host and agent applications, Win32 keyboard/mouse hooks and SendInput
sinks, TLS-authenticated TCP connections, a Local/Remote/Mirror router, five-peer
broadcasting, reconnect, groups, grid edge switching and key-release recovery.
The GUI was already wired to the actual backend. Do not rebuild these components.

On this Linux machine, the baseline configured and built successfully and **35/35
GoogleTests passed** on 2026-10-26. The first restricted run passed 26 and failed
9 because socket creation returned `Operation not permitted`; the authorized
loopback rerun passed all 35 in 4.06 s. Network tests use real TLS but fake input
sinks. They demonstrate protocol/routing behavior, not Windows input delivery.
No line/branch coverage percentage was measured.

The baseline has GUI-subsystem CMake targets and startup error dialogs, so its
intended entry point is a normal double-click desktop application. It lacks custom
icons and VERSIONINFO. Its deployment paths disagree (root executables versus Qt
`bin` defaults). Both windows silently hide to the tray on close without a saved
choice. Discovery is absent; explicit IP/fingerprint entry works in code.

Prior PROGRESS.md reports a Windows Debug/Release build and 35 tests at revision
40ee415, followed by failed install staging. That is historical repository
reporting, not a Windows run observed in this audit. This environment has clang-cl
but no verified Windows SDK/link/runtime, MSVC installation, Wine or Inno compiler.
No Windows executable or installer has been produced or launched in this audit.
Usability on real Windows machines and production safety remain **unverified**.
Maturity: engineering prototype approaching a laboratory alpha, not a release.

## Repository inventory and architecture

| Area | Actual implementation |
|---|---|
| Languages/build | C++20, CMake, JSON, GitHub Actions YAML, Inno Setup; no web stack |
| Dependencies | Qt >=6.4 Core/Widgets/Network; standalone Asio (fallback pinned 1.32.0); OpenSSL TLS/Crypto; GoogleTest; platform threads. vcpkg.json pins a baseline |
| Entry points | apps/host/main.cpp:16 Qt host; apps/agent/gui_main.cpp:16 Qt agent; apps/agent/main.cpp:18 developer console agent; tests/integration/windows_input.cpp:9 opt-in F24 harness |
| Libraries | sharekeys_core: types/state/codec/router/workspace; sharekeys_network: Asio/TLS/HostEngine/AgentService; sharekeys_input: Windows hooks/sinks; sharekeys_desktop: Qt settings |
| Presentation | src/gui/host_window.cpp:47 and src/gui/agent_window.cpp:24, existing dark Widgets windows; desktop_settings.cpp provides JSON and login startup |
| Windows APIs | SetWindowsHookExW WH_KEYBOARD_LL/WH_MOUSE_LL, GetMessageW, PostThreadMessageW, SendInput, GetCursorPos/GetSystemMetrics, CreateFileW, CryptProtectData/CryptUnprotectData; Qt QSettings wraps HKCU Run |
| Configuration | Per-user host.json and authorized-host.json, QSaveFile atomic writes; identity.crt and DPAPI-protected identity.key on Windows; QLockFile for GUI single-instance checks |
| Installer/CI | Separate per-user Inno host/agent scripts; PowerShell Release staging/signing/verification; Windows build and explicitly dispatched release workflows; Linux preview/tests/analyzer/sanitizers |

### One physical keyboard event, end to end

These references identify functions in the current working tree. Windows-only
steps have not been executed here.

1. `KeyboardHook::run` (`src/input/win32_keyboard.cpp:27`) installs WH_KEYBOARD_LL
   on its own message-loop thread. `procedure` (:42) reads KBDLLHOOKSTRUCT,
   excludes injected/tagged events and constructs scan/VK/extended/down data.
2. Host main's KeyboardHook callback calls `HostEngine::captureKey`
   (`src/core/host_engine.cpp:186`). `ShortcutGate::process`
   (`src/routing/router.cpp:75`) consumes shortcuts and balances local suppression.
   Capture publishes a record into `MpscQueue<CapturedInput,2048>`
   (`include/sharekeys/queue.hpp:14`), never doing socket I/O in the hook.
3. `HostEngine::pump` (`src/core/host_engine.cpp:206`) consumes on the Asio thread every millisecond and
   calls `InputRouter::processKeyboardEvent` (`src/routing/router.cpp:50`).
   `recipients` (:6) selects one connected remote or selected mirror peers.
   Mirror capture returns false for normal keys, preserving local execution.
4. The router's send callback in `HostEngine::HostEngine` (:5) calls `Peer::send`
   (`src/network/network.cpp:40`); `enqueue` (:32) assigns session/sequence and
   calls `protocol::encode` (`src/protocol/protocol.cpp:31`). `Peer::write` (:43)
   asynchronously writes the TLS stream; each peer has a bounded queue.
5. Remote `Peer::readPrefix` (`src/network/network.cpp:52`) reads a bounded four-byte length and body;
   `receive` (:68) checks order, session, handshake state and rate before dispatch.
   `AgentService::accept` (`src/core/agent_service.cpp:42`) installs the receiver
   that decodes KeyDown/KeyUp and calls `InjectedKeyboard::apply`
   (`src/core/keyboard.cpp:11`).
6. `Win32KeyboardSink::send` (`src/input/win32_keyboard.cpp:57`) builds INPUT,
   preserves scan/extended/up flags, tags it and checks SendInput's return value.
   `InjectedKeyboard` records accepted downs and releases tracked input on
   transition/disconnection/timeout/service shutdown. This does not prove that
   the foreground application acted on input.

### Transport, security and thread model

The host connects to agents listening on explicitly configured private IPv4
addresses (default TCP 24870). No broadcast discovery, DNS search or mDNS exists.
TLS >=1.3 uses P-256 self-signed identities and exact mutually pinned SHA-256 leaf
certificates with validity checks (`src/security/security.cpp:69`, `src/network/network.cpp:6`). Pairing is
manual out-of-band fingerprint exchange. PAIR_REQUEST/PAIR_APPROVED are reserved
wire types, rejected on established sessions; no on-wire pairing wizard exists.
The agent authorizes one host; host slots cap at five. Revocation closes the local
session and clears authorization. Credentials are protected with user DPAPI.

Frames are versioned, explicit big-endian fields: u32 length; u16 version/type;
u64 session/sequence; typed payload. Body limit 4096; per-peer output 1024 frames;
input rate 2000 messages/s. HELLO/AUTHENTICATE bind commands to a random session;
strict sequence rejects repeats/gaps. TCP_NODELAY, one-second heartbeats,
three-second inactivity threshold and 250 ms–30 s reconnect backoff exist.

Host keyboard/mouse hooks are separate producers of a bounded MPSC queue. One
Asio thread owns router, peers, device slots and reconnect timers. GUI commands
post to that thread; status copies use a mutex. Agent injection/networking share
one service thread; physical-state hooks use atomics. Qt timers sample snapshots
on the GUI thread. Ownership is RAII; hooks/sinks outlive callbacks that use them.
GUI rendering does not perform synchronous TCP reads or writes. GUI startup does
synchronously generate/load local identity files and agent start binds a socket.

## B. Feature matrix — current revision

“IMPLEMENTED AND TESTED” here is explicitly scoped to portable tests. It does not
imply physical Windows acceptance. No feature is assigned BROKEN solely because
Windows execution is unavailable.

| Feature | Status | Evidence | Next action |
|---|---|---|---|
| Keyboard capture | IMPLEMENTED BUT UNVERIFIED | win32_keyboard.cpp:27,42; hook/message thread | Physical Windows capture/stress |
| Mouse capture | IMPLEMENTED BUT UNVERIFIED | win32_mouse.cpp:32,42 | DPI, polling rates, movement |
| Remote keyboard injection | IMPLEMENTED BUT UNVERIFIED | win32_keyboard.cpp:57; state tested with fake sinks | SendInput + focused Notepad |
| Remote mouse injection | IMPLEMENTED BUT UNVERIFIED | win32_mouse.cpp:69; fake-sink cleanup tests | Interactive buttons/wheels |
| TCP networking | IMPLEMENTED AND TESTED | network_test.cpp; real TLS loopback | Real LAN/firewall/soak |
| Multi-PC support | IMPLEMENTED AND TESTED | Tls.FiveAgentsReceiveMirrorEventsAndEmergencyCleanup | Five distinct Windows agents |
| Keyboard mirroring | IMPLEMENTED BUT UNVERIFIED | Five-peer TLS and GUI toggle tests pass with fake sinks; physical path unverified | End-to-end Windows mirror |
| Device selection | PARTIALLY IMPLEMENTED | Saved records/checked mirror targets used by real widget test; add/unpair and native selection need interactive validation | Complete GUI/manual matrix |
| Hotkey switching | PARTIALLY IMPLEMENTED | ShortcutGate tests; Ctrl+Alt with configurable VK only | Windows interception; richer chords |
| GUI | IMPLEMENTED AND TESTED | Linux Debug/Release and offscreen host/agent startup plus three widget/TLS scenarios pass | Windows graphical/tray/focus acceptance |
| Security/pairing | PARTIALLY IMPLEMENTED | Mutual pins/TLS/rejection tests; Windows DPAPI unverified | Renewal, hostile-client testing |
| Automatic reconnection | IMPLEMENTED AND TESTED | Five-peer test restarts listener; Local after reconnect | Restart/loss on real LAN |
| Windows installer | IMPLEMENTED BUT UNVERIFIED | installer/host.iss, installer/agent.iss, installer/common.iss; split payload and Inno packaging in scripts/package_windows.ps1; disposable install/upgrade/uninstall check in scripts/test_windows_installer.ps1 | Run Windows workflow; test cross-version upgrade and clean VM |
| Discovery | NOT IMPLEMENTED | No discovery service/protocol | Clearly label manual pairing |
| Groups | PARTIALLY IMPLEMENTED | DeviceGroups tested; GUI separately stores JSON groups | Reconcile duplicate representation |
| Workspace edges | PARTIALLY IMPLEMENTED | Grid adjacency unit tests, native cursor APIs | Mixed DPI/display gaps |
| Emergency/cleanup | PARTIALLY IMPLEMENTED | Router, fake sinks, agent stop tests | GUI-close TLS regression; native failure tests |
| Login startup | IMPLEMENTED BUT UNVERIFIED | desktop_settings.cpp:75 HKCU Run | Standard-user login test |
| Logs/diagnostics | PARTIALLY IMPLEMENTED | Live status/retries/RTT, bounded transition history, export and build information | Export acceptance; persistent structured logging if required |
| App icon/version | IMPLEMENTED BUT UNVERIFIED | PNG loaded in Qt preview; seven-size ICO + configured RC VERSIONINFO | Explorer/taskbar/native resource verification |
| Explicit close preference | IMPLEMENTED AND TESTED | Preferences persist; default window close releases fake-sink modifier over TLS | Interactive Windows tray/minimize/logoff |
| Local mode | IMPLEMENTED AND TESTED | Routing unit tests and GUI emergency/close tests | Native suppression acceptance |
| Single-PC remote mode | IMPLEMENTED BUT UNVERIFIED | Router/AgentService tests pass; Windows capture/injection exists | Physical focus/suppression and mouse tests |
| Signing | NOT IMPLEMENTED | No certificate/sign pipeline | Release requirement |

## C. Technical debt and risks

This table records baseline findings and their disposition in this revision.
References in unresolved rows refer to current functions or the audited baseline
revision 07048325; the end-to-end trace above uses current exact source lines.

| Severity | Finding and supporting reference | Required disposition |
|---|---|---|
| High | Deployment mismatch: CMakeLists.txt installs executables at `.` while Qt deploy defaults CMAKE_INSTALL_BINDIR to `bin`; DLLs/qt.conf may be elsewhere | FIXED IN SOURCE: root layout aligned and isolated verifier added; Windows execution pending |
| High | HostEngine thread runs io_.run without exception boundary (host_engine.cpp:13). Allocation/crypto/handler exceptions can terminate process while input held | FIXED AND PORTABLY TESTED: HostEngine::fail at host_engine.cpp:17 restores Local, closes peers and publishes restart-required status |
| High | Emergency invalidates captured-input epochs but queued GUI mode commands are unversioned (host_engine.cpp:103,111,117); a queued command can activate after stop | FIXED AND PORTABLY TESTED: epoch checks in host_engine.cpp:122,133,149; queued activation regression |
| High | Windows native runtime and installer acceptance absent; special keys, UIPI and DPI unknown | Required Windows release gate, not an inferred success |
| Medium | Host destructor queues releases then immediately closes/stops (host_engine.cpp:15); release frames need not drain | TLS held-modifier destruction test now passes; still rely on close/timeout, not guaranteed release-frame delivery |
| Medium | Forced agent termination/SendInput rejection cannot guarantee release; physical state only observed after hook startup (keyboard.cpp:12,23) | Document limits and test graceful/failure paths |
| Medium | Hook GetMessage/PostThreadMessage results not fully handled and silent hook removal not detected (win32_keyboard.cpp:18,36; win32_mouse.cpp:28,38) | Hook health and error reporting |
| Medium | One pending agent authentication monopolizes listener for up to 5 s (agent_service.cpp:45; network.cpp:111); repeated untrusted LAN handshakes can deny availability | Bounded parallel admission/rate limits; hostile LAN test |
| Medium | Device JSON lacks comprehensive uniqueness/type/address validation (host_window.cpp:200); failed shortcut edits can later persist on unrelated save | Un-applied shortcut persistence fixed; comprehensive host-record validation still open |
| Medium | CLI duplicates AgentService and does not share GUI process lock (apps/agent/main.cpp:82) | Unify CLI on service and identity locking |
| Medium | Close silently hides and agent keeps accepting input (agent_window.cpp:126) | FIXED: default exit and saved opt-in tray; default cleanup tested with TLS/fake sinks, actual tray unverified |
| Medium | Identity PEM copies/BIO and exceptional paths not all cleansed (security.cpp:73,95,100); partial file creation needs manual recovery | Sensitive-buffer RAII, atomic creation/recovery |
| Medium | Certificate expiry after one year has no GUI renewal workflow (security.cpp:83) | Expiry indication and deliberate re-pairing |
| Medium | No fuzzing, measured coverage, long soak or latency bounds | Add measurement and fault-injection campaign |
| Low | GUI directly combines widget creation, JSON devices/groups and commands (host_window.cpp:32); DeviceGroups model unused by GUI | HostController adapter added; device JSON and group model unification still open |
| Low | Diagnostics overwrite prior events; version/host summary absent | IMPLEMENTED: bounded transition history/export and build metadata |

No exploitable remote memory-corruption defect was established by this review;
that is not a proof of absence. Bounded parser/queues, RAII and thread confinement
are strengths. Callback captures require owner lifetime discipline; shutdown tests
should accompany future changes. High-rate mice can exceed the intentional rate
limit and disconnect rather than silently drop key-up events.

## D. Readiness (0 = unusable/unvalidated, 5 = validated for that setting)

| Setting | Score | Evidence |
|---|---:|---|
| Local development | 4/5 | Debug/Release build, 40 CTest entries incl. three GUI scenarios; native target unavailable |
| Multi-PC laboratory | 2/5 | Secure five-peer simulation exists; needs MSVC packaging and interactive PCs |
| Internal demonstration | 2/5 | Linux UI/backend can be demonstrated; Windows demonstration not established |
| Controlled pilot | 1/5 | No completed clean-machine installer or Windows acceptance/soak |
| Production trading desk | 0/5 | No native correctness/latency/failure acceptance or signed release; input execution cannot be inferred from ACK |

## Revision results

See [PROGRESS.md](PROGRESS.md) for commands, test counts and tool/environment
failures. The GUI integration test uses real Peer/HostEngine/AgentService instances,
real TLS and fake input sinks. It verifies mirror enable/local preservation,
emergency release, close with a held modifier, agent stop and an unavailable
remote. Existing five-peer TLS mirror/order/reconnect coverage is retained.
Windows native paths, DPAPI, resources, staged DLL deployment, Inno compilation,
installer execution and interactive acceptance remain unexecuted here.

Additional source map: HostWindow `src/gui/host_window.cpp`, HostController
`src/gui/host_controller.cpp:5`, AgentService `src/core/agent_service.cpp:5`,
Windows GUI flags `CMakeLists.txt:87`, RC `resources/windows/application.rc.in`,
Inno entry points `installer/host.iss` and `installer/agent.iss` (shared setup in
`installer/common.iss`), Release packaging `scripts/package_windows.ps1`, install
round-trip `scripts/test_windows_installer.ps1`, and isolated stage verification
`scripts/verify_windows_stage.ps1`. All changes are uncommitted; no push, remote CI
dispatch or release publication occurred.
