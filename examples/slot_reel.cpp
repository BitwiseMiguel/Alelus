// A slot machine reel. Each symbol has an integer weight, and a spin picks a symbol with
// probability weight / total, exactly.
//
// The weights are prepared once in a WeightedSampler (Vose's alias method), so every spin
// takes the same short time, however many symbols the reel has. For weights that change
// at every pick, rng.weighted_index(weights) needs no preparation.

#include <alelus/alelus.hpp>

#include <array>
#include <cstdint>
#include <cstdio>

int main() {
    constexpr std::array<const char *, 5> symbols{"Cherry", "Lemon", "Bell", "Bar", "Seven"};
    constexpr std::array<int, 5> weights{40, 30, 15, 10, 5}; // total 100: read as percent

    const alelus::WeightedSampler reel(weights);
    alelus::FastRNG rng(7);

    constexpr std::uint64_t spins = 1'000'000;
    std::array<std::uint64_t, 5> hits{};
    for (std::uint64_t i = 0; i < spins; ++i)
        ++hits[rng.weighted_index(reel)];

    std::printf("%-8s %9s %9s\n", "Symbol", "Expected", "Observed");
    for (std::size_t s = 0; s < symbols.size(); ++s)
        std::printf("%-8s %8d%% %8.2f%%\n", symbols[s], weights[s],
                    100.0 * static_cast<double>(hits[s]) / static_cast<double>(spins));
}
