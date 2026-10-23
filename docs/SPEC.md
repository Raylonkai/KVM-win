# Project: ShareKeys — Windows-Native Multi-PC KVM & Input Mirroring

You are a senior Windows systems engineer, C++20 architect, and desktop application developer.

Your task is to **design, implement, compile, test, and package a production-oriented Windows-to-Windows software KVM application called ShareKeys**.

Do not stop after writing documentation, proposing architecture, or creating skeleton files. Implement functional code, tests, a desktop GUI, and installation infrastructure.

Work autonomously, make reasonable engineering decisions, and document them. Do not ask for confirmation at every stage.

## 1. Product requirements

ShareKeys allows one Windows host PC to control up to five additional Windows PCs across a local network using one physical keyboard and mouse.

The software must support three modes:

**Mode A — Local Mode**

- Keyboard and mouse operate normally on the host.
- No input is forwarded to remote computers.

**Mode B — Remote Control Mode**

- The user selects one remote PC.
- Keyboard and mouse events are routed to that machine.
- Support switching between machines using configurable shortcuts.
- Support configurable screen-edge transitions between PCs.
- Prevent unintended local input while exclusive remote control is active.
- Provide a reliable emergency shortcut to regain local control.

**Mode C — Mirror Mode (Core Feature)**

- A configurable shortcut, initially `Ctrl + Alt + M`, toggles keyboard mirroring.
- When active, physical keyboard events on the host are broadcast to all selected remote PCs while continuing to execute locally.
- Support up to five remote PCs simultaneously.
- Support targeting individual PCs or groups.
- Preserve key-down and key-up events, modifier combinations, extended-key flags, and event ordering.
- For example, pressing `Win + D` on the host should trigger the equivalent shortcut on all selected remote PCs.
- Typing into Notepad on the host should reproduce equivalent typing in focused Notepad windows on selected remote PCs.
- Mouse mirroring must be disabled by default and remain independent from keyboard mirroring.
- Mirror mode must have a highly visible active-state indicator.

This is an input-routing system, not a remote desktop streaming application. Do not implement video streaming.

## 2. Required technology stack

- Language: C++20
- Platform: Windows 10/11, x64
- Compiler: MSVC
- Build system: CMake
- GUI: Qt 6 Widgets
- Input capture: Win32 APIs, including `SetWindowsHookExW`, low-level keyboard/mouse hooks, and other APIs where appropriate
- Input injection: Win32 `SendInput`
- Global shortcuts: `RegisterHotKey` where appropriate
- Networking: standalone Asio or Boost.Asio using persistent TCP connections
- Testing: GoogleTest
- Configuration: JSON
- Installer: Inno Setup or WiX Toolset
- Logging: structured logging without recording typed keystrokes

Do not use Electron, React, Node.js, a web backend, cloud infrastructure, or external services.

Do not use kernel-mode drivers unless an unavoidable requirement is demonstrated. Document Windows API limitations rather than attempting to bypass security boundaries.

## 3. Application architecture

Create a monorepo containing:

```text
sharekeys/
├── apps/
│   ├── host/
│   └── agent/
├── include/
├── src/
│   ├── core/
│   ├── input/
│   ├── network/
│   ├── protocol/
│   ├── routing/
│   ├── security/
│   └── gui/
├── tests/
│   ├── unit/
│   └── integration/
├── installer/
├── docs/
├── CMakeLists.txt
└── README.md
```

Create two executable applications:

**ShareKeys Host**

- Captures physical keyboard/mouse events.
- Manages remote connections.
- Implements input routing and broadcasting.
- Provides the graphical control interface.

**ShareKeys Agent**

- Runs inside each remote Windows user's interactive session.
- Connects securely to the paired host or accepts authenticated local-network connections.
- Receives and validates input events.
- Injects authorized input through `SendInput`.
- Reports connection status and diagnostic information.

Shared protocol, routing, state-management, and security code must be reusable between executables.

Keep platform-specific implementations behind clean C++ interfaces.

## 4. Keyboard capture and injection

Implement:

- Keyboard down/up detection.
- Modifier key tracking.
- Scan codes and virtual-key codes.
- Extended-key support.
- Keyboard layout considerations.
- Input event sequencing.
- Physical versus injected event discrimination to prevent loops.
- Safe propagation of keyboard events to a dedicated networking thread.
- Lightweight hook callbacks with no blocking I/O.
- Correct handling of common Windows shortcuts.
- Safe cleanup of injected pressed-key states.

The keyboard hook must run on a thread with a proper Windows message loop.

The remote agent must use `SendInput` and check the return value.

