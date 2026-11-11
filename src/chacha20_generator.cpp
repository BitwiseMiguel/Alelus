#include "alelus/detail/chacha20_generator.hpp"

#include <algorithm>

#include "alelus/detail/secure_zero.hpp"

namespace alelus::detail {

ChaCha20Generator::ChaCha20Generator(const chacha20::Key &key) noexcept : m_key(key) {
}

ChaCha20Generator::~ChaCha20Generator() {
    secure_zero(m_key.data(), m_key.size());
    secure_zero(m_buffer.data(), m_buffer.size());
}

void ChaCha20Generator::rekey(const chacha20::Key &key) noexcept {
    secure_zero(m_buffer.data(), m_buffer.size());
    m_position = buffer_size; // empty: the next call refills with the new key
    m_key = key;
}

std::uint64_t ChaCha20Generator::next() noexcept {
    if (m_position >= buffer_size)
        refill();

    // 8 bytes -> one 64-bit number, little-endian (byte 0 is the least significant)
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < 8; ++i)
        value |= static_cast<std::uint64_t>(m_buffer[m_position + i]) << (8 * i);

    // Wipe those 8 bytes at once: a later look at the memory must not reveal numbers that
    // were already handed out
    secure_zero(m_buffer.data() + m_position, 8);
    m_position += 8;
    return value;
}

void ChaCha20Generator::refill() noexcept {
    // The key never repeats (it changes at every refill), so a fixed nonce is safe
    const chacha20::Nonce nonce{};
    for (std::size_t b = 0; b < blocks_per_refill; ++b) {
        chacha20::Block block = chacha20::block(m_key, static_cast<std::uint32_t>(b), nonce);
        std::copy(block.begin(), block.end(),
                  m_buffer.begin() + static_cast<std::ptrdiff_t>(64 * b));
        secure_zero(block.data(), block.size()); // the temporary copy on the stack, too
    }

    // The first 32 bytes become the new key, and are never handed out as numbers
    std::copy(m_buffer.begin(), m_buffer.begin() + 32, m_key.begin());
    secure_zero(m_buffer.data(), 32);
    m_position = 32;
}

} // namespace alelus::detail
