#pragma once

#include <cstdint>

namespace alelus {

/// SplitMix64 (Steele, Lea & Flood, 2014): a tiny generator with very good bit mixing.
/// Alelus uses it to turn a 64-bit seed into the 256-bit state of FastRNG, as recommended
/// by the authors of xoshiro256**.
class SplitMix64 {
public:
    explicit SplitMix64(std::uint64_t seed) noexcept : m_state(seed) {
    }

    /// Returns the next 64 bits and advances the state.
    std::uint64_t next() noexcept {
        // A counter: the constant is the fractional part of the golden ratio times 2^64.
        // It is odd, so the counter visits all 2^64 values before repeating.
        m_state += 0x9e3779b97f4a7c15ULL;

        // The mixing function: every input bit affects about half of the output bits
        std::uint64_t z = m_state;
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31);
    }

private:
    std::uint64_t m_state;
};

} // namespace alelus