Do not claim support for Windows secure desktops, Ctrl+Alt+Delete, elevated application control, or protected input when Windows security restrictions prevent it.

For v1, support identical keyboard layouts across machines; document mismatched-layout limitations.

## 5. Networking protocol

Implement a versioned, length-prefixed binary protocol.

Support at least these message types:

- HELLO
- PAIR_REQUEST
- PAIR_APPROVED
- AUTHENTICATE
- HEARTBEAT
- KEY_DOWN
- KEY_UP
- MOUSE_MOVE
- MOUSE_BUTTON
- MOUSE_WHEEL
- RELEASE_ALL_KEYS
- MIRROR_START
- MIRROR_STOP
- DEVICE_STATUS
- DISCONNECT
- ERROR

Use fixed-width integer types and explicit byte ordering.

Never transmit raw C++ struct memory over the network.

Maintain up to five active remote agent connections.

Implement:

- Asynchronous socket I/O.
- Per-agent outbound queues.
- Event sequencing.
- Persistent connections.
- TCP_NODELAY.
- Connection timeout detection.
- Bounded queues.
- Reconnection with exponential backoff.
- Session identifiers.
- Authentication and authorization.
- Safe handling of incomplete or malformed messages.

For the MVP, prioritize low-latency immediate broadcasting over scheduled synchronized execution.

Do not claim simultaneous execution is guaranteed across different operating systems or PCs.

## 6. Input routing engine

Create a dedicated input-routing state machine.

States:

```cpp
enum class ControlMode {
    Local,
    Remote,
    Mirror
};
```

Define explicit valid state transitions and cleanup behavior.

In Remote Mode, route events exclusively to the selected remote device where supported.

In Mirror Mode, broadcast keyboard events to all enabled devices and preserve normal host-side keyboard behavior.

Implement deterministic state changes so that toggling modes never leaves keys stuck on remote machines.

Implement emergency shutdown of remote input using a configurable shortcut, initially `Ctrl + Alt + Esc`.

Ensure emergency shortcuts are intercepted before general forwarding.

Create an abstraction similar to:

```cpp
class InputRouter {
public:
    void setMode(ControlMode mode);
    void setActiveDevice(DeviceId id);
    void setMirrorTargets(
        const std::vector<DeviceId>& targets
    );
    void processKeyboardEvent(
        const KeyboardEvent& event
    );
};
```

You may improve the interface where needed.

## 7. Reliability and safety

This software may be used on trading desks, so input correctness and predictable failure behavior are essential.

Implement:

- Pressed-key state tracking per remote machine.
- Key release on disconnection, timeout, or mode exit.
- Stale session detection.
- Duplicate event rejection.
- Out-of-order event handling.
- Safe recovery after reconnect.
- Heartbeats.
- Input forwarding suspension when a session becomes unhealthy.
- Emergency local-control recovery.
- Thread-safe queues.
- RAII-based cleanup.
- Appropriate shutdown synchronization.

Avoid deleting or releasing keys physically held by users on remote PCs when this can be determined.

If the host crashes or becomes unreachable, the agent must use a local timeout/recovery mechanism to release tracked injected keys.

A network acknowledgment means that an event was received or processed at the protocol layer. It must not be represented as proof that the foreground application executed the intended business action.

Mirror Mode should require deliberate user activation and be visually distinguishable from ordinary remote control, especially because duplicate input can be dangerous in order-entry applications.

## 8. Security

Treat remote input injection as a privileged capability.

Implement:

- Explicit host-agent pairing.
- Mutual authentication using a suitable established library/protocol.
- Encrypted communication.
- Per-device authorization.
- Session-bound commands.
- Revocation of paired hosts.
- Limits on message size and input rate.
- Defensive parsing.
- Secure credential storage using appropriate Windows mechanisms.
- LAN-only operation by default.

Do not invent cryptographic algorithms.

Do not expose unauthenticated input-injection endpoints.

Do not attempt to bypass UAC, UIPI, Windows login security, secure desktop isolation, or enterprise endpoint protection.

## 9. Desktop GUI

Build a functional desktop application using Qt 6 Widgets.

Use a clean, modern dark-theme interface.

Required views:

**Dashboard**

- Current control mode.
- Number of connected devices.
- Active remote machine.
- Mirror mode status.
- Enable/disable mirror action.
- Emergency stop control.

**Device Manager**

- List of registered PCs.
- Online/offline status.
- Device name.
- IP address.
- Pair/unpair.
- Connect/disconnect.
- Enable/disable a device as a mirror target.

**Workspace Layout**

- Visual arrangement of host and remote monitors.
- Configure machine positions.
- Configure screen-edge switching.
- Save and load layouts.

