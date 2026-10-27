#include "tray_app.h"

#include <algorithm>
#include <cwchar>

#include <windowsx.h>

#include "autostart.h"
#include "log.h"
#include "status_model.h"

namespace sa3cx {

namespace {

constexpr wchar_t kMessageWindowClass[] = L"SalesAssistant3CXHelperWindow";
constexpr wchar_t kPanelClass[] = L"SalesAssistant3CXHelperPanel";
constexpr wchar_t kTitle[] = L"Sales Assistant 3CX helper";

constexpr UINT kTrayMessage = WM_APP + 1;
constexpr UINT kStatusMessage = WM_APP + 2;
constexpr UINT kShowPanelMessage = WM_APP + 3;
constexpr UINT kCallMessage = WM_APP + 4;

constexpr UINT_PTR kReconnectTimer = 1;  // spinner and blinking icon
constexpr UINT_PTR kLiveTimer = 2;       // pulsing icon during a call
constexpr UINT_PTR kMeterTimer = 3;      // sound bar while the panel is open

constexpr DWORD kReconnectMinMs = 1200;
constexpr DWORD kReconnectMaxMs = 10000;
constexpr DWORD kClickDebounceMs = 300;  // the click that just closed the panel doesn't reopen it

// Copies into a fixed WCHAR array, always terminated.
template <std::size_t N>
void CopyText(wchar_t (&target)[N], const std::wstring& text) {
  wcsncpy(target, text.c_str(), N - 1);
  target[N - 1] = L'\0';
}

}  // namespace

const wchar_t* TrayApp::MessageWindowClass() { return kMessageWindowClass; }
UINT TrayApp::ShowPanelMessage() { return kShowPanelMessage; }

TrayApp::TrayApp(HINSTANCE instance, AppState& state, LocalAudioServer& server)
    : instance_(instance), state_(state), server_(server) {}

TrayApp::~TrayApp() {
  if (tray_.hWnd != nullptr) Shell_NotifyIconW(NIM_DELETE, &tray_);
  if (panel_ != nullptr) DestroyWindow(panel_);
}

void TrayApp::PostStatusChanged() {
  if (HWND window = message_window_.load()) PostMessageW(window, kStatusMessage, 0, 0);
}

void TrayApp::PostCallChanged(bool call_started) {
  if (HWND window = message_window_.load()) PostMessageW(window, kCallMessage, call_started, 0);
}

int TrayApp::Run() {
  icons_ = std::make_unique<TrayIconSet>();
  CreateWindows();

  tray_.cbSize = sizeof tray_;
  tray_.hWnd = message_window_;
  tray_.uID = 1;
  tray_.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
  tray_.uCallbackMessage = kTrayMessage;
  tray_.hIcon = icons_->Get(TrayIcon::kAmber);
  CopyText(tray_.szTip, TooltipText(TakeSnapshot(state_, false)));
  Shell_NotifyIconW(NIM_ADD, &tray_);
  RefreshTray();

  MSG message;
  while (GetMessageW(&message, nullptr, 0, 0) > 0) {
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }
  return 0;
}

void TrayApp::CreateWindows() {
  WNDCLASSW message_class{};
  message_class.lpfnWndProc = MessageWindowProc;
  message_class.hInstance = instance_;
  message_class.lpszClassName = kMessageWindowClass;
  RegisterClassW(&message_class);
  // A hidden top-level window (not message-only, so a second launch can find it).
  message_window_ = CreateWindowExW(WS_EX_TOOLWINDOW, kMessageWindowClass, kTitle, WS_POPUP, 0, 0,
                                    0, 0, nullptr, nullptr, instance_, this);

  WNDCLASSW panel_class{};
  panel_class.lpfnWndProc = PanelWindowProc;
  panel_class.hInstance = instance_;
  panel_class.lpszClassName = kPanelClass;
  panel_class.style = CS_DROPSHADOW;
  panel_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  RegisterClassW(&panel_class);
  panel_ = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, kPanelClass, kTitle, WS_POPUP, 0, 0, 10,
                           10, nullptr, nullptr, instance_, this);

