#pragma once

#include <cstddef>

namespace alelus::detail {

/// Writes zeros over memory in a way the optimiser is not allowed to remove.
///
/// A plain memset just before an object dies is a "dead store": nothing reads those zeros
/// afterwards, so compilers are allowed to delete it, and the secret stays in memory.
void secure_zero(void *data, std::size_t size) noexcept;

} // namespace alelus::detail
