// Records the sound of the 3CX app, as heard on the output device it plays to.
//
// Why the device and not the app: Windows' per-app capture (process loopback) returns the 3CX
// Store app's ringtone but silence for its calls (tested on Windows 11 build 26200; others report
// the same). The call audio is in the device's mix, so the device is recorded (WASAPI loopback,
// the same method as OBS's "Audio Output Capture") and filtered: sound passes only while the 3CX
// app's own session meter shows it is playing (plus 300 ms, to keep word endings).
//
// Runs on its own thread until AppState::running is false. Follows the 3CX app (started, closed,
// restarted) and the device it uses (headset switched).
#ifndef SA3CX_AUDIO_CAPTURE_H_
#define SA3CX_AUDIO_CAPTURE_H_

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "app_state.h"

namespace sa3cx {

struct CaptureOptions {
  std::wstring process_name;    // the 3CX app's executable
  uint32_t fixed_process_id = 0;  // diagnostics: capture this process instead
};

class AudioCapture {
 public:
  // Receives 100 ms frames of 16 kHz mono 16-bit sound (silence while 3CX is quiet).
  using FrameSink = std::function<void(const std::vector<int16_t>& frame)>;
  // Called when the 3CX app is found or lost, or recording starts or stops.
  using StateChanged = std::function<void()>;

  AudioCapture(AppState& state, CaptureOptions options, FrameSink sink, StateChanged on_changed);

  // Blocks until AppState::running is false. Call on a dedicated thread.
  void Run();

 private:
  uint32_t FindTarget() const;
  // Captures until the process exits, moves device, or Reconnect. False if it could not start.
  bool CaptureFrom(uint32_t process_id);

  AppState& state_;
  const CaptureOptions options_;
  FrameSink sink_;
  StateChanged on_changed_;
};

}  // namespace sa3cx

#endif  // SA3CX_AUDIO_CAPTURE_H_
