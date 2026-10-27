# Sales Assistant for 3CX (browser extension)

A Chrome / Edge / Brave extension (Manifest V3) for agents who use the **3CX web client in the
browser**. During a call it gives the Sales Assistant's Live Call page the call's sound: the customer
and the agent as **two separate channels**, straight from the 3CX page, plus "call started / ended".
It never answers, mutes or changes a call.

Security model and review: [SECURITY.md](SECURITY.md).

## How it works

| File | Runs in | What |
|---|---|---|
| `3cx-hook.js` | The 3CX page (main world, `document_start`) | Wraps `RTCPeerConnection` to watch the call 3CX makes; while connected, turns the received audio (customer) and sent audio (agent) into 16 kHz 16-bit frames of 100 ms and posts them to the page's own window |
| `3cx-bridge.js` | The 3CX page (extension's isolated world) | Checks each message (type, call ID, channel, size) and passes it to the background |
| `background.js` | Service worker | Checks again and relays to the Live Call page (`externally_connectable`); the backup "Use this tab" capture (`tabCapture`) |
| `popup.html`, `popup.js` | Toolbar popup | Status, and "Use this tab" |

Permissions: `tabCapture`, `activeTab` (only after the agent clicks the extension on a tab),
`storage` (session storage: the tab chosen with "Use this tab"). Content scripts run only on 3CX
cloud domains (`manifest.json` → `content_scripts.matches`). No remote code, no network requests.

## Installing

- **Testing:** `chrome://extensions` (or `edge://`, `brave://`) → Developer mode → **Load unpacked** →
  this folder (or the unzipped release). Reload the 3CX tab afterwards.
- **Company PCs:** IT force-installs it with the `ExtensionInstallForcelist` policy (Chrome, Edge and
  Brave); updates then come automatically from where the package is hosted.

The extension ID is always `maaccgabflblmdcconchciphmaibbmbo`: the `key` in `manifest.json` is the
public key; the private key is kept by the team and never published.

## Release package

```powershell
powershell -ExecutionPolicy Bypass -File package.ps1
```
Makes `dist\sales-assistant-for-3cx-<version>.zip` whose manifest allows only the hosted assistant
(`https://aicallagent.chassia.com`); the `localhost` entries in this folder are for development.
