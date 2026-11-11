#pragma once

// Test helper: a "fake" generator that returns the values it is given, in order.
// It makes it possible to test cases that a real generator almost never produces (the
// maximum value, a rejection...) and to count how many numbers a function consumed.

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

class ScriptedRng {
public:
    using result_type = std::uint64_t;
    static constexpr result_type min() {
        return 0;
    }
    static constexpr result_type max() {
        return std::numeric_limits<result_type>::max();
    }

    ScriptedRng(std::initializer_list<std::uint64_t> values) : m_values(values) {
    }

    explicit ScriptedRng(std::vector<std::uint64_t> values) : m_values(std::move(values)) {
    }

    result_type operator()() {
        if (m_next >= m_values.size())
            throw std::out_of_range("ScriptedRng: asked for more numbers than were given");
        return m_values[m_next++];
    }

    /// How many numbers have been consumed so far.
    [[nodiscard]] std::size_t consumed() const {
        return m_next;
    }

private:
    std::vector<std::uint64_t> m_values;
    std::size_t m_next = 0;
};
