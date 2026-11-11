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
/// Because the key changes at every refill, someone who later reads the generator's memory
/// cannot recompute numbers that were already handed out ("backtracking resistance").
///
/// It takes the key explicitly, so it is deterministic and can be tested with known values.
/// CryptoRNG uses it with a key from the operating system; users never pick the key.
class ChaCha20Generator {
public:
    static constexpr std::size_t blocks_per_refill = 16;
    static constexpr std::size_t buffer_size = blocks_per_refill * 64;        // 1024 bytes
    static constexpr std::size_t outputs_per_refill = (buffer_size - 32) / 8; // 124

    explicit ChaCha20Generator(const chacha20::Key &key) noexcept;

    /// Wipes the key and the buffer before the memory is released.
    ~ChaCha20Generator();

    // Key material must never be duplicated
    ChaCha20Generator(const ChaCha20Generator &) = delete;
    ChaCha20Generator &operator=(const ChaCha20Generator &) = delete;

    /// Returns the next 64 bits, refilling the buffer when it is used up.
    std::uint64_t next() noexcept;

    /// Replaces the key and throws away (wipes) everything left in the buffer.
    void rekey(const chacha20::Key &key) noexcept;

private:
    void refill() noexcept;

    chacha20::Key m_key;
    std::array<std::uint8_t, buffer_size> m_buffer{};
    std::size_t m_position = buffer_size; // "empty" buffer: the first next() fills it
};

} // namespace alelus::detail