**Settings**

- Hotkey configuration.
- Startup behavior.
- Network configuration.
- Device groups.
- Connection preferences.
- Diagnostic settings.

**Diagnostics**

- Connection state.
- Event delivery latency estimates.
- Network round-trip times.
- Reconnection attempts.
- Agent errors.

Keep GUI rendering independent from the networking and input threads.

## 10. Testing requirements

Implement automated unit tests for:

- Binary serialization/deserialization.
- Malformed packet handling.
- Sequence numbering.
- Input routing.
- Mode transitions.
- Device-group selection.
- Modifier state.
- Pressed-key cleanup.
- Reconnection.
- Queue limits.
- Authentication failure behavior.

Implement Windows integration tests for:

- Keyboard capture.
- Keyboard injection.
- Two-machine communication.
- Mirror mode.
- Remote control mode.
- Emergency stop.
- Connection loss during held modifiers.
- Host restart.
- Agent restart.

Where physical or multiple virtual Windows environments are required, create executable test harnesses and clear manual test instructions.

Never report tests as passed unless they were actually executed successfully.

## 11. Build and packaging

Provide:

- Working root CMake configuration.
- Debug and Release builds.
- MSVC compatibility.
- Dependency documentation.
- Reproducible build instructions.
- Host executable.
- Agent executable.
- Windows installer configuration.
- Uninstaller support.
- Optional agent autostart after Windows login.

Prefer dependency management through vcpkg if appropriate.

Do not require administrator privileges for ordinary usage unless genuinely necessary. Document privilege boundaries.

## 12. Implementation order

Implement the software incrementally in this order:

**Milestone 1:**
Create the project structure, CMake setup, shared types, and a working Windows keyboard capture/injection prototype.

**Milestone 2:**
Establish secure communication between one host and one agent. Demonstrate remote keyboard injection.

**Milestone 3:**
Implement keyboard mirroring, mode switching, device selection, and reliable key-state cleanup.

**Milestone 4:**
Extend connections to five remote agents and implement device-group broadcasting.

**Milestone 5:**
Implement mouse routing, cross-screen control, and reliable machine switching.

**Milestone 6:**
Implement the Qt GUI and connect it to the functional backend.

**Milestone 7:**
Add automated tests, diagnostics, installer, documentation, and final integration.

Build and test after each milestone, fixing errors before continuing.

## 13. Engineering standards

- Use modern, idiomatic C++20.
- Prefer RAII and explicit ownership.
- Avoid raw owning pointers.
- Use `std::unique_ptr`, `std::shared_ptr`, and `std::jthread` where appropriate.
- Avoid global mutable state.
- Clearly separate UI, transport, protocol, and input logic.
- Document threading and locking assumptions.
- Avoid blocking keyboard hooks.
- Avoid undefined behavior and data races.
- Use meaningful error handling.
- Handle Windows API failures explicitly.
- Compile with high warning levels.
- Run available static analysis.
- Avoid placeholder functionality presented as complete.

## 14. Required final deliverables

The repository must contain:

1. Functional C++ host application.
2. Functional C++ remote agent.
3. Working keyboard mirroring engine.
4. Remote keyboard and mouse control.
5. Five-device connection support.
6. Functional Qt desktop GUI.
7. Secure device pairing and communication.
8. Automated tests.
9. Windows installer configuration.
10. README containing build, installation, operation, and troubleshooting instructions.
11. Architecture documentation.
12. Known limitations and unsupported Windows security contexts.

## 15. Execution instructions

First inspect the existing repository, if one exists. Preserve useful code, conventions, and working functionality.

Then:

1. Produce a concise implementation plan.
2. Begin writing and modifying actual project files immediately.
3. Work milestone by milestone.
4. Execute available builds and tests.
5. Fix failures instead of simply describing them.
6. Keep changes modular and reviewable.
7. Record incomplete or unverified requirements transparently.
8. Continue until all feasible deliverables are implemented.

If the current development environment is Linux, use it for portable C++ components, protocol tests, and static analysis. Do not claim native Win32 components have been compiled or validated unless a Windows-capable compiler and runtime are available.

If Windows validation is unavailable, provide Windows build scripts and explicitly identify the tests that require Windows execution.

**Definition of Done:**

A user can install ShareKeys on one Windows host and five Windows agent PCs, pair the machines on a LAN, enable keyboard mirror mode with a shortcut, and have authorized keyboard events forwarded to all selected PCs with correct ordering, safe failure recovery, and a reliable emergency stop.

The system must never claim that an input was executed by an application solely because the corresponding network packet was acknowledged.

**Begin implementation now. Do not stop at architecture or pseudocode.**

