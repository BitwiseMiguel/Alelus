#include "catch.hpp"

#include "alelus/crypto_rng.hpp"
#include "alelus/detail/uniform.hpp"
#include "alelus/fast_rng.hpp"
#include "support/scripted_rng.hpp"
#include "support/stats.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <numeric>
#include <ranges>
#include <set>
#include <span>
#include <string>
#include <vector>

using alelus::CryptoRNG;
using alelus::FastRNG;

TEST_CASE("shuffle (algorithm)", "[shuffle]") {
    SECTION("Fisher-Yates, step by step") {
        // {a, b, c}, from the end (x = 1 gives j = 0 for any small n, with Lemire):
        //   i = 3: j = uniform_below(3) = 0 -> swap positions 2 and 0 -> {c, b, a}
        //   i = 2: j = uniform_below(2) = 0 -> swap positions 1 and 0 -> {b, c, a}
        ScriptedRng rng{1, 1};
        std::string s = "abc";
        alelus::detail::shuffle(rng, s);
        CHECK(s == "bca");
        CHECK(rng.consumed() == 2); // n - 1 numbers for n elements
    }
}

TEST_CASE("FastRNG: shuffle()", "[fast][shuffle]") {
    FastRNG rng(2026);

    SECTION("keeps the same elements") {
        std::vector<int> v(52);
        std::iota(v.begin(), v.end(), 0);
        const auto original = v;
        rng.shuffle(v);
        CHECK(std::is_permutation(v.begin(), v.end(), original.begin()));
        CHECK(v != original);
    }

    SECTION("0 and 1 elements are left as they are") {
        std::vector<int> empty;
        rng.shuffle(empty);
        CHECK(empty.empty());
        std::array<int, 1> one{42};
        rng.shuffle(one);
        CHECK(one[0] == 42);
    }

    SECTION("works with std::array, C arrays and std::string") {
        std::array<int, 5> a{1, 2, 3, 4, 5};
        int c[5] = {1, 2, 3, 4, 5};
        std::string s = "abcde";
        rng.shuffle(a);
        rng.shuffle(c);
        rng.shuffle(s);
        CHECK(std::is_permutation(a.begin(), a.end(), std::begin(c)));
        CHECK(std::is_permutation(s.begin(), s.end(), std::string("abcde").begin()));
    }

    SECTION("part of a vector, through std::span: only that part changes") {
        std::vector<int> v(10);
        std::iota(v.begin(), v.end(), 0); // 0 1 2 ... 9

        SECTION("from the 3rd element to the end") {
            rng.shuffle(std::span(v).subspan(2));
            CHECK(v[0] == 0);
            CHECK(v[1] == 1);
            CHECK(std::is_permutation(v.begin() + 2, v.end(),
                                      std::vector{2, 3, 4, 5, 6, 7, 8, 9}.begin()));
            CHECK_FALSE(std::is_sorted(v.begin() + 2, v.end())); // it did shuffle (fixed seed)
        }

        SECTION("5 elements, starting at the 3rd") {
            rng.shuffle(std::span(v).subspan(2, 5));
            CHECK(v[0] == 0);
            CHECK(v[1] == 1);
            CHECK(std::is_permutation(v.begin() + 2, v.begin() + 7,
                                      std::vector{2, 3, 4, 5, 6}.begin()));
            CHECK(v[7] == 7);
            CHECK(v[8] == 8);
            CHECK(v[9] == 9);
        }

        SECTION("std::views::drop gives the same result as subspan") {
            std::vector<int> w = v;
            FastRNG a(7), b(7);
            a.shuffle(std::span(v).subspan(2));
            b.shuffle(w | std::views::drop(2));
            CHECK(v == w);
        }

        SECTION("every element of the part can reach every position of the part") {
            // Over many shuffles of positions 2..4, each of the values 2, 3, 4 must appear at
            // position 2, and positions 0, 1 and 5..9 must never change
            std::set<int> at_position_2;
            for (int i = 0; i < 1000; ++i) {
                std::vector<int> u(10);
                std::iota(u.begin(), u.end(), 0);
                rng.shuffle(std::span(u).subspan(2, 3));
                at_position_2.insert(u[2]);
                REQUIRE(u[0] == 0);
                REQUIRE(u[1] == 1);
                REQUIRE(u[5] == 5);
            }
            CHECK(at_position_2 == std::set<int>{2, 3, 4});
        }
    }

    SECTION("the 6 orders of 3 elements are equally likely (chi-square)") {
        // The "obvious" version, swapping each position with ANY position in [0, n), has
        // 3 * 3 * 3 = 27 equally likely paths for 6 orders. 27 is not a multiple of 6, so
        // some orders must come out more often than others, and this test catches it.
        std::map<std::array<int, 3>, std::uint64_t> counts;
        for (int i = 0; i < 600000; ++i) {
            std::array<int, 3> a{0, 1, 2};
            rng.shuffle(a);
            ++counts[a];
        }
        REQUIRE(counts.size() == 6);
        std::vector<std::uint64_t> observed;
        for (const auto &entry : counts)
            observed.push_back(entry.second);
        CHECK(chi_square_uniform(observed) < CHI2_CRITICAL_P001_DF5);
    }

    SECTION("the same seed gives the same shuffle") {
        std::vector<int> v1(52), v2(52);
        std::iota(v1.begin(), v1.end(), 0);
        std::iota(v2.begin(), v2.end(), 0);
        FastRNG a(42), b(42);
        a.shuffle(v1);
        b.shuffle(v2);
        CHECK(v1 == v2);
    }
}

TEST_CASE("CryptoRNG: shuffle()", "[crypto][shuffle]") {
    CryptoRNG rng;

    SECTION("keeps the same elements") {
        std::vector<int> v(52);
        std::iota(v.begin(), v.end(), 0);
        const auto original = v;
        rng.shuffle(v);
        CHECK(std::is_permutation(v.begin(), v.end(), original.begin()));
    }
}
