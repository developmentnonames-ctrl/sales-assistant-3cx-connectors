// What the tray shows, as plain data: the status lines, the hint, the tooltip. Pure: built from a
// snapshot of AppState, no Windows calls, so it can be unit-tested.
#ifndef SA3CX_STATUS_MODEL_H_
#define SA3CX_STATUS_MODEL_H_

#include <string>
#include <vector>

namespace sa3cx {

class AppState;

enum class Level { kOk, kWarn, kBad, kIdle };

struct StatusSnapshot {
  bool app_found = false;
  bool capturing = false;
  bool in_call = false;
  bool reconnecting = false;
  int screens = 0;
  std::wstring caller_id;
  std::wstring other_apps;
};

struct StatusLine {
  Level level = Level::kIdle;
  std::wstring name;
  std::wstring value;
};

StatusSnapshot TakeSnapshot(const AppState& state, bool reconnecting);

// 3CX app, Customer sound, Live Call screen, Same device, Call.
std::vector<StatusLine> BuildStatusLines(const StatusSnapshot& snapshot);

// The worst of the first three lines (the ones that must be OK for calls to work).
Level OverallLevel(const StatusSnapshot& snapshot);

std::wstring HintText(const StatusSnapshot& snapshot);
std::wstring TooltipText(const StatusSnapshot& snapshot);

}  // namespace sa3cx

#endif  // SA3CX_STATUS_MODEL_H_
