#pragma once

#include <cstdint>

namespace alelus::detail {

/// The 128-bit result of a 64 x 64 multiplication, as two 64-bit halves.
struct U128 {
    std::uint64_t hi;
    std::uint64_t lo;
    friend constexpr bool operator==(const U128 &, const U128 &) = default;
};

/// Portable version: splits each number into 32-bit halves and multiplies them like long
/// multiplication by hand, with 32-bit "digits", adding the partial products with carries.
constexpr U128 mul_64x64_portable(std::uint64_t a, std::uint64_t b) noexcept {
    const std::uint64_t a_lo = a & 0xFFFFFFFFu, a_hi = a >> 32;
    const std::uint64_t b_lo = b & 0xFFFFFFFFu, b_hi = b >> 32;

    const std::uint64_t lo_lo = a_lo * b_lo;
    const std::uint64_t hi_lo = a_hi * b_lo;
    const std::uint64_t lo_hi = a_lo * b_hi;
    const std::uint64_t hi_hi = a_hi * b_hi;

    // The middle column is at most (2^32 - 1)^2 + 2 * (2^32 - 1) = 2^64 - 1: it never overflows
    const std::uint64_t middle = (lo_lo >> 32) + (hi_lo & 0xFFFFFFFFu) + lo_hi;
    const std::uint64_t hi = hi_hi + (hi_lo >> 32) + (middle >> 32);
    const std::uint64_t lo = (middle << 32) | (lo_lo & 0xFFFFFFFFu);
    return {hi, lo};
}

/// 64 x 64 -> 128-bit multiplication. GCC and Clang (MinGW included) have a 128-bit integer
/// type, and the processor does this in one instruction; other compilers use the portable
/// version.
constexpr U128 mul_64x64(std::uint64_t a, std::uint64_t b) noexcept {
#if defined(__SIZEOF_INT128__)
    // __extension__ silences -Wpedantic: __int128 is not part of standard C++
    __extension__ using uint128 = unsigned __int128;
    const uint128 product = static_cast<uint128>(a) * b;
    return {static_cast<std::uint64_t>(product >> 64), static_cast<std::uint64_t>(product)};
#else
    return mul_64x64_portable(a, b);
#endif
}

} // namespace alelus::detail
