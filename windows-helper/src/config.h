// Fixed settings. The program agents get takes none of these from the command line (only the
// diagnostics build does, see main.cpp), so they can't be changed by editing a shortcut.
#ifndef SA3CX_CONFIG_H_
#define SA3CX_CONFIG_H_

#include <cstddef>
#include <string>
#include <vector>

namespace sa3cx {

// The local WebSocket the Live Call page connects to: ws://127.0.0.1:47923/3cx-audio
inline constexpr unsigned short kDefaultPort = 47923;
inline constexpr char kAudioPath[] = "/3cx-audio";

// The 3CX app from the Microsoft Store.
inline constexpr wchar_t kDefaultProcessName[] = L"3CXSoftphone.exe";

// The only web pages that may connect (exact match on the browser-sent Origin header).
// Release builds: the hosted assistant only. Development builds (CMake -DSA3CX_DEV_ORIGINS=ON)
// also allow the assistant running on this PC.
inline const std::vector<std::string>& DefaultAllowedOrigins() {
  static const std::vector<std::string> origins = {
      "https://aicallagent.chassia.com",
#ifdef SA3CX_DEV_ORIGINS
      "http://localhost:3004",
      "http://127.0.0.1:3004",
#endif
  };
  return origins;
}

// Audio sent to the page: 16 kHz, 16-bit, mono, in 100 ms frames.
inline constexpr int kTargetSampleRate = 16000;
inline constexpr int kFrameSamples = 1600;

// Limits on what local programs may send or open (see SECURITY.md).
inline constexpr std::size_t kMaxClientFrameBytes = 4096;  // the page sends tiny JSON messages
inline constexpr std::size_t kMaxHandshakeBytes = 16384;
inline constexpr int kMaxScreens = 8;      // Live Call pages connected at once
inline constexpr int kMaxHandshakes = 16;  // connections still being set up at once
inline constexpr unsigned long kHandshakeTimeoutMs = 3000;
inline constexpr unsigned long kSendTimeoutMs = 500;

}  // namespace sa3cx

#endif  // SA3CX_CONFIG_H_
