#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <ranges>
#include <stdexcept>
#include <utility>
#include <vector>

#include "alelus/detail/uniform.hpp"

namespace alelus {

class WeightedSampler;

namespace detail {
template <Random64 Rng>
std::size_t weighted_index(Rng &rng, const WeightedSampler &sampler);
}

/// Integer weights, prepared once so that rng.weighted_index(sampler) picks an index in
/// constant time, however many weights there are. Use it when the same weights are used
/// many times (for example the symbol weights of a slot reel); for weights that change at
/// every pick, rng.weighted_index(weights) is better.
///
///     alelus::WeightedSampler sampler({60, 30, 10});
///     std::size_t i = rng.weighted_index(sampler); // 0: 60%, 1: 30%, 2: 10%
///
/// Exact: P(i) = weights[i] / sum(weights), with no floating point.
///
/// How it works (Vose's alias method): the weights are cut and pasted into n columns of the
/// same height, each holding at most two options: its own, and one other (its "alias").
/// Picking = choose a column uniformly, then choose between its two options.
class WeightedSampler {
public:
    /// From any container of integers. Throws std::invalid_argument if a weight is negative
    /// or they are all 0 (or there are none), and std::overflow_error if the sum of the
    /// weights times their number does not fit in 64 bits.
    template <std::ranges::input_range R>
        requires std::integral<std::ranges::range_value_t<R>> &&
                 (!std::same_as<std::ranges::range_value_t<R>, bool>)
    explicit WeightedSampler(const R &weights) {
        std::vector<std::uint64_t> w;
        for (const auto x : weights) {
            if (std::cmp_less(x, 0))
                throw std::invalid_argument("WeightedSampler: weights must not be negative");
            w.push_back(static_cast<std::uint64_t>(x));
        }
        build(std::move(w));
    }

    /// From a list written in place: WeightedSampler sampler({60, 30, 10});
    WeightedSampler(std::initializer_list<std::uint64_t> weights)
        : WeightedSampler(std::vector<std::uint64_t>(weights)) {
    }

    /// The number of weights, that is, of possible indices.
    [[nodiscard]] std::size_t size() const noexcept {
        return m_own.size();
    }

private:
    void build(std::vector<std::uint64_t> weights);

    template <detail::Random64 Rng>
    friend std::size_t detail::weighted_index(Rng &rng, const WeightedSampler &sampler);

    std::uint64_t m_height = 0;       // the height of every column: the sum of the weights
    std::vector<std::uint64_t> m_own; // units of column i that belong to option i
    std::vector<std::size_t> m_alias; // the option that fills the rest of column i
};

namespace detail {

/// Picks with a prepared WeightedSampler: a uniform column, then its own option or its alias.
template <Random64 Rng>
std::size_t weighted_index(Rng &rng, const WeightedSampler &sampler) {
    const auto column = static_cast<std::size_t>(uniform_below(rng, sampler.m_own.size()));
    const std::uint64_t r = uniform_below(rng, sampler.m_height);
    return r < sampler.m_own[column] ? column : sampler.m_alias[column];
}

} // namespace detail

} // namespace alelus
