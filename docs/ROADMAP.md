# Reliability roadmap

The product contract remains SPEC.md. Preserve the existing engine; current local
evidence and open defects are tracked in PROJECT_STATUS.md and PROGRESS.md.

1. **Complete Windows build, packaging and clean-machine acceptance.** Run the
   revised MSVC CI, inspect deployed DLLs/plugins, install/upgrade/uninstall as a
   standard user, test desktop/Start menu launch, explicit close/tray behavior,
   login startup and retained settings. Do not infer this from Linux tests.
2. **Validate input correctness on one host and five real Windows agents.** Execute
   WINDOWS_TESTING.md, including held modifiers during stop/loss/restart, special
   keys/layouts, physical-key coexistence, mouse wheels/buttons and DPI/edges.
   Record actual observations and scheduling/latency distributions.
3. **Harden failure and input-thread health.** Detect silently lost hooks, handle
   Win32 message-loop/post failures, test failed SendInput releases and shutdown
   time bounds, fault-inject network/worker stalls and run long load/soak tests.
4. **Harden authorization, configuration and maintenance.** Validate all saved
   host records atomically, bound pre-auth admission against LAN denial of service,
   add expiry/renewal UI and sensitive-buffer cleanup, unify CLI with AgentService,
   fuzz protocol/state transitions and review dependency licenses/security updates.
5. **Prepare a controlled pilot and signed release.** Sign binaries/installer,
   establish reproducible release provenance and rollback/support procedures,
   then add authenticated discovery-assisted setup (never automatic trust), richer
   hotkey editing and monitor geometry. Require all reliability gates before
   production use; input ACKs never establish a trading application's action.

Discovery, on-wire pairing, per-monitor geometry and guaranteed simultaneous
execution are not current capabilities. Guaranteed simultaneous execution across
PCs is not promised by the specification either. No production readiness claim is
made before the acceptance evidence exists.
