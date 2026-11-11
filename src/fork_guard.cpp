#include "alelus/detail/fork_guard.hpp"

#include <stdexcept>

#if !defined(_WIN32)
#include <atomic>
#include <pthread.h>
#endif

namespace alelus::detail {

#if defined(_WIN32)

// Windows has no fork(): nothing can duplicate the generator's memory behind its back
std::uint64_t fork_generation() noexcept {
    return 0;
}

void install_fork_handler() {
}

#else

namespace {

std::atomic<std::uint64_t> g_fork_generation{0};

// Runs in the child process right after fork(). Only very simple work is allowed here
// (the child has just one thread and other locks may be held), so it only adds one.
void on_fork_child() noexcept {
    g_fork_generation.fetch_add(1, std::memory_order_relaxed);
}

} // namespace

std::uint64_t fork_generation() noexcept {
    return g_fork_generation.load(std::memory_order_relaxed);
}

void install_fork_handler() {
    // A local static is initialised exactly once, even with several threads: the handler
    // is registered only the first time this runs
    static const bool registered = pthread_atfork(nullptr, nullptr, on_fork_child) == 0;
    if (!registered)
        throw std::runtime_error("install_fork_handler: pthread_atfork failed");
}

#endif

} // namespace alelus::detail
