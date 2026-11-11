#include "catch.hpp"

#include "alelus/detail/chacha20.hpp"
#include "support/hex.hpp"

#include <array>
#include <bit>
#include <cstdint>

namespace cc = alelus::detail::chacha20;

// All expected values are the official test vectors of RFC 8439,
// https://www.rfc-editor.org/rfc/rfc8439

TEST_CASE("ChaCha20: quarter round", "[chacha20]") {
    SECTION("RFC 8439, section 2.1.1") {
        std::uint32_t a = 0x11111111, b = 0x01020304, c = 0x9b8d6f43, d = 0x01234567;
        cc::quarter_round(a, b, c, d);
        CHECK(a == 0xea2a92f4);
        CHECK(b == 0xcb1cf8ce);
        CHECK(c == 0x4581472e);
        CHECK(d == 0x5881c4bb);
    }
}

TEST_CASE("ChaCha20: block function", "[chacha20]") {
    const cc::Nonce zero_nonce{};

    SECTION("RFC 8439, section 2.3.2") {
        cc::Key key{};
        for (int i = 0; i < 32; ++i)
            key[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(i); // 00 01 ... 1f
        const auto nonce = array_from_hex<12>("000000090000004a00000000");
        CHECK(
            cc::block(key, 1, nonce) ==
            array_from_hex<64>("10f1e7e4d13b5915500fdd1fa32071c4c7d1f4c733c068030422aa9ac3d46c4e"
                               "d2826446079faa0914c2d705d98b02a2b5129cd1de164eb9cbd083e8a2503c4e"));
    }

    SECTION("RFC 8439, appendix A.1 #1: zero key, counter 0") {
        CHECK(
            cc::block(cc::Key{}, 0, zero_nonce) ==
            array_from_hex<64>("76b8e0ada0f13d90405d6ae55386bd28bdd219b8a08ded1aa836efcc8b770dc7"
                               "da41597c5157488d7724e03fb8d84a376a43b8f41518a11cc387b669b2ee6586"));
    }

    SECTION("RFC 8439, appendix A.1 #2: zero key, counter 1") {
        CHECK(
            cc::block(cc::Key{}, 1, zero_nonce) ==
            array_from_hex<64>("9f07e7be5551387a98ba977c732d080dcb0f29a048e3656912c6533e32ee7aed"
                               "29b721769ce64e43d57133b074d839d531ed1f28510afb45ace10a1f4b794d6f"));
    }

    SECTION("RFC 8439, appendix A.1 #3: last key byte 01, counter 1") {
        cc::Key key{};
        key[31] = 0x01;
        CHECK(
            cc::block(key, 1, zero_nonce) ==
            array_from_hex<64>("3aeb5224ecf849929b9d828db1ced4dd832025e8018b8160b82284f3c949aa5a"
                               "8eca00bbb4a73bdad192b5c42f73f2fd4e273644c8b36125a64addeb006c13a0"));
    }

    SECTION("RFC 8439, appendix A.1 #4: second key byte ff, counter 2") {
        cc::Key key{};
        key[1] = 0xff;
        CHECK(
            cc::block(key, 2, zero_nonce) ==
            array_from_hex<64>("72d54dfbf12ec44b362692df94137f328fea8da73990265ec1bbbea1ae9af0ca"
                               "13b25aa26cb4a648cb9b9d1be65b2c0924a66c54d545ec1b7374f4872e99f096"));
    }

    SECTION("RFC 8439, appendix A.1 #5: last nonce byte 02, counter 0") {
        cc::Nonce nonce{};
        nonce[11] = 0x02;
        CHECK(
            cc::block(cc::Key{}, 0, nonce) ==
            array_from_hex<64>("c2c64d378cd536374ae204b9ef933fcd1a8b2288b3dfa49672ab765b54ee27c7"
                               "8a970e0e955c14f3a88e741b97c286f75f8fc299e8148362fa198a39531bed6d"));
    }

    SECTION("a one-bit change in the key changes about half of the output bits") {
        // The "avalanche effect": what makes the output look unrelated to the key
        cc::Key key{};
        cc::Key flipped{};
        flipped[0] = 0x01;
        const cc::Block a = cc::block(key, 0, zero_nonce);
        const cc::Block b = cc::block(flipped, 0, zero_nonce);
        int different_bits = 0;
        for (std::size_t i = 0; i < a.size(); ++i)
            different_bits += std::popcount(static_cast<unsigned>(a[i] ^ b[i]));
        // 512 bits: expect about 256. 200..312 is about 5 standard deviations either side.
        CHECK(different_bits > 200);
        CHECK(different_bits < 312);
    }

    SECTION("block_words gives the same bits as block, read as little-endian numbers") {
        cc::Key key{};
        for (std::size_t i = 0; i < key.size(); ++i)
            key[i] = static_cast<std::uint8_t>(i);
        const cc::Nonce nonce{0, 0, 0, 9, 0, 0, 0, 0x4a, 0, 0, 0, 0};
        for (std::uint32_t counter : {0u, 1u, 0xffffffffu}) {
            const cc::Block bytes = cc::block(key, counter, nonce);
            std::array<std::uint64_t, 8> words{};
            cc::block_words(key, counter, nonce, words);
            for (std::size_t i = 0; i < 8; ++i) {
                std::uint64_t expected = 0;
                for (std::size_t j = 0; j < 8; ++j)
                    expected |= static_cast<std::uint64_t>(bytes[8 * i + j]) << (8 * j);
                CHECK(words[i] == expected);
            }
        }
    }
}
