# Security review: Sales Assistant for 3CX (browser extension)

Version reviewed: **1.1.0** (all files in this folder). Date: 2026-10-27. Internal review by the
development team, not a third-party audit.

## 1. What it can and can't do

| | |
|---|---|
| Platform | Chrome / Edge / Brave 116+, Manifest V3 |
| Permissions | `tabCapture`, `activeTab`, `storage` (session only). **No** host permissions, `webRequest`, cookies, history, downloads or `<all_urls>` |
| Content scripts | Only on 3CX cloud domains (`*.my3cx.*`, `*.3cx.*`, see `manifest.json`) |
| Talks to | The Live Call page of the assistant only (`externally_connectable`: release build `https://aicallagent.chassia.com`) |
| Network | **None.** No `fetch`/XHR, no remote scripts, no `eval`; MV3 default content security policy |
| Stores | The ID of the tab chosen with "Use this tab", in session storage (cleared when the browser closes). No audio, no call data |

### Data flow

```
3CX page: RTCPeerConnection (customer = received track, agent = sent track)
  └─ 3cx-hook.js ─postMessage (own window only)─▶ 3cx-bridge.js ─checked─▶ background.js ─checked─▶
     Live Call page (externally_connectable origin only) ─HTTPS/WSS─▶ Sales Assistant server
```

Audio flows only while a call is connected in 3CX.

## 2. Method

Line-by-line review of every file against the usual browser-extension checklist (Chrome extension
security guidance, OWASP): permissions, content-script isolation, message passing and trust
boundaries, external connections, storage, remote code, CSP.

## 3. Findings and fixes

| # | Finding | Severity | Status (1.1.0) |
|---|---|---|---|
| 1 | **Messages from the 3CX page were trusted**: any script on the 3CX page could post messages the bridge forwarded (fake "call started", fake audio). | Medium | **Fixed**: the bridge accepts only messages from the page's own window and origin, of a known type, with our call-ID format, channel 1 or 2, and at most 3,200 bytes of audio; the background checks the same again |
| 2 | **Popup commands accepted from any extension context** (e.g. "use this tab" from a content script). | Low | **Fixed**: only from the extension's own `popup.html`; the tab ID must be an integer |
| 3 | **`postMessage(..., '*')`** in the hook. | Low | **Fixed**: posts to `window.location.origin` only |
| 4 | **Development addresses** (`http://localhost:3004`) allowed to connect. | Low | **Fixed for releases**: `package.ps1` builds a release whose manifest allows only `https://aicallagent.chassia.com` |
| 5 | Audio is relayed to every connected Live Call page of the allowed origin (two tabs both receive it). | Low | Accepted: same user, same assistant; the page itself decides whether a call is running |

## 4. Remaining risks and recommendations

1. **Content-script scope.** The hook runs on all 3CX cloud domains so it works for any customer. For
   one company, narrow `content_scripts.matches` to its own 3CX address before release.
2. **The hook sees the call's audio inside the 3CX page** (by design). Anything that already runs on
   that page (3CX's own code, other extensions with access) could see it too; the extension adds no
   new way out of the browser: it sends only to the assistant's page.
3. **Distribution.** Use IT policy (`ExtensionInstallForcelist`) or an unlisted Chrome Web Store
   listing; "Load unpacked" is for testing only. Keep the extension's private key (`ext_key.pem`)
   offline and out of every repository.
4. **Not tested** on Edge and Chrome stable channels other than the versions used during development.
