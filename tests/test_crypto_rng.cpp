#include "catch.hpp"

#include "alelus/crypto_rng.hpp"
#include "alelus/detail/secure_zero.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <numeric>
#include <random>
#include <type_traits>
#include <vector>

using alelus::CryptoRNG;

// The same standard interface as FastRNG
static_assert(std::uniform_random_bit_generator<CryptoRNG>);
static_assert(CryptoRNG::min() == 0);
static_assert(CryptoRNG::max() == UINT64_MAX);

// Only one way to build it: without arguments, keyed from the operating system
static_assert(std::is_default_constructible_v<CryptoRNG>);

// The secret lives in exactly one place
static_assert(!std::is_copy_constructible_v<CryptoRNG>);
static_assert(!std::is_copy_assignable_v<CryptoRNG>);
static_assert(!std::is_move_constructible_v<CryptoRNG>);

TEST_CASE("CryptoRNG: generation", "[crypto]") {
    // Not deterministic: the key is real entropy. Each check can only fail by chance with
    // a probability far below 1 in a billion.

    SECTION("two generators give different sequences") {
        CryptoRNG a, b;
        int equal = 0;
        for (int i = 0; i < 1000; ++i)
            equal += (a() == b());
        CHECK(equal == 0);
    }

    SECTION("half of the bits are set, across many refills") {
        CryptoRNG rng;
        const int n = 200000; // about 1600 refills of 124 numbers
        std::uint64_t ones = 0;
        for (int i = 0; i < n; ++i)
            ones += static_cast<std::uint64_t>(std::popcount(rng()));
        // 12.8 million bits: standard deviation of the fraction ~0.00014; 0.0011 is ~8 of them
        const double fraction = static_cast<double>(ones) / (64.0 * n);
        CHECK(fraction == Approx(0.5).margin(0.0011));
    }

    SECTION("works with the standard library") {
        CryptoRNG rng;
        std::uniform_int_distribution<int> die(1, 6);
        for (int i = 0; i < 1000; ++i) {
            const int roll = die(rng);
            REQUIRE(roll >= 1);
            REQUIRE(roll <= 6);
        }
        std::vector<int> deck(52);
        std::iota(deck.begin(), deck.end(), 0);
        std::shuffle(deck.begin(), deck.end(), rng);
        CHECK(std::is_sorted(deck.begin(), deck.end()) == false);
    }
}

TEST_CASE("secure_zero", "[crypto]") {
    SECTION("wipes exactly the bytes it is given") {
        std::array<std::uint8_t, 100> buffer{};
        buffer.fill(0xAB);
        alelus::detail::secure_zero(buffer.data(), 50);
        CHECK(std::all_of(buffer.begin(), buffer.begin() + 50, [](auto b) { return b == 0; }));
        CHECK(std::all_of(buffer.begin() + 50, buffer.end(), [](auto b) { return b == 0xAB; }));
    }
}
