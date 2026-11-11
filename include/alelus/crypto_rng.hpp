#pragma once

#include <cstdint>
#include <limits>

#include "alelus/detail/chacha20_generator.hpp"

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
    result_type operator()() noexcept;
    // -------------------------------------------------------------------------------------

    /// Gets a 256-bit key from the operating system.
    /// Throws std::runtime_error if the operating system cannot provide it.
    CryptoRNG();

    // Neither copyable nor movable: the secret state lives in exactly one place
    CryptoRNG(const CryptoRNG &) = delete;
    CryptoRNG &operator=(const CryptoRNG &) = delete;

private:
    detail::ChaCha20Generator m_generator;
};

} // namespace alelus
