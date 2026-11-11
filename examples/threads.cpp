// One FastRNG per thread, made independent with jump(): each copy starts 2^128 numbers
// after the previous one, so no two threads can ever use overlapping parts of the sequence.
//
// The work: estimate pi with the Monte Carlo method. A random point in the unit square
// falls inside the quarter circle x^2 + y^2 < 1 with probability pi / 4.
//
// The seed is fixed, so the result is the same on every run, however the threads are
// scheduled: each thread always gets the same generator and the same numbers.

#include <alelus/alelus.hpp>

#include <cstdint>
#include <cstdio>
#include <thread>
#include <vector>

int main() {
    constexpr int thread_count = 4;
    constexpr std::uint64_t points_per_thread = 1'000'000;

    alelus::FastRNG base(2026);
    std::vector<alelus::FastRNG> generators;
    for (int t = 0; t < thread_count; ++t) {
        generators.push_back(base);
        base.jump();
    }

    // Each thread writes only its own element: no locking needed
    std::vector<std::uint64_t> inside(thread_count, 0);
    std::vector<std::thread> workers;
    for (int t = 0; t < thread_count; ++t) {
        workers.emplace_back([&generators, &inside, t] {
            alelus::FastRNG &rng = generators[t];
            std::uint64_t count = 0;
            for (std::uint64_t i = 0; i < points_per_thread; ++i) {
                const double x = rng.uniform_real();
                const double y = rng.uniform_real();
                if (x * x + y * y < 1.0)
                    ++count;
            }
            inside[t] = count;
        });
    }
    for (std::thread &worker : workers)
        worker.join();

    std::uint64_t total = 0;
    for (const std::uint64_t count : inside)
        total += count;
    const double points = static_cast<double>(points_per_thread) * thread_count;
    std::printf("pi is about %.6f (%.0f points, %d threads)\n",
                4.0 * static_cast<double>(total) / points, points, thread_count);
}
