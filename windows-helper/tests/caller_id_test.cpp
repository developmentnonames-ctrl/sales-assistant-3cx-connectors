#include "caller_id.h"

#include "test.h"

TEST(CallerId, KeepsOnlyPhoneCharacters) {
  EXPECT_EQ(sa3cx::SanitizeCallerId("+94 77-123 4567"), std::string("+94771234567"));
  EXPECT_EQ(sa3cx::SanitizeCallerId("07\nFAKE LOG LINE"), std::string("07"));
  EXPECT_EQ(sa3cx::SanitizeCallerId("*123#"), std::string("*123#"));
}

TEST(CallerId, IsAtMostTwentyCharacters) {
  EXPECT_EQ(sa3cx::SanitizeCallerId(std::string(50, '9')).size(), 20u);
}

TEST(CallerId, IsMaskedInTheLog) {
  EXPECT_EQ(sa3cx::MaskCallerIdForLog("0771234567"), std::string("077*****67"));
  EXPECT_EQ(sa3cx::MaskCallerIdForLog("1031"), std::string("1031"));
  EXPECT_EQ(sa3cx::MaskCallerIdForLog(""), std::string(""));
}
