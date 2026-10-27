#include "resampler.h"

#include <cmath>
#include <vector>

#include "test.h"

TEST(Resampler, Turns48kHzIntoOneFrameOf16kHzPer100ms) {
  std::vector<std::vector<int16_t>> frames;
  sa3cx::Resampler resampler(48000, 16000, 1600,
                             [&](const std::vector<int16_t>& frame) { frames.push_back(frame); });
  const std::vector<int16_t> input(48000, 1000);  // 1 s
  resampler.Push(input.data(), input.size());
  EXPECT_EQ(frames.size(), 10u);
  EXPECT_EQ(frames[0].size(), 1600u);
  EXPECT_EQ(frames[9][1599], 1000);
}

TEST(Resampler, HandlesRatesThatAreNotWholeMultiples) {
  int frames = 0;
  sa3cx::Resampler resampler(44100, 16000, 1600, [&](const std::vector<int16_t>&) { ++frames; });
  const std::vector<int16_t> input(44100, 0);
  resampler.Push(input.data(), input.size());
  EXPECT_TRUE(frames == 9 || frames == 10);  // 1 s, rounding at the edge
}

TEST(Resampler, KeepsATonesFrequency) {
  std::vector<int16_t> output;
  sa3cx::Resampler resampler(48000, 16000, 1600, [&](const std::vector<int16_t>& frame) {
    output.insert(output.end(), frame.begin(), frame.end());
  });
  std::vector<int16_t> tone(48000);
  for (std::size_t i = 0; i < tone.size(); ++i) {
    tone[i] = static_cast<int16_t>(10000 * std::sin(2 * 3.14159265 * 440 * i / 48000.0));
  }
  resampler.Push(tone.data(), tone.size());
  int crossings = 0;
  for (std::size_t i = 1; i < output.size(); ++i) {
    if ((output[i - 1] < 0) != (output[i] < 0)) ++crossings;
  }
  EXPECT_TRUE(crossings / 2 >= 435 && crossings / 2 <= 445);  // still about 440 Hz
}
