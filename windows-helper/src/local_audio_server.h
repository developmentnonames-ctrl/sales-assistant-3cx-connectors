// The local WebSocket the Live Call page connects to: ws://127.0.0.1:<port>/3cx-audio.
//
// Who may connect: a program in the same Windows session whose browser-sent Origin is one of the
// assistant's addresses (see SECURITY.md). Several pages may connect; only the newest one (the
// "owner") gets the sound, and only while it says a call is on.
//
// Messages:
//   page -> helper  text   {"type":"call","active":true|false,"caller":"0771234567"}
//   helper -> page  text   {"type":"status",...,"owner":true|false,"screens":N}
//   helper -> page  binary 100 ms of 16 kHz 16-bit mono PCM (owner only, during a call)
#ifndef SA3CX_LOCAL_AUDIO_SERVER_H_
#define SA3CX_LOCAL_AUDIO_SERVER_H_

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "win32.h"

#include "app_state.h"

namespace sa3cx {

class LocalAudioServer {
 public:
  LocalAudioServer(AppState& state, unsigned short port, std::vector<std::string> allowed_origins);
  ~LocalAudioServer();
  LocalAudioServer(const LocalAudioServer&) = delete;
  LocalAudioServer& operator=(const LocalAudioServer&) = delete;

  // Binds 127.0.0.1:port and starts accepting. False if the port is taken.
  bool Start();
  void Stop();

  // Sends 100 ms of sound to the owner page if it is in a call. True if it was sent.
  bool SendAudio(const int16_t* samples, std::size_t count);

  // Tells every page the current status (e.g. the 3CX app was found or closed).
  void BroadcastStatus();

  // Reconnect: closes every page connection; pages reconnect by themselves.
  void DropAll();

 private:
  struct Client {
    SOCKET socket = INVALID_SOCKET;
    std::atomic<bool> alive{true};
    bool in_call = false;    // guarded by mutex_
    std::string caller_id;   // guarded by mutex_; sanitized
  };

  void AcceptLoop();
  void HandleConnection(SOCKET socket, unsigned short client_port_net);
  void ReadLoop(std::shared_ptr<Client> client);
  void OnText(Client* client, const std::string& text);

  // All *Locked methods require mutex_.
  Client* OwnerLocked() const;
  bool SendLocked(Client* client, uint8_t opcode, const void* data, std::size_t length);
  void CloseLocked(Client* client);
  void RemoveClosedLocked();
  void SendStatusLocked();
  void UpdateCallLocked();
  std::string StatusJson(bool owner) const;

  AppState& state_;
  const unsigned short port_;
  const std::vector<std::string> allowed_origins_;
  SOCKET listen_socket_ = INVALID_SOCKET;
  std::atomic<int> handshakes_{0};

  mutable std::mutex mutex_;
  std::vector<std::shared_ptr<Client>> clients_;  // oldest first; the last one is the owner
};

}  // namespace sa3cx

#endif  // SA3CX_LOCAL_AUDIO_SERVER_H_
