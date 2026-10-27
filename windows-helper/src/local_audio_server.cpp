#include "local_audio_server.h"

#include <algorithm>
#include <cstdio>
#include <thread>

#include "caller_id.h"
#include "config.h"
#include "log.h"
#include "process_info.h"
#include "version.h"
#include "ws_frame.h"
#include "ws_handshake.h"

namespace sa3cx {

namespace {

bool SendAll(SOCKET socket, const std::string& bytes) {
  const char* data = bytes.data();
  int remaining = static_cast<int>(bytes.size());
  while (remaining > 0) {
    int sent = send(socket, data, remaining, 0);
    if (sent <= 0) return false;
    data += sent;
    remaining -= sent;
  }
  return true;
}

void Reply(SOCKET socket, const char* status) {
  SendAll(socket, ws::ErrorResponse(status));
  closesocket(socket);
}

// A deliberately small reader for the one message the page sends:
// {"type":"call","active":true,"caller":"..."}. Unknown messages are ignored.
struct CallMessage {
  bool valid = false;
  bool active = false;
  std::string caller;
};

CallMessage ParseCallMessage(const std::string& text) {
  CallMessage message;
  if (text.find("\"type\":\"call\"") == std::string::npos) return message;
  message.valid = true;
  message.active = text.find("\"active\":true") != std::string::npos;
  const std::string key = "\"caller\":\"";
  const std::size_t start = text.find(key);
  if (start != std::string::npos) {
    const std::size_t from = start + key.size();
    const std::size_t end = text.find('"', from);
    if (end != std::string::npos) message.caller = text.substr(from, end - from);
  }
  return message;
}

}  // namespace

LocalAudioServer::LocalAudioServer(AppState& state, unsigned short port,
                                   std::vector<std::string> allowed_origins)
    : state_(state), port_(port), allowed_origins_(std::move(allowed_origins)) {}

LocalAudioServer::~LocalAudioServer() { Stop(); }

bool LocalAudioServer::Start() {
  WSADATA wsa;
  if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;
  listen_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listen_socket_ == INVALID_SOCKET) return false;

  BOOL exclusive = TRUE;  // no other program may bind the same port alongside us
  setsockopt(listen_socket_, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
             reinterpret_cast<const char*>(&exclusive), sizeof exclusive);
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(port_);
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  // this PC only
  if (bind(listen_socket_, reinterpret_cast<sockaddr*>(&address), sizeof address) != 0 ||
      listen(listen_socket_, SOMAXCONN) != 0) {
    log::Write("port %u is in use: is the helper already running?", port_);
    closesocket(listen_socket_);
    listen_socket_ = INVALID_SOCKET;
    return false;
  }
  std::thread([this] { AcceptLoop(); }).detach();
  log::Write("listening on ws://127.0.0.1:%u%s", port_, kAudioPath);
  return true;
}

void LocalAudioServer::Stop() {
  if (listen_socket_ != INVALID_SOCKET) {
    closesocket(listen_socket_);  // ends AcceptLoop
    listen_socket_ = INVALID_SOCKET;
  }
  DropAll();
}

void LocalAudioServer::AcceptLoop() {
  while (state_.running) {
    sockaddr_in peer{};
    int peer_length = sizeof peer;
    SOCKET socket = accept(listen_socket_, reinterpret_cast<sockaddr*>(&peer), &peer_length);
    if (socket == INVALID_SOCKET) break;
    if (handshakes_ >= kMaxHandshakes) {  // too many connections being set up at once
      closesocket(socket);
      continue;
    }
    ++handshakes_;
    std::thread([this, socket, port = peer.sin_port] {
      HandleConnection(socket, port);
      --handshakes_;
    }).detach();
  }
}

