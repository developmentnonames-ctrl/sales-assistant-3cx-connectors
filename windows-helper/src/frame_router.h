// Where each 100 ms frame of 3CX sound goes: the level shown in the tray, the Live Call page (if
// a call is on), the per-call counts in the log, and - diagnostics build only - a WAV file.
#ifndef SA3CX_FRAME_ROUTER_H_
#define SA3CX_FRAME_ROUTER_H_

#include <cstdint>
#include <vector>

#include "app_state.h"
#include "local_audio_server.h"
#include "wav_writer.h"

namespace sa3cx {

class FrameRouter {
 public:
  FrameRouter(AppState& state, LocalAudioServer& server, WavWriter* recording);

  // Called by the capture thread for every frame.
  void OnFrame(const std::vector<int16_t>& frame);

 private:
  struct CallCounts {
    int sent = 0;
    int total = 0;
    int with_sound = 0;
    int loudest = 0;
  };

  AppState& state_;
  LocalAudioServer& server_;
  WavWriter* recording_;  // not owned; may be null
  bool counting_ = false;
  CallCounts counts_;
  uint32_t last_report_tick_ = 0;
};

// Loudness of a frame, 0 (silent) to 1000 (full scale RMS).
int FrameLevel(const std::vector<int16_t>& frame);

}  // namespace sa3cx

#endif  // SA3CX_FRAME_ROUTER_H_