  // Rounded corners on Windows 11 (DWMWA_WINDOW_CORNER_PREFERENCE = DWMWCP_ROUND).
  // dwmapi.dll is loaded from System32 only.
  if (HMODULE dwm = LoadLibraryExW(L"dwmapi.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32)) {
    using SetAttribute = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
    if (auto set = reinterpret_cast<SetAttribute>(
            reinterpret_cast<void*>(GetProcAddress(dwm, "DwmSetWindowAttribute")))) {
      const DWORD round = 2;
      set(panel_, 33, &round, sizeof round);
    }
  }
}

LRESULT CALLBACK TrayApp::MessageWindowProc(HWND window, UINT message, WPARAM wparam,
                                            LPARAM lparam) {
  if (message == WM_NCCREATE) {
    auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
  }
  auto* app = reinterpret_cast<TrayApp*>(GetWindowLongPtrW(window, GWLP_USERDATA));
  return app != nullptr ? app->OnMessageWindow(window, message, wparam, lparam)
                        : DefWindowProcW(window, message, wparam, lparam);
}

LRESULT CALLBACK TrayApp::PanelWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
  if (message == WM_NCCREATE) {
    auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
  }
  auto* app = reinterpret_cast<TrayApp*>(GetWindowLongPtrW(window, GWLP_USERDATA));
  return app != nullptr ? app->OnPanel(window, message, wparam, lparam)
                        : DefWindowProcW(window, message, wparam, lparam);
}

LRESULT TrayApp::OnMessageWindow(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
  switch (message) {
    case kStatusMessage:
      RefreshTray();
      return 0;
    case kCallMessage:
      if (state_.in_call) {
        SetTimer(window, kLiveTimer, 300, nullptr);
      } else {
        KillTimer(window, kLiveTimer);
      }
      if (wparam != 0) ShowCallNotification();
      RefreshTray();
      return 0;
    case WM_TIMER:
      if (wparam == kLiveTimer) RefreshTray();
      return 0;
    case kShowPanelMessage:  // the .exe was run again
      ShowPanel();
      return 0;
    case kTrayMessage:
      if (LOWORD(lparam) == WM_LBUTTONUP || LOWORD(lparam) == WM_RBUTTONUP) {
        if (IsWindowVisible(panel_)) {
          HidePanel();
        } else if (GetTickCount() - panel_hidden_at_ > kClickDebounceMs) {
          ShowPanel();
        }
      }
      return 0;
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
    default:
      return DefWindowProcW(window, message, wparam, lparam);
  }
}

LRESULT TrayApp::OnPanel(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
  switch (message) {
    case WM_PAINT:
      panel_view::Paint(window, BuildPanelModel());
      return 0;
    case WM_ERASEBKGND:
      return 1;  // painted in full by WM_PAINT
    case WM_MOUSEMOVE: {
      const PanelRow row = panel_view::RowAt(window, GET_Y_LPARAM(lparam));
      if (row != hover_) {
        hover_ = row;
        InvalidateRect(window, nullptr, FALSE);
      }
      TRACKMOUSEEVENT track{sizeof track, TME_LEAVE, window, 0};
      TrackMouseEvent(&track);
      return 0;
    }
    case WM_MOUSELEAVE:
      hover_ = PanelRow::kNone;
      InvalidateRect(window, nullptr, FALSE);
      return 0;
    case WM_LBUTTONUP:
      switch (panel_view::RowAt(window, GET_Y_LPARAM(lparam))) {
        case PanelRow::kReconnect:
          if (!reconnecting_) StartReconnect();
          break;
        case PanelRow::kStartWithWindows:
          autostart::SetEnabled(!autostart::IsEnabled());
          InvalidateRect(window, nullptr, FALSE);
          break;
        case PanelRow::kExit:
          DestroyWindow(message_window_.load());
          break;
        case PanelRow::kNone:
          break;
      }
      return 0;
    case WM_TIMER:
      if (wparam == kReconnectTimer) AnimateReconnect();
      if (wparam == kMeterTimer) InvalidateRect(window, nullptr, FALSE);
      return 0;
    case WM_ACTIVATE:
      if (LOWORD(wparam) == WA_INACTIVE && IsWindowVisible(window)) HidePanel();  // clicked away
      return 0;
    case WM_KEYDOWN:
      if (wparam == VK_ESCAPE) HidePanel();
      return 0;
    case WM_DPICHANGED:
      if (IsWindowVisible(window)) ShowPanel();
      return 0;
    default:
      return DefWindowProcW(window, message, wparam, lparam);
  }
}

PanelModel TrayApp::BuildPanelModel() const {
  const StatusSnapshot snapshot = TakeSnapshot(state_, reconnecting_);
  PanelModel model;
  model.lines = BuildStatusLines(snapshot);
  model.hint = HintText(snapshot);
  model.reconnecting = reconnecting_;
  model.spinner_step = spinner_step_;
  model.hover = hover_;
  model.start_with_windows = autostart::IsEnabled();
  model.capturing = snapshot.capturing;
  const DWORD now = GetTickCount();
  model.level = now - state_.level_tick < 500 ? state_.level.load() : 0;
  model.sound_heard = state_.last_sound_tick != 0 && now - state_.last_sound_tick < 3000;
  return model;
}

