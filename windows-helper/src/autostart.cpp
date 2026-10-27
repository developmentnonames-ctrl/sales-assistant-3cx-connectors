#include "autostart.h"

#include <string>

#include "win32.h"

namespace sa3cx {
namespace autostart {

namespace {
constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kValueName[] = L"SalesAssistant3CXHelper";
}  // namespace

bool IsEnabled() {
  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_READ, &key) != ERROR_SUCCESS) return false;
  const bool found = RegQueryValueExW(key, kValueName, nullptr, nullptr, nullptr, nullptr) ==
                     ERROR_SUCCESS;
  RegCloseKey(key);
  return found;
}

void SetEnabled(bool enabled) {
  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS) return;
  if (enabled) {
    wchar_t path[MAX_PATH];
    const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (length > 0 && length < MAX_PATH) {
      const std::wstring command = std::wstring(L"\"") + path + L"\"";  // quoted: spaces in path
      RegSetValueExW(key, kValueName, 0, REG_SZ, reinterpret_cast<const BYTE*>(command.c_str()),
                     static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    }
  } else {
    RegDeleteValueW(key, kValueName);
  }
  RegCloseKey(key);
}

}  // namespace autostart
}  // namespace sa3cx
