// A minimal test runner (no third-party code, so the build has no outside dependencies).
//
//   TEST(Suite, Name) { EXPECT_TRUE(...); EXPECT_EQ(a, b); }
#ifndef SA3CX_TESTS_TEST_H_
#define SA3CX_TESTS_TEST_H_

#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace sa3cx::test {

struct TestCase {
  std::string name;
  std::function<void()> body;
};

std::vector<TestCase>& Registry();
void ReportFailure(const char* file, int line, const std::string& message);
int Failures();

struct Registrar {
  Registrar(std::string name, std::function<void()> body) {
    Registry().push_back({std::move(name), std::move(body)});
  }
};

}  // namespace sa3cx::test

#define SA3CX_CONCAT_INNER(a, b) a##b
#define SA3CX_CONCAT(a, b) SA3CX_CONCAT_INNER(a, b)

#define TEST(suite, name)                                                         \
  static void suite##_##name();                                                   \
  static ::sa3cx::test::Registrar SA3CX_CONCAT(registrar_, __LINE__)(#suite "." #name, \
                                                                     suite##_##name); \
  static void suite##_##name()

#define EXPECT_TRUE(condition)                                                    \
  do {                                                                            \
    if (!(condition)) ::sa3cx::test::ReportFailure(__FILE__, __LINE__, #condition); \
  } while (0)

#define EXPECT_EQ(actual, expected)                                               \
  do {                                                                            \
    if (!((actual) == (expected))) {                                              \
      ::sa3cx::test::ReportFailure(__FILE__, __LINE__, #actual " == " #expected); \
    }                                                                             \
  } while (0)

#endif  // SA3CX_TESTS_TEST_H_
