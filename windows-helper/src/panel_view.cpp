#include "panel_view.h"

#include <algorithm>
#include <cmath>

#include "win32.h"

#include <objidl.h>  // GDI+ needs the COM types WIN32_LEAN_AND_MEAN leaves out
#include <gdiplus.h>

#include "handle.h"
#include "version.h"

namespace sa3cx {
namespace panel_view {

namespace {

// Layout in 96-dpi units.
constexpr int kWidth = 340;
constexpr int kTop = 44;        // below the title
constexpr int kLineHeight = 30;
constexpr int kStatusLines = 5;  // + the sound bar line
constexpr int kHintHeight = 42;
constexpr int kActionHeight = 36;
constexpr int kActionRows = 3;
constexpr int kValueColumn = 168;

int ActionsTop() { return kTop + (kStatusLines + 1) * kLineHeight + kHintHeight + 9; }
int Height() { return ActionsTop() + kActionRows * kActionHeight + 8; }

int Scale(HWND window, int value) {
  return MulDiv(value, static_cast<int>(GetDpiForWindow(window)), 96);
}

struct Palette {
  COLORREF background, text, muted, rule, hover, good, warn, bad, accent;
};

bool SystemUsesDarkTheme() {
  DWORD light = 1;
  DWORD size = sizeof light;
  RegGetValueW(HKEY_CURRENT_USER,
               L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
               L"SystemUsesLightTheme", RRF_RT_REG_DWORD, nullptr, &light, &size);
  return light == 0;
}

Palette CurrentPalette() {
  if (SystemUsesDarkTheme()) {
    return {RGB(32, 32, 32),    RGB(240, 240, 240), RGB(160, 160, 160),
            RGB(60, 60, 60),    RGB(55, 55, 55),    RGB(76, 200, 130),
            RGB(240, 180, 60),  RGB(240, 100, 95),  RGB(96, 165, 250)};
  }
  return {RGB(249, 249, 249), RGB(26, 26, 26),  RGB(96, 96, 96),
          RGB(224, 224, 224), RGB(234, 234, 234), RGB(16, 124, 65),
          RGB(178, 106, 0),   RGB(196, 43, 28),   RGB(0, 95, 184)};
}

COLORREF LevelColor(const Palette& palette, Level level) {
  switch (level) {
    case Level::kOk: return palette.good;
    case Level::kWarn: return palette.warn;
    case Level::kBad: return palette.bad;
    case Level::kIdle: return palette.muted;
  }
  return palette.muted;
}

Gdiplus::Color ToGdiplus(COLORREF color, BYTE alpha = 255) {
  return Gdiplus::Color(alpha, GetRValue(color), GetGValue(color), GetBValue(color));
}

// A filled circle with a white tick (OK), cross (bad) or dash (warning); `plain`: no mark.
void DrawStatusMark(HDC dc, float cx, float cy, float r, COLORREF color, Level level, bool plain) {
  Gdiplus::Graphics g(dc);
  g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
  g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
  Gdiplus::SolidBrush fill(ToGdiplus(color));
  g.FillEllipse(&fill, cx - r, cy - r, 2 * r, 2 * r);
  if (plain) return;
  Gdiplus::Pen pen(Gdiplus::Color(255, 255, 255, 255), r * 0.26f);
  pen.SetStartCap(Gdiplus::LineCapRound);
  pen.SetEndCap(Gdiplus::LineCapRound);
  pen.SetLineJoin(Gdiplus::LineJoinRound);
  if (level == Level::kOk) {
    Gdiplus::PointF tick[3] = {{cx - r * 0.42f, cy + r * 0.02f},
                               {cx - r * 0.12f, cy + r * 0.32f},
                               {cx + r * 0.44f, cy - r * 0.30f}};
    g.DrawLines(&pen, tick, 3);
  } else if (level == Level::kBad) {
    const float d = r * 0.34f;
    g.DrawLine(&pen, cx - d, cy - d, cx + d, cy + d);
    g.DrawLine(&pen, cx - d, cy + d, cx + d, cy - d);
  } else {
    g.DrawLine(&pen, cx - r * 0.38f, cy, cx + r * 0.38f, cy);
  }
}

// A turning arc, for "Reconnecting…".
void DrawSpinner(HDC dc, float cx, float cy, float r, COLORREF color, int step, float width) {
  Gdiplus::Graphics g(dc);
  g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
  g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
  Gdiplus::Pen track(ToGdiplus(color, 60), width);
  g.DrawEllipse(&track, cx - r, cy - r, 2 * r, 2 * r);
  Gdiplus::Pen arc(ToGdiplus(color), width);
  arc.SetStartCap(Gdiplus::LineCapRound);
  arc.SetEndCap(Gdiplus::LineCapRound);
  g.DrawArc(&arc, cx - r, cy - r, 2 * r, 2 * r, step * 15.0f, 100.0f);
}

void DrawBar(HDC dc, int x, int cy, int width, int height, double fraction, COLORREF track,
             COLORREF fill) {
  Gdiplus::Graphics g(dc);
  g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
  Gdiplus::SolidBrush track_brush(ToGdiplus(track));
  const auto top = static_cast<Gdiplus::REAL>(cy - height / 2);
  g.FillRectangle(&track_brush, static_cast<Gdiplus::REAL>(x), top,
                  static_cast<Gdiplus::REAL>(width), static_cast<Gdiplus::REAL>(height));
  if (fraction <= 0.01) return;
  Gdiplus::SolidBrush fill_brush(ToGdiplus(fill));
  g.FillRectangle(&fill_brush, static_cast<Gdiplus::REAL>(x), top,
                  static_cast<Gdiplus::REAL>(width * fraction), static_cast<Gdiplus::REAL>(height));
}

HFONT MakeFont(HWND window, int size, int weight, const wchar_t* face) {
  return CreateFontW(-Scale(window, size), 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                     CLEARTYPE_QUALITY, 0, face);
}

void Text(HDC dc, const std::wstring& text, RECT rect, UINT format, COLORREF color) {
  SetTextColor(dc, color);
  DrawTextW(dc, text.c_str(), -1, &rect, format);
}

constexpr wchar_t kGlyphRefresh[] = {0xE72C, 0};  // Segoe MDL2 Assets
constexpr wchar_t kGlyphPower[] = {0xE7E8, 0};

}  // namespace

SIZE Size(HWND window) { return SIZE{Scale(window, kWidth), Scale(window, Height())}; }

PanelRow RowAt(HWND window, int y) {
  const int top = Scale(window, ActionsTop());
  if (y < top) return PanelRow::kNone;
  const int row = (y - top) / Scale(window, kActionHeight);
  return row >= 0 && row < kActionRows ? static_cast<PanelRow>(row) : PanelRow::kNone;
}

void Paint(HWND window, const PanelModel& model) {
  auto px = [window](int value) { return Scale(window, value); };
  PAINTSTRUCT paint;
  HDC window_dc = BeginPaint(window, &paint);
  RECT client;
  GetClientRect(window, &client);

  // Draw off-screen, then copy: no flicker.
  HDC dc = CreateCompatibleDC(window_dc);
  UniqueGdiObject<HBITMAP> buffer(CreateCompatibleBitmap(window_dc, client.right, client.bottom));
  {
    ScopedSelect select_buffer(dc, buffer.get());
    const Palette palette = CurrentPalette();
    UniqueGdiObject<HBRUSH> background(CreateSolidBrush(palette.background));
    FillRect(dc, &client, background.get());
    SetBkMode(dc, TRANSPARENT);

    UniqueGdiObject<HFONT> normal(MakeFont(window, 14, FW_NORMAL, L"Segoe UI"));
    UniqueGdiObject<HFONT> bold(MakeFont(window, 14, FW_SEMIBOLD, L"Segoe UI"));
    UniqueGdiObject<HFONT> title(MakeFont(window, 15, FW_SEMIBOLD, L"Segoe UI"));
    UniqueGdiObject<HFONT> small(MakeFont(window, 12, FW_NORMAL, L"Segoe UI"));
    UniqueGdiObject<HFONT> icons(MakeFont(window, 14, FW_NORMAL, L"Segoe MDL2 Assets"));
    ScopedSelect select_font(dc, title.get());
    const int left = px(16);
    const int right = client.right - px(16);
    const UINT single = DT_LEFT | DT_VCENTER | DT_SINGLELINE;

    Text(dc, L"Sales Assistant · 3CX helper", RECT{left, px(10), right, px(34)}, single,
         palette.text);

    // Status lines: mark, name, value. While reconnecting all are blue "Checking…".
    for (std::size_t i = 0; i < model.lines.size(); ++i) {
      const StatusLine& line = model.lines[i];
      const int y = px(kTop + static_cast<int>(i) * kLineHeight);
      const int cy = y + px(kLineHeight) / 2;
      const COLORREF color = model.reconnecting ? palette.accent : LevelColor(palette, line.level);
      DrawStatusMark(dc, static_cast<float>(left + px(8)), static_cast<float>(cy),
                     static_cast<float>(px(8)), color, line.level,
                     model.reconnecting || line.level == Level::kIdle);
      SelectObject(dc, bold.get());
      Text(dc, line.name, RECT{left + px(26), y, left + px(kValueColumn), y + px(kLineHeight)},
           single, palette.text);
      SelectObject(dc, normal.get());
      Text(dc, model.reconnecting ? L"Checking…" : line.value,
           RECT{left + px(kValueColumn), y, right, y + px(kLineHeight)}, single | DT_END_ELLIPSIS,
           color);
    }

    // The 3CX sound right now: a bar that moves with what the helper hears.
    {
      const int y = px(kTop + kStatusLines * kLineHeight);
      const int cy = y + px(kLineHeight) / 2;
      const COLORREF color = model.sound_heard ? palette.good : palette.muted;
      DrawStatusMark(dc, static_cast<float>(left + px(8)), static_cast<float>(cy),
                     static_cast<float>(px(8)), color, Level::kIdle, true);
      SelectObject(dc, bold.get());
      Text(dc, L"3CX sound", RECT{left + px(26), y, left + px(kValueColumn), y + px(kLineHeight)},
           single, palette.text);
      const int bar_x = left + px(kValueColumn);
      const int bar_width = right - bar_x - px(58);
      const double fraction = std::min(1.0, std::sqrt(model.level / 300.0));  // speech: 30-300
      DrawBar(dc, bar_x, cy, bar_width, px(8), fraction, palette.rule, palette.good);
      SelectObject(dc, normal.get());
      Text(dc, !model.capturing ? L"off" : (model.sound_heard ? L"sound" : L"silent"),
           RECT{bar_x + bar_width + px(8), y, right, y + px(kLineHeight)}, single, color);
    }

    // Hint.
    SelectObject(dc, small.get());
    const int hint_y = px(kTop + (kStatusLines + 1) * kLineHeight + 4);
    Text(dc, model.hint, RECT{left, hint_y, right, hint_y + px(kHintHeight - 6)},
         DT_LEFT | DT_WORDBREAK, palette.muted);

    // Rule.
    const int rule_y = px(ActionsTop() - 5);
    UniqueGdiObject<HBRUSH> rule(CreateSolidBrush(palette.rule));
    RECT rule_rect{0, rule_y, client.right, rule_y + std::max(1, px(1))};
    FillRect(dc, &rule_rect, rule.get());

    // Actions.
    const wchar_t* labels[kActionRows] = {
        model.reconnecting ? L"Reconnecting…" : L"Reconnect", L"Start with Windows", L"Exit"};
    for (int i = 0; i < kActionRows; ++i) {
      const auto row = static_cast<PanelRow>(i);
      const int y = px(ActionsTop() + i * kActionHeight);
      const int cy = y + px(kActionHeight) / 2;
      const int icon_x = left + px(7);
      const bool busy = row == PanelRow::kReconnect && model.reconnecting;
      if (model.hover == row && !busy) {
        UniqueGdiObject<HBRUSH> hover(CreateSolidBrush(palette.hover));
        UniqueGdiObject<HRGN> shape(CreateRoundRectRgn(px(6), y + px(2), client.right - px(6),
                                                       y + px(kActionHeight) - px(2), px(8), px(8)));
        FillRgn(dc, shape.get(), hover.get());
      }
      const RECT icon_rect{icon_x - px(9), cy - px(9), icon_x + px(10), cy + px(10)};
      if (row == PanelRow::kReconnect) {
        if (busy) {
          DrawSpinner(dc, icon_x + 0.5f, static_cast<float>(cy), static_cast<float>(px(7)),
                      palette.accent, model.spinner_step, static_cast<float>(px(2)));
        } else {
          SelectObject(dc, icons.get());
          Text(dc, kGlyphRefresh, icon_rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE, palette.text);
        }
      } else if (row == PanelRow::kStartWithWindows) {
        const bool on = model.start_with_windows;
        UniqueGdiObject<HBRUSH> box_fill(CreateSolidBrush(on ? palette.accent : palette.background));
        UniqueGdiObject<HPEN> box_line(
            CreatePen(PS_SOLID, std::max(1, px(1)), on ? palette.accent : palette.muted));
        RECT box{icon_x - px(7), cy - px(7), icon_x + px(7), cy + px(7)};
        {
          ScopedSelect select_fill(dc, box_fill.get());
          ScopedSelect select_line(dc, box_line.get());
          RoundRect(dc, box.left, box.top, box.right, box.bottom, px(4), px(4));
        }
        if (on) {
          SelectObject(dc, bold.get());
          Text(dc, L"✓", box, DT_CENTER | DT_VCENTER | DT_SINGLELINE, RGB(255, 255, 255));
        }
      } else {
        SelectObject(dc, icons.get());
        Text(dc, kGlyphPower, icon_rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE, palette.text);
      }
      SelectObject(dc, normal.get());
      Text(dc, labels[i], RECT{left + px(26), y, right, y + px(kActionHeight)}, single,
           busy ? palette.accent : palette.text);
    }

    // Version, bottom right.
    SelectObject(dc, small.get());
    wchar_t version[32];
    swprintf(version, 32, L"v%hs", kVersion);
    const int last_row = ActionsTop() + (kActionRows - 1) * kActionHeight;
    Text(dc, version, RECT{left, px(last_row), right, px(last_row + kActionHeight)},
         DT_RIGHT | DT_VCENTER | DT_SINGLELINE, palette.muted);

    BitBlt(window_dc, 0, 0, client.right, client.bottom, dc, 0, 0, SRCCOPY);
  }  // fonts and the buffer are deselected before they are deleted
  DeleteDC(dc);
  EndPaint(window, &paint);
}

}  // namespace panel_view
}  // namespace sa3cx
