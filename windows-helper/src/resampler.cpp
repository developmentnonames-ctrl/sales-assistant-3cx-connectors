#include "resampler.h"

#include <algorithm>

namespace sa3cx {

Resampler::Resampler(int input_rate, int output_rate, std::size_t frame_samples, FrameSink sink)
    : step_(static_cast<double>(input_rate) / output_rate),
      frame_samples_(frame_samples),
      sink_(std::move(sink)) {
  frame_.reserve(frame_samples_);
}

void Resampler::Push(const int16_t* samples, std::size_t count) {
  for (std::size_t i = 0; i < count; ++i) {
    sum_ += samples[i];
    ++count_;
    position_ += 1;
    if (position_ < step_) continue;
    position_ -= step_;
    const double average = sum_ / count_;
    frame_.push_back(static_cast<int16_t>(std::clamp(average, -32768.0, 32767.0)));
    sum_ = 0;
    count_ = 0;
    if (frame_.size() == frame_samples_) {
      sink_(frame_);
      frame_.clear();
    }
  }
}

}  // namespace sa3cx
