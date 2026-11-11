#include "alelus/crypto_rng.hpp"

#include <exception>

#include "alelus/detail/fork_guard.hpp"
#include "alelus/detail/secure_zero.hpp"
#include "alelus/os_entropy.hpp"

namespace alelus {

CryptoRNG::CryptoRNG() : m_generator(detail::chacha20::Key{}) {
    // The generator starts with a zero key, but it never uses it: its buffer starts empty,
    // and rekey_from_os() below replaces the key before the first number is produced.
    //
    // The fork handler must be installed BEFORE the key is read: a fork() between the two
    // would otherwise go unnoticed.
    detail::install_fork_handler();
    rekey_from_os();
}

void CryptoRNG::rekey_from_os() {
    m_fork_generation = detail::fork_generation();
    detail::chacha20::Key key{};
    os_random_bytes(key);
    m_generator.rekey(key);
    detail::secure_zero(key.data(), key.size()); // do not leave a copy of the key on the stack
}

CryptoRNG::result_type CryptoRNG::operator()() noexcept {
    if (detail::fork_generation() != m_fork_generation) [[unlikely]] {
        try {
            rekey_from_os();
        } catch (...) {
            // Carrying on would make the child repeat the parent's numbers. For a
            // cryptographic generator, stopping the program is the only safe option.
            std::terminate();
        }
    }
    return m_generator.next();
}

} // namespace alelus
