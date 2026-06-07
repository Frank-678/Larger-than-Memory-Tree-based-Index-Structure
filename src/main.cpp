#ifdef _WIN32
#include <windows.h>
#endif
#include <iostream>
#include "test_helpers.h"

// Declare test suites (defined in separate compilation units)
void RunAllCorrectnessTests();
void RunAllBenchmarks();

int main() {
    #ifdef _WIN32
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
    #endif

    std::cout << "==========================================" << std::endl;
    std::cout << "  LTM B+ Tree - Test Suite" << std::endl;
    std::cout << "==========================================" << std::endl;

    // Run correctness tests (verify core functionality)
    RunAllCorrectnessTests();
    PrintTestSummary();

    // Run benchmarks (measure performance)
    RunAllBenchmarks();

    std::cout << "\nAll tests completed." << std::endl;
    return g_fail_count > 0 ? 1 : 0;
}
