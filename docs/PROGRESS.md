# ShareKeys implementation progress

## Current audit revision — 2026-10-26

The historical milestone record below is preserved. Current implementation and
readiness are in [PROJECT_STATUS.md](PROJECT_STATUS.md). This revision remains
uncommitted; no changes have been pushed or published.

- [x] Read SPEC.md and audit every application/core/network/input/security/UI/test
  area before implementation; record architecture, full keyboard path, feature
  classifications, severity-ranked debt and separate readiness scores.
- [x] Preserve existing host/agent/backend and manual fingerprint pairing.
- [x] Add HostController Qt adapter without moving transport/hooks into widgets.
- [x] Add live host/count/target summary, actual mirror toggle, explicit unsupported
  discovery, About/build information and bounded diagnostic history/export.
- [x] Default both windows to exit; persist explicit close/minimize-to-tray choices.
  Host close always requests Local; Agent continues only by explicit tray choice.
- [x] Embed Qt PNG icon, add multi-size Windows ICO and RC VERSIONINFO templates;
  retain Windows GUI subsystem flags and startup exception dialogs.
- [x] Align Qt runtime deployment with root-level executables; include MSVC runtime,
  vcpkg license files, Inno icon/shortcuts and per-user upgrade/uninstall behavior.
- [x] Add separate Host and Agent Inno payloads, checked PowerShell Release packaging,
  isolated stage verifier, signing hooks, disposable installer round-trip checks,
  checksums and explicitly dispatched/publish-gated Windows release workflow.
- [x] Fix queued activation surviving emergency epochs, add worker exception
  recovery and prevent un-applied shortcut edits being persisted by other actions.
- [x] Add three real-widget/TLS/fake-sink scenarios, saved tray preference test,
  and three host safety tests (queued command, worker failure, host destruction).
- [x] Update README, ARCHITECTURE, GUI_DESIGN, BUILD_WINDOWS, ROADMAP and acceptance
  instructions; distinguish testable backend behavior from native Windows input.
- [ ] Build and run this revision with MSVC/Windows; inspect PE/icon/metadata and
  staged runtime verification, then compile/run installer and clean-VM acceptance.
- [ ] Run the updated Windows workflow for this exact revision; installers have not
  yet been generated in this Linux environment.
- [ ] Validate native hooks/SendInput, tray/minimize/login and one host/five PCs.

### Executed validation in this revision

| Check | Result and scope |
|---|---|
| Baseline GCC Debug configure/build | Passed; existing source, no implementation changes yet |
| Baseline CTest in restricted sandbox | 26 passed; 9 failed opening loopback sockets (`Operation not permitted`) |
| Baseline authorized CTest rerun | 35/35 passed; real TLS, fake native input |
| Expanded initial Debug build/suite | Passed, 37 CTest entries (before three host safety tests) |
| Final GCC Debug/Release builds | Passed with Qt host/agent and GUI integration executable |
| Final Debug CTest | 40/40 entries passed (42 GoogleTest cases: 35 backend + 4 settings + 3 GUI) |
| Final Release CTest | 40/40 entries passed, same scope |
| Clang ASan/UBSan/LeakSanitizer | 35/35 portable backend tests passed; Qt excluded |
| Linux host and agent smoke | Both returned zero using offscreen platform and fresh temporary identities |
| GUI visual inspection | Offscreen host mirror dashboard and agent setup rendered; previews under build/audit-evidence; not Windows screenshots |
| clang-tidy | Changed backend and five GUI translation units passed using a Clang compile database |
| Windows compile attempt | clang-cl stopped at missing `cstdint` (MSVC headers/toolchain absent); no Windows binary produced |
| Windows GUI/DPAPI/native input | Not executed |
| PowerShell/Inno/installer runtime | Not executed; PowerShell/Inno/Windows runtime absent |
| Windows CI for this revision | Not dispatched: local changes are uncommitted and user prohibited commit/push without approval |
| Whitespace review | git diff --check passed |

Commands: `cmake --preset linux`, `cmake --build --preset linux --parallel 2`,
`ctest --preset linux --parallel 2`, and corresponding build-release commands.
JUnit reports are saved in `build/audit-evidence/{debug,release,sanitizer}-tests.xml`.
The single `Desktop.GuiIntegration` CTest entry contains three GoogleTests.
Portable safety tests are in `tests/integration/network_test.cpp`; GUI coverage is
in `tests/integration/gui_test.cpp`. Neither fakes nor offscreen rendering prove
physical Windows behavior, timing guarantees or application execution.

The sanitizer build initially failed during GoogleTest discovery because the
sandbox disallows LeakSanitizer's required ptrace access. The authorized build/test
rerun passed without disabling sanitizers. clang-tidy initially rejected the GCC
Qt flag `-mno-direct-extern-access`; configuring `build/tidy` with clang++ generated
compatible flags and the six-file analysis passed. A GUI test compilation warning
about GoogleTest macros inside unbraced if statements was fixed before final builds.
The original Windows CI success recorded below is historical, not evidence for
this working tree. No Windows packaging or interactive milestone is checked off.

---

