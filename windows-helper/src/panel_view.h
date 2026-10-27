// Draws the tray panel (light or dark, any DPI) and maps clicks to its rows. Stateless: everything
// it shows comes in a PanelModel.
#ifndef SA3CX_PANEL_VIEW_H_
#define SA3CX_PANEL_VIEW_H_

#include <string>
#include <vector>

#include "win32.h"

#include "status_model.h"

namespace sa3cx {

enum class PanelRow { kNone = -1, kReconnect = 0, kStartWithWindows = 1, kExit = 2 };

struct PanelModel {
  std::vector<StatusLine> lines;  // from BuildStatusLines
  std::wstring hint;
  bool reconnecting = false;
  int spinner_step = 0;  // 0..23, 15 degrees each
  PanelRow hover = PanelRow::kNone;
  bool start_with_windows = false;
  bool capturing = false;
  int level = 0;         // 0-1000, the 3CX sound right now
  bool sound_heard = false;  // 3CX sound in the last 3 s
};

namespace panel_view {

// The panel's size in pixels for the window's DPI.
SIZE Size(HWND window);

// Paints the whole panel (double-buffered). Call from WM_PAINT.
void Paint(HWND window, const PanelModel& model);

// The action row at a y position (client pixels).
PanelRow RowAt(HWND window, int y);

}  // namespace panel_view
}  // namespace sa3cx

#endif  // SA3CX_PANEL_VIEW_H_
