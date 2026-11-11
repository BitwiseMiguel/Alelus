#pragma once

#include <array>
#include <cstdint>
#include <stdexcept>

namespace alelus {

/// Fast generator, reproducible from its state. Not suitable for cryptography.
/// It will use the xoshiro256** algorithm (Blackman & Vigna, 2018). For now it only
/// stores its state; it does not generate numbers yet.
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

    /// The current state.
    [[nodiscard]] const State &state() const noexcept {
        return m_state;
    }

private:
    State m_state;
};

} // namespace alelus
