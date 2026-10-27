const $ = (id) => document.getElementById(id);

function set(id, text, cls) {
  $(id).textContent = text;
  $(id).className = cls || '';
}

async function load() {
  const [tab] = await chrome.tabs.query({ active: true, currentWindow: true });
  const s = await chrome.runtime.sendMessage({ target: 'background', type: 'status' });
  const on3cx = /3cx/i.test(tab?.url || '');
  const thisTabHooked = !!tab && (s?.hookedTabIds || []).includes(tab.id);
  set('hook', s?.hooked ? 'connected' : 'not found', s?.hooked ? 'good' : 'warn');
  set('call', s?.inCall ? 'in a call' : 'no call', s?.inCall ? 'good' : 'muted');
  set('pages', s?.pages ? 'open' : 'not open', s?.pages ? 'good' : 'warn');
  let hint = '';
  if (!s?.hooked) hint = 'Open (or reload) the 3CX web client tab once after installing or updating this extension.';
  else if (!s?.pages) hint = 'Open the assistant\'s Live Call screen. It starts listening when you answer a call.';
  else hint = 'Ready. Answer calls in 3CX as usual: the assistant follows them by itself.';
  if (on3cx && !thisTabHooked) hint = 'Reload this 3CX tab once so the extension can follow its calls.';
  $('hint').textContent = hint;
  $('backup').hidden = !on3cx;
  if (on3cx && s?.grantedTabId === tab.id) $('grant').textContent = 'Using this tab ✓';
  $('version').textContent = `Version ${s?.version || ''}`;
}

$('grant').addEventListener('click', async () => {
  $('error').textContent = '';
  const [tab] = await chrome.tabs.query({ active: true, currentWindow: true });
  if (!tab?.id) return;
  // Opening this popup on the tab is the browser's permission to share its sound (activeTab).
  const reply = await chrome.runtime.sendMessage({ target: 'background', type: 'grant', tabId: tab.id });
  if (!reply?.ok) $('error').textContent = 'Could not save.';
  load();
});

load();
