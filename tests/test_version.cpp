#include "catch.hpp"

#include "alelus/alelus.hpp"

#include <string>

// The macros must work in #if: that is why they are macros
#if ALELUS_VERSION < ALELUS_MAKE_VERSION(0, 1, 0)
#error "ALELUS_VERSION does not work in #if"
#endif

static_assert(ALELUS_VERSION == ALELUS_MAKE_VERSION(ALELUS_VERSION_MAJOR, ALELUS_VERSION_MINOR,
                                                    ALELUS_VERSION_PATCH));

TEST_CASE("version", "[version]") {
    SECTION("the text matches the numbers") {
        const std::string expected = std::to_string(ALELUS_VERSION_MAJOR) + "." +
                                     std::to_string(ALELUS_VERSION_MINOR) + "." +
                                     std::to_string(ALELUS_VERSION_PATCH);
        CHECK(alelus::version == expected);
    }

    SECTION("versions compare in the right order") {
        // Each part has two digits of room: minor and patch must stay below 100
        CHECK(ALELUS_MAKE_VERSION(0, 1, 0) < ALELUS_MAKE_VERSION(0, 1, 1));
        CHECK(ALELUS_MAKE_VERSION(0, 1, 99) < ALELUS_MAKE_VERSION(0, 2, 0));
        CHECK(ALELUS_MAKE_VERSION(0, 99, 99) < ALELUS_MAKE_VERSION(1, 0, 0));
        CHECK(ALELUS_MAKE_VERSION(1, 2, 3) == 10203);
    }
}
