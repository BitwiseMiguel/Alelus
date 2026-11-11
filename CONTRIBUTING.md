# Contributing to Alelus

Thank you for your interest in Alelus. This document explains how to build the project,
what a change needs before it is merged, and the conventions the code follows.

## Getting started

1. Fork the repository and create a branch for your change:
   `git switch -c fix-uniform-int-overflow`
2. Build and run the tests:
   ```sh
   make tests
   ```
3. Make your change, add or update tests, and run them again.

You need a C++20 compiler (g++ 11+ or clang++ 14+), GNU `make` and, ideally,
`clang-format`. On Windows, use an MSYS2 or Git Bash shell with MinGW.

## Before opening a pull request

Every change must pass this checklist:

- [ ] `make clean && make tests WERROR=1` passes: it builds without warnings and every test
      passes.
- [ ] It also builds with the other compiler: `make clean && make tests WERROR=1 CXX=clang++`
      (or `CXX=g++` if you normally use clang).
- [ ] `make format` leaves no changes (`git diff` is empty after running it).
- [ ] New behaviour has tests, and fixed bugs have a test that failed before the fix.
- [ ] Public functions and classes have a short comment explaining what they do.

## Code conventions

- **Language:** everything in the code is in English: names, comments, error messages, test
  names and commit messages.
- **Standard:** C++20, with no compiler extensions.
- **Formatting:** whatever `.clang-format` produces. Run `make format`; do not format by hand.
- **Line endings:** CRLF in every text file. `.gitattributes` and `.clang-format` take care of it.
- **Naming:** `PascalCase` for types, `snake_case` for functions and variables, `m_` prefix for
  private members, `UPPER_CASE` for macros.
- **Headers:** `#pragma once`, public headers live in `include/alelus/`, implementation details in
  `include/alelus/detail/`.
- **Errors:** invalid arguments throw standard exceptions (`std::invalid_argument`,
  `std::out_of_range`, ...) with a message that names the function.

## Tests

- Tests use [Catch2](https://github.com/catchorg/Catch2) and live in `tests/`.
- Group tests by topic: one `TEST_CASE` per component and feature, named
  `"<Component>: <feature>"` (for example `"FastRNG: construction"`), with one `SECTION` per
  behaviour (for example `"the all-zero state is rejected"`).
- Shared setup goes at the top of the `TEST_CASE`, before the sections: Catch2 runs it again for
  each section, so every section starts from a clean state.
- Compile-time checks (`static_assert`) go at file scope, above the test cases.
- Statistical tests must use a **fixed seed**, so they are deterministic. A test that fails
  only sometimes is a bug in the test.
- When a test checks a value from a paper, an RFC or another implementation, say where the
  value comes from in a comment.

## Randomness and security

Changes to `CryptoRNG` or to anything that handles key material need extra care:

- Never add a fallback to a weak entropy source (time, addresses, `std::random_device`).
- Never make key material copyable, and wipe it when it is no longer needed.
- Explain the reasoning in the pull request. If in doubt, open an issue first.

## Commit messages

- Write in the imperative mood: "Add Lemire's method", not "Added" or "Adds".
- Keep the first line under about 70 characters; add details after a blank line if needed.
- One logical change per commit.

## Reporting bugs and requesting features

Open an issue on GitHub:

- **Bugs:** what you did, what you expected, what happened, your compiler and operating
  system, and the smallest piece of code that reproduces the problem.
- **Features:** the use case, and why the existing API does not cover it.

## License

By contributing, you agree that your contributions are licensed under the MIT License (see
[LICENSE](LICENSE)).
