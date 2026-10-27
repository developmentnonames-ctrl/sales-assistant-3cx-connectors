cmake_minimum_required(VERSION 3.20)
project(SalesAssistant3CXHelper VERSION 2.0.0 LANGUAGES CXX RC)

# Windows only: it records Windows audio and lives in the Windows tray.
if(NOT WIN32)
  message(FATAL_ERROR "The 3CX helper is a Windows program.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# ---- hardening and warnings ------------------------------------------------------------------
if(MSVC)
  set(SA3CX_COMPILE_OPTIONS /W4 /permissive- /sdl /guard:cf /GS /utf-8)
  set(SA3CX_LINK_OPTIONS /DYNAMICBASE /HIGHENTROPYVA /NXCOMPAT /GUARD:CF)
else()  # MinGW-w64 GCC / Clang
  set(SA3CX_COMPILE_OPTIONS -Wall -Wextra -Wpedantic -fstack-protector-strong -D_FORTIFY_SOURCE=2)
  set(SA3CX_LINK_OPTIONS -static -Wl,--dynamicbase,--nxcompat,--high-entropy-va)
  set(SA3CX_WINMAIN_OPTIONS -municode)  # wWinMain (UTF-16 entry point) for the two programs
  set(SA3CX_EXTRA_LIBS ssp)
endif()

set(SA3CX_SYSTEM_LIBS
  ole32 ws2_32 iphlpapi bcrypt crypt32 shell32 user32 gdi32 gdiplus uuid ksuser ${SA3CX_EXTRA_LIBS})

# Development builds also accept the assistant running on this PC (http://localhost:3004).
option(SA3CX_DEV_ORIGINS "Allow the local development assistant (never for release)" OFF)
if(SA3CX_DEV_ORIGINS)
  add_compile_definitions(SA3CX_DEV_ORIGINS)
endif()

# Windows 10 APIs, for every file whatever order it includes system headers in.
add_compile_definitions(WINVER=0x0A00 _WIN32_WINNT=0x0A00 NTDDI_VERSION=0x0A000000
                        WIN32_LEAN_AND_MEAN UNICODE _UNICODE)

# ---- resources (icon-less: version info and the manifest) in their own target ------------------
add_library(sa3cx_resources OBJECT res/helper.rc)

# ---- core: everything but the entry point (also linked into the tests) -----------------------
add_library(sa3cx_core STATIC
  src/app_state.cpp
  src/audio_capture.cpp
  src/autostart.cpp
  src/caller_id.cpp
  src/frame_router.cpp
  src/local_audio_server.cpp
  src/log.cpp
  src/panel_view.cpp
  src/process_info.cpp
  src/resampler.cpp
  src/status_model.cpp
  src/tray_app.cpp
  src/tray_icons.cpp
  src/wav_writer.cpp
  src/ws_frame.cpp
  src/ws_handshake.cpp
)
target_include_directories(sa3cx_core PUBLIC src)
target_compile_options(sa3cx_core PRIVATE ${SA3CX_COMPILE_OPTIONS})
target_link_libraries(sa3cx_core PUBLIC ${SA3CX_SYSTEM_LIBS})

# ---- the program agents get ---------------------------------------------------------------------
add_executable(sales-assistant-3cx-helper WIN32 src/main.cpp $<TARGET_OBJECTS:sa3cx_resources>)
target_compile_options(sales-assistant-3cx-helper PRIVATE ${SA3CX_COMPILE_OPTIONS} ${SA3CX_WINMAIN_OPTIONS})
target_link_options(sales-assistant-3cx-helper PRIVATE ${SA3CX_LINK_OPTIONS} ${SA3CX_WINMAIN_OPTIONS})
target_link_libraries(sales-assistant-3cx-helper PRIVATE sa3cx_core)

# ---- diagnostics build: test options (--record, --pid, --port, ...). Never give it to agents. --
add_executable(sales-assistant-3cx-helper-diag WIN32 src/main.cpp $<TARGET_OBJECTS:sa3cx_resources>)
target_compile_definitions(sales-assistant-3cx-helper-diag PRIVATE SA3CX_DIAGNOSTICS)
target_compile_options(sales-assistant-3cx-helper-diag PRIVATE ${SA3CX_COMPILE_OPTIONS} ${SA3CX_WINMAIN_OPTIONS})
target_link_options(sales-assistant-3cx-helper-diag PRIVATE ${SA3CX_LINK_OPTIONS} ${SA3CX_WINMAIN_OPTIONS})
target_link_libraries(sales-assistant-3cx-helper-diag PRIVATE sa3cx_core)

# ---- unit tests ------------------------------------------------------------------------------------
enable_testing()
add_executable(sa3cx_tests
  tests/test_main.cpp
  tests/caller_id_test.cpp
  tests/resampler_test.cpp
  tests/status_model_test.cpp
  tests/ws_frame_test.cpp
  tests/ws_handshake_test.cpp
)
target_compile_options(sa3cx_tests PRIVATE ${SA3CX_COMPILE_OPTIONS})
target_link_options(sa3cx_tests PRIVATE ${SA3CX_LINK_OPTIONS})
target_link_libraries(sa3cx_tests PRIVATE sa3cx_core)
add_test(NAME unit_tests COMMAND sa3cx_tests)