The product definition remains `SPEC.md`. Windows functionality is unverified until
compiled and exercised on Windows; Linux tests do not establish Windows support.

## Plan and current status

- [x] Read all repository instructions and the full specification; preserve both.
- [x] Milestone 1 implementation: C++20/CMake, event types, lock-free bounded SPSC
  handoff, modifier tracking, injectable keyboard state, Win32 hook message thread,
  physical/injected discrimination, SendInput and prototype executables.
- [x] Milestone 1 portable validation: initial six GoogleTests passed under GCC.
- [ ] Milestone 1 Windows validation: MSVC build and interactive hook/SendInput tests.
- [x] Milestone 2 implementation: explicit fingerprint pairing, OpenSSL TLS 1.3
  mutual authentication, framed asynchronous Asio transport, random session IDs,
  strict sequencing, agent receiver and injection dispatch, DPAPI identity storage.
- [x] Milestone 2 portable validation: real TLS loopback delivery and rejection of
  unpaired certificates passed; malformed lengths, stale sessions, duplicate
  sequences, incomplete-frame timeout and pre-auth commands tested.
- [ ] Milestone 2 Windows two-PC injection validation.
- [x] Milestone 3 implementation: Local/Remote/Mirror routing, shortcut interception,
  release-on-transition, held-key quarantine, independent mouse mirroring.
- [x] Milestone 3 portable routing, transitions, modifier and cleanup tests.
- [x] Milestone 4 implementation: five persistent connections, bounded independent
  queues, 250 ms–30 s exponential reconnect, per-device authorization and groups.
- [x] Milestone 4 initial five-agent TLS mirror and emergency-cleanup test passed.
- [ ] Milestones 3–4 Windows multi-PC acceptance matrix.
- [x] Milestone 5 implementation: relative/absolute motion, five buttons, vertical
  and horizontal wheels, physical-button protection, grid-based desktop edges,
  pointer centering and transition cooldown; portable layout/mouse tests added.
- [ ] Milestone 5 Windows mouse/edge/DPI/multi-monitor validation.
- [x] Milestone 6 implementation: dark Qt dashboard, device pairing/selection,
  mirror indicator, emergency control, group and shortcut settings, saved workspace,
  JSON configuration, connection/retry and protocol RTT diagnostics.
- [x] Milestone 6 Linux GUI build and offscreen startup/shutdown smoke test.
- [x] Milestone 6 desktop agent setup GUI, tray controls, per-user settings,
  saved host/agent startup preferences, and asynchronous agent receiver service.
- [x] Host and agent GUI smoke tests added to the Windows CI workflow.
- [ ] Milestone 6 Windows GUI and extended usability testing.
- [x] Milestone 7 infrastructure: Debug/Release presets, pinned vcpkg baseline,
  Windows/Linux GitHub Actions, Inno Setup configuration, interactive Windows
  integration executable. Packaging has not been executed on Windows.
- [ ] Milestone 7 final Windows acceptance, installer/uninstaller verification,
  dependency license/security audit, code signing and production hardening.

## Environment

Arch Linux: GCC 16.2.1, CMake 4.4.4, Ninja 1.13.2, GoogleTest 1.18.0,
OpenSSL 3.6.5, Qt 6.12.0. No MinGW compiler or Wine found in the initial inspection.
Standalone Asio and Boost.Asio headers were not installed.

Downloaded standalone Asio 1.32.0 to `/tmp` and verified the archive SHA-256;
the CMake fallback pins that same archive/hash. No user-session VMs are registered
(`virsh -c qemu:///session list --all`). No Windows-capable compiler was found.

## Validation and failures encountered

- Milestone 1: GCC Debug build, 6/6 portable tests passed.
- Milestone 2: GCC build, 13/13 tests passed including real TLS loopback.
- Routing/group layer: 17 non-network tests passed after correcting shell quoting
  of the GoogleTest filter (zsh interprets unquoted wildcards).
- Initial application/mouse layer: GCC and Qt host compiled; offscreen smoke test
  exited successfully. Fixed a declaration-order compile error in the event pump
  and a Qt member-shadow warning.
- Initial expanded suite: 25/25 GCC tests passed, including five TLS peers.
- Clang 23.1.3 AddressSanitizer, UndefinedBehaviorSanitizer and LeakSanitizer:
  25/25 tests passed before the final edge/RTT additions; final results below.
- Sandbox initially denied DNS, VM runtime-directory access and loopback sockets;
  approved reruns succeeded. LeakSanitizer also required running outside the
  sandbox; its first discovery run failed due to restricted ptrace support.
- Initial clang-tidy run reported only `optin.performance.Padding` in intentionally
  cache-aligned queues/owner layout. Excluded that optional performance check in
  `.clang-tidy`; all eight portable implementation translation units then passed.
  Re-ran changed routing/host code after the queue/emergency changes; passed.
- Added real agent reconnection, outbound queue overflow and input-rate-limit
  tests after the initial 25-test run.
