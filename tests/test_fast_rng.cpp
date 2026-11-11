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
