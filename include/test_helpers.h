#pragma once
#include <string>
#include <iostream>
#include <iomanip>

// Global test counters
inline int g_pass_count = 0;
inline int g_fail_count = 0;

// Assertion macro with description
inline void AssertTopologicalPoint(bool condition, const std::string& func_name,
                                    int point_id, const std::string& desc) {
    if (condition) {
        std::cout << "[PASS] " << func_name << " | Point " << std::setw(2) << point_id
                  << " : " << desc << std::endl;
        g_pass_count++;
    } else {
        std::cout << "[FAIL] " << func_name << " | Point " << std::setw(2) << point_id
                  << " : " << desc << std::endl;
        g_fail_count++;
        std::exit(EXIT_FAILURE);
    }
}

// Print summary of test results
inline void PrintTestSummary() {
    std::cout << "\n==========================================" << std::endl;
    std::cout << "  Test Summary: " << g_pass_count << " passed, "
              << g_fail_count << " failed" << std::endl;
    std::cout << "==========================================" << std::endl;
}
