// The caller's number as sent by the Live Call page: cleaned before use, masked before logging.
#ifndef SA3CX_CALLER_ID_H_
#define SA3CX_CALLER_ID_H_

#include <string>

namespace sa3cx {

// Keeps only phone characters (0-9 + * #), at most 20.
std::string SanitizeCallerId(const std::string& raw);

// "0771234567" -> "077*****67". Short values (an extension like "1031") are kept.
std::string MaskCallerIdForLog(const std::string& clean);

}  // namespace sa3cx

#endif  // SA3CX_CALLER_ID_H_
