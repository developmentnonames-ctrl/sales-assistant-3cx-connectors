#include "caller_id.h"

namespace sa3cx {

namespace {
constexpr std::size_t kMaxCallerIdLength = 20;
}  // namespace

std::string SanitizeCallerId(const std::string& raw) {
  std::string clean;
  for (char ch : raw) {
    bool phone_char = (ch >= '0' && ch <= '9') || ch == '+' || ch == '*' || ch == '#';
    if (phone_char) clean.push_back(ch);
    if (clean.size() == kMaxCallerIdLength) break;
  }
  return clean;
}

std::string MaskCallerIdForLog(const std::string& clean) {
  if (clean.size() <= 5) return clean;
  return clean.substr(0, 3) + std::string(clean.size() - 5, '*') + clean.substr(clean.size() - 2);
}

}  // namespace sa3cx
