#include "alelus/detail/secure_zero.hpp"

#include <atomic>
#include <cstdint>
#include <cstring>

#if defined(_MSC_VER)
#include <windows.h>
#endif

namespace alelus::detail {

void secure_zero(void *data, std::size_t size) noexcept {
#if defined(__GNUC__) || defined(__clang__)
    std::memset(data, 0, size);
    // An empty assembly statement that "uses" the pointer and "touches" memory: the compiler
    // must assume the zeros are read, so it cannot delete the memset above
    __asm__ __volatile__("" : : "r"(data) : "memory");
#elif defined(_MSC_VER)
    SecureZeroMemory(data, size); // documented for exactly this purpose
#else
    // Every write through a volatile pointer must be performed
    volatile std::uint8_t *p = static_cast<volatile std::uint8_t *>(data);
    while (size--)
        *p++ = 0;
    std::atomic_signal_fence(std::memory_order_seq_cst);
#endif
}

} // namespace alelus::detail