void TrayApp::RefreshTray() {
  const StatusSnapshot snapshot = TakeSnapshot(state_, reconnecting_);
  const DWORD now = GetTickCount();
  TrayIcon icon;
  if (reconnecting_) {
    icon = (now / 400) % 2 ? TrayIcon::kBlue : TrayIcon::kBlueDim;
  } else if (snapshot.in_call) {
    icon = (now / 600) % 2 ? TrayIcon::kLive : TrayIcon::kLiveDim;
  } else {
    switch (OverallLevel(snapshot)) {
      case Level::kOk: icon = TrayIcon::kGreen; break;
      case Level::kWarn: icon = TrayIcon::kAmber; break;
      default: icon = TrayIcon::kRed; break;
    }
  }
  CopyText(tray_.szTip, TooltipText(snapshot));
  tray_.uFlags = NIF_TIP | NIF_ICON;
  tray_.hIcon = icons_->Get(icon);
  Shell_NotifyIconW(NIM_MODIFY, &tray_);
  if (panel_ != nullptr && IsWindowVisible(panel_)) InvalidateRect(panel_, nullptr, FALSE);
}

void TrayApp::ShowPanel() {
  const SIZE size = panel_view::Size(panel_);
  // Above the tray icon, inside the work area (the taskbar may be on any side).
  NOTIFYICONIDENTIFIER id{sizeof id, tray_.hWnd, tray_.uID, {}};
  RECT icon{};
  POINT anchor;
  if (Shell_NotifyIconGetRect(&id, &icon) == S_OK) {
    anchor = {(icon.left + icon.right) / 2, icon.top};
  } else {
    GetCursorPos(&anchor);
  }
  MONITORINFO monitor{};
  monitor.cbSize = sizeof monitor;
  GetMonitorInfoW(MonitorFromPoint(anchor, MONITOR_DEFAULTTONEAREST), &monitor);
  const RECT work = monitor.rcWork;
  const int x = std::max<int>(work.left + 8, std::min<int>(anchor.x - size.cx / 2,
                                                           work.right - size.cx - 8));
  int y;
  if (anchor.y < work.top + 50) {
    y = work.top + 8;  // taskbar at the top
  } else {
    y = std::min<int>(anchor.y, work.bottom) - size.cy - 8;
  }
  SetWindowPos(panel_, HWND_TOPMOST, x, y, size.cx, size.cy, SWP_SHOWWINDOW);
  SetForegroundWindow(panel_);
  SetTimer(panel_, kMeterTimer, 120, nullptr);
  hover_ = PanelRow::kNone;
  InvalidateRect(panel_, nullptr, FALSE);
}

void TrayApp::HidePanel() {
  ShowWindow(panel_, SW_HIDE);
  KillTimer(panel_, kMeterTimer);
  panel_hidden_at_ = GetTickCount();
}

void TrayApp::ShowCallNotification() {
  const std::wstring caller = state_.CallerId();
  NOTIFYICONDATAW notice = tray_;
  notice.uFlags = NIF_INFO;
  notice.dwInfoFlags = NIIF_INFO | NIIF_NOSOUND;
  CopyText(notice.szInfoTitle, L"Call answered");
  std::wstring text = L"Sending the customer's sound to Live Call";
  if (!caller.empty()) text += L" (caller " + caller + L")";
  CopyText(notice.szInfo, text);
  Shell_NotifyIconW(NIM_MODIFY, &notice);
}

void TrayApp::StartReconnect() {
  log::Write("reconnect pressed");
  reconnecting_ = true;
  reconnect_started_ = GetTickCount();
  state_.restart_capture = true;  // the recorder lets go of the 3CX app and finds it again
  server_.DropAll();              // Live Call pages reconnect by themselves within 2 s
  SetTimer(panel_, kReconnectTimer, 60, nullptr);
  RefreshTray();
}

// Every 60 ms while reconnecting: turn the spinner, blink the icon, stop once all is back.
void TrayApp::AnimateReconnect() {
  const DWORD elapsed = GetTickCount() - reconnect_started_;
  spinner_step_ = (spinner_step_ + 1) % 24;
  const bool back = state_.capturing && state_.screens > 0;
  const bool no_app = state_.app_process_id == 0 && elapsed > 2500;
  if ((back && elapsed > kReconnectMinMs) || elapsed > kReconnectMaxMs || no_app) {
    reconnecting_ = false;
    KillTimer(panel_, kReconnectTimer);
  }
  RefreshTray();
}

}  // namespace sa3cx
