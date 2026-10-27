// RAII owners for Win32 handles, so every handle is closed on every path.
#ifndef SA3CX_HANDLE_H_
#define SA3CX_HANDLE_H_

#include "win32.h"

namespace sa3cx {

// Owns a HANDLE closed with CloseHandle. Treats both nullptr and INVALID_HANDLE_VALUE as empty.
class UniqueHandle {
 public:
  UniqueHandle() = default;
  explicit UniqueHandle(HANDLE handle) : handle_(handle) {}
  ~UniqueHandle() { Reset(); }
  UniqueHandle(const UniqueHandle&) = delete;
  UniqueHandle& operator=(const UniqueHandle&) = delete;
  UniqueHandle(UniqueHandle&& other) noexcept : handle_(other.Release()) {}
  UniqueHandle& operator=(UniqueHandle&& other) noexcept {
    if (this != &other) Reset(other.Release());
    return *this;
  }

  HANDLE get() const { return handle_; }
  bool valid() const { return handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE; }
  HANDLE Release() {
    HANDLE handle = handle_;
    handle_ = nullptr;
    return handle;
  }
  void Reset(HANDLE handle = nullptr) {
    if (valid()) CloseHandle(handle_);
    handle_ = handle;
  }

 private:
  HANDLE handle_ = nullptr;
};

// Owns a GDI object (font, brush, pen, bitmap, region) deleted with DeleteObject.
template <typename T>
class UniqueGdiObject {
 public:
  explicit UniqueGdiObject(T object) : object_(object) {}
  ~UniqueGdiObject() {
    if (object_ != nullptr) DeleteObject(object_);
  }
  UniqueGdiObject(const UniqueGdiObject&) = delete;
  UniqueGdiObject& operator=(const UniqueGdiObject&) = delete;
  T get() const { return object_; }

 private:
  T object_;
};

// Selects a GDI object into a device context and restores the previous one when it goes away.
class ScopedSelect {
 public:
  ScopedSelect(HDC dc, HGDIOBJ object) : dc_(dc), previous_(SelectObject(dc, object)) {}
  ~ScopedSelect() { SelectObject(dc_, previous_); }
  ScopedSelect(const ScopedSelect&) = delete;
  ScopedSelect& operator=(const ScopedSelect&) = delete;

 private:
  HDC dc_;
  HGDIOBJ previous_;
};

}  // namespace sa3cx

#endif  // SA3CX_HANDLE_H_
