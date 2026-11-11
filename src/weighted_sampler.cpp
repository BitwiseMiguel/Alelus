#include "alelus/weighted_sampler.hpp"

#include <limits>

namespace alelus {

void WeightedSampler::build(std::vector<std::uint64_t> weights) {
    const std::uint64_t n = weights.size();
    std::uint64_t total = 0;
    for (const std::uint64_t w : weights) {
        if (w > std::numeric_limits<std::uint64_t>::max() - total)
            throw std::overflow_error(
                "WeightedSampler: the sum of the weights does not fit in 64 bits");
        total += w;
    }
    if (total == 0)
        throw std::invalid_argument(
            "WeightedSampler: the sum of the weights must be greater than zero");
    if (total > std::numeric_limits<std::uint64_t>::max() / n)
        throw std::overflow_error(
            "WeightedSampler: the sum of the weights times their number does not fit in 64 bits");

    // Multiply every weight by n. Then there are n * total units in all, and each of the
    // n columns must hold exactly "total" of them: everything stays an exact integer.
    m_height = total;
    m_own.assign(n, 0);
    m_alias.resize(n);
    std::vector<std::uint64_t> scaled(n);
    std::vector<std::size_t> small, large; // below the column height / at or above it
    for (std::size_t i = 0; i < n; ++i) {
        scaled[i] = weights[i] * n;
        m_alias[i] = i;
        (scaled[i] < m_height ? small : large).push_back(i);
    }

    // Pair a small option with a large one: the small one keeps its units in its own
    // column, and the large one fills the rest of that column (it becomes the alias).
    // The large one then has less left, and may become small itself.
    std::size_t next_small = 0; // "small" is used as a queue, in order
    while (next_small < small.size() && !large.empty()) {
        const std::size_t s = small[next_small++];
        const std::size_t l = large.back();
        m_own[s] = scaled[s];
        m_alias[s] = l;
        scaled[l] -= m_height - scaled[s];
        if (scaled[l] < m_height) {
            large.pop_back();
            small.push_back(l);
        }
    }

    // Whatever is left fills its own column exactly. With integers the arithmetic is exact,
    // so only large options can be left here (with exactly the column height).
    for (const std::size_t l : large)
        m_own[l] = m_height;
    for (; next_small < small.size(); ++next_small)
        m_own[small[next_small]] = m_height;
}

} // namespace alelus
