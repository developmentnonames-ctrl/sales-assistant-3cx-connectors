#include "status_model.h"

#include "test.h"

using sa3cx::Level;
using sa3cx::StatusSnapshot;

TEST(StatusModel, EverythingConnectedIsGreen) {
  StatusSnapshot s;
  s.app_found = true;
  s.capturing = true;
  s.screens = 1;
  EXPECT_TRUE(sa3cx::OverallLevel(s) == Level::kOk);
  const auto lines = sa3cx::BuildStatusLines(s);
  EXPECT_EQ(lines.size(), 5u);
  EXPECT_EQ(lines[2].value, std::wstring(L"Connected"));
  EXPECT_EQ(lines[3].value, std::wstring(L"Only 3CX"));
}

TEST(StatusModel, MissingAppOrPageIsRed) {
  StatusSnapshot s;
  EXPECT_TRUE(sa3cx::OverallLevel(s) == Level::kBad);
  s.app_found = true;
  s.capturing = true;
  EXPECT_TRUE(sa3cx::OverallLevel(s) == Level::kBad);  // no Live Call page yet
}

TEST(StatusModel, TwoScreensAndOtherAppsWarn) {
  StatusSnapshot s;
  s.app_found = true;
  s.capturing = true;
  s.screens = 2;
  s.other_apps = L"brave.exe";
  EXPECT_TRUE(sa3cx::OverallLevel(s) == Level::kWarn);
  const auto lines = sa3cx::BuildStatusLines(s);
  EXPECT_EQ(lines[3].value, std::wstring(L"Also: brave.exe"));
}

TEST(StatusModel, ShowsTheCallerDuringACall) {
  StatusSnapshot s;
  s.app_found = true;
  s.capturing = true;
  s.screens = 1;
  s.in_call = true;
  s.caller_id = L"1031";
  const auto lines = sa3cx::BuildStatusLines(s);
  EXPECT_EQ(lines[1].value, std::wstring(L"Sending to Live Call"));
  EXPECT_EQ(lines[4].value, std::wstring(L"With 1031"));
}
