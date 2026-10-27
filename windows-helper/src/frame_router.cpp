#include "frame_router.h"

#include <algorithm>
#include <cmath>

#include "win32.h"

#include "log.h"

namespace sa3cx {

namespace {
constexpr int kSoundLevel = 10;          // a frame at or above this counts as sound
constexpr uint32_t kReportEveryMs = 5000;
}  // namespace

int FrameLevel(const std::vector<int16_t>& frame) {
  if (frame.empty()) return 0;
  double sum = 0;
  for (int16_t sample : frame) sum += static_cast<double>(sample) * sample;
  return static_cast<int>(std::sqrt(sum / frame.size()) / 32768.0 * 1000);
}

FrameRouter::FrameRouter(AppState& state, LocalAudioServer& server, WavWriter* recording)
    : state_(state), server_(server), recording_(recording) {}

void FrameRouter::OnFrame(const std::vector<int16_t>& frame) {
  const int level = FrameLevel(frame);
  const uint32_t now = GetTickCount();
  state_.level = level;
  state_.level_tick = now;
  if (level >= kSoundLevel) state_.last_sound_tick = now;

  const bool sent = server_.SendAudio(frame.data(), frame.size());
  if (recording_ != nullptr) recording_->Write(frame);

  // Per-call counts, so a call where nothing arrived can be explained from the log.
  const bool in_call = state_.in_call;
  if (in_call && !counting_) {
    counts_ = CallCounts{};
    last_report_tick_ = now;
  }
  if (!in_call && counting_) {
    log::Write("call ended: sent %d of %d frames to Live Call, %d with sound, loudest %d/1000",
               counts_.sent, counts_.total, counts_.with_sound, counts_.loudest);
  }
  counting_ = in_call;
  if (!in_call) return;
  ++counts_.total;
  if (sent) ++counts_.sent;
  if (level >= kSoundLevel) ++counts_.with_sound;
  counts_.loudest = std::max(counts_.loudest, level);
  if (now - last_report_tick_ >= kReportEveryMs) {
    log::Write("in call: sent %d of %d frames, %d with sound, loudest %d/1000", counts_.sent,
               counts_.total, counts_.with_sound, counts_.loudest);
    last_report_tick_ = now;
  }
}

}  // namespace sa3cx
