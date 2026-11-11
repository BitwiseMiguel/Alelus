// A random password from CryptoRNG. Every character is chosen uniformly from the alphabet,
// with no bias, so each one adds log2(alphabet size) bits of entropy.
//
// This is a job for CryptoRNG, not FastRNG: CryptoRNG has no seed and its key comes from
// the operating system, so nobody can reproduce or predict the password. With FastRNG,
// anyone who learned the seed (or enough outputs) could compute it.

#include <alelus/alelus.hpp>

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <string>
#include <string_view>

int main() {
    constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                          "abcdefghijklmnopqrstuvwxyz"
                                          "0123456789";
    constexpr std::size_t length = 20;

    alelus::CryptoRNG rng;
    std::string password;
    for (std::size_t i = 0; i < length; ++i)
        password += alphabet[rng.uniform_int(alphabet.size())]; // index in [0, size)

    const double bits =
        static_cast<double>(length) * std::log2(static_cast<double>(alphabet.size()));
    std::printf("%s (about %.0f bits of entropy)\n", password.c_str(), bits);
}
