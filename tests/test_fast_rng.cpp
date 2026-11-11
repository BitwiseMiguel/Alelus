#include "catch.hpp"

#include "alelus/fast_rng.hpp"

#include <stdexcept>
#include <type_traits>

using alelus::FastRNG;

// Compile-time checks: they cannot live inside a TEST_CASE, and if one fails the tests do
// not even compile.

// A FastRNG is only numbers in memory: copying it is allowed, and useful (a copy will
// continue the same sequence).
static_assert(std::is_copy_constructible_v<FastRNG>);
static_assert(std::is_copy_assignable_v<FastRNG>);

// "explicit": a State does not silently turn into a FastRNG
static_assert(!std::is_convertible_v<FastRNG::State, FastRNG>);

TEST_CASE("FastRNG: construction", "[fast]") {
    // Catch2 runs this TEST_CASE once for each SECTION, from the top. Code here, before
    // the sections, is shared setup: every section starts with a fresh copy of it.
    const FastRNG::State state{1, 2, 3, 4};

    SECTION("stores the state it was given") {
        const FastRNG rng(state);
        CHECK(rng.state() == state);
    }

    SECTION("a copy has the same state") {
        const FastRNG original(state);
        const FastRNG copy = original;
        CHECK(copy.state() == original.state());
    }

    SECTION("the all-zero state is rejected") {
        CHECK_THROWS_AS(FastRNG({0, 0, 0, 0}), std::invalid_argument);
    }

    SECTION("a single non-zero bit is enough") {
        CHECK_NOTHROW(FastRNG({0, 0, 0, 1}));
        CHECK_NOTHROW(FastRNG({0x8000000000000000ULL, 0, 0, 0}));
    }
}

TEST_CASE("FastRNG: next()", "[fast]") {
    FastRNG rng({1, 2, 3, 4});

    SECTION("reproduces the reference implementation") {
        // Outputs of the authors' C implementation (xoshiro256starstar.c) from the same state
        CHECK(rng.next() == 0x0000000000002d00ULL);
        CHECK(rng.next() == 0x0000000000000000ULL);
        CHECK(rng.next() == 0x000000005a007080ULL);
        CHECK(rng.next() == 0x10e0000000009d80ULL);
        CHECK(rng.next() == 0x10e0b61ce1009d80ULL);
        CHECK(rng.next() == 0x0870021ce143ad00ULL);
    }

    SECTION("advances the state as the algorithm describes") {
        // Work this out by hand before reading the expected value
        (void)rng.next();
        const FastRNG::State expected{7, 0, 0x40002, 0x0000c00000000000ULL};
        CHECK(rng.state() == expected);
    }

    SECTION("a copy continues the same sequence") {
        (void)rng.next();
        FastRNG copy = rng;
        for (int i = 0; i < 1000; ++i)
            REQUIRE(copy.next() == rng.next());
    }

    SECTION("a difference in the last word takes two outputs to show") {
        // The first output depends only on s[1], and the second one does not see s[3] yet.
        // So two states that differ only in s[3] start with the same two outputs, and only
        // then diverge. This is why the state should come from a well-mixed seed.
        FastRNG other({1, 2, 3, 5});
        CHECK(other.next() == rng.next());
        CHECK(other.next() == rng.next());

        int equal = 0;
        for (int i = 0; i < 1000; ++i)
            equal += (other.next() == rng.next());
        CHECK(equal == 0);
    }
}

TEST_CASE("FastRNG: seeding", "[fast]") {
    SECTION("the state is the first four SplitMix64 outputs") {
        const FastRNG rng(0);
        const FastRNG::State expected{0xe220a8397b1dcdafULL, 0x6e789e6aa1b965f4ULL,
                                      0x06c45d188009454fULL, 0xf88bb8a8724c81ecULL};
        CHECK(rng.state() == expected);
    }

    SECTION("reference sequence for seed 0") {
        FastRNG rng(0);
        CHECK(rng.next() == 0x99ec5f36cb75f2b4ULL);
        CHECK(rng.next() == 0xbf6e1f784956452aULL);
        CHECK(rng.next() == 0x1a5f849d4933e6e0ULL);
        CHECK(rng.next() == 0x6aa594f1262d2d2cULL);
        CHECK(rng.next() == 0xbba5ad4a1f842e59ULL);
    }

    SECTION("reference sequence for seed 42") {
        FastRNG rng(42);
        CHECK(rng.next() == 0x15780b2e0c2ec716ULL);
        CHECK(rng.next() == 0x6104d9866d113a7eULL);
        CHECK(rng.next() == 0xae17533239e499a1ULL);
        CHECK(rng.next() == 0xecb8ad4703b360a1ULL);
        CHECK(rng.next() == 0xfde6dc7fe2ec5e64ULL);
    }

    SECTION("the same seed gives the same sequence") {
        FastRNG a(2026), b(2026);
        for (int i = 0; i < 1000; ++i)
            REQUIRE(a.next() == b.next());
    }

    SECTION("neighbouring seeds differ from the very first output") {
        // Compare with "a difference in the last word takes two outputs to show": seeds 1
        // and 2 differ in one bit, but SplitMix64 spreads that difference over all 256 bits
        FastRNG a(1), b(2);
        int equal = 0;
        for (int i = 0; i < 1000; ++i)
            equal += (a.next() == b.next());
        CHECK(equal == 0);
    }
}
