# Controllable Delta integration (Windows)

Use an AntiMicroX-Delta build and a Controllable Delta build containing this
bridge. The bridge is enabled by default on both sides. AntiMicroX's General
settings and the mod's controller settings can disable it.

When the mod's controller setup is complete and at least one selected controller
is attached and successfully supplies input, AntiMicroX pauses mapped output
from **all controllers** while that Minecraft window is in the foreground.
Titles, executable names, worlds, and recent button activity are not detection
criteria. Title screens and menus work too. A background game cannot pause
mapping for another foreground Minecraft instance.

Pausing releases generated keys and mouse buttons, stops mapping timers and
pending macros, and preserves profiles and each controller's manual enable
setting. Returning to the desktop, disabling integration, disconnecting the
controller or game, and missing heartbeats remove only the temporary pause.
Held buttons, hats and axes must return to release/neutral before generating
output again. Sensors discard their previous state and accept fresh samples.
Already launched external programs continue running.

Notifications are asynchronous. A short delay on activation is possible; this
protocol does not acknowledge release completion before Minecraft applies its
first input. Exclusive, zero-delay input ownership would require a separate
acknowledgement protocol.

## Protocol v1

The dedicated Qt local server name is
`antimicroxDeltaControllableV1-<Windows user SID>-<Windows logon session ID>`.
The mod opens `\\.\pipe\<server name>`. The existing CLI instance endpoint is
independent. The server uses `QLocalServer::UserAccessOption` and additionally
checks the native pipe client PID, process token SID and OS session. This is a
cooperative local protocol, not authentication of a particular mod binary.

One UTF-8 JSON object per LF-terminated frame, at most 4096 bytes including LF:

```json
{"protocol":1,"product":"controllable-delta","type":"state","session":"12345678-1234-1234-1234-123456789abc","seq":42,"pid":12345,"hwnd":"0x0000000000123456","observed_uptime_ms":12345678,"controller_ready":true}
```

Numbers are nonnegative integers at most `2^53-1`. PID must equal the native
pipe client PID. HWND is a nonzero hexadecimal string, obtained from
`GLFWNativeWin32.glfwGetWin32Window`, and must belong to that process. Session is
a UUID per game launch, retained across reconnections. Sequence increases with
new client-thread observations. Old sequences, duplicate observations, future
timestamps, and observations older than 750 ms cannot renew the lease.

The mod publishes a fresh observation every 250 ms or on state changes. Its I/O
thread holds only the latest observation and does not sample controller or
Minecraft objects. An adopted update retains its state for 1500 ms from
receipt; the 750 ms check applies at receipt. The receiver checks leases,
process lifetime and foreground HWND every 100 ms and when frames arrive. All
time values use Windows `GetTickCount64`, independent of calendar changes.

The receiver limits active connections to eight and retained session records to
64, frames to 4096 bytes, and processing per read callback. Initial valid state
must arrive within 500 ms. A disconnect clears only state currently owned by
that connection, so a late close cannot undo a newer reconnection. Invalid
messages terminate that connection. IPC startup failure leaves normal mapping
available. There are no commands to load profiles, start processes or execute
code through this endpoint.

## Focused verification

`NativeGameInputTests` verifies strict schema validation, sequence replay,
freshness versus lease expiry, multiple game windows, connection replacement,
limits, and process exit policy. Its Windows IPC test uses a hidden native
window and the actual Qt named pipe to verify OS peer PID lookup, fragmented
and batched frames, disconnection, initial timeout and foreground/lease
transitions. It does not move desktop focus or start Minecraft.

`InputReleaseLatchTests` verifies holding, neutral and new-input transitions.
`InputSuspensionTests` exercises the production SDL input pipeline with a virtual
controller: paused event draining, raw monitoring, manual disable preservation,
held-input latches, stale event rejection, set-change timers, calibrated profile
transfers, radial stick neutral detection, and pending mouse-state cleanup. It
uses a headless Qt application and generates no operating-system input.
The mod has focused bridge tests alongside its existing NeoForge Java tests.
Compile both Fabric and NeoForge for the versions configured in that repository.
Final hardware verification should check real controller inputs, Alt+Tab,
toggle/turbo/macros and sensors, hotplug/profile changes while paused, and game
termination. Compilation and protocol tests cannot replace that hardware check.