void LocalAudioServer::HandleConnection(SOCKET socket, unsigned short client_port_net) {
  DWORD timeout = kHandshakeTimeoutMs;
  setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout),
             sizeof timeout);

  // 1. Read the HTTP upgrade request (bounded in size and time).
  std::string request;
  char chunk[2048];
  while (request.find("\r\n\r\n") == std::string::npos && request.size() < kMaxHandshakeBytes) {
    int received = recv(socket, chunk, sizeof chunk, 0);
    if (received <= 0) {
      closesocket(socket);
      return;
    }
    request.append(chunk, received);
  }
  const ws::HandshakeRequest handshake = ws::ParseHandshake(request);

  // 2. Only the assistant's own pages...
  if (std::find(allowed_origins_.begin(), allowed_origins_.end(), handshake.origin) ==
      allowed_origins_.end()) {
    log::Write("refused a connection from origin '%s'", ws::Printable(handshake.origin, 80).c_str());
    Reply(socket, "403 Forbidden");
    return;
  }
  // 3. ...opened by a program of the same signed-in Windows user (shared terminal servers).
  const uint32_t peer_process = LoopbackPeerProcess(client_port_net, htons(port_));
  if (!IsInOurSession(peer_process)) {
    log::Write("refused a connection from another Windows session or an unknown program");
    Reply(socket, "403 Forbidden");
    return;
  }
  // 4. A well-formed WebSocket upgrade, within the page limit.
  if (!ws::IsValidUpgrade(handshake, kAudioPath)) {
    Reply(socket, "400 Bad Request");
    return;
  }
  if (state_.screens >= kMaxScreens) {
    Reply(socket, "503 Service Unavailable");
    return;
  }
  if (!SendAll(socket, ws::SwitchingProtocolsResponse(ws::ComputeAcceptKey(handshake.key)))) {
    closesocket(socket);
    return;
  }

  timeout = 0;  // reads now block (a page may stay quiet for hours)
  setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout),
             sizeof timeout);
  DWORD send_timeout = kSendTimeoutMs;  // a stuck page must not hold up the sound
  setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&send_timeout),
             sizeof send_timeout);
  int no_delay = 1;
  setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&no_delay),
             sizeof no_delay);

  auto client = std::make_shared<Client>();
  client->socket = socket;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    clients_.push_back(client);  // the newest page becomes the owner
    state_.screens = static_cast<int>(clients_.size());
    SendStatusLocked();
  }
  log::Write("Live Call page connected (%s, %ls), %d screen(s)", handshake.origin.c_str(),
             ProcessName(peer_process).c_str(), state_.screens.load());
  state_.NotifyStatusChanged();
  ReadLoop(client);
}

void LocalAudioServer::ReadLoop(std::shared_ptr<Client> client) {
  std::vector<uint8_t> buffer;
  char chunk[4096];
  bool open = true;
  while (open && client->alive) {
    int received = recv(client->socket, chunk, sizeof chunk, 0);
    if (received <= 0) break;
    buffer.insert(buffer.end(), chunk, chunk + received);
    while (open) {
      ws::ParseResult result = ws::ParseClientFrame(buffer, kMaxClientFrameBytes);
      if (result.status == ws::ParseStatus::kNeedMore) {
        if (buffer.size() > kMaxClientFrameBytes + 14) open = false;  // can't be one small frame
        break;
      }
      if (result.status == ws::ParseStatus::kInvalid) {
        log::Write("closed a connection that sent an invalid message");
        open = false;
        break;
      }
      buffer.erase(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(result.consumed));
      const ws::Frame& frame = result.frame;
      if (frame.opcode == ws::kClose) {
        open = false;
      } else if (frame.opcode == ws::kText) {
        OnText(client.get(), std::string(frame.payload.begin(), frame.payload.end()));
      } else if (frame.opcode == ws::kPing) {
        std::lock_guard<std::mutex> lock(mutex_);
        SendLocked(client.get(), ws::kPong, frame.payload.data(), frame.payload.size());
      }
    }
  }
  {
    std::lock_guard<std::mutex> lock(mutex_);
    CloseLocked(client.get());
    RemoveClosedLocked();
    SendStatusLocked();  // the next newest page becomes the owner
  }
  state_.NotifyStatusChanged();
}

