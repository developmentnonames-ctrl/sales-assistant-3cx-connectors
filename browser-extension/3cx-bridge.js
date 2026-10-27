// Runs in the 3CX web client tab (the extension's isolated world). Passes what 3cx-hook.js posts
// (call started / ended, 100 ms PCM frames per channel) to the extension's background, which hands
// it to the assistant's Live Call page. Reconnects if the background restarts.
(() => {
  const TAG = '__lca3cx';
  let port = null;
  let hello = false;
  let currentCall = null; // re-announced if the background restarts mid-call

  function connect() {
    try {
      port = chrome.runtime.connect({ name: '3cx-tab' });
    } catch (_) {
      return; // the extension was reloaded: this old copy stops; the tab picks up the new one on reload
    }
    port.onDisconnect.addListener(() => {
      port = null;
      setTimeout(connect, 1000);
    });
    if (hello) port.postMessage({ type: 'hello' });
    if (currentCall) port.postMessage({ type: 'call', state: 'started', callId: currentCall });
  }

  function toBase64(buffer) {
    const bytes = new Uint8Array(buffer);
    let binary = '';
    for (let i = 0; i < bytes.length; i += 0x8000) binary += String.fromCharCode.apply(null, bytes.subarray(i, i + 0x8000));
    return btoa(binary);
  }

  // Messages come from the page's own scripts, so they are checked before being passed on: a
  // known type, a call ID of our own format, channel 1 or 2, 100 ms of 16-bit audio at most.
  const CALL_ID = /^w\d{1,15}-\d{1,6}$/;
  const MAX_PCM_BYTES = 3200;
  function isValid(message) {
    if (message.type === 'hello') return true;
    if (message.type === 'call') {
      return (message.state === 'started' || message.state === 'ended') && CALL_ID.test(message.callId || '');
    }
    if (message.type === 'pcm') {
      return (message.channel === 1 || message.channel === 2) && CALL_ID.test(message.callId || '') &&
        message.pcm instanceof ArrayBuffer && message.pcm.byteLength > 0 && message.pcm.byteLength <= MAX_PCM_BYTES;
    }
    return false;
  }

  window.addEventListener('message', (event) => {
    if (event.source !== window || event.origin !== window.location.origin) return;
    if (!event.data || event.data[TAG] !== true || !isValid(event.data)) return;
    const message = event.data;
    if (message.type === 'hello') hello = true;
    if (message.type === 'call') currentCall = message.state === 'started' ? message.callId : null;
    if (!port) return;
    try {
      if (message.type === 'pcm') {
        port.postMessage({ type: 'pcm', channel: message.channel, callId: message.callId, b64: toBase64(message.pcm) });
      } else if (message.type === 'call' || message.type === 'hello') {
        port.postMessage({ type: message.type, state: message.state, callId: message.callId });
      }
    } catch (_) {
      port = null; // the port closed between checks; onDisconnect reconnects
    }
  });

  connect();
})();
