// A small text log: %LOCALAPPDATA%\SalesAssistant3CXHelper\helper.log (kept under 1 MB, one old
// copy). Holds no audio and no full phone numbers. Thread-safe.
#ifndef SA3CX_LOG_H_
#define SA3CX_LOG_H_

#if defined(__GNUC__) || defined(__clang__)
#define SA3CX_PRINTF_FORMAT(format_index, args_index) \
  __attribute__((format(printf, format_index, args_index)))
#else
#define SA3CX_PRINTF_FORMAT(format_index, args_index)
#endif

namespace sa3cx {
namespace log {

// Opens the log file. Until then (and if it can't be opened) lines go to the debugger only.
void Open();

// Also writes every line to stdout (--console).
void EchoToConsole(bool on);

// printf-style; one line, the time is added.
void Write(const char* format, ...) SA3CX_PRINTF_FORMAT(1, 2);

}  // namespace log
}  // namespace sa3cx

#endif  // SA3CX_LOG_H_
