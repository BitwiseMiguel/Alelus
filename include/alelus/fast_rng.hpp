#pragma once

#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <stdexcept>

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
