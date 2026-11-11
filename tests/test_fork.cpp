// fork() only exists on Linux, macOS and BSD. On Windows this file compiles to nothing.

#include "catch.hpp"

#if !defined(_WIN32)

#include "alelus/crypto_rng.hpp"
#include "alelus/fast_rng.hpp"

#include <array>
#include <cstdint>
#include <sys/wait.h>
#include <unistd.h>

namespace {

constexpr int N = 4;

/// Calls fork(). The child writes its next N numbers into a pipe and exits; the parent
/// reads them and returns them.
template <class Rng>
std::array<std::uint64_t, N> numbers_from_child(Rng &rng) {
    int fds[2];
    REQUIRE(pipe(fds) == 0);
    const pid_t pid = fork();
    REQUIRE(pid >= 0);
    if (pid == 0) { // child
        close(fds[0]);
        std::array<std::uint64_t, N> mine{};
        for (auto &v : mine)
            v = rng();
        const auto written = write(fds[1], mine.data(), sizeof(mine));
        _exit(written == static_cast<ssize_t>(sizeof(mine)) ? 0 : 1);
    }
    close(fds[1]);
    std::array<std::uint64_t, N> theirs{};
    REQUIRE(read(fds[0], theirs.data(), sizeof(theirs)) == static_cast<ssize_t>(sizeof(theirs)));
    close(fds[0]);
    int status = 0;
    waitpid(pid, &status, 0);
    REQUIRE(WIFEXITED(status));
    REQUIRE(WEXITSTATUS(status) == 0);
    return theirs;
}

} // namespace

TEST_CASE("fork()", "[fork]") {
    SECTION("CryptoRNG: parent and child give different numbers") {
        alelus::CryptoRNG rng;
        (void)rng(); // the buffer already holds 123 numbers: the child must NOT reuse them
        const auto child = numbers_from_child(rng);
        std::array<std::uint64_t, N> parent{};
        for (auto &v : parent)
            v = rng();
        CHECK(child != parent);
    }

    SECTION("FastRNG: parent and child repeat the same numbers, as documented") {
        // A FastRNG is just numbers in memory: the child inherits a copy and repeats the
        // sequence. For a reproducible generator this is the expected behaviour, not a bug.
        alelus::FastRNG rng(123);
        const auto child = numbers_from_child(rng);
        std::array<std::uint64_t, N> parent{};
        for (auto &v : parent)
            v = rng();
        CHECK(child == parent);
    }
}

#endif
