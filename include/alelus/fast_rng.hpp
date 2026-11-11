#pragma once

#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <stdexcept>

#include "alelus/splitmix64.hpp"

namespace alelus {

/// Fast generator, reproducible from its state. Not suitable for cryptography.
/// Algorithm: xoshiro256** 1.0 (Blackman & Vigna, 2018), https://prng.di.unimi.it/
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

    /// Two generators are equal if their states are equal, that is, if they will produce
    /// exactly the same sequence. "= default" asks the compiler to compare every member.
    friend bool operator==(const FastRNG &, const FastRNG &) = default;

    /// The current state.
    [[nodiscard]] const State &state() const noexcept {
        return m_state;
    }

private:
    static State expand_seed(std::uint64_t seed) noexcept {
        SplitMix64 sm(seed);
        // Inside braces, C++ guarantees left-to-right evaluation: the four calls happen in order
        return State{sm.next(), sm.next(), sm.next(), sm.next()};
    }

    State m_state;
};

} // namespace alelus
