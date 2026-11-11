#include <alelus/alelus.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>

int main() {
    alelus::FastRNG rng(42); // same seed, same numbers, on every compiler and system

    int die = rng.uniform_int(1, 6); // 1..6, both included
    double x = rng.uniform_real();   // [0, 1)
    bool hit = rng.bernoulli(0.25);  // true 25% of the time

    std::vector<int> cards{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    rng.shuffle(cards);
    std::size_t i = rng.weighted_index(std::vector{60, 30, 10}); // 0: 60%, 1: 30%, 2: 10%

    alelus::CryptoRNG secure;       // unpredictable: no seed exists
    std::uint64_t token = secure(); // 64 random bits

    std::printf("%d %f %d %d %zu %llx\n", die, x, hit, cards[0], i,
                static_cast<unsigned long long>(token));
}
