# Sales Assistant – 3CX connectors

Two small client-side components that give the **Sales Assistant**'s Live Call page the sound of an
agent's **3CX** calls, so the assistant can transcribe the call and suggest answers to the agent in
real time. The assistant itself (server and web app) is a separate product and is not in this
repository.

| Component | For agents who use | What it is |
|---|---|---|
| [`browser-extension/`](browser-extension/) | The 3CX web client in Chrome, Edge or Brave | A Manifest V3 browser extension |
| [`windows-helper/`](windows-helper/) | The 3CX app from the Microsoft Store (Windows) | A small C++ tray program |

Both components:
- run **only on the agent's PC**, send sound **only to the assistant's Live Call page** in the same
  browser/PC, and make **no other network connections**;
- send sound **only during a call**;
- never answer, mute, transfer or end calls, and never change 3CX;
- need **no administrator rights** and store **no audio**.

```
3CX app / 3CX web client ──(this repository)──▶ Live Call page in the agent's browser ──HTTPS──▶ Sales Assistant
```

## Security

- [SECURITY.md](SECURITY.md): the overall model and how to report a problem.
- [windows-helper/SECURITY.md](windows-helper/SECURITY.md) and
  [browser-extension/SECURITY.md](browser-extension/SECURITY.md): the reviews, findings and fixes.

## Building

- **Windows helper:** `windows-helper\build.cmd` (CMake + Ninja, MinGW-w64 GCC or MSVC); runs the
  unit tests. See [windows-helper/README.md](windows-helper/README.md).
- **Browser extension:** no build step; `browser-extension\package.ps1` makes the release zip. See
  [browser-extension/README.md](browser-extension/README.md).

## Versions

See [CHANGELOG.md](CHANGELOG.md).
