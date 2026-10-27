// The extension's coordinator: a relay between the 3CX web client tab and the assistant's Live Call page.
//
// Main path (no clicks): 3cx-hook.js + 3cx-bridge.js run inside the 3CX tab and send "call started /
// ended" and 100 ms PCM frames (1 = customer, 2 = agent) over a port named '3cx-tab'. The Live Call
// page connects with chrome.runtime.connect(EXTENSION_ID, {name: 'live-call'}) (allowed by
// externally_connectable) and gets everything forwarded. Nothing is stored; no audio leaves the browser
// except through the assistant's own page.
//
// Backup path: if the hook can't run (an old 3CX tab opened before the extension was installed), the
// agent clicks the extension on the 3CX tab and "Use this tab"; the Live Call page then asks for that
// tab's sound with {type: 'tab_stream'} (tab capture: the customer only).

const VERSION = chrome.runtime.getManifest().version;
const tabs = new Map(); // 3CX tab id -> { port, callId }
const pages = new Set(); // Live Call page ports

const POPUP_URL = chrome.runtime.getURL('popup.html');
const CALL_ID = /^w\d{1,15}-\d{1,6}$/;
const MAX_PCM_BASE64 = 4300; // 3200 bytes (100 ms of 16 kHz 16-bit) in base64, with margin

function badge() {
  const inCall = [...tabs.values()].some((t) => t.callId);
  chrome.action.setBadgeText({ text: inCall ? 'ON' : '' });
  if (inCall) chrome.action.setBadgeBackgroundColor({ color: '#12805c' });
}

// Messages from our content script in the 3CX tab: checked again here (defence in depth).
function isValidTabMessage(message) {
  if (!message || typeof message !== 'object') return false;
  if (message.type === 'hello') return true;
  if (message.type === 'call') {
    return (message.state === 'started' || message.state === 'ended') && CALL_ID.test(message.callId || '');
  }
  if (message.type === 'pcm') {
    return (message.channel === 1 || message.channel === 2) && CALL_ID.test(message.callId || '') &&
      typeof message.b64 === 'string' && message.b64.length > 0 && message.b64.length <= MAX_PCM_BASE64;
  }
  return false;
}

function summary() {
  const list = [...tabs.values()];
  const active = list.find((t) => t.callId);
  return { hooked: list.length > 0, inCall: !!active, callId: active?.callId ?? null };
}

function toPages(message) {
  for (const page of pages) {
    try { page.postMessage(message); } catch (_) { pages.delete(page); }
  }
}

chrome.runtime.onConnect.addListener((port) => {
  if (port.name !== '3cx-tab' || !port.sender?.tab?.id) return;
  const tabId = port.sender.tab.id;
  const entry = { port, callId: null };
  tabs.set(tabId, entry);
  toPages({ type: 'status', ...summary() });
  port.onMessage.addListener((message) => {
    if (!isValidTabMessage(message)) return;
    if (message.type === 'pcm') {
      if (entry.callId && message.callId === entry.callId) {
        toPages({ type: 'pcm', channel: message.channel, callId: message.callId, b64: message.b64 });
      }
    } else if (message.type === 'call') {
      if (message.state === 'started') entry.callId = message.callId;
      else if (message.state === 'ended' && entry.callId === message.callId) entry.callId = null;
      toPages({ type: 'call', state: message.state, callId: message.callId });
      badge();
    }
  });
  port.onDisconnect.addListener(() => {
    if (tabs.get(tabId) !== entry) return;
    tabs.delete(tabId);
    if (entry.callId) toPages({ type: 'call', state: 'ended', callId: entry.callId }); // tab closed mid-call
    toPages({ type: 'status', ...summary() });
    badge();
  });
});

// The assistant's Live Call page (only the origins in manifest externally_connectable).
chrome.runtime.onConnectExternal.addListener((port) => {
  if (port.name !== 'live-call') return;
  pages.add(port);
  port.postMessage({ type: 'status', version: VERSION, ...summary() });
  port.onDisconnect.addListener(() => pages.delete(port));
});

chrome.runtime.onMessageExternal.addListener((message, sender, sendResponse) => {
  (async () => {
    if (message?.type === 'ping') {
      sendResponse({ ok: true, version: VERSION, ...summary() });
    } else if (message?.type === 'tab_stream') {
      const { grantedTabId } = await chrome.storage.session.get('grantedTabId');
      if (!grantedTabId) {
        sendResponse({ ok: false, error: 'Reload the 3CX tab (or click the extension on it and "Use this tab").' });
        return;
      }
      try {
        await chrome.tabs.get(grantedTabId);
      } catch {
        sendResponse({ ok: false, error: 'The 3CX tab was closed.' });
        return;
      }
      if (!sender.tab?.id) {
        sendResponse({ ok: false, error: 'Only a page in a tab can receive the call sound.' });
        return;
      }
      try {
        const streamId = await chrome.tabCapture.getMediaStreamId({ targetTabId: grantedTabId, consumerTabId: sender.tab.id });
        sendResponse({ ok: true, streamId });
      } catch (error) {
        sendResponse({ ok: false, error: `The browser refused to share the 3CX tab: ${error.message}. Click the extension on the 3CX tab and "Use this tab" again.` });
      }
    } else {
      sendResponse({ ok: false, error: 'Unknown request.' });
    }
  })();
  return true;
});

// The popup (and only the popup: not content scripts).
chrome.runtime.onMessage.addListener((message, sender, sendResponse) => {
  if (message?.target !== 'background' || sender.tab || sender.url !== POPUP_URL) return false;
  (async () => {
    if (message.type === 'status') {
      const { grantedTabId } = await chrome.storage.session.get('grantedTabId');
      sendResponse({ ok: true, version: VERSION, ...summary(), hookedTabIds: [...tabs.keys()], pages: pages.size, grantedTabId: grantedTabId ?? null });
    } else if (message.type === 'grant' && Number.isInteger(message.tabId)) {
      await chrome.storage.session.set({ grantedTabId: message.tabId });
      sendResponse({ ok: true });
    } else {
      sendResponse({ ok: false });
    }
  })();
  return true;
});

// After the extension is installed or updated, 3CX tabs that were already open don't have the hook:
// say so in the popup (the agent reloads that tab once).
chrome.runtime.onInstalled.addListener(() => badge());
