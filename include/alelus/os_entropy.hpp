#pragma once

#include <cstdint>
#include <span>

namespace alelus {

/// Fills the buffer with cryptographically secure random bytes from the operating system:
///   Linux: getrandom()   macOS and BSD: getentropy()   Windows: BCryptGenRandom()
/// Throws std::runtime_error if the operating system cannot provide them. There is
/// deliberately no weaker fallback (clock, addresses, std::random_device).
void os_random_bytes(std::span<std::uint8_t> out);

} // namespace alelus
