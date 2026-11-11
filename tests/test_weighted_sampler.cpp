#include "catch.hpp"

#include "alelus/crypto_rng.hpp"
#include "alelus/fast_rng.hpp"
#include "alelus/weighted_sampler.hpp"
#include "support/scripted_rng.hpp"
#include "support/stats.hpp"

#include <array>
#include <cstdint>
#include <stdexcept>
#include <vector>

using alelus::CryptoRNG;
using alelus::FastRNG;
using alelus::WeightedSampler;

TEST_CASE("WeightedSampler (algorithm)", "[weighted][sampler]") {
    SECTION("the worked example: weights {A: 1, B: 2, C: 5}") {
        // 3 columns of height 8:
        //   column A: AAACCCCC   (A if r < 3, otherwise its alias C)
        //   column B: BBBBBBCC   (B if r < 6, otherwise its alias C)
        //   column C: CCCCCCCC   (always C)
        // Each pick draws a column with uniform_below(3), then r with uniform_below(8).
        // Lemire: x = 2^62 -> column 0, 2^63 -> column 1, max -> column 2; for height 8,
        // r is simply the top 3 bits of x (r << 61).
        const WeightedSampler sampler({1, 2, 5});
        const std::uint64_t col_a = 0x4000000000000000ULL, col_b = 0x8000000000000000ULL,
                            col_c = UINT64_MAX;
        auto r = [](std::uint64_t value) {
            return value << 61;
        };

        ScriptedRng rng{col_a, r(2), col_a, r(3), col_b, r(5),
                        col_b, r(6), col_c, r(0), col_c, r(7)};
        CHECK(alelus::detail::weighted_index(rng, sampler) == 0); // column A, r = 2: A
        CHECK(alelus::detail::weighted_index(rng, sampler) == 2); // column A, r = 3: C
        CHECK(alelus::detail::weighted_index(rng, sampler) == 1); // column B, r = 5: B
        CHECK(alelus::detail::weighted_index(rng, sampler) == 2); // column B, r = 6: C
        CHECK(alelus::detail::weighted_index(rng, sampler) == 2); // column C, r = 0: C
        CHECK(alelus::detail::weighted_index(rng, sampler) == 2); // column C, r = 7: C
    }

    SECTION("exact: every (column, r) pair once gives exactly the weights") {
        // weights {1, 2, 4, 1}: 4 columns of height 8. Both are powers of 2, so Lemire never
        // rejects and x = c << 62 gives column c, x = r << 61 gives r. Visiting all 4 * 8 = 32
        // pairs once must give each index exactly weight * 4 times: {4, 8, 16, 4}.
        const WeightedSampler sampler({1, 2, 4, 1});
        std::vector<std::uint64_t> script;
        for (std::uint64_t c = 0; c < 4; ++c) {
            for (std::uint64_t r = 0; r < 8; ++r) {
                script.push_back(c << 62);
                script.push_back(r << 61);
            }
        }
        ScriptedRng rng(script);
        std::array<int, 4> counts{};
        for (int i = 0; i < 32; ++i)
            ++counts[alelus::detail::weighted_index(rng, sampler)];
        CHECK(counts == std::array<int, 4>{4, 8, 16, 4});
    }
}

TEST_CASE("FastRNG: weighted_index(WeightedSampler)", "[fast][weighted][sampler]") {
    FastRNG rng(2026);

    SECTION("follows the weights (chi-square)") {
        const WeightedSampler sampler({1, 2, 3, 4}); // 10%, 20%, 30%, 40%
        std::vector<std::uint64_t> counts(4, 0);
        for (int i = 0; i < 1000000; ++i)
            ++counts[rng.weighted_index(sampler)];
        CHECK(chi_square(counts, {0.1, 0.2, 0.3, 0.4}) < CHI2_CRITICAL_P001_DF3);
    }

    SECTION("a weight of 0 is never picked") {
        const WeightedSampler sampler({1, 0, 3});
        for (int i = 0; i < 100000; ++i)
            REQUIRE(rng.weighted_index(sampler) != 1);
    }

    SECTION("a single positive weight is always picked") {
        const WeightedSampler sampler({0, 0, 5, 0});
        for (int i = 0; i < 1000; ++i)
            REQUIRE(rng.weighted_index(sampler) == 2);
    }

    SECTION("can be built from any container of integers, or a list in place") {
        const std::vector<int> v{1, 2, 3};
        const std::array<std::uint32_t, 3> a{1, 2, 3};
        const short c[3] = {1, 2, 3};
        CHECK(WeightedSampler(v).size() == 3);
        CHECK(WeightedSampler(a).size() == 3);
        CHECK(WeightedSampler(c).size() == 3);
        CHECK(WeightedSampler({60, 30, 10}).size() == 3);
    }

    SECTION("invalid weights are rejected") {
        const std::vector<int> empty;
        const std::vector<int> zeros{0, 0, 0};
        const std::vector<int> negative{1, -1, 3};
        const std::vector<std::uint64_t> sum_too_big{UINT64_MAX, 1};
        // The sum fits (2^64 - 1), but sum * number of weights (2) does not
        const std::vector<std::uint64_t> scaled_too_big{0x8000000000000000ULL,
                                                        0x7FFFFFFFFFFFFFFFULL};
        CHECK_THROWS_AS(WeightedSampler(empty), std::invalid_argument);
        CHECK_THROWS_AS(WeightedSampler(zeros), std::invalid_argument);
        CHECK_THROWS_AS(WeightedSampler(negative), std::invalid_argument);
        CHECK_THROWS_AS(WeightedSampler(sum_too_big), std::overflow_error);
        CHECK_THROWS_AS(WeightedSampler(scaled_too_big), std::overflow_error);
    }

    SECTION("the same seed gives the same picks") {
        const WeightedSampler sampler({60, 30, 10});
        FastRNG a(42), b(42);
        for (int i = 0; i < 1000; ++i)
            REQUIRE(a.weighted_index(sampler) == b.weighted_index(sampler));
    }
}

TEST_CASE("CryptoRNG: weighted_index(WeightedSampler)", "[crypto][weighted][sampler]") {
    CryptoRNG rng;

    SECTION("stays in range and skips zero weights") {
        const WeightedSampler sampler({60, 0, 30, 10});
        for (int i = 0; i < 10000; ++i) {
            const std::size_t k = rng.weighted_index(sampler);
            REQUIRE(k < 4);
            REQUIRE(k != 1);
        }
    }
}
