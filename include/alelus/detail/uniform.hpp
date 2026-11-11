#pragma once

#include <bit>
#include <concepts>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <type_traits>
#include <utility>

// Internal: the algorithms behind the methods of FastRNG and CryptoRNG. They are written
// once here, so both classes share the same tested code.
namespace alelus::detail {

/// A generator that returns 64 uniform random bits per call (FastRNG, CryptoRNG, ...).
template <class Rng>
concept Random64 = std::uniform_random_bit_generator<Rng> &&
                   std::same_as<typename Rng::result_type, std::uint64_t> && (Rng::min() == 0) &&
                   (Rng::max() == std::numeric_limits<std::uint64_t>::max());

/// Uniform integer in [0, n), with no modulo bias.
///
/// Algorithm: bitmask with rejection. Keep only as many low bits as n - 1 needs; if the
/// result is still too large, throw it away and draw again. Every accepted value had the
/// same chance, so the result is exactly uniform. At most half of the draws are rejected,
/// so on average it needs fewer than 2 draws.
template <Random64 Rng>
std::uint64_t uniform_below(Rng &rng, std::uint64_t n) {
    if (n == 0)
        throw std::invalid_argument("uniform_below: n must be greater than zero");

    const std::uint64_t max_value = n - 1;
    // A mask with as many bits as max_value. The "| 1" avoids countl_zero(0) == 64, which
    // would make the shift undefined.
    const std::uint64_t mask = ~std::uint64_t{0} >> std::countl_zero(max_value | 1);

    std::uint64_t x;
    do {
        x = rng() & mask;
    } while (x > max_value);
    return x;
}

/// Uniform integer in the closed interval [lo, hi], for any integer type except bool.
template <std::integral T, Random64 Rng>
    requires(!std::same_as<T, bool>)
T uniform_int(Rng &rng, T lo, T hi) {
    if (lo > hi)
        throw std::invalid_argument("uniform_int: lo is greater than hi");

    // Work in the unsigned type of the same size: there, subtraction and addition wrap
    // around in a well-defined way, which also handles negative numbers. The outer cast
    // matters for small types: uint8_t - uint8_t is computed as int and could be negative.
    using U = std::make_unsigned_t<T>;
    const auto span =
        static_cast<std::uint64_t>(static_cast<U>(static_cast<U>(hi) - static_cast<U>(lo)));

    // Full 64-bit range: span + 1 would overflow to 0. Every 64-bit value is valid.
    if (span == std::numeric_limits<std::uint64_t>::max())
        return static_cast<T>(rng());

    const std::uint64_t offset = uniform_below(rng, span + 1);
    return static_cast<T>(static_cast<U>(static_cast<U>(lo) + static_cast<U>(offset)));
}

/// Marks "no result type was given": use the common type of the two arguments.
struct DeduceType {};

/// The result type of uniform_int(lo, hi): the type given explicitly, or else the common
/// type of A and B (the "larger" one, following the usual C++ rules).
template <class R, class A, class B>
using UniformIntResult =
    std::conditional_t<std::same_as<R, DeduceType>, std::common_type_t<A, B>, R>;

/// Uniform integer in [lo, hi] when lo and hi may have different types.
///
/// Both are converted to the result type, but only after checking that they fit:
/// std::in_range compares the VALUES, unlike a plain conversion. Without the check,
/// uniform_int(-1, 5u) would turn -1 into 4294967295, silently.
/// Throws std::out_of_range if lo or hi does not fit in the result type.
template <class R, Random64 Rng, std::integral A, std::integral B>
UniformIntResult<R, A, B> uniform_int_mixed(Rng &rng, A lo, B hi) {
    using T = UniformIntResult<R, A, B>;
    static_assert(std::integral<T> && !std::same_as<T, bool>,
                  "uniform_int: the result type must be an integer type");
    if (!std::in_range<T>(lo) || !std::in_range<T>(hi))
        throw std::out_of_range("uniform_int: lo or hi does not fit in the result type");
    return uniform_int(rng, static_cast<T>(lo), static_cast<T>(hi));
}

} // namespace alelus::detail
