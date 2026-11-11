#include "catch.hpp"

#include "alelus/splitmix64.hpp"

using alelus::SplitMix64;

TEST_CASE("SplitMix64: next()", "[splitmix64]") {
    // Reference values: outputs of the C implementation published with xoshiro256**
    // (https://prng.di.unimi.it/splitmix64.c), checked independently in Python

    SECTION("reference sequence for seed 0") {
        SplitMix64 sm(0);
        CHECK(sm.next() == 0xe220a8397b1dcdafULL);
        CHECK(sm.next() == 0x6e789e6aa1b965f4ULL);
        CHECK(sm.next() == 0x06c45d188009454fULL);
        CHECK(sm.next() == 0xf88bb8a8724c81ecULL);
        CHECK(sm.next() == 0x1b39896a51a8749bULL);
    }

    SECTION("reference sequence for seed 1234567") {
        SplitMix64 sm(1234567);
        CHECK(sm.next() == 0x599ed017fb08fc85ULL);
        CHECK(sm.next() == 0x2c73f08458540fa5ULL);
        CHECK(sm.next() == 0x883ebce5a3f27c77ULL);
        CHECK(sm.next() == 0x3fbef740e9177b3fULL);
        CHECK(sm.next() == 0xe3b8346708cb5ecdULL);
    }

    SECTION("a copy continues the same sequence") {
        SplitMix64 a(99);
        (void)a.next();
        SplitMix64 b = a;
        for (int i = 0; i < 100; ++i)
            REQUIRE(a.next() == b.next());
    }
}
