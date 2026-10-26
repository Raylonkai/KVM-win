# Windows acceptance matrix (not yet executed)

Use disposable Windows 10/11 x64 interactive desktops with identical keyboard
layouts and ordinary, equal-integrity applications. Do not run input tests in
production trading/order-entry applications. Record OS build, compiler, layout,
privilege level, monitor/DPI configuration and exact executable revision.

## Compile and single-machine prototype

1. Follow README MSVC Debug/Release instructions. Save build output and CTest XML.
2. In an unlocked interactive session, run
   `sharekeys_windows_input_test.exe --allow-input`. It must report ordered injected
   F24 down/up observed by its low-level hook. This tests injection and the hook,
   not physical capture or a foreground application's response.
3. Run the Agent GUI smoke test (`sharekeys_agent.exe --smoke-test`) to verify
   startup and shutdown without enabling hooks. F24 injection is covered only by
   the opt-in harness above and must be tested on a disposable desktop.
4. Launch/close the host repeatedly; verify hooks stop, Local input remains usable
   and no extra ShareKeys process/thread remains. Minimize the host in Mirror mode
   and confirm the separate amber banner remains visible without taking focus.

## Two-machine and five-machine tests

Pair as documented. Open Notepad on each PC and manually focus it. Record results
in a test report; do not infer application behavior from protocol diagnostics.

| Scenario | Expected observation |
| --- | --- |
| Local mode | Typing and mouse affect only host |
| Mirror one target | Ordered text, repeats, key-up and modifiers on host and target |
| Mirror five targets | Same physical sequence reaches all five selected targets; no simultaneous-execution guarantee |
| Unchecked target/group change | Unselected PCs receive no new input; previous recipients release held state |
| Win+D, Shift/Ctrl/Alt combinations, extended navigation keys | Equivalent shortcut behavior on equal layouts; no stuck modifiers |
| Remote mode | Selected target gets input; ordinary local input suppressed; local up events remain balanced |
| Emergency while modifier held | Local control restored, synthetic remote modifiers released; shortcut itself not forwarded |
| Mouse default | No mouse mirroring until separately enabled |
| Mouse input | Move, left/right/middle/X buttons, both wheel axes; no stuck buttons across switches |
| Edge layout | Host↔agent and agent↔agent adjacent transitions, pointer centering, 750 ms cooldown; none during mirror |
| Offline neighbor | Edge movement does not enter disconnected machine |
| Agent physically holds a modifier/button | Synthetic cleanup does not release the observed physical hold |
| Yank network during Ctrl/Shift down | Agent releases within roughly 3–4 s plus scheduling delay; host returns Local |
| Kill host during held input | Agent timeout releases tracked input; restart requires deliberate mode activation |
| Gracefully stop/restart agent | Keys released before exit; host reconnects with backoff and stays Local |
| Wrong/revoked pin | Authentication fails; no input injection |
| Expired certificate | Authentication fails; requires new identity and explicit pairing |
| Host restart | Settings persist, mode starts Local; connection follows the saved auto-connect preference |
| Elevated app/secure desktop | Unsupported injection fails safely; no security bypass |

Repeat with left/right modifiers, AltGr layouts, Pause/Break, PrintScreen, media
keys, high polling-rate mice, mixed DPI and virtual desktops with negative monitor
coordinates. These cases are currently unverified and must not be advertised as
working. Test graceful shutdown separately from forced agent process termination;
no user-mode process can clean its keys after it has itself been forcibly killed.

## Installer

Build the Inno Setup installer from staged Release binaries. Install as a standard
user on a clean VM without development tools. Confirm Qt platform plugins,
OpenSSL/VC runtime dependencies, Start menu shortcuts for both GUIs, first-run
identities, agent pairing/listening, host reconnect preference, optional login
startup, tray operations, and uninstall. Verify Run entries are removed and
per-user identity files are retained as documented. Check dependency licenses
before redistribution; this does not constitute a release/license audit or signed
build.


## Desktop revision acceptance additions

- Run `scripts/verify_windows_stage.ps1` with both offscreen and windows platforms.
  Check native title-bar/taskbar icon and Explorer File properties manually.
- On a disposable VM run `scripts/test_windows_installer.ps1`; separately install
  as a standard user without Visual Studio/Qt/OpenSSL on PATH. Test an upgrade
  with existing pairings and verify identity fingerprints are unchanged.
- Default Close exits both roles and releases a held remote Ctrl/Shift. Opt into
  close-to-tray: Host returns Local; Agent continues only by that explicit choice.
  Test Quit and logoff, unavailable tray, minimize/restore, and login startup.
- Test emergency while rapidly clicking Remote/Mirror: previously queued activation
  must not resume routing. GUI status must follow actual engine state, including
  engine-error restart-required diagnostics and disabled activation buttons.
- Check host name/count/targets and mirror toggle; export diagnostics and confirm
  no captured key values/text. Discovery must remain visibly unavailable.

No checkbox in this document records an executed Windows result. Store the actual
revision, OS/tool versions, steps and observed pass/failure in a separate acceptance
record and link it from PROGRESS.md before closing these gates.
