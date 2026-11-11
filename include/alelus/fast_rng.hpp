#pragma once

#include <array>
#include <bit>
#include <concepts>
#include <cstdint>
#include <limits>
#include <ranges>
#include <stdexcept>

#include "alelus/detail/uniform.hpp"
#include "alelus/os_entropy.hpp"
#include "alelus/splitmix64.hpp"

namespace alelus {

/// Fast generator. Not suitable for cryptography.
/// Algorithm: xoshiro256** 1.0 (Blackman & Vigna, 2018), https://prng.di.unimi.it/
///
/// - With a seed, FastRNG(seed): the sequence can be repeated. Use it for simulations
///   that must be reproducible.
/// - Without a seed, FastRNG(): the 256 bits of state come from the operating system and
///   the sequence is different every time. Use it when repeating is not wanted.
class FastRNG {
public:
    /// 256 bits of state, as four 64-bit words.
    using State = std::array<std::uint64_t, 4>;

    // --- std::uniform_random_bit_generator ----------------------------------------
    // These four members are what the standard library requires from a generator.
    // With them, FastRNG works with std::shuffle, std::uniform_int_distribution, etc.

    /// The type of the numbers produced.
    using result_type = std::uint64_t;

    /// The smallest and largest values operator() can return. They must be static and
    /// constexpr: the standard library reads them at compile time, without an object.
    static constexpr result_type min() noexcept {
        return 0;
    }
    static constexpr result_type max() noexcept {
        return std::numeric_limits<result_type>::max();
    }

    /// Same as next(): calling the generator like a function is how the standard library
    /// asks for a number.
    result_type operator()() noexcept {
        return next();
    }
    // -------------------------------------------------------------------------------

    /// Gets all 256 bits of state from the operating system, so the sequence is different
    /// every time. Throws std::runtime_error if the operating system cannot provide them.
    FastRNG() : m_state{} {
        os_random_bytes({reinterpret_cast<std::uint8_t *>(m_state.data()), sizeof(m_state)});

        // 256 random bits are all zero with probability 2^-256: if it happens, the entropy
        // source is broken. Retrying would risk an endless loop, so report it instead.
        if ((m_state[0] | m_state[1] | m_state[2] | m_state[3]) == 0)
            throw std::runtime_error("FastRNG: the operating system returned only zeros");
    }

    /// Starts from the given state.
    /// Throws std::invalid_argument if every word is zero: xoshiro256** would then
    /// return zero forever, so that state is never valid.
    explicit FastRNG(const State &state) : m_state(state) {
        if ((m_state[0] | m_state[1] | m_state[2] | m_state[3]) == 0)
            throw std::invalid_argument("FastRNG: the all-zero state is invalid");
    }

    /// Starts from a 64-bit seed. SplitMix64 expands it into the 256 bits of state, so that
    /// similar seeds (1, 2, 3, ...) still give completely different states.
    /// No zero check is needed: SplitMix64 never returns four zeros in a row.
    explicit FastRNG(std::uint64_t seed) noexcept : m_state(expand_seed(seed)) {
    }

    /// Returns the next 64 random bits and advances the state.
    std::uint64_t next() noexcept {
        // The "**" scrambler: the output is computed from s[1] BEFORE the state changes
        const std::uint64_t result = std::rotl(m_state[1] * 5, 7) * 9;

        // The linear engine: shifts, XORs and one rotation mix the four words
        const std::uint64_t t = m_state[1] << 17;
        m_state[2] ^= m_state[0];
        m_state[3] ^= m_state[1];
        m_state[1] ^= m_state[2];
        m_state[0] ^= m_state[3];
        m_state[2] ^= t;
        m_state[3] = std::rotl(m_state[3], 45);

        return result;
    }

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

    /// Advances n steps without returning anything, like the standard library engines.
    void discard(unsigned long long n) noexcept {
        while (n-- > 0)
            (void)next();
    }

    /// Advances 2^128 steps at once. Typical use: one copy per thread, each one jumped
    /// once more than the previous. The sequences are then 2^128 numbers apart and can
    /// never overlap (FastRNG has a period of 2^256 - 1, so there is room for 2^128 of them).
    void jump() noexcept {
        constexpr State polynomial{0x180ec6d33cfd0abaULL, 0xd5a61266f0c9392cULL,
                                   0xa9582618e03fc9aaULL, 0x39abdc4529b1661cULL};
        apply_jump(polynomial);
    }

    /// Advances 2^192 steps at once: for example one long_jump() per machine, then jump()
    /// for each thread on that machine.
    void long_jump() noexcept {
        constexpr State polynomial{0x76e15d3efefdcbbfULL, 0xc5004e441c522fb3ULL,
                                   0x77710069854ee241ULL, 0x39109bb02acbe635ULL};
        apply_jump(polynomial);
    }

    /// Two generators are equal if their states are equal, that is, if they will produce
    /// exactly the same sequence. "= default" asks the compiler to compare every member.
    friend bool operator==(const FastRNG &, const FastRNG &) = default;

    /// The current state.
    [[nodiscard]] const State &state() const noexcept {
        return m_state;
    }

private:
    /// The state transition only uses XOR, shifts and rotations, so it is LINEAR over GF(2)
    /// (bits, where addition is XOR). Advancing k steps is therefore T^k(s) for a 256x256
    /// bit matrix T, and T^k can be written as a polynomial in T of degree < 256:
    ///     T^k(s) = c0*s + c1*T(s) + c2*T^2(s) + ... + c255*T^255(s)
    /// The jump constants are those 256 coefficients c0..c255 (published by the authors).
    /// So: walk 256 steps, and XOR together the states whose coefficient is 1.
    void apply_jump(const State &polynomial) noexcept {
        State sum{0, 0, 0, 0};
        for (const std::uint64_t word : polynomial) {
            for (int bit = 0; bit < 64; ++bit) {
                if (word & (std::uint64_t{1} << bit)) {
                    for (int i = 0; i < 4; ++i)
                        sum[i] ^= m_state[i];
                }
                (void)next();
            }
        }
        m_state = sum;
    }

    static State expand_seed(std::uint64_t seed) noexcept {
        SplitMix64 sm(seed);
        // Inside braces, C++ guarantees left-to-right evaluation: the four calls happen in order
        return State{sm.next(), sm.next(), sm.next(), sm.next()};
    }

    State m_state;
};

} // namespace alelus
