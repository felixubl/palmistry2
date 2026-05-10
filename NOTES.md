# palmistry2

## What This Is

Palmistry — a header-only C++17 zero-lookup-table 7-card poker hand evaluator. Single evaluator in `include/pokereval/evaluator.hpp`, no dependencies, no lookup tables.

## Build Commands

```sh
make                # build all tools into build/
make check          # build and run correctness tests
make benchmark      # build and run benchmark suite
make clean          # remove build/
```

Exhaustive correctness check over all C(52,7) hands:
```sh
./build/pokereval_check --random 0 --exhaustive
```

CMake alternative:
```sh
cmake -S . -B build-cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-cmake
```

Direct single-file compile:
```sh
c++ -O3 -march=native -std=c++17 -Iinclude tools/check.cpp -o pokereval_check
```

## Architecture

### Evaluator (`include/pokereval/evaluator.hpp`)

`pokereval::Evaluator` — flush-first zero-table evaluator. Interface: `evaluate(Hand)` and `evaluate(std::array<Card,7>)`, returning a `Score` (packed `uint32_t`, higher = better hand).

### Core data types (`types.hpp`)

- `Card` = `uint8_t`: `suit * 16 + rank` (ranks 0..12 = 2..A, suits 0..3)
- `Hand` = `uint64_t`: four 16-bit suit lanes, rank bits at positions 0..12
- `Score` = `uint32_t`: `category(4b) | r0(4b) | r1(4b) | kickers(13b)` (kickers = rank bitmask)

### Internals

- `bitops.hpp` — `popcount32`, `high_bit_index`, `low_bit_index` helpers
- `rank_masks.hpp` — extracts per-suit 13-bit rank masks from a packed `Hand`
- `random.hpp` — deterministic xoshiro256++ RNG for reproducible benchmarks/tests

### Key evaluation flow

1. Pack 7 `Card` values into a `Hand` (four 16-bit suit lanes)
2. Extract rank masks per suit
3. Detect flush (any suit lane with popcount >= 5)
4. If flush: check straight flush, otherwise score top-5 flush ranks
5. If no flush: merge all suit lanes into a combined rank mask, detect straights, then classify by rank multiplicities (quads, full house, trips, pairs, high card)

## Code Style

- LLVM-based clang-format: 4-space indent, 120-column limit
- Spaces everywhere except Makefile (tabs)
- All evaluator code is header-only with `#pragma once`
- `noexcept` on all evaluator functions
