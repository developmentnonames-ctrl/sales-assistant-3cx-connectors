# Changelog

Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). Versions: [SemVer](https://semver.org/).

## Windows helper

### 2.0.0 — 2026-10-27
- Restructured into modules (Google C++ style), CMake build, 21 unit tests; same behaviour as 1.9.0.
- Release builds accept only `https://aicallagent.chassia.com`; `localhost` only in development builds.

### 1.9.0 — 2026-10-27 (security review)
- Fixed: crash from a crafted WebSocket frame (length overflow); unbounded frame buffering; no
  connection limits; unmasked/fragmented frames accepted.
- Added: same-Windows-session check for connecting programs; phone numbers masked in the log; log
  fields sanitised; test options only in a diagnostics build; `asInvoker` manifest; stack protector
  and fortify; `dwmapi.dll` loaded from System32 only; at most 1 s of silence filled after sleep.

### 1.8.0 — 2026-10-23
- Records the 3CX app's output device, filtered to the 3CX app's own sound (Windows' per-app capture
  returns only the ringtone for the 3CX Store app).
- Tray panel: status lines, "Same device" check, live sound bar, Reconnect, Start with Windows.
- Sound only during a call, to one Live Call page at a time.

## Browser extension

### 1.1.0 — 2026-10-27 (security review)
- Messages from the 3CX page are checked (type, call ID, channel, size) in the bridge and again in
  the background; popup commands only from the popup; `postMessage` to the page's own origin.
- `package.ps1` builds a release that allows only `https://aicallagent.chassia.com`.

### 1.0.0 — 2026-10-22
- Follows 3CX web client calls (WebRTC) and gives the Live Call page the customer and agent as
  separate channels; backup "Use this tab" capture.