- GCC Debug/Release and Clang sanitizer builds subsequently passed 29/29 tests.
- Review found cross-hook ordering could change a quick modifier-click sequence;
  replaced the separate capture queues with a bounded 2048-slot MPSC queue.
  Added two-producer capacity/order coverage, emergency epochs to invalidate
  delayed publications, and hook-side held-key repeat quarantine. The five-peer
  integration test now checks combined keyboard/mouse ordering and heartbeat RTT.

### Final locally executed results — 2026-10-22

| Check | Result |
| --- | --- |
| GCC 16.2.1 Debug, Qt desktop apps included | Build passed; 35/35 GoogleTests passed |
| GCC 16.2.1 Release, Qt desktop apps included | Build passed; 35/35 GoogleTests passed |
| Clang 23.1.3 ASan + UBSan + LeakSanitizer, portable backend | Build passed; 32/32 GoogleTests passed (Qt desktop tests excluded) |
| clang-tidy analyzer checks, intentional padding check excluded | Passed |
| Qt 6.12 offscreen host and agent startup/shutdown with generated identities | Passed |
| Windows/MSVC, DPAPI, hooks, SendInput and interactive acceptance | Not run |
| GitHub Actions Windows build/tests | Build and 35/35 tests passed; install staging failed (details below) |
| Inno Setup packaging | Not reached in the failed Windows workflow |

Commands used: `cmake --build build`, `cmake --build build-release`,
`cmake --build build-asan`, and `ctest --test-dir <build-dir> --output-on-failure -j 2`.
GUI smoke checks used `QT_QPA_PLATFORM=offscreen` for both executables. Sanitizer
build uses `-fsanitize=address,undefined -fno-omit-frame-pointer` with Clang; its
configuration excludes Qt desktop targets. TLS tests and LeakSanitizer were run
with required sandbox permissions. No Windows test is counted as passing.

### Windows CI follow-up — 2026-10-23

The `40ee415` Windows Actions run compiled Debug and Release and passed all 35
tests. Staging failed during `cmake --install` because Qt rejected the relative
`build/stage/bin/qt.conf` path. The workflow and README now use an absolute stage
directory; this correction still requires a new Windows Actions run. The GUI
smoke test and Inno Setup packaging were skipped in that failed run.

## Remaining limitations

- **Windows native runtime/input and installer remain unverified in this environment.**
  CI compiled Debug/Release and passed tests, but staging has not yet been
  re-verified after the absolute-prefix correction. This includes
  the native host/agent entry points, low-level hooks, SendInput, DPAPI, cursor
  transitions, Windows integration executable, deployment DLL collection and
  installer/uninstaller. GitHub Actions has not completed packaging.
  The product Definition of Done has not been demonstrated.
- Pairing is explicit out-of-band fingerprint exchange. PAIR_REQUEST/PAIR_APPROVED
  are reserved, not an interactive on-wire pairing flow. Agent authorization is
  one host per user profile; stop listening before revoking/replacing it.
- Host diagnostics provide connection reasons, retries and protocol RTT, not measured
  foreground input delivery latency or application-action confirmation. Some
  agent-side injection errors appear on the host as a connection failure; inspect
  agent stderr for the specific error.
- Edge layout uses one grid cell per combined virtual desktop, with pointer
  centering, rather than pixel-perfect monitor geometry or continuous coordinate
  transfer. Mixed DPI, display gaps, pointer acceleration, screen corners and
  high-polling-rate mice require Windows testing. Rate limits deliberately suspend
  input rather than silently dropping key/button events.
- Physical key/button protection depends on observing physical downs after hook
  startup. Windows merges physical and injected logical states; this is best
  effort, not guaranteed ownership isolation. Release physically held controls
  before starting/testing an agent. AltGr, Pause/Break/E1 sequences, PrintScreen,
  media keys and mismatched layouts need explicit acceptance results.
- Windows can silently remove low-level hooks that exceed its callback timeout.
  The callbacks avoid blocking work, but automatic detection/reinstallation of
  silently removed hooks is not implemented. Hook startup and SendInput errors
  are checked; stress validation and stronger hook-health diagnostics remain.
- Host failure is covered by the agent timeout and tracked-key release. Forced
  termination of the agent itself cannot execute its RAII cleanup; graceful
  agent shutdown and hard termination must be treated separately.
- Reconnect returns to Local. Persisted devices may be configured to auto-connect
  on host startup, but mode always remains Local. Mirror/mouse-mirror state is
  never restored automatically. Shortcut
  modifiers are currently Ctrl+Alt with configurable virtual-key triggers.
- Production readiness still needs Windows acceptance, fuzzing/extended soak,
  dependency security/license audit, signed packages and measured latency/load
  results. The pinned vcpkg baseline is reproducible, not a claim that dependency
  versions are current or audited.

## Next acceptance gate

Run the Windows workflow and the matrix in `WINDOWS_TESTING.md` on Windows 10/11
with one host and five agents, fixing all compile/runtime failures before release.
Do not mark Windows milestones complete from portable test results alone.

## Safety boundaries

Identical keyboard layouts are required for v1. Secure desktops, Ctrl+Alt+Delete,
UIPI/elevated targets, login screens and protected input are unsupported. Physical
key protection depends on observing physical events after the agent hook starts.
SendInput acceptance is not evidence of a foreground application's action.
