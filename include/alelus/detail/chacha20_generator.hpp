#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "alelus/detail/chacha20.hpp"

namespace alelus::detail {

/// Internal: random numbers from ChaCha20 with the "fast-key-erasure" construction
/// (D. J. Bernstein, 2017). Each refill:
///
///   1. generates 16 ChaCha20 blocks (1024 bytes) with the current key, nonce 0 and
///      counters 0..15;
///   2. takes the first 32 bytes as the NEW key: the old key no longer exists anywhere;
///   3. hands out the other 992 bytes as 124 numbers of 64 bits (little-endian).
///
/// The buffer is kept as 64-bit numbers rather than bytes: the blocks are written straight
/// into it, and next() reads one number instead of assembling 8 bytes.
///
/// Because the key changes at every refill, someone who later reads the generator's memory
/// cannot recompute numbers that were already handed out ("backtracking resistance").
///
/// It takes the key explicitly, so it is deterministic and can be tested with known values.
/// CryptoRNG uses it with a key from the operating system; users never pick the key.
class ChaCha20Generator {
public:
    static constexpr std::size_t blocks_per_refill = 16;
    static constexpr std::size_t buffer_words = blocks_per_refill * 8;          // 128 x 64 bits
    static constexpr std::size_t key_words = 4;                                 // 32 bytes
    static constexpr std::size_t outputs_per_refill = buffer_words - key_words; // 124

    explicit ChaCha20Generator(const chacha20::Key &key) noexcept;

    /// Wipes the key and the buffer before the memory is released.
    ~ChaCha20Generator();

    // Key material must never be duplicated
    ChaCha20Generator(const ChaCha20Generator &) = delete;
    ChaCha20Generator &operator=(const ChaCha20Generator &) = delete;

    /// Returns the next 64 bits, refilling the buffer when it is used up.
    std::uint64_t next() noexcept {
        if (m_position >= buffer_words)
            refill();
        const std::uint64_t value = m_words[m_position];
        // Wipe it at once: a later look at the memory must not reveal numbers that were
        // already handed out. The volatile write is one instruction the optimiser is not
        // allowed to remove, so it does the job of secure_zero without a function call.
        static_cast<volatile std::uint64_t &>(m_words[m_position]) = 0;
        ++m_position;
        return value;
    }

    /// Replaces the key and throws away (wipes) everything left in the buffer.
    void rekey(const chacha20::Key &key) noexcept;

private:
    void refill() noexcept;

    chacha20::Key m_key;
    std::array<std::uint64_t, buffer_words> m_words{};
    std::size_t m_position = buffer_words; // "empty" buffer: the first next() fills it
};

} // namespace alelus::detail
