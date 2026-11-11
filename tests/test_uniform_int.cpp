#include "catch.hpp"

#include "alelus/crypto_rng.hpp"
#include "alelus/detail/uniform.hpp"
#include "alelus/fast_rng.hpp"
#include "support/scripted_rng.hpp"
#include "support/stats.hpp"

#include <cstdint>
#include <random>
#include <set>
#include <stdexcept>
#include <type_traits>
#include <vector>

using alelus::CryptoRNG;
using alelus::FastRNG;
using alelus::detail::Random64;

static_assert(Random64<FastRNG>);
static_assert(Random64<CryptoRNG>);
static_assert(Random64<ScriptedRng>);
static_assert(!Random64<std::mt19937>); // only 32 bits per call: not accepted

TEST_CASE("uniform_below (Lemire's algorithm)", "[uniform]") {
    // These tests use a scripted generator to see exactly which numbers are consumed.
    // Expected values computed independently in Python with exact integers.

    SECTION("the result is the high half of x * n") {
        // x = 2^62, n = 6: x * 6 = 1.5 * 2^64 -> high half 1, low half 2^63 (accepted)
        ScriptedRng rng{0x4000000000000000ULL};
        CHECK(alelus::detail::uniform_below(rng, 6) == 1);
        CHECK(rng.consumed() == 1);
    }

    SECTION("rejects when the low half is below 2^64 mod n") {
        // n = 6, 2^64 mod 6 = 4:
        //   x = 0    -> x * 6 = 0:              low half 0 < 4, rejected
        //   x = 2^63 -> x * 6 = 3 * 2^64:       low half 0 < 4, rejected
        //   x = max  -> x * 6 = 6 * 2^64 - 6:   high half 5, accepted
        ScriptedRng rng{0, 0x8000000000000000ULL, UINT64_MAX};
        CHECK(alelus::detail::uniform_below(rng, 6) == 5);
        CHECK(rng.consumed() == 3);
    }

    SECTION("huge ranges") {
        // n = 2^63 + 1, 2^64 mod n = 2^63 - 1:
        //   x = 2 -> x * n = 2^64 + 2: low half 2 < 2^63 - 1, rejected
        //   x = 1 -> x * n = 2^63 + 1: high half 0, accepted
        ScriptedRng rng{2, 1};
        CHECK(alelus::detail::uniform_below(rng, 0x8000000000000001ULL) == 0);
        CHECK(rng.consumed() == 2);
    }

    SECTION("almost never needs a second number") {
        // Counts the draws for 100000 dice: a rejection happens with probability 4 / 2^64
        struct Counting {
            using result_type = std::uint64_t;
            static constexpr result_type min() {
                return 0;
            }
            static constexpr result_type max() {
                return UINT64_MAX;
            }
            FastRNG inner{9};
            std::uint64_t calls = 0;
            result_type operator()() {
                ++calls;
                return inner();
            }
        } rng;
        for (int i = 0; i < 100000; ++i)
            (void)alelus::detail::uniform_below(rng, 6);
        CHECK(rng.calls == 100000);
    }

    SECTION("the full 64-bit range uses one draw, unchanged") {
        ScriptedRng rng{0x8000000000000000ULL, 0x0123456789abcdefULL};
        CHECK(alelus::detail::uniform_int<std::int64_t>(rng, INT64_MIN, INT64_MAX) == INT64_MIN);
        CHECK(alelus::detail::uniform_int<std::uint64_t>(rng, 0, UINT64_MAX) ==
              0x0123456789abcdefULL);
        CHECK(rng.consumed() == 2);
    }
}

