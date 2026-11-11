# Alelus

A C++20 random number library with two generators that share the same interface:

| | `FastRNG` | `CryptoRNG` |
|---|---|---|
| Algorithm | xoshiro256** 1.0 (Blackman & Vigna) | ChaCha20 (RFC 8439) with fast-key-erasure |
| Seed | Optional: `FastRNG(42)` repeats, `FastRNG()` does not | None: keyed from the operating system |
| Speed (64 random bits) | about 1 ns | about 17 ns |
| Use it for | Simulation, Monte Carlo, games, testing | When nobody must be able to predict the numbers |

Both satisfy `std::uniform_random_bit_generator`, so they also work with `std::shuffle` and
the `<random>` distributions.

## Example

```cpp
#include <alelus/alelus.hpp>

#include <cstdio>
#include <vector>

int main() {
    alelus::FastRNG rng(42); // same seed, same numbers, on every compiler and system

    int die = rng.uniform_int(1, 6);           // 1..6, both included
    double x = rng.uniform_real();             // [0, 1)
    bool hit = rng.bernoulli(0.25);            // true 25% of the time

    std::vector<int> cards{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    rng.shuffle(cards);
    std::size_t i = rng.weighted_index(std::vector{60, 30, 10}); // 0: 60%, 1: 30%, 2: 10%

    alelus::CryptoRNG secure;                  // unpredictable: no seed exists
    std::uint64_t token = secure();            // 64 random bits

    std::printf("%d %f %d %d %zu %llx\n", die, x, hit, cards[0], i,
                static_cast<unsigned long long>(token));
}
```

## API

Every method below exists in both `FastRNG` and `CryptoRNG`. Invalid arguments throw
standard exceptions (`std::invalid_argument`, `std::out_of_range`, `std::overflow_error`);
each header documents exactly when.

| Call | Result |
|---|---|
| `rng()` | 64 random bits (`std::uint64_t`) |
| `rng.uniform_int(lo, hi)` | Integer in `[lo, hi]`, both included. Type of the arguments (the larger one if they differ), or `uniform_int<T>(lo, hi)` |
| `rng.uniform_int(n)` | Integer in `[0, n)`: `uniform_int(v.size())` is a valid index of `v` |
| `rng.uniform_int<T>()` | Any value of `T` (default `std::uint64_t`) |
| `rng.uniform_real()` | `double` in `[0, 1)` |
| `rng.uniform_real(b)`, `rng.uniform_real(a, b)` | `double` in `[0, b)`, `[a, b)` |
| `rng.bernoulli()` | `true` with probability 1/2 |
| `rng.bernoulli(p)` | `true` with probability `p` |
| `rng.bernoulli(k, n)` | `true` with probability exactly `k / n`, no floating point |
| `rng.shuffle(range)` | Shuffles in place. For part of a vector: `rng.shuffle(std::span(v).subspan(2))` |
| `rng.weighted_index(weights)` | Index chosen with probability proportional to its integer weight |
| `rng.weighted_index(sampler)` | The same, in constant time, with weights prepared in a `WeightedSampler` |

Only `FastRNG`:

| Call | Result |
|---|---|
| `FastRNG(seed)` | Reproducible: a 64-bit seed, expanded with SplitMix64 |
| `FastRNG()` | Not reproducible: 256 bits of state from the operating system |
| `FastRNG(state)` | Starts from an exact 256-bit state (`FastRNG::State`) |
| `next()`, `discard(n)` | Next 64 bits; skip `n` numbers |
| `jump()`, `long_jump()` | Skip 2^128 or 2^192 numbers: independent streams for threads |
| `state()`, `==` | Read the state; compare two generators |

