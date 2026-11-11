#include "catch.hpp"

#include "alelus/detail/chacha20_generator.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

using alelus::detail::ChaCha20Generator;
namespace cc = alelus::detail::chacha20;

static_assert(!std::is_copy_constructible_v<ChaCha20Generator>);
static_assert(!std::is_copy_assignable_v<ChaCha20Generator>);
static_assert(ChaCha20Generator::outputs_per_refill == 124);

namespace {

// Reads 8 bytes of a block as a little-endian number, the same way the generator does
std::uint64_t le64(const cc::Block &block, std::size_t offset) {
    std::uint64_t v = 0;
    for (std::size_t i = 0; i < 8; ++i)
        v |= static_cast<std::uint64_t>(block[offset + i]) << (8 * i);
    return v;
}

} // namespace

TEST_CASE("ChaCha20Generator: next()", "[chacha20][generator]") {
    ChaCha20Generator gen(cc::Key{}); // zero key: only acceptable in tests
    const cc::Nonce nonce{};

    SECTION("the outputs come from the keystream, after the 32 key bytes") {
        // Rebuilds the expected outputs directly from chacha20::block
        const cc::Block block0 = cc::block(cc::Key{}, 0, nonce);
        const cc::Block block1 = cc::block(cc::Key{}, 1, nonce);
        CHECK(gen.next() == le64(block0, 32)); // output 0: bytes 32..39 of block 0
        CHECK(gen.next() == le64(block0, 40)); // output 1: bytes 40..47
        (void)gen.next();                      // output 2: bytes 48..55
        CHECK(gen.next() == le64(block0, 56)); // output 3: last 8 bytes of block 0
        CHECK(gen.next() == le64(block1, 0));  // output 4: first 8 bytes of block 1
    }

    SECTION("after 124 outputs the key changes") {
        for (std::size_t i = 0; i < ChaCha20Generator::outputs_per_refill; ++i)
            (void)gen.next();

        // The new key is the first 32 bytes of block 0 under the old key
        const cc::Block block0 = cc::block(cc::Key{}, 0, nonce);
        cc::Key next_key{};
        for (std::size_t i = 0; i < 32; ++i)
            next_key[i] = block0[i];
        CHECK(gen.next() == le64(cc::block(next_key, 0, nonce), 32));
    }

    SECTION("reference values") {
        // Computed independently with OpenSSL's ChaCha20 applying the same construction.
        // The indices hit the start and the end of each refill.
        std::vector<std::uint64_t> out;
        for (int i = 0; i < 400; ++i)
            out.push_back(gen.next());
        CHECK(out[0] == 0x8d4857517c5941daULL);
        CHECK(out[123] == 0x1860838dcd11369aULL);
        CHECK(out[124] == 0xdb3cb94528adbdafULL);
        CHECK(out[247] == 0x23370c5a83198faeULL);
        CHECK(out[248] == 0x1a6158f577965c83ULL);
        CHECK(out[399] == 0x0313b4c6f160e7beULL);
    }

    SECTION("different keys give different sequences") {
        cc::Key other{};
        other[0] = 1;
        ChaCha20Generator gen2(other);
        int equal = 0;
        for (int i = 0; i < 1000; ++i)
            equal += (gen.next() == gen2.next());
        CHECK(equal == 0);
    }
}

TEST_CASE("ChaCha20Generator: rekey()", "[chacha20][generator]") {
    SECTION("equals a new generator with that key") {
        cc::Key k1{}, k2{};
        k2[5] = 9;
        ChaCha20Generator gen(k1), fresh(k2);
        for (int i = 0; i < 10; ++i)
            (void)gen.next(); // leaves the buffer partly used
        gen.rekey(k2);
        for (int i = 0; i < 300; ++i)
            REQUIRE(gen.next() == fresh.next());
    }
}
