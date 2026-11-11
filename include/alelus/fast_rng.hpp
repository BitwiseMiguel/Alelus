#pragma once

#include <array>
#include <bit>
#include <cstdint>
#include <stdexcept>

#include "alelus/splitmix64.hpp"

namespace alelus {

/// Fast generator, reproducible from its state. Not suitable for cryptography.
/// Algorithm: xoshiro256** 1.0 (Blackman & Vigna, 2018), https://prng.di.unimi.it/
class FastRNG {
public:
    /// 256 bits of state, as four 64-bit words.
    using State = std::array<std::uint64_t, 4>;

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
