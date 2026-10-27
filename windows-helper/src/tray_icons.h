// The tray icon in every state: a round icon in one colour with a white ring, drawn at the size
// the tray uses on this screen. Owns the icons and destroys them.
#ifndef SA3CX_TRAY_ICONS_H_
#define SA3CX_TRAY_ICONS_H_

#include "win32.h"

namespace sa3cx {

enum class TrayIcon {
  kGreen,     // all connected
  kAmber,     // starting
  kRed,       // something not connected
  kBlue,      // reconnecting (blinks with kBlueDim)
  kBlueDim,
  kLive,      // in a call (pulses with kLiveDim)
  kLiveDim,
  kCount,
};

class TrayIconSet {
 public:
  TrayIconSet();
  ~TrayIconSet();
  TrayIconSet(const TrayIconSet&) = delete;
  TrayIconSet& operator=(const TrayIconSet&) = delete;

  HICON Get(TrayIcon which) const { return icons_[static_cast<int>(which)]; }

 private:
  HICON icons_[static_cast<int>(TrayIcon::kCount)] = {};
};

}  // namespace sa3cx

#endif  // SA3CX_TRAY_ICONS_H_
