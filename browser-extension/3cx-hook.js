// Runs INSIDE the 3CX web client page (main world, before 3CX's own code), on 3CX hosts only.
// The 3CX V20 web client makes its calls with RTCPeerConnection in this page (checked in its code:
// `peerConnection.ontrack = e => remoteStream$.next(e.streams[0])`, the mic added with addTrack /
// replaceTrack, `peerConnection.close()` at the end). This script watches those connections and,
// while a call is connected, turns:
//   the received audio (the other party = the customer)  -> channel 1
//   the sent audio (the agent's microphone)              -> channel 2
// into 16 kHz 16-bit mono PCM in 100 ms frames, posted to the extension's bridge script. It never
// changes, mutes, answers or ends anything: 3CX's own call works exactly as before.
(() => {
  if (window.__lcaCallHook) return;
  window.__lcaCallHook = true;
  const Native = window.RTCPeerConnection;
  if (!Native) return;

  const TAG = '__lca3cx';
  const CH_CUSTOMER = 1;
  const CH_AGENT = 2;
  let ctx = null;
  const taps = new Map(); // track id -> { nodes, channel }
  let activePc = null;
  let callId = null;
  let seq = 0;

  // To this same window only (the bridge script listens here), never to other origins.
  const post = (message, transfer) =>
    window.postMessage({ [TAG]: true, ...message }, window.location.origin, transfer || []);

  function audioContext() {
    if (!ctx || ctx.state === 'closed') ctx = new AudioContext();
    if (ctx.state === 'suspended') ctx.resume().catch(() => {});
    return ctx;
  }

  function tap(track, channel) {
    if (!track || track.kind !== 'audio' || taps.has(track.id)) return;
    const c = audioContext();
    const source = c.createMediaStreamSource(new MediaStream([track]));
    // ScriptProcessor (not an AudioWorklet): no module URL, so the 3CX page's own CSP never blocks it.
    const processor = c.createScriptProcessor(4096, 1, 1);
    const step = c.sampleRate / 16000;
    let pos = 0, sum = 0, count = 0, length = 0;
    let frame = new Int16Array(1600);
    processor.onaudioprocess = (e) => {
      const input = e.inputBuffer.getChannelData(0);
      for (let i = 0; i < input.length; i++) {
        sum += input[i];
        count += 1;
        pos += 1;
        if (pos >= step) {
          pos -= step;
          const v = Math.max(-1, Math.min(1, sum / count));
          frame[length++] = v < 0 ? v * 0x8000 : v * 0x7fff;
          sum = 0;
          count = 0;
          if (length === frame.length) {
            post({ type: 'pcm', channel, callId, pcm: frame.buffer }, [frame.buffer]);
            frame = new Int16Array(1600);
            length = 0;
          }
        }
      }
    };
    const silent = c.createGain();
    silent.gain.value = 0; // a processor must reach the destination to run; nothing is heard from it
    source.connect(processor);
    processor.connect(silent);
    silent.connect(c.destination);
    taps.set(track.id, { nodes: [source, processor, silent], channel });
    track.addEventListener('ended', () => untap(track.id));
  }

  function untap(id) {
    const t = taps.get(id);
    if (!t) return;
    t.nodes.forEach((n) => { try { n.disconnect(); } catch (_) { /* already */ } });
    taps.delete(id);
  }

  function untapAll() {
    [...taps.keys()].forEach(untap);
  }

  function startCall(pc) {
    if (activePc === pc) return;
    if (activePc) endCall(activePc); // a second call (the first on hold): follow the new one
    activePc = pc;
    callId = `w${Date.now()}-${++seq}`;
    post({ type: 'call', state: 'started', callId });
    pc.getReceivers().forEach((r) => tap(r.track, CH_CUSTOMER));
    pc.getSenders().forEach((s) => tap(s.track, CH_AGENT));
  }

  function endCall(pc) {
    if (activePc !== pc) return;
    untapAll();
    post({ type: 'call', state: 'ended', callId });
    activePc = null;
    callId = null;
  }

  function watch(pc) {
    pc.addEventListener('track', (e) => {
      if (activePc === pc) tap(e.track, CH_CUSTOMER);
    });
    pc.addEventListener('connectionstatechange', () => {
      const state = pc.connectionState;
      if (state === 'connected') startCall(pc);
      else if (state === 'failed' || state === 'closed') endCall(pc);
    });
    const nativeClose = pc.close.bind(pc);
    pc.close = function close() { // close() fires no state event: catch the end of the call here
      endCall(pc);
      return nativeClose();
    };
    const nativeAddTrack = pc.addTrack.bind(pc);
    pc.addTrack = function addTrack(track, ...streams) {
      const sender = nativeAddTrack(track, ...streams);
      if (activePc === pc) tap(track, CH_AGENT);
      return sender;
    };
  }

  // The agent switched microphones mid-call: 3CX swaps the sent track.
  const nativeReplace = RTCRtpSender.prototype.replaceTrack;
  RTCRtpSender.prototype.replaceTrack = function replaceTrack(track) {
    const result = nativeReplace.apply(this, arguments);
    if (activePc && track && activePc.getSenders().includes(this)) tap(track, CH_AGENT);
    return result;
  };

  function RTCPeerConnection(...args) {
    const pc = new Native(...args);
    try { watch(pc); } catch (_) { /* never break 3CX */ }
    return pc;
  }
  RTCPeerConnection.prototype = Native.prototype; // instanceof keeps working
  Object.setPrototypeOf(RTCPeerConnection, Native); // statics (generateCertificate) too
  window.RTCPeerConnection = RTCPeerConnection;

  post({ type: 'hello' });
})();
