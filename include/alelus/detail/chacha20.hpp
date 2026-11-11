#pragma once

#include <array>
#include <cstdint>

// Internal: the building block of CryptoRNG. Not part of the public API.
namespace alelus::detail::chacha20 {

using Key = std::array<std::uint8_t, 32>;   // 256 bits
using Nonce = std::array<std::uint8_t, 12>; // 96 bits
using Block = std::array<std::uint8_t, 64>; // 512 bits of keystream

/// The ChaCha "quarter round" (RFC 8439, section 2.1): mixes four 32-bit words using only
/// additions, rotations and XORs ("ARX").
void quarter_round(std::uint32_t &a, std::uint32_t &b, std::uint32_t &c, std::uint32_t &d) noexcept;

/// The ChaCha20 block function (RFC 8439, section 2.3): 20 rounds over a 16-word state made
/// of constants, key, block counter and nonce. Returns 64 bytes of keystream, little-endian.
[[nodiscard]] Block block(const Key &key, std::uint32_t counter, const Nonce &nonce) noexcept;

} // namespace alelus::detail::chacha20
