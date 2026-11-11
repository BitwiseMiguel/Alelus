#pragma once

#include <array>
#include <bit>
#include <cstdint>
#include <span>

// Internal: the building block of CryptoRNG. Not part of the public API.
namespace alelus::detail::chacha20 {

using Key = std::array<std::uint8_t, 32>;   // 256 bits
using Nonce = std::array<std::uint8_t, 12>; // 96 bits
using Block = std::array<std::uint8_t, 64>; // 512 bits of keystream

/// The ChaCha "quarter round" (RFC 8439, section 2.1): mixes four 32-bit words using only
/// additions, rotations and XORs ("ARX").
///
/// Defined in the header so the compiler can put it inside the block function: it is tiny
/// and runs 80 times per block, so a call each time costs more than the work itself. When
/// it lived in the .cpp, g++ -O2 kept it as a separate function. always_inline turns
/// "the optimiser usually inlines it" into a guarantee.
[[gnu::always_inline]] inline void quarter_round(std::uint32_t &a, std::uint32_t &b,
                                                 std::uint32_t &c, std::uint32_t &d) noexcept {
    // Laid out exactly as in RFC 8439, section 2.1, so the two are easy to compare
    // clang-format off
    a += b; d ^= a; d = std::rotl(d, 16);
    c += d; b ^= c; b = std::rotl(b, 12);
    a += b; d ^= a; d = std::rotl(d, 8);
    c += d; b ^= c; b = std::rotl(b, 7);
    // clang-format on
}

/// The ChaCha20 block function (RFC 8439, section 2.3): 20 rounds over a 16-word state made
/// of constants, key, block counter and nonce. Returns 64 bytes of keystream, little-endian.
[[nodiscard]] Block block(const Key &key, std::uint32_t counter, const Nonce &nonce) noexcept;

/// The same block, written straight into 8 numbers of 64 bits. Number i is bytes 8i..8i+7
/// of block() read as little-endian, so both give the same bits on any processor.
void block_words(const Key &key, std::uint32_t counter, const Nonce &nonce,
                 std::span<std::uint64_t, 8> out) noexcept;

} // namespace alelus::detail::chacha20
