#pragma once

// Test helper: chi-square statistics.
//
// The statistical tests always use a FIXED seed, so they are deterministic: they either
// always pass or always fail. A test that fails only sometimes would be useless.

#include <cstddef>
#include <cstdint>
#include <vector>

/// Chi-square statistic of observed counts against a uniform distribution.
inline double chi_square_uniform(const std::vector<std::uint64_t> &observed) {
    std::uint64_t total = 0;
    for (const auto count : observed)
        total += count;
    const double expected = static_cast<double>(total) / static_cast<double>(observed.size());
    double chi2 = 0.0;
    for (const auto count : observed) {
        const double diff = static_cast<double>(count) - expected;
        chi2 += diff * diff / expected;
    }
    return chi2;
}

/// Chi-square statistic of observed counts against expected probabilities.
inline double chi_square(const std::vector<std::uint64_t> &observed,
                         const std::vector<double> &probabilities) {
    std::uint64_t total = 0;
    for (const auto count : observed)
        total += count;
    double chi2 = 0.0;
    for (std::size_t i = 0; i < observed.size(); ++i) {
        const double expected = probabilities[i] * static_cast<double>(total);
        const double diff = static_cast<double>(observed[i]) - expected;
        chi2 += diff * diff / expected;
    }
    return chi2;
}

// Critical values of the chi-square distribution for p = 0.001, by degrees of freedom
// (number of categories - 1). Source: standard chi-square tables.
inline constexpr double CHI2_CRITICAL_P001_DF1 = 10.828;
inline constexpr double CHI2_CRITICAL_P001_DF5 = 20.515;
