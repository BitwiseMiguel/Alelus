#include "alelus/os_entropy.hpp"

#include <cstddef>
#include <stdexcept>

#if defined(_WIN32)
// windows.h must come before bcrypt.h
// clang-format off
#include <windows.h>
#include <bcrypt.h>
// clang-format on
#elif defined(__linux__)
#include <cerrno>
#include <sys/random.h>
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#include <unistd.h>
#if defined(__APPLE__)
#include <sys/random.h>
#endif
#else
#error "Alelus: no supported operating-system entropy source on this platform"
#endif

namespace alelus {

void os_random_bytes(std::span<std::uint8_t> out) {
    std::uint8_t *p = out.data();
    std::size_t size = out.size();

#if defined(_WIN32)
    while (size > 0) {
        // BCryptGenRandom takes a ULONG length: ask in chunks that fit
        const ULONG chunk = size > 0x7FFFFFFFu ? 0x7FFFFFFFu : static_cast<ULONG>(size);
        if (!BCRYPT_SUCCESS(BCryptGenRandom(nullptr, p, chunk, BCRYPT_USE_SYSTEM_PREFERRED_RNG)))
            throw std::runtime_error("os_random_bytes: BCryptGenRandom failed");
        p += chunk;
        size -= chunk;
    }
#elif defined(__linux__)
    while (size > 0) {
        // getrandom may return fewer bytes than asked, or be interrupted by a signal (EINTR)
        const ssize_t got = getrandom(p, size, 0);
        if (got < 0) {
            if (errno == EINTR)
                continue;
            throw std::runtime_error("os_random_bytes: getrandom failed");
        }
        p += got;
        size -= static_cast<std::size_t>(got);
    }
#else
    while (size > 0) {
        // getentropy accepts at most 256 bytes per call
        const std::size_t chunk = size > 256 ? 256 : size;
        if (getentropy(p, chunk) != 0)
            throw std::runtime_error("os_random_bytes: getentropy failed");
        p += chunk;
        size -= chunk;
    }
#endif
}

} // namespace alelus
