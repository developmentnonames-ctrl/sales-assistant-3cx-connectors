#include "ws_frame.h"

namespace sa3cx {
namespace ws {

namespace {

bool IsKnownOpcode(uint8_t opcode) {
  return opcode == kText || opcode == kBinary || opcode == kClose || opcode == kPing ||
         opcode == kPong;
}

}  // namespace

ParseResult ParseClientFrame(const std::vector<uint8_t>& buffer, std::size_t max_payload) {
  ParseResult result;
  if (buffer.size() < 2) return result;

  const uint8_t first = buffer[0];
  const uint8_t second = buffer[1];
  const bool fin = (first & 0x80) != 0;
  const bool reserved_bits = (first & 0x70) != 0;
  const uint8_t opcode = first & 0x0F;
  const bool masked = (second & 0x80) != 0;

  // Rules that need no length: reject at once.
  if (!fin || reserved_bits || !IsKnownOpcode(opcode) || !masked) {
    result.status = ParseStatus::kInvalid;
    return result;
  }

  uint64_t length = second & 0x7F;
  std::size_t header = 2;
  if (length == 126) {
    if (buffer.size() < 4) return result;
    length = (static_cast<uint64_t>(buffer[2]) << 8) | buffer[3];
    header = 4;
  } else if (length == 127) {
    if (buffer.size() < 10) return result;
    length = 0;
    for (int i = 0; i < 8; ++i) length = (length << 8) | buffer[2 + i];
    header = 10;
  }
  if (length > max_payload) {  // before any arithmetic with it
    result.status = ParseStatus::kInvalid;
    return result;
  }

  const std::size_t mask_at = header;
  const std::size_t payload_at = header + 4;
  const std::size_t total = payload_at + static_cast<std::size_t>(length);
  if (buffer.size() < total) return result;

  result.frame.opcode = opcode;
  result.frame.payload.assign(buffer.begin() + payload_at, buffer.begin() + total);
  for (std::size_t i = 0; i < result.frame.payload.size(); ++i) {
    result.frame.payload[i] ^= buffer[mask_at + (i % 4)];
  }
  result.consumed = total;
  result.status = ParseStatus::kFrame;
  return result;
}

std::string EncodeServerFrame(uint8_t opcode, const void* data, std::size_t length) {
  std::string frame;
  frame.reserve(length + 10);
  frame.push_back(static_cast<char>(0x80 | opcode));
  if (length < 126) {
    frame.push_back(static_cast<char>(length));
  } else if (length < 65536) {
    frame.push_back(static_cast<char>(126));
    frame.push_back(static_cast<char>((length >> 8) & 0xFF));
    frame.push_back(static_cast<char>(length & 0xFF));
  } else {
    frame.push_back(static_cast<char>(127));
    for (int i = 7; i >= 0; --i) {
      frame.push_back(static_cast<char>((static_cast<uint64_t>(length) >> (8 * i)) & 0xFF));
    }
  }
  frame.append(static_cast<const char*>(data), length);
  return frame;
}

}  // namespace ws
}  // namespace sa3cx
