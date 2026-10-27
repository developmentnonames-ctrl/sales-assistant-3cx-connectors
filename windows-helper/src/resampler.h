// Turns mono 16-bit audio at any rate into 16 kHz frames of kFrameSamples (100 ms), by averaging
// the input samples that fall into each output sample. Pure: no I/O.
#ifndef SA3CX_RESAMPLER_H_
#define SA3CX_RESAMPLER_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace sa3cx {

class Resampler {
 public:
  using FrameSink = std::function<void(const std::vector<int16_t>& frame)>;

  Resampler(int input_rate, int output_rate, std::size_t frame_samples, FrameSink sink);

  // Feeds mono input; calls the sink for every complete output frame.
  void Push(const int16_t* samples, std::size_t count);

 private:
  const double step_;  // input samples per output sample
  const std::size_t frame_samples_;
  FrameSink sink_;
  double position_ = 0;
  double sum_ = 0;
  int count_ = 0;
  std::vector<int16_t> frame_;
};

}  // namespace sa3cx

#endif  // SA3CX_RESAMPLER_H_
