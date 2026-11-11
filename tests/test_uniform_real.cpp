#include "catch.hpp"

#include "alelus/crypto_rng.hpp"
#include "alelus/detail/uniform.hpp"
#include "alelus/fast_rng.hpp"
#include "support/scripted_rng.hpp"

#include <cfloat>
#include <cmath>
#include <limits>
#include <stdexcept>

using alelus::CryptoRNG;
using alelus::FastRNG;

TEST_CASE("uniform_real (algorithm)", "[uniform]") {
    // A scripted generator shows exactly which values come out of which bits

    SECTION("[0, 1): the extremes") {
        ScriptedRng rng{0, UINT64_MAX, 0x7FF};
        CHECK(alelus::detail::uniform_real(rng) == 0.0);
        CHECK(alelus::detail::uniform_real(rng) == 0x1.fffffffffffffp-1); // 1 - 2^-53
        CHECK(alelus::detail::uniform_real(rng) == 0.0); // the 11 low bits are discarded
    }

    SECTION("[0, 1): values are multiples of 2^-53") {
        ScriptedRng rng{1ULL << 11, 3ULL << 11};
        CHECK(alelus::detail::uniform_real(rng) == 0x1.0p-53);
        CHECK(alelus::detail::uniform_real(rng) == 3 * 0x1.0p-53);
    }

    SECTION("[lo, hi): when rounding gives hi, it draws again") {
        // With the largest value, 100 + (1 - 2^-53) * 1 rounds to exactly 101: doubles near
        // 101 are 2^-46 apart, much more than 2^-53. That draw must be rejected.
        ScriptedRng rng{UINT64_MAX, 0};
        CHECK(alelus::detail::uniform_real(rng, 100.0, 101.0) == 100.0);
        CHECK(rng.consumed() == 2);
    }
}

TEST_CASE("FastRNG: uniform_real()", "[fast][uniform]") {
    FastRNG rng(77);

    SECTION("no arguments: [0, 1)") {
        for (int i = 0; i < 100000; ++i) {
            const double u = rng.uniform_real();
            REQUIRE(u >= 0.0);
            REQUIRE(u < 1.0);
        }
    }

    SECTION("one argument: [0, b)") {
        for (int i = 0; i < 100000; ++i) {
            const double u = rng.uniform_real(2.5);
            REQUIRE(u >= 0.0);
            REQUIRE(u < 2.5);
        }
    }

    SECTION("two arguments: [a, b)") {
        for (int i = 0; i < 100000; ++i) {
            const double u = rng.uniform_real(-3.0, -1.0);
            REQUIRE(u >= -3.0);
            REQUIRE(u < -1.0);
        }
    }

    SECTION("integers as arguments are converted to double") {
        const double u = rng.uniform_real(1, 6);
        CHECK(u >= 1.0);
        CHECK(u < 6.0);
    }

    SECTION("the largest possible range does not give infinity") {
        for (int i = 0; i < 10000; ++i)
            REQUIRE(std::isfinite(rng.uniform_real(-DBL_MAX, DBL_MAX)));
    }

    SECTION("invalid arguments are rejected") {
        const double inf = std::numeric_limits<double>::infinity();
        const double nan = std::numeric_limits<double>::quiet_NaN();
        CHECK_THROWS_AS(rng.uniform_real(1.0, 1.0), std::invalid_argument); // empty
        CHECK_THROWS_AS(rng.uniform_real(2.0, 1.0), std::invalid_argument); // reversed
        CHECK_THROWS_AS(rng.uniform_real(0.0, inf), std::invalid_argument);
        CHECK_THROWS_AS(rng.uniform_real(nan, 1.0), std::invalid_argument);
        CHECK_THROWS_AS(rng.uniform_real(0.0), std::invalid_argument); // [0, 0) is empty
        CHECK_THROWS_AS(rng.uniform_real(-1.0), std::invalid_argument);
    }

    SECTION("mean 1/2 and variance 1/12, as for U(0, 1)") {
        // With n = 10^6, the standard error of the mean is ~0.0003; the margin is 5 of them
        const int n = 1000000;
        double sum = 0.0, sum_sq = 0.0;
        for (int i = 0; i < n; ++i) {
            const double u = rng.uniform_real();
            sum += u;
            sum_sq += u * u;
        }
        const double mean = sum / n;
        const double variance = sum_sq / n - mean * mean;
        CHECK(mean == Approx(0.5).margin(0.0015));
        CHECK(variance == Approx(1.0 / 12.0).margin(0.001));
    }

    SECTION("the same seed gives the same values") {
        FastRNG a(42), b(42);
        for (int i = 0; i < 1000; ++i)
            REQUIRE(a.uniform_real() == b.uniform_real());
    }
}

TEST_CASE("CryptoRNG: uniform_real()", "[crypto][uniform]") {
    CryptoRNG rng;

    SECTION("the three forms stay in range") {
        for (int i = 0; i < 10000; ++i) {
            const double a = rng.uniform_real();
            const double b = rng.uniform_real(10.0);
            const double c = rng.uniform_real(-1.0, 1.0);
            REQUIRE((a >= 0.0 && a < 1.0));
            REQUIRE((b >= 0.0 && b < 10.0));
            REQUIRE((c >= -1.0 && c < 1.0));
        }
    }
}