Also available: `alelus::WeightedSampler` (Vose's alias method, for weights used many times),
`alelus::os_random_bytes(span)` (bytes straight from the operating system) and
`alelus::SplitMix64`.

## Guarantees

- **Exact.** `uniform_int`, `bernoulli(k, n)`, `shuffle` and `weighted_index` have no bias at
  all: integer algorithms with rejection (Lemire's method for integers, Fisher–Yates,
  Vose's alias method with integer weights). `uniform_real` returns one of 2^53 equally
  spaced values and never returns its upper bound.
- **Reproducible.** With the same seed, `FastRNG` gives the same results on every compiler and
  operating system, for every method. The `<random>` distributions and `std::shuffle` do not
  promise this: their algorithms differ between standard libraries.
  One known exception: `uniform_real(a, b)` and `uniform_real(b)` compute `a + u * (b - a)`.
  Where the processor has FMA instructions (ARM, or x86 with `-march=haswell` or newer),
  compilers may fuse that into one operation with a single rounding, so the last bit of the
  result can differ from a machine without them. `uniform_real()` is exact everywhere.
- **Tested against references.** xoshiro256** and SplitMix64 against the authors' code,
  ChaCha20 against the RFC 8439 test vectors, the CryptoRNG generator against OpenSSL, and the
  distributions with chi-square tests on fixed seeds.

## Threads

A generator must not be used by two threads at the same time. Give each thread its own:

```cpp
alelus::FastRNG base(42);
std::vector<alelus::FastRNG> per_thread;
for (int t = 0; t < threads; ++t) {
    per_thread.push_back(base);
    base.jump(); // the next copy starts 2^128 numbers later: the streams never overlap
}
```

`CryptoRNG` cannot be copied: create one per thread with `CryptoRNG()`.

## CryptoRNG security

- The key comes from the operating system: `getrandom()` on Linux, `getentropy()` on macOS and
  BSD, `BCryptGenRandom()` on Windows. If that fails, the constructor throws: there is no
  weaker fallback.
- Fast-key-erasure (D. J. Bernstein, 2017): every 124 numbers, part of the ChaCha20 output
  becomes the new key and the old one is wiped. Each number is wiped from memory as it is
  handed out. Someone who reads the memory later cannot recover numbers already produced.
- On Linux and macOS, after `fork()` the child gets a new key, so parent and child never
  share numbers. (Windows has no `fork()`.)
- Key material cannot be copied and is wiped when the generator is destroyed.

Alelus is a learning project and has **not been audited**. For real security needs, prefer
the operating system directly or an audited library such as libsodium.

## Performance

Measured with `make bench` (g++ 13, `-O2`, Intel Xeon at 2.3 GHz). The numbers vary with the
machine and compiler; run `make bench` to get yours.

| Operation | FastRNG | CryptoRNG | Standard library |
|---|---|---|---|
| 64 random bits | 1.0 ns | 16.8 ns | 2.0 ns (`mt19937_64`) |
| `uniform_int(1, 6)` | 2.2 ns | 18.5 ns | 1.6 ns (`uniform_int_distribution` + FastRNG) |
| `uniform_real()` | 1.4 ns | 17.6 ns | 4.8 ns (`uniform_real_distribution` + FastRNG) |
| `shuffle` of 52 cards | 123 ns | 924 ns | 112 ns (`std::shuffle` + FastRNG) |
| `weighted_index`, 100 weights | 82 ns | | |
| `weighted_index`, `WeightedSampler`, 100 weights | 8.3 ns | | |

## Building

You need a C++20 compiler and GNU `make`. Tested with g++ 13 and clang++ 18 on Linux, and with
MinGW on Windows (MSYS2 or Git Bash). `clang-format` is optional, for `make format`.

```sh
make             # build the library
make tests       # build and run the tests
make bench       # measure every operation (always a release build)
make dist        # package headers + release library in dist/alelus/
make format      # format every source file
make clean       # delete build/ and dist/
make help        # all targets and options
```

Options can be combined with any target:

```sh
make tests MODE=release      # optimised build (-O2) instead of debug
make tests WERROR=1          # warnings are errors
make tests CXX=clang++       # another compiler
make tests T="[crypto]"      # only the tests with a tag or name
make bench B="shuffle"       # only the measurements whose name contains it
```

## Using Alelus in another project

Run `make dist`, copy `dist/alelus/` into your project, and compile with:

```sh
g++ -std=c++20 -I alelus/include main.cpp alelus/lib/libalelus.a
```

On Windows, add `-lbcrypt` at the end; on Linux and macOS, add `-pthread`.

## Project layout

```
include/alelus/          public headers (alelus.hpp includes all of them)
include/alelus/detail/   implementation details: ChaCha20, the algorithms, helpers
src/                     library sources
tests/                   unit tests (Catch2); tests/support/ has shared test helpers
bench/                   benchmark (make bench)
```

All text files use Windows line endings (CRLF). `.gitattributes` keeps them that way on every
operating system, and `.clang-format` writes CRLF when it formats.

## References

- D. Blackman, S. Vigna, *Scrambled Linear Pseudorandom Number Generators* (2018),
  <https://prng.di.unimi.it/>
- Y. Nir, A. Langley, *ChaCha20 and Poly1305 for IETF Protocols*, RFC 8439 (2018)
- D. J. Bernstein, *Fast-key-erasure random-number generators* (2017),
  <https://blog.cr.yp.to/20170723-random.html>
- D. Lemire, *Fast Random Integer Generation in an Interval* (2019)
- M. D. Vose, *A Linear Algorithm for Generating Random Numbers with a Given Distribution*
  (1991)
- G. L. Steele, D. Lea, C. H. Flood, *Fast Splittable Pseudorandom Number Generators* (2014)

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md).

## License

MIT. See [LICENSE](LICENSE).
