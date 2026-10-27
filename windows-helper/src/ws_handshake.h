// The WebSocket opening handshake (HTTP Upgrade). Parsing is pure; the accept key uses Windows CNG.
#ifndef SA3CX_WS_HANDSHAKE_H_
#define SA3CX_WS_HANDSHAKE_H_

#include <string>

namespace sa3cx {
namespace ws {

struct HandshakeRequest {
  std::string request_line;  // e.g. "GET /3cx-audio HTTP/1.1"
  std::string origin;
  std::string upgrade;  // lower-cased
  std::string version;
  std::string key;
};

// Reads the request line and the headers this program checks (names are case-insensitive).
HandshakeRequest ParseHandshake(const std::string& request);

// True if the request asks for `path` and is a well-formed WebSocket upgrade (version 13, a key
// of sane length).
bool IsValidUpgrade(const HandshakeRequest& request, const std::string& path);

// Sec-WebSocket-Accept: base64(SHA-1(key + RFC 6455 GUID)).
std::string ComputeAcceptKey(const std::string& key);

// The 101 response and a minimal error response.
std::string SwitchingProtocolsResponse(const std::string& accept_key);
std::string ErrorResponse(const char* status);

// For logging a value that came from outside: printable ASCII only, at most `max_length`.
std::string Printable(const std::string& value, std::size_t max_length);

}  // namespace ws
}  // namespace sa3cx

#endif  // SA3CX_WS_HANDSHAKE_H_
