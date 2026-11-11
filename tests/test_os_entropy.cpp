#include "catch.hpp"

#include "alelus/os_entropy.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <vector>

using alelus::os_random_bytes;

TEST_CASE("os_random_bytes", "[entropy]") {
    // These checks are NOT deterministic: the bytes are real entropy. That is why the
    // margins are huge: each check fails by chance with probability far below 1 in a billion.

    SECTION("asking for zero bytes works") {
        std::vector<std::uint8_t> empty;
        CHECK_NOTHROW(os_random_bytes(empty));
    }

    SECTION("the bytes are not all zero") {
        std::vector<std::uint8_t> buffer(4096, 0);
        os_random_bytes(buffer);
        CHECK(std::any_of(buffer.begin(), buffer.end(), [](std::uint8_t b) { return b != 0; }));
    }

    SECTION("two calls give different bytes") {
        std::vector<std::uint8_t> a(64), b(64);
        os_random_bytes(a);
        os_random_bytes(b);
        CHECK(a != b);
    }

    SECTION("a large request is filled, with half of the bits set") {
        // 1 MB: more than 256 bytes, so on macOS/BSD the code must make several calls
        std::vector<std::uint8_t> buffer(1 << 20);
        os_random_bytes(buffer);

        // Each bit is 1 with probability 1/2. Over 8 million bits, the standard deviation
        // of the fraction is sqrt(0.25 / 8M) ~ 0.00018; 0.0011 is about 6 deviations.
        std::uint64_t ones = 0;
        for (const std::uint8_t byte : buffer)
            ones += static_cast<std::uint64_t>(std::popcount(byte));
        const double fraction =
            static_cast<double>(ones) / (8.0 * static_cast<double>(buffer.size()));
        CHECK(fraction == Approx(0.5).margin(0.0011));
    }
}
