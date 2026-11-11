#include "alelus/detail/chacha20_generator.hpp"

#include "alelus/detail/secure_zero.hpp"

namespace alelus::detail {

ChaCha20Generator::ChaCha20Generator(const chacha20::Key &key) noexcept : m_key(key) {
}

ChaCha20Generator::~ChaCha20Generator() {
    secure_zero(m_key.data(), m_key.size());
    secure_zero(m_words.data(), sizeof(m_words));
}

void ChaCha20Generator::rekey(const chacha20::Key &key) noexcept {
    secure_zero(m_words.data(), sizeof(m_words));
    m_position = buffer_words; // empty: the next call refills with the new key
    m_key = key;
}

void ChaCha20Generator::refill() noexcept {
    // The key never repeats (it changes at every refill), so a fixed nonce is safe
    const chacha20::Nonce nonce{};

    // Each block goes straight into its 8 places in the buffer: no temporary copy
    for (std::size_t b = 0; b < blocks_per_refill; ++b) {
        const std::span<std::uint64_t, 8> slot(m_words.data() + 8 * b, 8);
        chacha20::block_words(m_key, static_cast<std::uint32_t>(b), nonce, slot);
    }

    // The first 4 numbers (32 bytes) become the new key, and are never handed out.
    // Their bytes, little-endian, are exactly the first 32 bytes of the keystream.
    for (std::size_t i = 0; i < key_words; ++i)
        for (std::size_t j = 0; j < 8; ++j)
            m_key[8 * i + j] = static_cast<std::uint8_t>(m_words[i] >> (8 * j));
    secure_zero(m_words.data(), key_words * sizeof(std::uint64_t));
    m_position = key_words;
}

} // namespace alelus::detail
