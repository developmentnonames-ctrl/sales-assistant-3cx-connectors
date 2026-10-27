#include "test.h"

namespace sa3cx::test {

namespace {
int g_failures = 0;
}  // namespace

std::vector<TestCase>& Registry() {
  static std::vector<TestCase> tests;
  return tests;
}

void ReportFailure(const char* file, int line, const std::string& message) {
  ++g_failures;
  std::cout << "    FAILED " << file << ":" << line << ": " << message << "\n";
}

int Failures() { return g_failures; }

}  // namespace sa3cx::test

int main() {
  int failed_tests = 0;
  for (const auto& test : sa3cx::test::Registry()) {
    const int before = sa3cx::test::Failures();
    test.body();
    const bool passed = sa3cx::test::Failures() == before;
    if (!passed) ++failed_tests;
    std::cout << (passed ? "[  OK  ] " : "[ FAIL ] ") << test.name << "\n";
  }
  std::cout << sa3cx::test::Registry().size() - failed_tests << " of "
            << sa3cx::test::Registry().size() << " tests passed\n";
  return failed_tests == 0 ? 0 : 1;
}
