// Sales Assistant 3CX helper: entry point. Wires the parts together:
//
//   AudioCapture (own thread) --frames--> FrameRouter --> LocalAudioServer --> Live Call page
//                                                    \--> AppState <-- TrayApp (UI thread)
//
// See README.md for what it does and SECURITY.md for its security model.

#include <string>
#include <thread>

#include "win32.h"

#include <objidl.h>  // GDI+ needs the COM types WIN32_LEAN_AND_MEAN leaves out
#include <gdiplus.h>
#include <shellapi.h>

#include "app_state.h"
#include "audio_capture.h"
#include "config.h"
#include "frame_router.h"
#include "local_audio_server.h"
#include "log.h"
#include "tray_app.h"
#include "version.h"
#include "wav_writer.h"

namespace sa3cx {
namespace {

struct Options {
  bool console = false;
#ifdef SA3CX_DIAGNOSTICS
  unsigned short port = kDefaultPort;
  std::wstring process_name = kDefaultProcessName;
  uint32_t process_id = 0;
  std::vector<std::string> extra_origins;
  std::wstring record_path;
  int record_seconds = 10;
#endif
};

#ifdef SA3CX_DIAGNOSTICS
std::string ToUtf8(const std::wstring& text) {
  const int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
  if (size <= 1) return "";
  std::string result(size - 1, '\0');
  WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, result.data(), size, nullptr, nullptr);
  return result;
}
#endif

// The program agents get accepts only --console (and --hidden, from older Start-with-Windows
// entries; it changes nothing). Test options exist only in the diagnostics build.
Options ParseOptions() {
  Options options;
  int argc = 0;
  LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  if (argv == nullptr) return options;
  for (int i = 1; i < argc; ++i) {
    const std::wstring arg = argv[i];
    if (arg == L"--console") options.console = true;
#ifdef SA3CX_DIAGNOSTICS
    auto next = [&]() { return i + 1 < argc ? std::wstring(argv[++i]) : std::wstring(); };
    if (arg == L"--port") {
      const int port = _wtoi(next().c_str());
      if (port > 1024 && port < 65536) options.port = static_cast<unsigned short>(port);
    } else if (arg == L"--process") {
      options.process_name = next();
    } else if (arg == L"--pid") {
      options.process_id = static_cast<uint32_t>(_wtoi(next().c_str()));
    } else if (arg == L"--allow-origin") {
      options.extra_origins.push_back(ToUtf8(next()));
    } else if (arg == L"--record") {
      options.record_path = next();
    } else if (arg == L"--seconds") {
      options.record_seconds = _wtoi(next().c_str());
    }
#endif
  }
  LocalFree(argv);
  return options;
}

void AttachConsoleIfAsked(const Options& options) {
  if (!options.console) return;
  if (!AttachConsole(ATTACH_PARENT_PROCESS)) AllocConsole();
  FILE* ignored = nullptr;
  freopen_s(&ignored, "CONOUT$", "w", stdout);
  log::EchoToConsole(true);
}

int Main(HINSTANCE instance) {
  // Any DLL loaded at run time comes from System32 only, never from the .exe's folder.
  SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32);
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

  const Options options = ParseOptions();
  AttachConsoleIfAsked(options);

  bool recording = false;
#ifdef SA3CX_DIAGNOSTICS
  recording = !options.record_path.empty();
#endif

  // One helper per signed-in user: running it again just opens the panel.
  HANDLE single_instance = CreateMutexW(nullptr, TRUE, L"Local\\SalesAssistant3CXHelper");
  if (GetLastError() == ERROR_ALREADY_EXISTS && !recording) {
    if (HWND other = FindWindowW(TrayApp::MessageWindowClass(), nullptr)) {
      PostMessageW(other, TrayApp::ShowPanelMessage(), 0, 0);
    }
    return 0;
  }

  log::Open();
  log::Write("Sales Assistant 3CX helper %s started", kVersion);

  AppState state;
  unsigned short port = kDefaultPort;
  std::vector<std::string> origins = DefaultAllowedOrigins();
  CaptureOptions capture_options{kDefaultProcessName, 0};
  std::unique_ptr<WavWriter> wav;
#ifdef SA3CX_DIAGNOSTICS
  port = options.port;
  origins.insert(origins.end(), options.extra_origins.begin(), options.extra_origins.end());
  capture_options = CaptureOptions{options.process_name, options.process_id};
  if (recording) wav = std::make_unique<WavWriter>(options.record_path);
#endif

  LocalAudioServer server(state, port, origins);
  if (!server.Start() && !recording) {
    MessageBoxW(nullptr, L"The Sales Assistant 3CX helper is already running (or its port is in use).",
                L"Sales Assistant 3CX helper", MB_ICONINFORMATION);
    return 1;
  }

  FrameRouter router(state, server, wav.get());
  AudioCapture capture(
      state, capture_options, [&router](const std::vector<int16_t>& frame) { router.OnFrame(frame); },
      [&state, &server] {
        server.BroadcastStatus();
        state.NotifyStatusChanged();
      });

  Gdiplus::GdiplusStartupInput gdiplus_input;
  ULONG_PTR gdiplus_token = 0;
  Gdiplus::GdiplusStartup(&gdiplus_token, &gdiplus_input, nullptr);

  int exit_code = 0;
  {
    TrayApp tray(instance, state, server);
    state.on_status_changed = [&tray] { tray.PostStatusChanged(); };
    state.on_call_changed = [&tray](bool started) { tray.PostCallChanged(started); };

    std::thread capture_thread([&capture] { capture.Run(); });
    if (recording) {
#ifdef SA3CX_DIAGNOSTICS
      Sleep(static_cast<DWORD>(options.record_seconds > 0 ? options.record_seconds : 10) * 1000);
#endif
    } else {
      exit_code = tray.Run();
    }
    state.running = false;
    server.Stop();
    capture_thread.join();
    state.on_status_changed = nullptr;
    state.on_call_changed = nullptr;
  }
  Gdiplus::GdiplusShutdown(gdiplus_token);
  if (wav) log::Write("saved %u bytes of audio", wav->bytes_written());
  CloseHandle(single_instance);
  return exit_code;
}

}  // namespace
}  // namespace sa3cx

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) { return sa3cx::Main(instance); }
