#pragma once

// Test helper: converts hexadecimal text (for example "0a1b") into bytes, so that test
// vectors can be copied from the RFC as they are written there.

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <vector>

inline std::vector<std::uint8_t> from_hex(std::string_view hex) {
    auto nibble = [](char c) -> std::uint8_t {
        if (c >= '0' && c <= '9')
            return static_cast<std::uint8_t>(c - '0');
        if (c >= 'a' && c <= 'f')
            return static_cast<std::uint8_t>(c - 'a' + 10);
        if (c >= 'A' && c <= 'F')
            return static_cast<std::uint8_t>(c - 'A' + 10);
        throw std::invalid_argument("from_hex: invalid character");
    };
    std::vector<std::uint8_t> bytes;
    std::size_t i = 0;
    while (i < hex.size()) {
        if (hex[i] == ' ' || hex[i] == ':') { // separators are allowed
            ++i;
            continue;
        }
        if (i + 1 >= hex.size())
            throw std::invalid_argument("from_hex: odd number of digits");
        bytes.push_back(static_cast<std::uint8_t>(nibble(hex[i]) << 4 | nibble(hex[i + 1])));
        i += 2;
    }
    return bytes;
}

template <std::size_t N>
std::array<std::uint8_t, N> array_from_hex(std::string_view hex) {
    const auto bytes = from_hex(hex);
    if (bytes.size() != N)
        throw std::invalid_argument("array_from_hex: wrong size");
    std::array<std::uint8_t, N> out{};
    for (std::size_t i = 0; i < N; ++i)
        out[i] = bytes[i];
    return out;
}
