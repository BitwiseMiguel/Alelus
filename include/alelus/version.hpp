#pragma once

#include <string_view>

// The version of Alelus, following Semantic Versioning (https://semver.org):
//   MAJOR changes when existing code may stop compiling or behave differently,
//   MINOR when features are added, PATCH for bug fixes.
// While MAJOR is 0, the API is not yet considered stable: any release may change it.
//
// They are macros, not constants, so that they also work in #if:
//   #if ALELUS_VERSION >= ALELUS_MAKE_VERSION(0, 2, 0)
#define ALELUS_VERSION_MAJOR 0
#define ALELUS_VERSION_MINOR 1
#define ALELUS_VERSION_PATCH 0

/// Turns a version into one number that compares correctly: 0.1.0 -> 100, 1.2.3 -> 10203.
#define ALELUS_MAKE_VERSION(major, minor, patch) ((major) * 10000 + (minor) * 100 + (patch))
#define ALELUS_VERSION \
    ALELUS_MAKE_VERSION(ALELUS_VERSION_MAJOR, ALELUS_VERSION_MINOR, ALELUS_VERSION_PATCH)

// The "#" operator turns a macro argument into a string. The second macro is needed so
// that the argument is expanded first: the result is "0", not "ALELUS_VERSION_MAJOR".
#define ALELUS_STRINGIFY_(x) #x
#define ALELUS_STRINGIFY(x) ALELUS_STRINGIFY_(x)

namespace alelus {

/// The version as text, for example "0.1.0". Built from the macros above, so the two can
/// never disagree.
// clang-format off
inline constexpr std::string_view version = ALELUS_STRINGIFY(ALELUS_VERSION_MAJOR) "."
                                            ALELUS_STRINGIFY(ALELUS_VERSION_MINOR) "."
                                            ALELUS_STRINGIFY(ALELUS_VERSION_PATCH);
// clang-format on

} // namespace alelus
