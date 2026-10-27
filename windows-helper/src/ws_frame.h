// WebSocket frames (RFC 6455), the small subset this program needs. Pure functions: no I/O.
//
// Frames from the browser are parsed strictly: they must be masked, unfragmented, use a known
// opcode and be at most `max_payload` bytes. Anything else is kInvalid and the caller closes the
// connection. The length is checked before any size arithmetic, so a huge declared length can't
// overflow it.
#ifndef SA3CX_WS_FRAME_H_
#define SA3CX_WS_FRAME_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace sa3cx {
namespace ws {

enum Opcode : uint8_t {
  kText = 0x1,
  kBinary = 0x2,
  kClose = 0x8,
  kPing = 0x9,
  kPong = 0xA,
};

struct Frame {
  uint8_t opcode = 0;
  std::vector<uint8_t> payload;  // unmasked
};

enum class ParseStatus {
  kNeedMore,  // not a whole frame yet
  kFrame,     // one frame parsed; `consumed` bytes used from the buffer
  kInvalid,   // breaks the rules above: close the connection
};

struct ParseResult {
  ParseStatus status = ParseStatus::kNeedMore;
  Frame frame;
  std::size_t consumed = 0;
};

// Parses one client frame from the start of `buffer`.
ParseResult ParseClientFrame(const std::vector<uint8_t>& buffer, std::size_t max_payload);

// Encodes one unmasked server frame (FIN set).
std::string EncodeServerFrame(uint8_t opcode, const void* data, std::size_t length);

}  // namespace ws
}  // namespace sa3cx

#endif  // SA3CX_WS_FRAME_H_
