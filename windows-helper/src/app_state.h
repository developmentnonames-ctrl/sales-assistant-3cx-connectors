// State shared between the capture thread, the server's threads and the tray (UI) thread.
// Flags and counters are atomics; strings are behind a mutex. The tray learns about changes
// through two callbacks, set once before any thread starts.
#ifndef SA3CX_APP_STATE_H_
#define SA3CX_APP_STATE_H_

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>

namespace sa3cx {

class AppState {
 public:
  AppState() = default;
  AppState(const AppState&) = delete;
  AppState& operator=(const AppState&) = delete;

  // Set by the program.
  std::atomic<bool> running{true};
  std::atomic<bool> restart_capture{false};  // Reconnect: let go of the 3CX app, find it again

  // Set by the capture thread.
  std::atomic<uint32_t> app_process_id{0};  // the 3CX app; 0 if it is not running
  std::atomic<bool> capturing{false};
  std::atomic<int> level{0};                 // loudness of the last 100 ms, 0-1000
  std::atomic<uint32_t> level_tick{0};       // GetTickCount() when `level` was set
  std::atomic<uint32_t> last_sound_tick{0};  // the last time 3CX sound was heard

  // Set by the server.
  std::atomic<int> screens{0};       // Live Call pages connected
  std::atomic<bool> in_call{false};  // the page in use says a call is on

  std::wstring CallerId() const;
  void SetCallerId(const std::wstring& caller_id);
  std::wstring OtherApps() const;  // other apps playing on the 3CX device; "" = only 3CX
  void SetOtherApps(const std::wstring& names);

  // Change notifications; may be called from any thread.
  std::function<void()> on_status_changed;
  std::function<void(bool call_started)> on_call_changed;
  void NotifyStatusChanged() const;
  void NotifyCallChanged(bool call_started) const;

 private:
  mutable std::mutex mutex_;
  std::wstring caller_id_;
  std::wstring other_apps_;
};

}  // namespace sa3cx

#endif  // SA3CX_APP_STATE_H_
