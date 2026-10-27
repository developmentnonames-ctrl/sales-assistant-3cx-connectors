// The user interface: the tray icon and the panel above it. Runs the Windows message loop on the
// main thread. Other threads reach it only through AppState's callbacks (posted messages).
#ifndef SA3CX_TRAY_APP_H_
#define SA3CX_TRAY_APP_H_

#include <atomic>
#include <memory>

#include "win32.h"

#include <shellapi.h>

#include "app_state.h"
#include "local_audio_server.h"
#include "panel_view.h"
#include "tray_icons.h"

namespace sa3cx {

class TrayApp {
 public:
  TrayApp(HINSTANCE instance, AppState& state, LocalAudioServer& server);
  ~TrayApp();
  TrayApp(const TrayApp&) = delete;
  TrayApp& operator=(const TrayApp&) = delete;

  // Creates the windows and the tray icon, then runs until Exit. Returns the exit code.
  int Run();

  // From any thread: ask the UI to refresh / note a call change.
  void PostStatusChanged();
  void PostCallChanged(bool call_started);

  // Window class of the hidden message window; a second launch finds it to open the panel.
  static const wchar_t* MessageWindowClass();
  static UINT ShowPanelMessage();

 private:
  static LRESULT CALLBACK MessageWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
  static LRESULT CALLBACK PanelWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
  LRESULT OnMessageWindow(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
  LRESULT OnPanel(HWND window, UINT message, WPARAM wparam, LPARAM lparam);

  void CreateWindows();
  void RefreshTray();
  void ShowPanel();
  void HidePanel();
  void ShowCallNotification();
  void StartReconnect();
  void AnimateReconnect();
  PanelModel BuildPanelModel() const;

  HINSTANCE instance_;
  AppState& state_;
  LocalAudioServer& server_;
  std::unique_ptr<TrayIconSet> icons_;
  std::atomic<HWND> message_window_{nullptr};  // read by other threads (Post*)
  HWND panel_ = nullptr;
  NOTIFYICONDATAW tray_{};

  bool reconnecting_ = false;
  DWORD reconnect_started_ = 0;
  int spinner_step_ = 0;
  PanelRow hover_ = PanelRow::kNone;
  DWORD panel_hidden_at_ = 0;
};

}  // namespace sa3cx

#endif  // SA3CX_TRAY_APP_H_
