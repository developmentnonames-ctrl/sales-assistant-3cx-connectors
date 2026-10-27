// The frame parser faces any local program: these cover the attacks from the security review.
#include "ws_frame.h"

#include <cstdint>
#include <vector>

#include "test.h"

namespace {

using sa3cx::ws::ParseClientFrame;
using sa3cx::ws::ParseStatus;

constexpr std::size_t kMax = 4096;

// A masked client frame (FIN set) with the given opcode and payload.
std::vector<uint8_t> ClientFrame(uint8_t opcode, const std::string& payload) {
  const uint8_t mask[4] = {0x11, 0x22, 0x33, 0x44};
  std::vector<uint8_t> frame = {static_cast<uint8_t>(0x80 | opcode)};
  if (payload.size() < 126) {
    frame.push_back(static_cast<uint8_t>(0x80 | payload.size()));
  } else {
    frame.push_back(0x80 | 126);
    frame.push_back(static_cast<uint8_t>(payload.size() >> 8));
    frame.push_back(static_cast<uint8_t>(payload.size() & 0xFF));
  }
  frame.insert(frame.end(), mask, mask + 4);
  for (std::size_t i = 0; i < payload.size(); ++i) {
    frame.push_back(static_cast<uint8_t>(payload[i]) ^ mask[i % 4]);
  }
  return frame;
}

}  // namespace

TEST(WsFrame, ParsesAMaskedTextFrame) {
  const auto result = ParseClientFrame(ClientFrame(0x1, "{\"type\":\"call\"}"), kMax);
  EXPECT_TRUE(result.status == ParseStatus::kFrame);
  EXPECT_EQ(result.frame.opcode, 0x1);
  EXPECT_EQ(std::string(result.frame.payload.begin(), result.frame.payload.end()),
            std::string("{\"type\":\"call\"}"));
}

TEST(WsFrame, WaitsForTheRestOfAFrame) {
  auto frame = ClientFrame(0x1, "hello");
  frame.pop_back();
  EXPECT_TRUE(ParseClientFrame(frame, kMax).status == ParseStatus::kNeedMore);
  EXPECT_TRUE(ParseClientFrame({0x81}, kMax).status == ParseStatus::kNeedMore);
}

TEST(WsFrame, RejectsUnmaskedFrames) {
  const std::vector<uint8_t> frame = {0x81, 0x02, 'h', 'i'};
  EXPECT_TRUE(ParseClientFrame(frame, kMax).status == ParseStatus::kInvalid);
}

TEST(WsFrame, RejectsFragmentsReservedBitsAndUnknownOpcodes) {
  auto fragment = ClientFrame(0x1, "a");
  fragment[0] &= 0x7F;  // FIN cleared
  EXPECT_TRUE(ParseClientFrame(fragment, kMax).status == ParseStatus::kInvalid);
  auto reserved = ClientFrame(0x1, "a");
  reserved[0] |= 0x40;
  EXPECT_TRUE(ParseClientFrame(reserved, kMax).status == ParseStatus::kInvalid);
  EXPECT_TRUE(ParseClientFrame(ClientFrame(0x3, "a"), kMax).status == ParseStatus::kInvalid);
  EXPECT_TRUE(ParseClientFrame(ClientFrame(0x0, "a"), kMax).status == ParseStatus::kInvalid);
}

TEST(WsFrame, RejectsFramesOverTheLimit) {
  EXPECT_TRUE(ParseClientFrame(ClientFrame(0x1, std::string(5000, 'x')), kMax).status ==
              ParseStatus::kInvalid);
}

TEST(WsFrame, RejectsAHugeDeclaredLengthWithoutOverflow) {
  // 14 + length wraps around to 1: this crashed version 1.8.0.
  std::vector<uint8_t> frame = {0x81, 0x80 | 127};
  const uint64_t length = ~uint64_t{0} - 12;
  for (int i = 7; i >= 0; --i) frame.push_back(static_cast<uint8_t>(length >> (8 * i)));
  frame.insert(frame.end(), 20, 0);
  EXPECT_TRUE(ParseClientFrame(frame, kMax).status == ParseStatus::kInvalid);
}

TEST(WsFrame, EncodesServerFramesWithTheRightLengthForm) {
  const std::string small = sa3cx::ws::EncodeServerFrame(0x1, "abc", 3);
  EXPECT_EQ(small.size(), 5u);
  EXPECT_EQ(static_cast<uint8_t>(small[0]), 0x81);
  EXPECT_EQ(static_cast<uint8_t>(small[1]), 3);
  const std::string audio(3200, '\0');
  const std::string medium = sa3cx::ws::EncodeServerFrame(0x2, audio.data(), audio.size());
  EXPECT_EQ(medium.size(), 3204u);
  EXPECT_EQ(static_cast<uint8_t>(medium[1]), 126);
}
