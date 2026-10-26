# Desktop GUI design

The existing Qt 6 Widgets implementation is retained and extended. The desktop
name is KVM-Win; ShareKeys remains the internal product identity and executable
prefix. The UI uses Fusion with a dark slate palette, light text, blue actions,
a red emergency stop and an amber mirror indicator. `resources/app.svg` describes
the keyboard/three-screen icon; generated PNG is embedded in Qt and ICO in Windows.

## Host

`src/gui/host_window.cpp` creates these views:

- Persistent title/mode strip and Emergency stop; a separate non-focusable,
  always-on-top mirror banner remains visible when the main window is minimized.
- Dashboard: host machine name, connected count, current mode, mirror state,
  selected PC and online mirror targets; Local, selected Remote and mirror toggle.
  Mode actions require an online target and available native input. Local remains
  available. Mouse mirror is independent and off on launch.
- Devices: up to five saved names/IPs/ports/pins with current connection state,
  checked mirror membership, manual pair/connect/disconnect/unpair. Discovery is
  explicitly disabled and labeled unimplemented. Pins require trusted comparison.
- Workspace: existing grid layout and opt-in screen-edge switching; one combined
  desktop per cell. This is not monitor-perfect geometry or video streaming.
- Settings: existing Ctrl+Alt trigger configuration and groups, reconnect preference,
  per-user login startup, saved close/minimize-to-tray choices. Applied shortcuts,
  rather than un-applied edits, are persisted. Defaults remain M/Esc/Tab.
- About / Diagnostics: version/platform/compiler/configuration/Qt information,
  connection health, retries, protocol RTT, latest event and a bounded 500-block
  transition history that can be exported. Captured keys are never added to it.
  History is in memory until explicitly exported; it is not a persistent log service.

No status claims business-action confirmation. On Linux, routing buttons and mouse
mirror are disabled in the application; the preview allows configuration and TCP
connections. GUI tests explicitly supply fake-sink input capability.

## Agent

`src/gui/agent_window.cpp` preserves the setup window: local fingerprint, copy
button, verified host pin, private address selection, port, save/start, stop/unpair,
automatic listening and login startup. Native listening is disabled if no input
service factory exists (Linux preview/smoke). Version information and close/minimize
preferences are added. Live status comes from AgentService's snapshot every 200 ms.

## Lifecycle and safety

Close exits by default. The setting **Closing the window keeps … in the tray** is
explicit and saved per application in desktop.json. If there is no system tray,
close always exits even when the preference is checked. Host close always requests
emergency Local mode and release, including when hiding. Agent close stops its
service unless the user selected continued operation in the tray. Tray Quit always
stops/exits. Minimize-to-tray is independent; minimizing preserves current routing.
The active mirror banner makes that visible. Startup uses `--background` only for
optional login startup and shows a window when no tray is available.

Qt owns widgets. HostWindow owns a HostController adapter which emits sampled
statusChanged snapshots on the GUI thread every 100 ms. Commands delegate to the
existing HostEngine, which posts them to its Asio thread. The GUI has no hook,
transport or routing implementation. Network operations never run in Qt callbacks.
AgentWindow owns AgentService and destroys it before the Win32 sinks/hooks in main.
Host hooks stop before HostEngine is destroyed. Engine shutdown closes sockets;
AgentService releases tracked keys on disconnect, timeout and graceful stop.

Tests in `tests/integration/gui_test.cpp` drive real widget buttons and a real TLS
agent with fake sinks: mirror preserves local suppression semantics; emergency
and closing release a held modifier; agent close releases its input; unavailable
agents leave the GUI responsive/Local. They do not validate native hooks, tray
interaction, focus, SendInput or screen edges on Windows.
