# Security review: Sales Assistant 3CX helper

Version reviewed: **2.0.0** (all of `src/`, `res/`, `CMakeLists.txt`). Findings below were fixed in 1.9.0;
2.0.0 is the same program restructured into modules with unit tests, re-tested against every attack below.
Date: 2026-10-27. Reviewed by the development team (internal review, not a third-party audit).

## 1. What the program does

A small Windows tray program for agents who use the 3CX app from the Microsoft Store. During a call it
records the sound the 3CX app plays (the customer's voice) and passes it to the Sales Assistant's Live
Call page, open in the browser **on the same PC**, so the assistant can transcribe it and suggest answers.

| | |
|---|---|
| Language / size | C++17, ~2,000 lines in small modules (see README "Code layout"), no third-party libraries; 21 unit tests |
| Runs as | The signed-in Windows user. **Never asks for administrator rights** (`asInvoker`) |
| Network | **Listens on 127.0.0.1:47923 only** (loopback). Makes **no outgoing connections**, no internet access |
| Reads | The sound of the output device the 3CX app uses (Windows WASAPI loopback); the list of processes (to find `3CXSoftphone.exe`) and audio sessions |
| Writes | A text log `%LOCALAPPDATA%\SalesAssistant3CXHelper\helper.log` (≤1 MB, one old copy); one registry value `HKCU\...\Run\SalesAssistant3CXHelper` only if the user ticks "Start with Windows" |
| Never | Records or touches the microphone; changes 3CX, its calls or settings; writes audio to disk; sends data off the PC |

### Data flow

```
3CX app ──plays──▶ speaker/headset ──(loopback, filtered to 3CX)──▶ helper
helper ──ws://127.0.0.1:47923 (same PC, same user, allowed web page only)──▶ Live Call page in the browser
Live Call page ──HTTPS/WSS (existing app, signed-in user)──▶ Sales Assistant server
```

Sound leaves the helper **only while the Live Call page says a call is on**, and only to **one** page.

## 2. Method

1. Line-by-line review of the whole source against the usual checklist for local desktop agents
   (OWASP Desktop App Security Top 10, CWE Top 25, Microsoft SDL guidance): exposure of local ports,
   authentication of local clients, input parsing, resource exhaustion, privacy/PII, files and registry,
   privileges, DLL loading, binary hardening, supply chain, update path.
2. Attack tests against the running program (a script acting as a malicious local program): bad and
   missing Origin, unmasked frames, a 200 MB frame, a length that overflows the size calculation, 300
   simultaneous connections, control characters in fields. The parser cases are also unit tests
   (`tests/ws_frame_test.cpp`, `tests/ws_handshake_test.cpp`, `tests/caller_id_test.cpp`).
3. Inspection of the built `.exe`: security flags (ASLR, NX), imported DLLs, embedded strings, manifest.

## 3. Findings and fixes

| # | Finding | Severity | Status (1.9.0) |
|---|---|---|---|
| 1 | **Crash from a crafted WebSocket frame**: a 64-bit length near 2⁶⁴ overflowed the size calculation and crashed the program (confirmed in testing). Reachable only by a program already running on the same PC. | High (local denial of service) | **Fixed**: frames over 4 KB are refused *before* any size arithmetic |
| 2 | **Unbounded memory**: a frame claiming 200 MB was buffered. | Medium | **Fixed**: 4 KB limit; the connection is closed |
| 3 | **Protocol not enforced**: unmasked, fragmented frames and reserved bits were accepted. | Low | **Fixed**: only masked, unfragmented frames with known opcodes (RFC 6455) |
| 4 | **No limit on connections / threads**: 300 connections were all accepted. | Medium | **Fixed**: at most 8 pages and 16 connections being set up; the rest are closed |
| 5 | **Shared terminal servers (RDS/Citrix)**: 127.0.0.1 is shared by all users of one machine, so another user's browser could have connected and received someone else's calls. | High (in that setup) | **Fixed**: a connection is accepted only if the connecting program runs in **the same Windows session** (checked via the TCP table owner PID → session ID) |
| 6 | **Local authentication relies on the browser Origin header**: any program can send any Origin. | Low (by design) | **Mitigated** by #5. A malicious program running as the same user could connect, but it could already record the speakers itself, so no extra access is gained |
| 7 | **Phone numbers in the log** in plain text. | Low (privacy) | **Fixed**: masked (`077*****67`); only `0-9 + * #` kept, at most 20 characters |
| 8 | **Log injection**: fields from the page were logged as received. | Low | **Fixed**: caller sanitised; refused Origins logged with control characters replaced, max 80 characters |
| 9 | **Test options in the shipped program** (`--record` to a WAV file, `--allow-origin`, `--pid`, `--port`, `--process`). | Medium | **Fixed**: only in a separate diagnostics build (`build.cmd diag`); the normal build accepts only `--console` and `--hidden` (verified in the binary) |
| 10 | **Unbounded silence fill** after the PC sleeps: could allocate gigabytes. | Medium (stability) | **Fixed**: at most 1 s of silence is filled |
| 11 | **No explicit privilege level** in the manifest. | Low | **Fixed**: `requestedExecutionLevel asInvoker` |
| 12 | **Build hardening**: no stack protector. | Low | **Fixed**: `-fstack-protector-strong`, `_FORTIFY_SOURCE=2`; ASLR (high entropy) and NX/DEP confirmed in the PE header |
| 13 | **DLL loading**: `dwmapi.dll` imported statically (planting risk if the .exe sits in a writable folder). | Low | **Fixed**: loaded at run time from System32 only; `SetDefaultDllDirectories(System32)` for later loads. Remaining static imports are Windows system DLLs |
| 14 | **Development addresses in the release build** (`http://localhost:3004`). | Low | **Fixed**: only with `-DSA3CX_DEV_ORIGINS=ON` (`build.cmd dev`); the release build accepts only `https://aicallagent.chassia.com` (verified in the binary) |

## 4. Controls checklist

| Area | Control | Result |
|---|---|---|
| Network exposure | Bound to 127.0.0.1 only; `SO_EXCLUSIVEADDRUSE` | ✓ |
| Client authentication | Origin allow-list (release: the hosted assistant only) + same Windows session | ✓ |
| Input validation | Handshake ≤16 KB with 3 s timeout; path, `Upgrade`, version 13, key length checked; frames ≤4 KB, masked, unfragmented | ✓ |
| Resource limits | 8 pages, 16 pending connections, send timeout 500 ms, bounded buffers | ✓ |
| Data minimisation | Sound only during a call, only to one page, only while 3CX itself is making sound | ✓ |
| Data at rest | No audio stored; log in the user's profile (user-only permissions), no secrets in it | ✓ |
| Data in transit | Loopback only (never on the network); the browser sends it on over the app's HTTPS/WSS | ✓ |
| Least privilege | Normal user; no admin, no service, no driver, no microphone | ✓ |
| Persistence | Only the user's own Run key, only on request, quoted path | ✓ |
| Secrets | None in the program (no keys, tokens, passwords); verified by scanning the source and binary | ✓ |
| Binary hardening | ASLR (high entropy), NX, stack protector, fortify | ✓ |
| Dependencies | Windows SDK + MinGW-w64 runtime only; no third-party code | ✓ |
| Code signing | **Not signed** (see 5) | ✗ |
| Updates | None built in; new versions are installed by hand / by IT | n/a |

## 5. Remaining risks and recommendations

1. **Code signing.** The `.exe` is unsigned, so Windows SmartScreen warns when it is downloaded, and
   users can't verify the publisher. Recommended: sign it (a company code-signing certificate, or an
   internal certificate trusted via Group Policy), and deploy it through IT (Intune/GPO) to a folder users
   can't write to (e.g. `C:\Program Files\Sales Assistant\`).
