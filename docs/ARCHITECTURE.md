# Architecture and protocol

`SPEC.md` remains the product specification. The repository root is the monorepo
root (no additional `sharekeys/` directory is required).

## Ownership and threads

- `sharekeys_core` contains portable event types, injected-key/button ownership,
  wire codec, routing and grid layout. Router/injection objects are single-thread
  confined; fake sinks permit deterministic tests without desktop side effects.
- Windows `KeyboardHook` and `MouseHook` own dedicated message-loop threads and
  low-level hooks. Hook callbacks filter all injected input, update physical state,
  detect shortcuts before forwarding, and enqueue fixed-size records. No socket
  operations, disk writes or GUI calls run in these callbacks. One bounded MPSC
  queue connects both hooks to the host networking thread, preserving publication
  order between keyboard and mouse events. The queue never waits for space.
- `HostEngine` owns one Asio event-loop thread, TLS context, router and up to five
  device slots. Each connection has its own bounded outbound queue. GUI commands
  are posted to this thread. The GUI reads a mutex-protected status snapshot;
  hooks use atomic suppression/emergency state and never acquire this mutex.
- Qt widgets and rendering stay on the main GUI thread. Input hooks are destroyed
  before the engine. Engine shutdown requests releases, closes sockets (which
  also triggers agent cleanup), stops its event loop and joins its thread.
- The agent setup window owns an `AgentService` for each listener session. Its
  TLS accept/read loop and keyboard/mouse injection run on a dedicated Asio
  thread; status is marshaled to the Qt thread through a callback. Stop closes
  the listener, disconnects the peer, requests release and joins that thread.
  Agent settings are saved atomically in the per-user application-data folder;
  optional login startup uses a per-user Windows Run key. Host settings persist
  paired devices and optional reconnect, but routing starts in Local mode.
- Agent network handling and input injection are confined to its Asio thread.
  Separate hooks observe physical state. A 100 ms recovery timer retries failed
  releases while disconnected; no new session is admitted until cleanup succeeds.

The input queue has 2048 slots. Producers reserve slots atomically; a consumer
cannot overtake an earlier unfinished publication. Allocation and locks are absent
from push. After eight failed reservation attempts, capture fails safely to Local
rather than spinning indefinitely on a hook thread. This establishes a shared
publication order; actual hardware scheduling and inter-PC execution times are
not synchronized.

## Authentication and pairing

Each interactive user generates a self-signed P-256 identity through OpenSSL.
Windows encrypts private-key PEM using user-scoped DPAPI; only the protected blob
is written. Linux development identities are mode-0600 PEM files. Creation refuses
to overwrite existing identities. Partial identity creation fails closed.

Pairing explicitly authorizes the peer's SHA-256 leaf-certificate fingerprint on
both PCs. Fingerprints must be exchanged through a trusted channel. TLS 1.3 proves
possession of the pinned private key; a CA certificate or hostname is not the
trust anchor. Exact pins, certificate validity times and mutual certificate
presentation are required. No automatic trust-on-first-use is implemented.
PAIR_REQUEST and PAIR_APPROVED are reserved wire types; pairing is currently a
local administrative/configuration operation, and these types are rejected on
an established input session. Revocation is performed from the agent setup UI
after stopping its listener.

The agent sends HELLO with a cryptographically random, nonzero 64-bit session ID
after TLS authentication. The client sends AUTHENTICATE bound to that session;
the server confirms AUTHENTICATE before application commands are admitted. Every
direction starts at sequence 1. Commands before confirmation, stale session IDs,
duplicates and gaps close the connection. Reconnect creates a fresh session and
forces Local mode. Identity storage, pin lists and the logged-in account are
part of the trust boundary; a compromised same-user process is out of scope.

## Binary format (version 1)

All multibyte integers are big endian, written field-by-field. No C++ object memory
is serialized. A frame is a four-byte unsigned body length, followed by:

| Field | Bytes | Meaning |
| --- | ---: | --- |
| Version | 2 | 1 |
| Type | 2 | HELLO=1 through ERROR=16; see `protocol.hpp` |
| Session | 8 | Nonzero server-generated session |
| Sequence | 8 | Strictly increasing, per direction |
| Payload | 0–4076 | Validated by message type |

Minimum body size is 20; maximum is 4096. Asio reads the prefix and bounded body
exactly, including partial TCP reads. A timeout applies even to incomplete bodies.

- KEY_DOWN/KEY_UP: scan code (u16), virtual key (u16), extended flag (u8).
- MOUSE_MOVE: absolute flag (u8), X and Y (i32, two's-complement). Relative deltas
  are limited to ±32767; absolute coordinates are normalized 0–65535 over the
  combined virtual desktop.
- MOUSE_BUTTON: button index (u8, left/right/middle/X1/X2), down flag (u8).
- MOUSE_WHEEL: vertical and horizontal deltas (i32 each), limited to ±32767.
- HEARTBEAT: empty or a request/response flag (u8) followed by a nonce (u64).
  Nonce-matched replies provide protocol RTT, never application latency/acceptance.
- DEVICE_STATUS: up to 256 bytes; cursor edge report is `{1, edge}`, where edges
  are none=0, left=1, right=2, top=3, bottom=4. Cursor reports are sent every 100 ms.
- ERROR: up to 256 bytes, reserved diagnostics. Other types have no payload.

Per-peer output is capped at 1024 messages and input at 2000 messages per second.
Overflow/rate errors close that session. The agent also limits TLS/application
handshakes to 5 seconds; heartbeats occur every second with a 3-second inactivity
threshold. Reconnect backs off from 250 ms to 30 seconds. IPv4 private/link-local
and loopback destinations are allowed; public addresses and wildcard agent binds
are refused. Network topology/firewall policy still determines physical LAN reach.

## Input correctness

Injected key/button state records only accepted down events. Repeated downs are
preserved. Unknown ups are ignored. Release failures remain tracked for retry.
Observed physical holds suppress synthetic releases so local agent users' keys
are not deliberately released. Physical ownership before hook startup cannot be
identified reliably, and Windows merges physical/injected logical key state.

Every mode/target change releases the old recipients. Held inputs are quarantined
until their up events, so an old modifier is not transferred into another target.
Exclusive local suppression follows each key/button's original down decision,
including its eventual up, even across mode changes. Emergency commands use an
atomic path independent of queue capacity; the pipeline returns to Local and
drains stale queued commands without forwarding them. Capture records carry an
emergency epoch so delayed producer publication cannot re-enable an old command;
hook-side quarantine also blocks repeats of keys held across emergency recovery.
Mouse mirroring is separate
and disabled on startup. Edge transitions use grid adjacency, center both cursors
and wait 750 ms before accepting another transition.

Identical keyboard layouts are required. SendInput success means only that Windows
accepted an input record. Neither it nor a network reply proves that a trading
application (or any other application) executed a business action.
