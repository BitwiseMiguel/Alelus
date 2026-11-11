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
