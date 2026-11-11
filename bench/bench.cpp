// Alelus benchmark: how many nanoseconds each operation takes.
//
//   make bench                 run every measurement (always built in release mode)
//   make bench B=uniform_int   run only the measurements whose name contains "uniform_int"
//
// Numbers depend on the machine, the compiler and what else is running. Compare versions of
// the code on the same machine, not numbers between machines.

#include "alelus/alelus.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <numeric>
#include <random>
#include <string>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

// Every measured result is folded into g_sink, and g_sink is written to a volatile variable at
// the end of main(). A volatile write must happen, so every result it depends on must be
// computed: the compiler cannot decide that a measured loop is useless and delete it.
std::uint64_t g_sink = 0;
volatile std::uint64_t g_volatile_sink = 0;

template <class T>
void consume(T value) {
    g_sink ^= static_cast<std::uint64_t>(value);
}

/// Runs body() n times and returns the elapsed time in nanoseconds.
template <class Body>
double time_ns(std::uint64_t n, Body &body) {
    const auto start = Clock::now();
    for (std::uint64_t i = 0; i < n; ++i)
        body();
    const auto end = Clock::now();
    return std::chrono::duration<double, std::nano>(end - start).count();
}

/// Measures the cost of one call of body(), in nanoseconds:
///  1. calibrate: double the number of calls until one run takes at least 100 ms;
///  2. measure 5 runs of that size and keep the median, which ignores the occasional run
///     that was slowed down by the operating system.
template <class Body>
double measure(Body body) {
    std::uint64_t n = 1000;
    while (time_ns(n, body) < 100e6)
        n *= 2;
    std::array<double, 5> runs{};
    for (auto &r : runs)
        r = time_ns(n, body) / static_cast<double>(n);
    std::sort(runs.begin(), runs.end());
    return runs[runs.size() / 2];
}

const char *g_filter = nullptr;
const char *g_pending_section = nullptr; // printed just before its first measurement
int g_measured = 0;

template <class Body>
void bench(const char *name, Body body) {
    if (g_filter != nullptr && std::strstr(name, g_filter) == nullptr)
        return;
    if (g_pending_section != nullptr) {
        std::printf("\n%s\n", g_pending_section);
        g_pending_section = nullptr;
    }
    const double ns = measure(body);
    std::printf("  %-52s %9.2f ns   %9.1f M/s\n", name, ns, 1e3 / ns);
    ++g_measured;
}

/// Starts a section. Its title is only printed if one of its measurements runs, so a
/// filter does not leave empty titles behind.
void section(const char *title) {
    g_pending_section = title;
}

} // namespace

int main(int argc, char **argv) {
    if (argc > 1 && argv[1][0] != '\0')
        g_filter = argv[1];

    std::printf("Alelus benchmark (median of 5 runs, release build)\n");
#if defined(__VERSION__)
    std::printf("Compiler: %s\n", __VERSION__);
#endif

    alelus::FastRNG fast(42);
    alelus::CryptoRNG crypto;
    std::mt19937_64 mt(42); // the standard library's 64-bit generator, for reference

    section("Raw 64-bit numbers");
    bench("FastRNG()", [&] { consume(fast()); });
    bench("CryptoRNG()", [&] { consume(crypto()); });
    bench("std::mt19937_64 (reference)", [&] { consume(mt()); });

    section("uniform_int");
    bench("FastRNG uniform_int(1, 6)", [&] { consume(fast.uniform_int(1, 6)); });
    bench("CryptoRNG uniform_int(1, 6)", [&] { consume(crypto.uniform_int(1, 6)); });
    {
        std::uniform_int_distribution<int> die(1, 6);
        bench("std::uniform_int_distribution(1, 6) + FastRNG", [&] { consume(die(fast)); });
        bench("std::uniform_int_distribution(1, 6) + mt19937_64", [&] { consume(die(mt)); });
    }
    // n = 2^32 + 1 was the worst case of the old bitmask algorithm (almost half of the draws
    // rejected). Kept to show that Lemire's method has no such case.
    bench("FastRNG uniform_int(2^32 + 1)",
          [&] { consume(fast.uniform_int(std::uint64_t{0x100000001})); });

    section("uniform_real");
    bench("FastRNG uniform_real()", [&] { consume(fast.uniform_real() * 1e9); });
    bench("CryptoRNG uniform_real()", [&] { consume(crypto.uniform_real() * 1e9); });
    bench("FastRNG uniform_real(-1.0, 1.0)", [&] { consume(fast.uniform_real(-1.0, 1.0) * 1e9); });
    {
        std::uniform_real_distribution<double> u(0.0, 1.0);
        bench("std::uniform_real_distribution + FastRNG", [&] { consume(u(fast) * 1e9); });
    }

    section("bernoulli");
    bench("FastRNG bernoulli()", [&] { consume(fast.bernoulli()); });
    bench("FastRNG bernoulli(0.3)", [&] { consume(fast.bernoulli(0.3)); });
    bench("FastRNG bernoulli(1, 3)", [&] { consume(fast.bernoulli(1, 3)); });

    section("shuffle (per shuffle of 52 cards)");
    {
        std::vector<int> deck(52);
        std::iota(deck.begin(), deck.end(), 0);
        bench("FastRNG shuffle(52)", [&] {
            fast.shuffle(deck);
            consume(deck[0]);
        });
        bench("CryptoRNG shuffle(52)", [&] {
            crypto.shuffle(deck);
            consume(deck[0]);
        });
        bench("std::shuffle(52) + FastRNG", [&] {
            std::shuffle(deck.begin(), deck.end(), fast);
            consume(deck[0]);
        });
    }

    section("weighted_index");
    {
        const std::vector<std::uint64_t> four{10, 20, 30, 40};
        std::vector<std::uint64_t> hundred(100);
        std::iota(hundred.begin(), hundred.end(), 1); // 1, 2, ..., 100
        bench("FastRNG weighted_index(4 weights)", [&] { consume(fast.weighted_index(four)); });
        bench("FastRNG weighted_index(100 weights)",
              [&] { consume(fast.weighted_index(hundred)); });
        bench("CryptoRNG weighted_index(4 weights)", [&] { consume(crypto.weighted_index(four)); });

        const alelus::WeightedSampler four_prepared(four);
        const alelus::WeightedSampler hundred_prepared(hundred);
        bench("FastRNG weighted_index(WeightedSampler, 4 weights)",
              [&] { consume(fast.weighted_index(four_prepared)); });
        bench("FastRNG weighted_index(WeightedSampler, 100 weights)",
              [&] { consume(fast.weighted_index(hundred_prepared)); });
        bench("WeightedSampler construction (100 weights)", [&] {
            const alelus::WeightedSampler s(hundred);
            consume(s.size());
        });
    }

    if (g_measured == 0)
        std::printf("\nNo measurement name contains \"%s\".\n", g_filter);

    // Not printed: this write is what keeps the measured code alive (see g_sink above)
    g_volatile_sink = g_sink;
    return 0;
}
