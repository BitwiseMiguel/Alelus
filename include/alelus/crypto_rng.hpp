#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <ranges>
#include <stdexcept>

#include "alelus/detail/chacha20_generator.hpp"
#include "alelus/detail/uniform.hpp"
#include "alelus/weighted_sampler.hpp"

namespace alelus {

/// Cryptographically secure generator: ChaCha20 with "fast-key-erasure" (RFC 8439,
/// D. J. Bernstein 2017), keyed from the operating system.
///
/// There is no seed and no way to choose the key: the sequence can never be repeated or
/// predicted. Slower than FastRNG; use it when unpredictability matters (for example, the
/// random numbers of a real game).
class CryptoRNG {
public:
    // --- std::uniform_random_bit_generator (the same interface as FastRNG) ---------------
    using result_type = std::uint64_t;
    static constexpr result_type min() noexcept {
        return 0;
    }
    static constexpr result_type max() noexcept {
        return std::numeric_limits<result_type>::max();
    }
    /// Returns the next 64 bits. If the process was duplicated with fork() since the last
    /// call, gets a new key from the operating system first: otherwise parent and child would
    /// produce the same numbers.
    result_type operator()() noexcept;
    // -------------------------------------------------------------------------------------

    /// Gets a 256-bit key from the operating system.
    /// Throws std::runtime_error if the operating system cannot provide it.
    CryptoRNG();

    /// Uniform integer in [lo, hi], both included, with no modulo bias.
    /// The result has the type of the arguments: rng.uniform_int(1, 6) returns an int.
    /// If they differ, the larger common type: rng.uniform_int(1, v.size()) returns a size_t.
    /// A type can also be given: rng.uniform_int<std::int16_t>(1, 6).
    /// Throws std::invalid_argument if lo > hi, and std::out_of_range if lo or hi does not
    /// fit in the result type (for example uniform_int(-1, 5u), with a negative unsigned).
    template <class R = detail::DeduceType, std::integral A, std::integral B>
        requires(!std::same_as<A, bool> && !std::same_as<B, bool>)
    detail::UniformIntResult<R, A, B> uniform_int(A lo, B hi) {
        return detail::uniform_int_mixed<R>(*this, lo, hi);
    }

    /// Uniform integer in [0, n): n itself is NOT included. rng.uniform_int(6) returns 0 to 5,
    /// and rng.uniform_int(v.size()) returns a valid index of v.
    /// Throws std::invalid_argument if n <= 0 (there would be no value to return).
    template <std::integral T>
        requires(!std::same_as<T, bool>)
    T uniform_int(T n) {
        if (n <= 0)
            throw std::invalid_argument("uniform_int: n must be greater than zero");
        // The cast matters for small types: n - 1 is computed as int
        return detail::uniform_int(*this, T{0}, static_cast<T>(n - 1));
    }

    /// Uniform integer over every value of T. By default T is std::uint64_t (64 random bits);
    /// rng.uniform_int<int>() covers every int, negative ones included.
    template <std::integral T = std::uint64_t>
        requires(!std::same_as<T, bool>)
    T uniform_int() {
        return detail::uniform_int(*this, std::numeric_limits<T>::min(),
                                   std::numeric_limits<T>::max());
    }

    /// Uniform real in [0, 1): 0 can come out, 1 never does.
    double uniform_real() {
        return detail::uniform_real(*this);
    }

    /// Uniform real in [0, b): b itself never comes out.
    /// Throws std::invalid_argument unless b is positive and finite.
    double uniform_real(double b) {
        return detail::uniform_real(*this, 0.0, b);
    }

    /// Uniform real in [a, b): b itself never comes out.
    /// Throws std::invalid_argument unless a < b, both finite.
    double uniform_real(double a, double b) {
        return detail::uniform_real(*this, a, b);
    }

    /// True with probability 1/2, like a fair coin.
    bool bernoulli() {
        return detail::bernoulli(*this);
    }

    /// True with probability p (for example 0.02 for 2%).
    /// Throws std::invalid_argument if p is outside [0, 1] or is NaN.
    bool bernoulli(double p) {
        return detail::bernoulli(*this, p);
    }

    /// True with probability exactly k / n (for example bernoulli(1, 3) for exactly 1/3),
    /// with no floating-point rounding.
    /// Throws std::invalid_argument unless n > 0 and 0 <= k <= n.
    template <std::integral K, std::integral N>
        requires(!std::same_as<K, bool> && !std::same_as<N, bool>)
    bool bernoulli(K k, N n) {
        return detail::bernoulli(*this, k, n);
    }

    /// Shuffles a sequence in place (std::vector, std::array, a C array, ...): every order is
    /// equally likely. Unlike std::shuffle, the result for a given seed is the same on every
    /// compiler and operating system.
    ///
    /// To shuffle only part of a vector, pass a std::span over that part. Only those positions
    /// change, and only among themselves:
    ///     rng.shuffle(std::span(v).subspan(2));     // from the 3rd element to the end
    ///     rng.shuffle(std::span(v).subspan(2, 5));  // 5 elements, starting at the 3rd
    template <std::ranges::random_access_range R>
        requires std::permutable<std::ranges::iterator_t<R>>
    void shuffle(R &&range) {
        detail::shuffle(*this, range);
    }

    /// Picks an index with probability proportional to its integer weight, exactly. For
    /// example, with the weights {60, 30, 10} index 0 comes out 60% of the time, index 1 30%
    /// and index 2 10%. Weights can be in any container of integers. A weight of 0 is never
    /// picked.
    /// Throws std::invalid_argument if a weight is negative or they are all 0 (or there are
    /// none), and std::overflow_error if their sum does not fit in 64 bits.
    template <std::ranges::forward_range R>
        requires std::integral<std::ranges::range_value_t<R>> &&
                 (!std::same_as<std::ranges::range_value_t<R>, bool>)
    std::size_t weighted_index(const R &weights) {
        return detail::weighted_index(*this, weights);
    }

    /// Picks an index using weights prepared once in a WeightedSampler: constant time, however
    /// many weights there are. Use it when the same weights are used many times.
    std::size_t weighted_index(const WeightedSampler &sampler) {
        return detail::weighted_index(*this, sampler);
    }

    // Neither copyable nor movable: the secret state lives in exactly one place
    CryptoRNG(const CryptoRNG &) = delete;
    CryptoRNG &operator=(const CryptoRNG &) = delete;

private:
    void rekey_from_os();

    detail::ChaCha20Generator m_generator;
    std::uint64_t m_fork_generation = 0; // the fork counter when the key was last set
};

} // namespace alelus
