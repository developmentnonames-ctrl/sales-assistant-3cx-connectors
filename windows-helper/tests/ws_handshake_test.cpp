#include "ws_handshake.h"

#include "test.h"

namespace {

std::string Request(const std::string& path, const std::string& extra) {
  return "GET " + path +
         " HTTP/1.1\r\nHost: 127.0.0.1:47923\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
         "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n" +
         extra + "\r\n";
}

}  // namespace

TEST(WsHandshake, ReadsTheHeadersCaseInsensitively) {
  const auto parsed = sa3cx::ws::ParseHandshake(Request("/3cx-audio", "oRiGiN: http://localhost:3004\r\n"));
  EXPECT_EQ(parsed.origin, std::string("http://localhost:3004"));
  EXPECT_EQ(parsed.upgrade, std::string("websocket"));
  EXPECT_EQ(parsed.version, std::string("13"));
  EXPECT_TRUE(sa3cx::ws::IsValidUpgrade(parsed, "/3cx-audio"));
}

TEST(WsHandshake, RejectsOtherPathsAndBadUpgrades) {
  EXPECT_TRUE(!sa3cx::ws::IsValidUpgrade(sa3cx::ws::ParseHandshake(Request("/other", "")), "/3cx-audio"));
  EXPECT_TRUE(!sa3cx::ws::IsValidUpgrade(sa3cx::ws::ParseHandshake(Request("/3cx-audioX", "")), "/3cx-audio"));
  auto no_upgrade = sa3cx::ws::ParseHandshake("GET /3cx-audio HTTP/1.1\r\nSec-WebSocket-Key: a\r\n\r\n");
  EXPECT_TRUE(!sa3cx::ws::IsValidUpgrade(no_upgrade, "/3cx-audio"));
  auto long_key = sa3cx::ws::ParseHandshake(Request("/3cx-audio", ""));
  long_key.key = std::string(65, 'k');
  EXPECT_TRUE(!sa3cx::ws::IsValidUpgrade(long_key, "/3cx-audio"));
}

TEST(WsHandshake, ComputesTheRfc6455AcceptKey) {
  // The example from RFC 6455, section 1.3.
  EXPECT_EQ(sa3cx::ws::ComputeAcceptKey("dGhlIHNhbXBsZSBub25jZQ=="),
            std::string("s3pPLMBiTxaQ9kYGzzhZRbK+xOo="));
}

TEST(WsHandshake, PrintableStripsControlCharacters) {
  EXPECT_EQ(sa3cx::ws::Printable("a\r\nb\x01", 80), std::string("a??b?"));
  EXPECT_EQ(sa3cx::ws::Printable(std::string(200, 'x'), 80).size(), 80u);
}
