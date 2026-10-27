# Sales Assistant 3CX helper (Windows)

A small Windows tray program for call-centre agents who use the **3CX app from the Microsoft Store**.
During a call it gives the Sales Assistant's Live Call page, open in the browser on the same PC, the
**customer's voice** from the 3CX app, so the assistant can transcribe it and suggest answers.

- Records the sound device the 3CX app plays to, **filtered to the 3CX app** (sound passes only while
  Windows' per-app meter shows the 3CX app making sound).
- Sends it **only during a call** and **only to one Live Call page**, over a WebSocket on
  **127.0.0.1** (this PC only). Never uses the microphone; never writes audio to disk.
- Never touches 3CX, its calls or its settings. Runs as the signed-in user, no admin rights.
- Windows 10 version 1809 or newer, or Windows 11 (the same as the 3CX Store app).

Security model and review: [SECURITY.md](SECURITY.md).

## Using it

1. Run `sales-assistant-3cx-helper.exe`. It lives in the **tray** (bottom right). The icon's colour:
   green = ready, red = something not connected, amber = starting, blinking blue = reconnecting,
   pulsing green with a dot = in a call.
2. Click the icon for the panel: 3CX app, Customer sound, Live Call screen, Same device, Call, a live
   3CX sound bar, a hint, and **Reconnect**, **Start with Windows**, **Exit**.
3. Open the assistant's Live Call page with Call audio **3CX calls**. Live Call starts and ends with
   the 3CX call by itself; hold Space while you talk.

Best setup: in the 3CX app, Settings → Audio → **Speaker = the headset**; Windows sounds on the laptop
speakers. Then nothing but 3CX plays on the device the helper records.

Log (for support): `%LOCALAPPDATA%\SalesAssistant3CXHelper\helper.log` (no audio, phone numbers masked).

## Building

Requirements: CMake 3.20+, Ninja, and MinGW-w64 GCC (tested: WinLibs GCC 16.2, UCRT) or MSVC.

```bat
build.cmd        :: release: accepts only https://aicallagent.chassia.com
build.cmd dev    :: development: also accepts http://localhost:3004 (never give it to agents)
```

Each build makes three programs in `build\` (or `build-dev\`) and runs the unit tests:

| Program | What |
|---|---|
| `sales-assistant-3cx-helper.exe` | The program agents get |
| `sales-assistant-3cx-helper-diag.exe` | Diagnostics build with test options (`--record file.wav --seconds N`, `--pid`, `--process`, `--port`, `--allow-origin`). Never give it to agents |
| `sa3cx_tests.exe` | Unit tests (also run by `ctest`) |

Hardening: stack protector, `_FORTIFY_SOURCE=2`, ASLR (high entropy), DEP/NX; MSVC builds add
`/sdl /guard:cf`. The manifest asks for no admin rights (`asInvoker`).

## Code layout

Google C++ Style Guide (`.clang-format`), C++17, no third-party libraries.

| Path | What |
|---|---|
| `src/main.cpp` | Entry point: options, single instance, wiring |
| `src/config.h`, `src/version.h` | Fixed settings (port, allowed pages, limits) and the version |
| `src/app_state.*` | State shared between threads (atomics + one mutex) |
| `src/audio_capture.*` | Finds the 3CX app and its device; WASAPI loopback; the 3CX-only filter |
| `src/resampler.*` | Any rate → 16 kHz mono, 100 ms frames |
| `src/frame_router.*` | Where each frame goes: level, Live Call page, per-call counts, WAV (diagnostics) |
| `src/local_audio_server.*` | The 127.0.0.1 WebSocket: who may connect, one owner, sound only in calls |
| `src/ws_frame.*`, `src/ws_handshake.*` | Strict WebSocket parsing (pure, unit-tested) |
| `src/process_info.*` | Processes by name/ID; who owns a local connection; same-session check |
| `src/caller_id.*` | Cleaning and masking the caller's number |
| `src/status_model.*` | What the tray shows, as data (pure, unit-tested) |
| `src/tray_app.*`, `src/panel_view.*`, `src/tray_icons.*` | The tray icon and panel |
| `src/autostart.*`, `src/log.*`, `src/wav_writer.*`, `src/handle.h`, `src/win32.h` | Small helpers |
| `res/` | Version info and the manifest |
| `tests/` | Unit tests |

## Messages (for the Live Call page)

`ws://127.0.0.1:47923/3cx-audio`

| Direction | Type | Content |
|---|---|---|
| page → helper | text | `{"type":"call","active":true,"caller":"0771234567"}` when a call starts; `active:false` when it ends |
| helper → page | text | `{"type":"status","version":"2.0.0","app_running":true,"capturing":true,"sample_rate":16000,"owner":true,"screens":1}` |
| helper → page | binary | 100 ms of 16 kHz, 16-bit, mono PCM (only to the owner page, only during a call) |