TEST_CASE("FastRNG: uniform_int()", "[fast][uniform]") {
    FastRNG rng(2026);

    SECTION("lo > hi is rejected") {
        CHECK_THROWS_AS(rng.uniform_int(5, 4), std::invalid_argument);
    }

    SECTION("a single-value range always returns that value") {
        for (int i = 0; i < 100; ++i)
            REQUIRE(rng.uniform_int(-7, -7) == -7);
    }

    SECTION("results stay inside the range") {
        const std::vector<std::uint64_t> sizes{
            2, 3, 6, 7, 1000, 0x100000001ULL, 0x8000000000000001ULL, UINT64_MAX};
        for (const std::uint64_t n : sizes) {
            for (int i = 0; i < 1000; ++i)
                REQUIRE(rng.uniform_int<std::uint64_t>(0, n - 1) < n);
        }
    }

    SECTION("small and signed types reach every value") {
        std::set<int> seen;
        for (int i = 0; i < 100000; ++i)
            seen.insert(rng.uniform_int<std::int8_t>(-128, 127));
        CHECK(seen.size() == 256);

        std::set<int> small;
        for (int i = 0; i < 1000; ++i)
            small.insert(rng.uniform_int(-3, 3));
        CHECK(small.size() == 7);
        CHECK(*small.begin() == -3);
        CHECK(*small.rbegin() == 3);
    }

    SECTION("a six-sided die is uniform (chi-square)") {
        std::vector<std::uint64_t> counts(6, 0);
        for (int i = 0; i < 600000; ++i)
            ++counts[static_cast<std::size_t>(rng.uniform_int(1, 6) - 1)];
        CHECK(chi_square_uniform(counts) < CHI2_CRITICAL_P001_DF5);
    }

    SECTION("there is no modulo bias") {
        // With n = 3 * 2^62, "rng() % n" would give the values below 2^62 TWICE the
        // probability of the others: the fraction below n/3 would be 1/2 instead of 1/3.
        const std::uint64_t n = 3ULL << 62;
        const int draws = 300000;
        int below_third = 0;
        for (int i = 0; i < draws; ++i)
            below_third += (rng.uniform_int<std::uint64_t>(0, n - 1) < (1ULL << 62));
        const double fraction = static_cast<double>(below_third) / draws;
        CHECK(fraction == Approx(1.0 / 3.0).margin(0.005));
    }

    SECTION("two arguments of different types: the larger common type") {
        const std::vector<int> v{10, 20, 30};
        static_assert(std::is_same_v<decltype(rng.uniform_int(1, v.size())), std::size_t>);
        static_assert(std::is_same_v<decltype(rng.uniform_int(1, 6LL)), long long>);
        std::set<std::size_t> seen;
        for (int i = 0; i < 1000; ++i)
            seen.insert(rng.uniform_int(1, v.size()));
        CHECK(seen == std::set<std::size_t>{1, 2, 3});
    }

    SECTION("two arguments: a value that does not fit is rejected, not converted") {
        // int and unsigned have the common type unsigned: -1 would silently become 4294967295
        CHECK_THROWS_AS(rng.uniform_int(-1, 5u), std::out_of_range);
        CHECK_NOTHROW(rng.uniform_int(0, 5u));
    }

    SECTION("two arguments with an explicit type") {
        static_assert(std::is_same_v<decltype(rng.uniform_int<std::int16_t>(1, 6)), std::int16_t>);
        for (int i = 0; i < 100; ++i) {
            const std::int16_t x = rng.uniform_int<std::int16_t>(1, 6);
            REQUIRE(x >= 1);
            REQUIRE(x <= 6);
        }
        // 300 does not fit in a uint8_t: rejected instead of being cut down to 44
        CHECK_THROWS_AS(rng.uniform_int<std::uint8_t>(0, 300), std::out_of_range);
    }

    SECTION("one argument: from 0 to n, n not included") {
        std::set<int> seen;
        for (int i = 0; i < 1000; ++i)
            seen.insert(rng.uniform_int(6));
        CHECK(seen == std::set<int>{0, 1, 2, 3, 4, 5});
    }

    SECTION("one argument: n = 1 always gives 0, n <= 0 is rejected") {
        for (int i = 0; i < 100; ++i)
            REQUIRE(rng.uniform_int(1) == 0);
        CHECK_THROWS_AS(rng.uniform_int(0), std::invalid_argument);
        CHECK_THROWS_AS(rng.uniform_int(-1), std::invalid_argument);
    }

    SECTION("one argument: a container size gives a valid index") {
        const std::vector<int> v{10, 20, 30};
        std::set<std::size_t> seen;
        for (int i = 0; i < 1000; ++i)
            seen.insert(rng.uniform_int(v.size())); // no "- 1", no explicit type
        CHECK(seen == std::set<std::size_t>{0, 1, 2});
    }

    SECTION("one argument: small types") {
        std::set<int> seen;
        for (int i = 0; i < 100000; ++i)
            seen.insert(rng.uniform_int(std::uint8_t{255}));
        CHECK(seen.size() == 255); // 0 to 254
        CHECK(*seen.rbegin() == 254);
    }

    SECTION("no arguments: the full 64-bit range, the same as operator()") {
        FastRNG copy = rng;
        for (int i = 0; i < 100; ++i)
            REQUIRE(rng.uniform_int() == copy());
        static_assert(std::is_same_v<decltype(rng.uniform_int()), std::uint64_t>);
    }

    SECTION("no arguments with a type: every value of that type") {
        std::set<int> seen;
        for (int i = 0; i < 100000; ++i)
            seen.insert(rng.uniform_int<std::int8_t>());
        CHECK(seen.size() == 256); // -128 to 127
        CHECK(*seen.begin() == -128);
        CHECK(*seen.rbegin() == 127);
    }

    SECTION("the same seed gives the same rolls") {
        FastRNG a(42), b(42);
        for (int i = 0; i < 1000; ++i)
            REQUIRE(a.uniform_int(1, 6) == b.uniform_int(1, 6));
    }
}

TEST_CASE("CryptoRNG: uniform_int()", "[crypto][uniform]") {
    CryptoRNG rng;

    SECTION("lo > hi is rejected") {
        CHECK_THROWS_AS(rng.uniform_int(5, 4), std::invalid_argument);
    }

    SECTION("one and zero arguments") {
        std::set<int> seen;
        for (int i = 0; i < 1000; ++i)
            seen.insert(rng.uniform_int(3));
        CHECK(seen == std::set<int>{0, 1, 2});
        static_assert(std::is_same_v<decltype(rng.uniform_int()), std::uint64_t>);
        CHECK(rng.uniform_int() != rng.uniform_int()); // equal with probability 2^-64
    }

    SECTION("a die stays in range and every face appears") {
        std::set<int> faces;
        for (int i = 0; i < 10000; ++i) {
            const int roll = rng.uniform_int(1, 6);
            REQUIRE(roll >= 1);
            REQUIRE(roll <= 6);
            faces.insert(roll);
        }
        CHECK(faces.size() == 6);
    }
}
