#include "catch.hpp"

#include "alelus/crypto_rng.hpp"
#include "alelus/detail/uniform.hpp"
#include "alelus/fast_rng.hpp"
#include "support/scripted_rng.hpp"
#include "support/stats.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

using alelus::CryptoRNG;
using alelus::FastRNG;

namespace {

/// Counts [false, true] over n calls of f.
template <class F>
std::vector<std::uint64_t> count_outcomes(int n, F f) {
    std::vector<std::uint64_t> counts(2, 0);
    for (int i = 0; i < n; ++i)
        ++counts[f() ? 1 : 0];
    return counts;
}

} // namespace

TEST_CASE("bernoulli (algorithm)", "[bernoulli]") {
    SECTION("no arguments: the top bit decides") {
        ScriptedRng rng{0x7FFFFFFFFFFFFFFFULL, 0x8000000000000000ULL};
        CHECK_FALSE(alelus::detail::bernoulli(rng));
        CHECK(alelus::detail::bernoulli(rng));
    }

    SECTION("p: true exactly when uniform_real() < p") {
        // uniform_real() of the first value is just below 0.5, of the second exactly 0.5
        ScriptedRng rng{((1ULL << 52) - 1) << 11, 1ULL << 63};
        CHECK(alelus::detail::bernoulli(rng, 0.5));
        CHECK_FALSE(alelus::detail::bernoulli(rng, 0.5));
    }

    SECTION("k / n: true exactly when uniform_int in [0, n) < k") {
        // n = 3 (Lemire, 2^64 mod 3 = 1):
        //   x = 2^62 -> 0, and 0 < 1 is true
        //   x = max  -> 2, and 2 < 1 is false
        //   x = 0    -> rejected; then x = 2^63 -> 1, and 1 < 1 is false
        ScriptedRng rng{0x4000000000000000ULL, UINT64_MAX, 0, 0x8000000000000000ULL};
        CHECK(alelus::detail::bernoulli(rng, 1, 3));
        CHECK_FALSE(alelus::detail::bernoulli(rng, 1, 3));
        CHECK_FALSE(alelus::detail::bernoulli(rng, 1, 3));
        CHECK(rng.consumed() == 4);
    }
}

TEST_CASE("FastRNG: bernoulli()", "[fast][bernoulli]") {
    FastRNG rng(2026);

    SECTION("edge cases: never and always") {
        for (int i = 0; i < 1000; ++i) {
            REQUIRE_FALSE(rng.bernoulli(0.0));
            REQUIRE(rng.bernoulli(1.0));
            REQUIRE_FALSE(rng.bernoulli(0, 5));
            REQUIRE(rng.bernoulli(5, 5));
        }
    }

    SECTION("no arguments: a fair coin (chi-square)") {
        const auto counts = count_outcomes(1000000, [&] { return rng.bernoulli(); });
        CHECK(chi_square(counts, {0.5, 0.5}) < CHI2_CRITICAL_P001_DF1);
    }

    SECTION("p = 0.3 (chi-square)") {
        const auto counts = count_outcomes(1000000, [&] { return rng.bernoulli(0.3); });
        CHECK(chi_square(counts, {0.7, 0.3}) < CHI2_CRITICAL_P001_DF1);
    }

    SECTION("k / n = 1/3 (chi-square)") {
        const auto counts = count_outcomes(1000000, [&] { return rng.bernoulli(1, 3); });
        CHECK(chi_square(counts, {2.0 / 3.0, 1.0 / 3.0}) < CHI2_CRITICAL_P001_DF1);
    }

    SECTION("k and n may have different integer types") {
        const std::vector<int> v(10);
        CHECK_NOTHROW(rng.bernoulli(3, v.size())); // int and size_t
    }

    SECTION("invalid arguments are rejected") {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        CHECK_THROWS_AS(rng.bernoulli(-0.1), std::invalid_argument);
        CHECK_THROWS_AS(rng.bernoulli(1.1), std::invalid_argument);
        CHECK_THROWS_AS(rng.bernoulli(nan), std::invalid_argument);
        CHECK_THROWS_AS(rng.bernoulli(1, 0), std::invalid_argument);  // n = 0
        CHECK_THROWS_AS(rng.bernoulli(4, 3), std::invalid_argument);  // k > n
        CHECK_THROWS_AS(rng.bernoulli(-1, 3), std::invalid_argument); // k < 0
    }

    SECTION("the same seed gives the same outcomes") {
        FastRNG a(42), b(42);
        for (int i = 0; i < 1000; ++i)
            REQUIRE(a.bernoulli(0.3) == b.bernoulli(0.3));
    }
}

TEST_CASE("CryptoRNG: bernoulli()", "[crypto][bernoulli]") {
    CryptoRNG rng;

    SECTION("the three forms") {
        const auto coin = count_outcomes(10000, [&] { return rng.bernoulli(); });
        CHECK(coin[0] > 0);
        CHECK(coin[1] > 0);
        CHECK_FALSE(rng.bernoulli(0.0));
        CHECK(rng.bernoulli(1.0));
        CHECK(rng.bernoulli(3, 3));
    }
}
