// Windows headers in the one order that works for every file (Winsock 2 must come before
// windows.h). Include this instead of <windows.h>.
#ifndef SA3CX_WIN32_H_
#define SA3CX_WIN32_H_

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef WINVER
#define WINVER 0x0A00  // Windows 10 (also set for every file by CMakeLists.txt)
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#endif  // SA3CX_WIN32_H_
