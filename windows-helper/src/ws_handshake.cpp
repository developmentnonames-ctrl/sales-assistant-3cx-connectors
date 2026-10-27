#include "ws_handshake.h"

#include <algorithm>
#include <cctype>
#include <cstdint>

#include "win32.h"

#include <bcrypt.h>
#include <wincrypt.h>

namespace sa3cx {
namespace ws {

namespace {

constexpr char kGuid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
constexpr std::size_t kMaxKeyLength = 64;

std::string ToLower(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return text;
}

std::string Trim(const std::string& text) {
  const auto first = text.find_first_not_of(" \t");
  if (first == std::string::npos) return "";
  const auto last = text.find_last_not_of(" \t");
  return text.substr(first, last - first + 1);
}

// The value of header `name` (case-insensitive), or "".
std::string HeaderValue(const std::string& request, const std::string& lower_request,
                        const std::string& name) {
  const std::string key = "\r\n" + ToLower(name) + ":";
  const std::size_t at = lower_request.find(key);
  if (at == std::string::npos) return "";
  const std::size_t start = at + key.size();
  const std::size_t end = request.find("\r\n", start);
  return Trim(request.substr(start, end == std::string::npos ? std::string::npos : end - start));
}

}  // namespace

HandshakeRequest ParseHandshake(const std::string& request) {
  HandshakeRequest parsed;
  const std::string lower = ToLower(request);
  parsed.request_line = request.substr(0, request.find("\r\n"));
  parsed.origin = HeaderValue(request, lower, "Origin");
  parsed.upgrade = ToLower(HeaderValue(request, lower, "Upgrade"));
  parsed.version = HeaderValue(request, lower, "Sec-WebSocket-Version");
  parsed.key = HeaderValue(request, lower, "Sec-WebSocket-Key");
  return parsed;
}

bool IsValidUpgrade(const HandshakeRequest& request, const std::string& path) {
  const std::string expected = "GET " + path + " HTTP/1.1";
  return request.request_line == expected && request.upgrade == "websocket" &&
         request.version == "13" && !request.key.empty() && request.key.size() <= kMaxKeyLength;
}

std::string ComputeAcceptKey(const std::string& key) {
  const std::string input = key + kGuid;
  uint8_t digest[20] = {0};
  BCRYPT_ALG_HANDLE algorithm = nullptr;
  if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA1_ALGORITHM, nullptr, 0) == 0) {
    BCryptHash(algorithm, nullptr, 0,
               reinterpret_cast<PUCHAR>(const_cast<char*>(input.data())),
               static_cast<ULONG>(input.size()), digest, sizeof digest);
    BCryptCloseAlgorithmProvider(algorithm, 0);
  }
  char encoded[64];
  DWORD encoded_length = sizeof encoded;
  if (!CryptBinaryToStringA(digest, sizeof digest, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                            encoded, &encoded_length)) {
    return "";
  }
  return std::string(encoded, encoded_length);
}

std::string SwitchingProtocolsResponse(const std::string& accept_key) {
  return "HTTP/1.1 101 Switching Protocols\r\n"
         "Upgrade: websocket\r\n"
         "Connection: Upgrade\r\n"
         "Sec-WebSocket-Accept: " +
         accept_key + "\r\n\r\n";
}

std::string ErrorResponse(const char* status) {
  return std::string("HTTP/1.1 ") + status + "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
}

std::string Printable(const std::string& value, std::size_t max_length) {
  std::string shown = value.substr(0, max_length);
  for (char& ch : shown) {
    if (ch < 0x20 || ch > 0x7E) ch = '?';
  }
  return shown;
}

}  // namespace ws
}  // namespace sa3cx
