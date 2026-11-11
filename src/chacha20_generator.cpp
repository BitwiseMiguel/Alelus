#include "alelus/detail/chacha20_generator.hpp"

#include <algorithm>

namespace alelus::detail {

ChaCha20Generator::ChaCha20Generator(const chacha20::Key &key) noexcept : m_key(key) {
}

std::uint64_t ChaCha20Generator::next() noexcept {
    if (m_position >= buffer_size)
        refill();

    // 8 bytes -> one 64-bit number, little-endian (byte 0 is the least significant)
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < 8; ++i)
        value |= static_cast<std::uint64_t>(m_buffer[m_position + i]) << (8 * i);
    m_position += 8;
    return value;
}

void ChaCha20Generator::refill() noexcept {
    // The key never repeats (it changes at every refill), so a fixed nonce is safe
    const chacha20::Nonce nonce{};
    for (std::size_t b = 0; b < blocks_per_refill; ++b) {
        const chacha20::Block block = chacha20::block(m_key, static_cast<std::uint32_t>(b), nonce);
        std::copy(block.begin(), block.end(),
                  m_buffer.begin() + static_cast<std::ptrdiff_t>(64 * b));
    }

    // The first 32 bytes become the new key, and are never handed out as numbers
    std::copy(m_buffer.begin(), m_buffer.begin() + 32, m_key.begin());
    m_position = 32;
}

} // namespace alelus::detail
