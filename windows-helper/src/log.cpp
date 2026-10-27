#include "log.h"

#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <string>

#include "win32.h"

namespace sa3cx {
namespace log {
namespace {

constexpr unsigned long kMaxLogBytes = 1000000;

std::mutex g_mutex;
FILE* g_file = nullptr;
bool g_echo = false;

}  // namespace

void Open() {
  wchar_t base[MAX_PATH];
  DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", base, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) return;
  std::wstring dir = std::wstring(base) + L"\\SalesAssistant3CXHelper";
  CreateDirectoryW(dir.c_str(), nullptr);  // inherits the user-only permissions of the profile
  std::wstring path = dir + L"\\helper.log";
  WIN32_FILE_ATTRIBUTE_DATA attributes;
  if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &attributes) &&
      (attributes.nFileSizeHigh > 0 || attributes.nFileSizeLow > kMaxLogBytes)) {
    MoveFileExW(path.c_str(), (path + L".old").c_str(), MOVEFILE_REPLACE_EXISTING);
  }
  std::lock_guard<std::mutex> lock(g_mutex);
  g_file = _wfopen(path.c_str(), L"a");
}

void EchoToConsole(bool on) { g_echo = on; }

void Write(const char* format, ...) {
  char message[1024];
  va_list args;
  va_start(args, format);
  vsnprintf(message, sizeof message, format, args);
  va_end(args);

  SYSTEMTIME now;
  GetLocalTime(&now);
  char line[1100];
  snprintf(line, sizeof line, "%02d:%02d:%02d %s\n", now.wHour, now.wMinute, now.wSecond, message);

  std::lock_guard<std::mutex> lock(g_mutex);
  OutputDebugStringA(line);
  if (g_echo) {
    fputs(line, stdout);
    fflush(stdout);
  }
  if (g_file != nullptr) {
    fputs(line, g_file);
    fflush(g_file);
  }
}

}  // namespace log
}  // namespace sa3cx
