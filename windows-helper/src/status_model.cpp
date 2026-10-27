#include "status_model.h"

#include <algorithm>

#include "app_state.h"

namespace sa3cx {

StatusSnapshot TakeSnapshot(const AppState& state, bool reconnecting) {
  StatusSnapshot snapshot;
  snapshot.app_found = state.app_process_id != 0;
  snapshot.capturing = state.capturing;
  snapshot.in_call = state.in_call;
  snapshot.reconnecting = reconnecting;
  snapshot.screens = state.screens;
  snapshot.caller_id = state.CallerId();
  snapshot.other_apps = state.OtherApps();
  return snapshot;
}

std::vector<StatusLine> BuildStatusLines(const StatusSnapshot& s) {
  std::vector<StatusLine> lines;

  lines.push_back(s.app_found ? StatusLine{Level::kOk, L"3CX app", L"Found"}
                              : StatusLine{Level::kBad, L"3CX app", L"Not open"});

  if (s.capturing) {
    lines.push_back({Level::kOk, L"Customer sound",
                     s.in_call ? L"Sending to Live Call" : L"Ready (listening)"});
  } else if (s.app_found) {
    lines.push_back({Level::kWarn, L"Customer sound", L"Starting…"});
  } else {
    lines.push_back({Level::kBad, L"Customer sound", L"Waiting for 3CX"});
  }

  if (s.screens > 1) {
    lines.push_back({Level::kWarn, L"Live Call screen", L"2+ open: newest is used"});
  } else if (s.screens == 1) {
    lines.push_back({Level::kOk, L"Live Call screen", L"Connected"});
  } else {
    lines.push_back({Level::kBad, L"Live Call screen", L"Not connected"});
  }

  if (!s.capturing) {
    lines.push_back({Level::kIdle, L"Same device", L"-"});
  } else if (s.other_apps.empty()) {
    lines.push_back({Level::kOk, L"Same device", L"Only 3CX"});
  } else {
    lines.push_back({Level::kWarn, L"Same device", L"Also: " + s.other_apps});
  }

  if (!s.in_call) {
    lines.push_back({Level::kIdle, L"Call", L"No call"});
  } else {
    lines.push_back({Level::kOk, L"Call", s.caller_id.empty() ? L"In a call" : L"With " + s.caller_id});
  }
  return lines;
}

Level OverallLevel(const StatusSnapshot& snapshot) {
  const std::vector<StatusLine> lines = BuildStatusLines(snapshot);
  Level worst = Level::kOk;
  for (std::size_t i = 0; i < 3; ++i) worst = std::max(worst, lines[i].level);
  return worst;
}

std::wstring HintText(const StatusSnapshot& s) {
  if (s.reconnecting) return L"Reconnecting…";
  if (!s.app_found) return L"Open the 3CX app. The helper finds it by itself.";
  if (!s.capturing) return L"Starting to record the 3CX app…";
  if (s.screens == 0) {
    return L"Open Live Call in the browser (Call audio: 3CX calls), or reload it (F5).";
  }
  if (s.in_call) {
    return L"In a call: the customer's sound goes to Live Call. Hold Space there while you talk.";
  }
  if (!s.other_apps.empty()) {
    return L"Other apps play on 3CX's device. Best: 3CX Speaker = headset, Windows sounds = "
           L"laptop speakers.";
  }
  if (s.screens > 1) {
    return L"More than one Live Call screen is open: only the newest one gets the 3CX sound. "
           L"Close the others.";
  }
  return L"Ready. Answer calls in the 3CX app; hold Space in Live Call while you talk.";
}

std::wstring TooltipText(const StatusSnapshot& s) {
  std::wstring text = L"Sales Assistant 3CX helper\n";
  if (s.reconnecting) return text + L"Reconnecting…";
  if (s.in_call) return text + L"In a call: sending the customer's sound to Live Call";
  const std::vector<StatusLine> lines = BuildStatusLines(s);
  for (std::size_t i = 0; i < 3; ++i) {
    text += lines[i].level == Level::kOk ? L"✓ " : L"✗ ";
    text += lines[i].name;
    if (i < 2) text += L"  ";
  }
  return text;
}

}  // namespace sa3cx