void LocalAudioServer::OnText(Client* client, const std::string& text) {
  const CallMessage message = ParseCallMessage(text);
  if (!message.valid) return;
  const std::string caller = SanitizeCallerId(message.caller);
  std::lock_guard<std::mutex> lock(mutex_);
  client->in_call = message.active;
  client->caller_id = caller;
  log::Write("Live Call%s: %s %s", client == OwnerLocked() ? "" : " (not the one in use)",
             message.active ? "call on" : "call off", MaskCallerIdForLog(caller).c_str());
  UpdateCallLocked();
}

bool LocalAudioServer::SendAudio(const int16_t* samples, std::size_t count) {
  std::lock_guard<std::mutex> lock(mutex_);
  Client* owner = OwnerLocked();
  if (owner == nullptr || !owner->in_call) return false;
  if (SendLocked(owner, ws::kBinary, samples, count * sizeof(int16_t))) return true;
  RemoveClosedLocked();
  SendStatusLocked();
  return false;
}

void LocalAudioServer::BroadcastStatus() {
  std::lock_guard<std::mutex> lock(mutex_);
  SendStatusLocked();
}

void LocalAudioServer::DropAll() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& client : clients_) CloseLocked(client.get());
    clients_.clear();
    state_.screens = 0;
    UpdateCallLocked();
  }
  state_.NotifyStatusChanged();
}

LocalAudioServer::Client* LocalAudioServer::OwnerLocked() const {
  return clients_.empty() ? nullptr : clients_.back().get();
}

bool LocalAudioServer::SendLocked(Client* client, uint8_t opcode, const void* data,
                                  std::size_t length) {
  if (!client->alive) return false;
  if (SendAll(client->socket, ws::EncodeServerFrame(opcode, data, length))) return true;
  CloseLocked(client);
  return false;
}

void LocalAudioServer::CloseLocked(Client* client) {
  if (!client->alive.exchange(false)) return;
  shutdown(client->socket, SD_BOTH);
  closesocket(client->socket);  // also wakes its ReadLoop
}

void LocalAudioServer::RemoveClosedLocked() {
  clients_.erase(std::remove_if(clients_.begin(), clients_.end(),
                                [](const std::shared_ptr<Client>& c) { return !c->alive; }),
                 clients_.end());
  state_.screens = static_cast<int>(clients_.size());
}

void LocalAudioServer::SendStatusLocked() {
  const Client* owner = OwnerLocked();
  for (auto& client : clients_) {
    const std::string status = StatusJson(client.get() == owner);
    SendLocked(client.get(), ws::kText, status.data(), status.size());
  }
  RemoveClosedLocked();
  UpdateCallLocked();
}

void LocalAudioServer::UpdateCallLocked() {
  const Client* owner = OwnerLocked();
  const bool active = owner != nullptr && owner->in_call;
  const std::string caller = active ? owner->caller_id : std::string();
  state_.SetCallerId(std::wstring(caller.begin(), caller.end()));  // digits only: ASCII
  const bool was_active = state_.in_call.exchange(active);
  if (active != was_active) state_.NotifyCallChanged(active);
}

std::string LocalAudioServer::StatusJson(bool owner) const {
  char json[300];
  snprintf(json, sizeof json,
           "{\"type\":\"status\",\"version\":\"%s\",\"app_running\":%s,\"capturing\":%s,"
           "\"sample_rate\":%d,\"owner\":%s,\"screens\":%d}",
           kVersion, state_.app_process_id ? "true" : "false",
           state_.capturing ? "true" : "false", kTargetSampleRate, owner ? "true" : "false",
           state_.screens.load());
  return json;
}

}  // namespace sa3cx
