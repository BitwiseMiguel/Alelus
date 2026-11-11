#include "catch.hpp"

#include "alelus/crypto_rng.hpp"
#include "alelus/detail/uniform.hpp"
#include "alelus/fast_rng.hpp"
#include "support/scripted_rng.hpp"
#include "support/stats.hpp"

#include <array>
#include <cstdint>
#include <stdexcept>
#include <vector>

using alelus::CryptoRNG;
using alelus::FastRNG;

TEST_CASE("weighted_index (algorithm)", "[weighted]") {
    // weights {2, 3}: total 5, so r = uniform_below(5) (mask 0b111).
    // Values 0 and 1 belong to index 0; values 2, 3 and 4 belong to index 1.
    const std::vector<std::uint64_t> weights{2, 3};

    SECTION("r inside the first block gives index 0") {
        ScriptedRng rng{1};
        CHECK(alelus::detail::weighted_index(rng, weights) == 0);
    }

    SECTION("r at the start of the second block gives index 1") {
        ScriptedRng rng{2};
        CHECK(alelus::detail::weighted_index(rng, weights) == 1);
    }

    SECTION("r = 7 is outside [0, 5): rejected by uniform_below, then 4 gives index 1") {
        ScriptedRng rng{7, 4};
        CHECK(alelus::detail::weighted_index(rng, weights) == 1);
        CHECK(rng.consumed() == 2);
    }
}

TEST_CASE("FastRNG: weighted_index()", "[fast][weighted]") {
    FastRNG rng(2026);

    SECTION("follows the weights (chi-square)") {
        const std::vector<std::uint64_t> weights{1, 2, 3, 4}; // 10%, 20%, 30%, 40%
        std::vector<std::uint64_t> counts(4, 0);
        for (int i = 0; i < 1000000; ++i)
            ++counts[rng.weighted_index(weights)];
        CHECK(chi_square(counts, {0.1, 0.2, 0.3, 0.4}) < CHI2_CRITICAL_P001_DF3);
    }

    SECTION("a weight of 0 is never picked") {
        const std::vector<int> weights{1, 0, 3};
        for (int i = 0; i < 100000; ++i)
            REQUIRE(rng.weighted_index(weights) != 1);
    }

    SECTION("a single positive weight is always picked") {
        const std::vector<int> weights{0, 0, 5, 0};
        for (int i = 0; i < 1000; ++i)
            REQUIRE(rng.weighted_index(weights) == 2);
    }

    SECTION("any container of integers works") {
        const std::vector<int> v{1, 2, 3};
        const std::array<std::uint32_t, 3> a{1, 2, 3};
        const short c[3] = {1, 2, 3};
        CHECK(rng.weighted_index(v) < 3);
        CHECK(rng.weighted_index(a) < 3);
        CHECK(rng.weighted_index(c) < 3);
    }

    SECTION("invalid weights are rejected") {
        const std::vector<int> empty;
        const std::vector<int> zeros{0, 0, 0};
        const std::vector<int> negative{1, -1, 3};
        const std::vector<std::uint64_t> too_big{UINT64_MAX, 1};
        CHECK_THROWS_AS(rng.weighted_index(empty), std::invalid_argument);
        CHECK_THROWS_AS(rng.weighted_index(zeros), std::invalid_argument);
        CHECK_THROWS_AS(rng.weighted_index(negative), std::invalid_argument);
        CHECK_THROWS_AS(rng.weighted_index(too_big), std::overflow_error);
    }

    SECTION("the same seed gives the same picks") {
        const std::vector<int> weights{60, 30, 10};
        FastRNG a(42), b(42);
        for (int i = 0; i < 1000; ++i)
            REQUIRE(a.weighted_index(weights) == b.weighted_index(weights));
    }
}

TEST_CASE("CryptoRNG: weighted_index()", "[crypto][weighted]") {
    CryptoRNG rng;

    SECTION("stays in range and skips zero weights") {
        const std::vector<int> weights{60, 0, 30, 10};
        for (int i = 0; i < 10000; ++i) {
            const std::size_t k = rng.weighted_index(weights);
            REQUIRE(k < 4);
            REQUIRE(k != 1);
        }
    }
}
