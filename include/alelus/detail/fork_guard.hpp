#pragma once

#include <cstdint>

namespace alelus::detail {

/// A counter that changes in the child process after every fork().
/// On Windows, which has no fork(), it is always 0.
std::uint64_t fork_generation() noexcept;

/// Registers, once per process, the function that makes the counter change.
/// Throws std::runtime_error if it cannot be registered.
void install_fork_handler();

} // namespace alelus::detail
