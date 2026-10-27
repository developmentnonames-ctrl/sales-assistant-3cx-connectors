#include "app_state.h"

namespace sa3cx {

std::wstring AppState::CallerId() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return caller_id_;
}

void AppState::SetCallerId(const std::wstring& caller_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  caller_id_ = caller_id;
}

std::wstring AppState::OtherApps() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return other_apps_;
}

void AppState::SetOtherApps(const std::wstring& names) {
  std::lock_guard<std::mutex> lock(mutex_);
  other_apps_ = names;
}

void AppState::NotifyStatusChanged() const {
  if (on_status_changed) on_status_changed();
}

void AppState::NotifyCallChanged(bool call_started) const {
  if (on_call_changed) on_call_changed(call_started);
}

}  // namespace sa3cx