2. **Same-user malware** could connect to the helper (it can send any Origin), but such a program could
   already record the PC's sound directly; the helper adds no new capability for it.
3. **Other sound on the same device.** The helper records the device 3CX plays to and filters it to the
   moments the 3CX app is making sound; another app playing *at the same moment* would be included.
   Recommended setup: 3CX's speaker = the headset, Windows sounds = the laptop speakers. The tray panel
   shows "Same device" with a warning when another app plays there.
4. **Call recording consent.** Customer calls are processed by the assistant; the company's existing
   call-recording / monitoring notice should cover it.
5. **Not tested** on Windows 10, on a shared terminal server, or with antivirus/EDR products that may
   flag audio loopback capture. Recommended before rollout.

## 6. How to verify

- **Build** from source: `build.cmd` (CMake + Ninja, MinGW-w64 GCC 16.2 WinLibs UCRT, or MSVC); it
  runs the unit tests. The diagnostics program (`*-diag.exe`) and `build.cmd dev` are for the team only.
- **Check the flags**: `objdump -p sales-assistant-3cx-helper.exe` → `DYNAMIC_BASE`, `HIGH_ENTROPY_VA`,
  `NX_COMPAT`; the manifest contains `asInvoker`.
- **Check the port**: `netstat -ano | findstr 47923` shows `127.0.0.1:47923 LISTENING` only.
- **Check behaviour**: `%LOCALAPPDATA%\SalesAssistant3CXHelper\helper.log` lists every connection
  (with the connecting program, e.g. `brave.exe`), refusals, and per-call counts of frames sent.
