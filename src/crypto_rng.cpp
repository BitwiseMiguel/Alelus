#include "alelus/crypto_rng.hpp"

#include "alelus/detail/secure_zero.hpp"
#include "alelus/os_entropy.hpp"

namespace alelus {

CryptoRNG::CryptoRNG() : m_generator(detail::chacha20::Key{}) {
    // The generator starts with a zero key, but it never uses it: its buffer starts empty,
    // and rekey() below replaces the key before the first number is produced
    detail::chacha20::Key key{};
    os_random_bytes(key);
    m_generator.rekey(key);
    detail::secure_zero(key.data(), key.size()); // do not leave a copy of the key on the stack
}

CryptoRNG::result_type CryptoRNG::operator()() noexcept {
    return m_generator.next();
}

} // namespace alelus
