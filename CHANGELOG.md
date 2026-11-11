# Changelog

All notable changes to Alelus are listed here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and versions follow
[Semantic Versioning](https://semver.org/): while the major version is 0, the API may still
change in any release.

## 0.1.0 - 2026-11-11

The first release.

### Added

- `FastRNG`: xoshiro256** 1.0. Reproducible from a 64-bit seed (expanded with SplitMix64),
  or seeded with 256 bits from the operating system. `jump()` and `long_jump()` for
  independent streams per thread or machine.
- `CryptoRNG`: ChaCha20 (RFC 8439) with fast-key-erasure, keyed from the operating system
  (`getrandom`, `getentropy`, `BCryptGenRandom`). Wipes every number as it is handed out and
  every key it replaces, and gets a new key after `fork()`.
- Methods shared by both generators, all exact (no bias):
  - `uniform_int` (Lemire's method), with mixed argument types checked for range;
  - `uniform_real`, with 53 random bits;
  - `bernoulli()`, `bernoulli(p)` and `bernoulli(k, n)` (exactly k / n);
  - `shuffle` (Fisher–Yates), on any random-access range or `std::span`;
  - `weighted_index` over integer weights, linear or in constant time with
    `WeightedSampler` (Vose's alias method with integers).
- Both generators satisfy `std::uniform_random_bit_generator`.
- `os_random_bytes`, `SplitMix64` and the version in `alelus/version.hpp`.
- Tests against published references (xoshiro256**, SplitMix64, RFC 8439, OpenSSL) and
  chi-square tests with fixed seeds; benchmark (`make bench`); examples (`make examples`).
- CI on Linux (g++, clang, and both under AddressSanitizer and UndefinedBehaviorSanitizer),
  macOS and Windows (MSYS2).

### Known limitations

- `uniform_real(a, b)` may differ in the last bit between machines with and without FMA
  instructions, because compilers can fuse `a + u * (b - a)` into one operation.
- `CryptoRNG` has not been audited. For real security needs, prefer the operating system or
  an audited library.
- The sanitizers (`make tests SANITIZE=1`) are not available with MinGW on Windows.
