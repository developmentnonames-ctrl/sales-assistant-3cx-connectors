# Security

## Model

These components handle call audio on the agent's PC. The design keeps that audio inside the PC and
inside the browser session of the signed-in agent until the assistant's own page sends it, over
HTTPS, to the assistant.

| Question | Browser extension | Windows helper |
|---|---|---|
| Where does audio come from? | The 3CX web client's own call connection (WebRTC) in the 3CX tab | The output device the 3CX app plays to, filtered to the moments the 3CX app makes sound |
| Where does it go? | The Live Call page of the allowed origin, via the browser's extension messaging | The Live Call page, via a WebSocket on 127.0.0.1 |
| Who may receive it? | Origins in `externally_connectable` (release: `https://aicallagent.chassia.com`) | The same allowed origin **and** a program in the same Windows session |
| When? | While a 3CX call is connected | While the page in use says a call is on; one page at a time |
| Stored? | No | No (a text log with masked numbers only) |
| Network access? | None | None (listens on 127.0.0.1 only) |
| Privileges | Browser extension permissions: `tabCapture`, `activeTab`, `storage` | Normal user (`asInvoker`) |
| Third-party code | None | None |

Detailed reviews, with every finding and its fix:
- [windows-helper/SECURITY.md](windows-helper/SECURITY.md)
- [browser-extension/SECURITY.md](browser-extension/SECURITY.md)

## Reporting a problem

Please report security issues privately to the development team (not in a public issue), with the
component, version, steps to reproduce and impact. We aim to acknowledge within 2 working days.
