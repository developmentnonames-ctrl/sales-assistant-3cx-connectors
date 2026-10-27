#include "tray_icons.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace sa3cx {

namespace {

// `dot`: radius of a white centre dot as a share of the size (0 = none). Anti-aliased edge.
HICON MakeRoundIcon(COLORREF color, int max_alpha, double dot) {
  const int size = GetSystemMetrics(SM_CXSMICON);
  BITMAPV5HEADER header{};
  header.bV5Size = sizeof header;
  header.bV5Width = size;
  header.bV5Height = -size;  // top-down
  header.bV5Planes = 1;
  header.bV5BitCount = 32;
  header.bV5Compression = BI_BITFIELDS;
  header.bV5RedMask = 0x00FF0000;
  header.bV5GreenMask = 0x0000FF00;
  header.bV5BlueMask = 0x000000FF;
  header.bV5AlphaMask = 0xFF000000;

  void* bits = nullptr;
  HDC screen = GetDC(nullptr);
  HBITMAP color_bitmap = CreateDIBSection(screen, reinterpret_cast<BITMAPINFO*>(&header),
                                          DIB_RGB_COLORS, &bits, nullptr, 0);
  ReleaseDC(nullptr, screen);
  if (color_bitmap == nullptr) return nullptr;

  auto* pixels = static_cast<uint32_t*>(bits);
  const double center = (size - 1) / 2.0;
  const double outer = size / 2.0 - 0.5;
  const double ring = outer - std::max(1.0, size / 10.0);
  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      const double distance = std::sqrt((x - center) * (x - center) + (y - center) * (y - center));
      const double coverage = std::clamp(outer - distance + 0.5, 0.0, 1.0);
      const bool colored = distance < ring && distance >= dot * size;
      const int r = colored ? GetRValue(color) : 255;
      const int g = colored ? GetGValue(color) : 255;
      const int b = colored ? GetBValue(color) : 255;
      const int alpha = static_cast<int>(coverage * max_alpha);
      // Premultiplied alpha.
      pixels[y * size + x] = (static_cast<uint32_t>(alpha) << 24) | ((r * alpha / 255) << 16) |
                             ((g * alpha / 255) << 8) | (b * alpha / 255);
    }
  }
  HBITMAP mask = CreateBitmap(size, size, 1, 1, nullptr);
  ICONINFO info{TRUE, 0, 0, mask, color_bitmap};
  HICON icon = CreateIconIndirect(&info);
  DeleteObject(mask);
  DeleteObject(color_bitmap);
  return icon;
}

}  // namespace

TrayIconSet::TrayIconSet() {
  const COLORREF green = RGB(26, 160, 100);
  const COLORREF blue = RGB(40, 120, 230);
  icons_[static_cast<int>(TrayIcon::kGreen)] = MakeRoundIcon(green, 255, 0);
  icons_[static_cast<int>(TrayIcon::kAmber)] = MakeRoundIcon(RGB(226, 150, 30), 255, 0);
  icons_[static_cast<int>(TrayIcon::kRed)] = MakeRoundIcon(RGB(214, 58, 46), 255, 0);
  icons_[static_cast<int>(TrayIcon::kBlue)] = MakeRoundIcon(blue, 255, 0);
  icons_[static_cast<int>(TrayIcon::kBlueDim)] = MakeRoundIcon(blue, 90, 0);
  icons_[static_cast<int>(TrayIcon::kLive)] = MakeRoundIcon(green, 255, 0.17);
  icons_[static_cast<int>(TrayIcon::kLiveDim)] = MakeRoundIcon(green, 150, 0.17);
}

TrayIconSet::~TrayIconSet() {
  for (HICON icon : icons_) {
    if (icon != nullptr) DestroyIcon(icon);
  }
}

}  // namespace sa3cx
