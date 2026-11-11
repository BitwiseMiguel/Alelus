#include "catch.hpp"

#include "alelus/detail/mul128.hpp"
#include "alelus/fast_rng.hpp"

using alelus::detail::mul_64x64;
using alelus::detail::mul_64x64_portable;
using alelus::detail::U128;

TEST_CASE("mul_64x64", "[mul128]") {
    SECTION("known products") {
        CHECK(mul_64x64(0, UINT64_MAX) == U128{0, 0});
        CHECK(mul_64x64(1, UINT64_MAX) == U128{0, UINT64_MAX});
        CHECK(mul_64x64(2, 0x8000000000000000ULL) == U128{1, 0});
        // (2^64 - 1)^2 = 2^128 - 2^65 + 1
        CHECK(mul_64x64(UINT64_MAX, UINT64_MAX) == U128{0xFFFFFFFFFFFFFFFEULL, 1});
        // Checked in Python: hex(a * b >> 64), hex(a * b & (2**64 - 1))
        CHECK(mul_64x64(0x123456789abcdefULL, 0xfedcba987654321ULL) ==
              U128{0x00121fa00ad77d74ULL, 0x22236d88fe5618cfULL});
    }

    SECTION("the portable version gives the same result") {
        alelus::FastRNG rng(5);
        for (int i = 0; i < 100000; ++i) {
            const auto a = rng(), b = rng();
            REQUIRE(mul_64x64_portable(a, b) == mul_64x64(a, b));
        }
        STATIC_REQUIRE(mul_64x64_portable(UINT64_MAX, UINT64_MAX) ==
                       U128{0xFFFFFFFFFFFFFFFEULL, 1});
    }
}
