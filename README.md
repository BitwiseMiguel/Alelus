# Alelus

A C++20 random number library with two generators:

| | `FastRNG` | `CryptoRNG` |
|---|---|---|
| Algorithm | xoshiro256** (Blackman & Vigna) | ChaCha20 (RFC 8439) |
| Reproducible from a seed | Yes | No: keyed from the operating system |
| Use it for | Simulation, Monte Carlo, testing | When unpredictability matters |

> **Status: work in progress.** The project is being built incrementally and nothing is
> usable yet. This README describes the goal and will be updated as features land.

## Goals

- Both generators satisfy `std::uniform_random_bit_generator`, so they work with the
  standard library (`std::shuffle`, `<random>` distributions) as well as with Alelus.
- Exactly uniform integers and reals, with no modulo bias.
- Every algorithm validated against published test vectors or independent references.
- Builds with `make` on Linux, macOS and Windows (MSYS2 or Git Bash with MinGW).

## Requirements

- A C++20 compiler: g++ 11 or newer, or clang++ 14 or newer
- GNU `make`
- Optional: `clang-format`, to format the code (`make format`)

## Building

```sh
make             # build the library
make tests       # build and run the tests
make bench       # measure the speed of every operation (release build)
make dist        # package headers + release library in dist/alelus/
make help        # all targets and options
```

## Using Alelus in another project

Run `make dist`, copy `dist/alelus/` into your project, and compile with:

```sh
g++ -std=c++20 -I alelus/include main.cpp alelus/lib/libalelus.a
```

On Windows, add `-lbcrypt` at the end; on Linux and macOS, add `-pthread`.

## Project layout

```
include/alelus/   public headers
src/              library sources
tests/            unit tests (Catch2)
bench/            benchmark (make bench)
```

All text files use Windows line endings (CRLF). `.gitattributes` keeps them that way on every
operating system, and `.clang-format` writes CRLF when it formats.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md).

## License

MIT. See [LICENSE](LICENSE).
