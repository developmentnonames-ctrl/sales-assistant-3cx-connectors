@echo off
rem Builds the helper, the diagnostics build and the unit tests with CMake + Ninja (MinGW-w64 or
rem MSVC on PATH), then runs the tests. Output: build\sales-assistant-3cx-helper.exe
rem   build.cmd        release: accepts only https://aicallagent.chassia.com
rem   build.cmd dev    development: also accepts http://localhost:3004 (never give it to agents)
cd /d "%~dp0"
set DEV=OFF
set OUT=build
if /i "%1"=="dev" (set DEV=ON& set OUT=build-dev)
cmake -S . -B %OUT% -G Ninja -DCMAKE_BUILD_TYPE=Release -DSA3CX_DEV_ORIGINS=%DEV% || exit /b 1
cmake --build %OUT% || exit /b 1
ctest --test-dir %OUT% --output-on-failure || exit /b 1
