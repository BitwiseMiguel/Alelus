#pragma once

#include <bit>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <random>
#include <ranges>
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

/// Uniform real in [0, 1).
///
/// A double has 53 bits of precision, so take the top 53 random bits and scale them by
/// 2^-53. The possible results are k / 2^53 for k = 0 ... 2^53 - 1: evenly spaced and all
/// equally likely. 1.0 itself can never come out.
template <Random64 Rng>
double uniform_real(Rng &rng) {
    return static_cast<double>(rng() >> 11) * 0x1.0p-53;
}

/// Uniform real in [lo, hi). Throws std::invalid_argument unless lo < hi, both finite.
template <Random64 Rng>
double uniform_real(Rng &rng, double lo, double hi) {
    if (!std::isfinite(lo) || !std::isfinite(hi) || !(lo < hi))
        throw std::invalid_argument("uniform_real: requires lo < hi, both finite");

    const double span = hi - lo;
    double result;
    do {
        if (std::isfinite(span)) {
            result = lo + uniform_real(rng) * span;
        } else {
            // hi - lo overflowed to infinity (for example [-DBL_MAX, DBL_MAX)). Work at half
            // scale: multiplying by 0.5 and by 2 is exact, so the result is the same.
            result = 2.0 * (0.5 * lo + uniform_real(rng) * (0.5 * hi - 0.5 * lo));
        }
        // Rounding can make lo + u * span come out as exactly hi. Draw again instead of
        // clamping, so that no value receives extra probability.
    } while (result >= hi);
    return result;
}

/// True with probability 1/2. Uses only the top bit, which is as random as any other.
template <Random64 Rng>
bool bernoulli(Rng &rng) {
    return (rng() >> 63) != 0;
}

/// True with probability p, for p in [0, 1].
///
/// uniform_real() is in [0, 1), so it falls below p with probability p. The edges need no
/// special case: nothing is below 0 (p = 0 is never true), and uniform_real() never
/// reaches 1 (p = 1 is always true).
/// Throws std::invalid_argument if p is outside [0, 1] or is NaN.
template <Random64 Rng>
bool bernoulli(Rng &rng, double p) {
    // Written as !(inside) on purpose: every comparison with NaN is false, so NaN is caught
    // here; "p < 0.0 || p > 1.0" would let NaN through.
    if (!(p >= 0.0 && p <= 1.0))
        throw std::invalid_argument("bernoulli: p must be in [0, 1]");
    return uniform_real(rng) < p;
}

/// True with probability EXACTLY k / n, with no floating point involved: a uniform integer
/// in [0, n) is below k in exactly k of the n cases. 1.0 / 3 is not exactly 1/3 as a double;
/// bernoulli(rng, 1, 3) is.
/// Throws std::invalid_argument unless n > 0 and 0 <= k <= n.
template <Random64 Rng, std::integral K, std::integral N>
bool bernoulli(Rng &rng, K k, N n) {
    // std::cmp_* compare the values correctly even when K and N mix signed and unsigned
    if (std::cmp_less_equal(n, 0) || std::cmp_less(k, 0) || std::cmp_greater(k, n))
        throw std::invalid_argument("bernoulli: requires n > 0 and 0 <= k <= n");
    return uniform_below(rng, static_cast<std::uint64_t>(n)) < static_cast<std::uint64_t>(k);
}

/// Shuffles a sequence in place: every one of the n! orders is equally likely.
///
/// Algorithm: Fisher-Yates, from the end to the start. Position i - 1 receives an element
/// chosen uniformly among positions 0..i - 1 (itself included), and is then left alone.
/// That gives n * (n - 1) * ... * 1 = n! equally likely paths, one per order.
template <Random64 Rng, std::ranges::random_access_range R>
    requires std::permutable<std::ranges::iterator_t<R>>
void shuffle(Rng &rng, R &&range) {
    const auto first = std::ranges::begin(range);
    const auto size = static_cast<std::uint64_t>(std::ranges::distance(range));
    using Diff = std::ranges::range_difference_t<R>;
    for (std::uint64_t i = size; i > 1; --i) {
        const std::uint64_t j = uniform_below(rng, i); // in [0, i - 1]
        std::ranges::iter_swap(first + static_cast<Diff>(i - 1), first + static_cast<Diff>(j));
    }
}

/// Picks an index with probability proportional to its integer weight:
///     P(i) = weights[i] / (sum of all weights)
/// It is exact: only integers are involved, so {1, 2} gives exactly 1/3 and 2/3.
///
/// Algorithm: draw r uniformly in [0, total), then walk the weights subtracting each one;
/// the index where r falls below the current weight is the answer. Each index owns a block
/// of exactly weights[i] values of r.
///
/// Throws std::invalid_argument if a weight is negative or the total is 0 (including an
/// empty list), and std::overflow_error if the total does not fit in 64 bits.
template <Random64 Rng, std::ranges::forward_range R>
    requires std::integral<std::ranges::range_value_t<R>> &&
             (!std::same_as<std::ranges::range_value_t<R>, bool>)
std::size_t weighted_index(Rng &rng, const R &weights) {
    std::uint64_t total = 0;
    for (const auto w : weights) {
        if (std::cmp_less(w, 0))
            throw std::invalid_argument("weighted_index: weights must not be negative");
        const auto weight = static_cast<std::uint64_t>(w);
        if (weight > std::numeric_limits<std::uint64_t>::max() - total)
            throw std::overflow_error(
                "weighted_index: the sum of the weights does not fit in 64 bits");
        total += weight;
    }
    if (total == 0)
        throw std::invalid_argument(
            "weighted_index: the sum of the weights must be greater than zero");

    std::uint64_t r = uniform_below(rng, total);
    std::size_t index = 0;
    for (const auto w : weights) {
        const auto weight = static_cast<std::uint64_t>(w);
        if (r < weight)
            return index;
        r -= weight;
        ++index;
    }
    return index; // never reached: r < total guarantees a return inside the loop
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
