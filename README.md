> **Archived.** This is an early generation of an evaluator that is still being
> worked on. The current one is [palmistry3](https://github.com/felixubl/palmistry3). Its
> [LINEAGE.md](https://github.com/felixubl/palmistry3/blob/main/LINEAGE.md) explains how this fits in, and every
> generation, this one included, lives in that repository too, under
> `history/7-palmistry2/`.

# Poker Evaluator Suite

This repository contains four standalone 7-card Texas Hold'em evaluators with the same public shape and score format:

| Variant | Table data | Purpose |
| --- | ---: | --- |
| `pokereval::nolut::Evaluator` | 0 bytes | Pure arithmetic/bit-operation evaluator. This is the baseline design goal. |
| `pokereval::nolut_flush_first::Evaluator` | 0 bytes | Faster zero-table evaluator. |
| `pokereval::rank_lut::Evaluator` | 24 KiB | Rank-mask table evaluator for popcount, straight end, and high bit. |
| `pokereval::packed_rank_lut::Evaluator` | 48 KiB | Rank-mask table evaluator with packed top-five ranks. |

All four evaluators are header-only and dependency-free. They use the same packed `uint32_t` score, where higher
integer values are better hands.

`nolut_flush_first` keeps the zero-table contract but reuses the detected flush lane and removes defensive zero checks
from internal bit scans whose inputs are already known to be nonzero.

## Layout

```text
include/pokereval/
  types.hpp                    cards, hand packing, score packing
  bitops.hpp                   tiny bit-operation helpers
  rank_masks.hpp               shared rank-mask extraction
  evaluator_nolut.hpp          zero-table evaluator
  evaluator_nolut_flush_first.hpp
                               faster zero-table evaluator
  evaluator_rank_lut.hpp        24 KiB rank-mask table evaluator
  evaluator_packed_rank_lut.hpp 48 KiB packed-rank table evaluator
  oracle.hpp                   slow 21x five-card correctness oracle
  random.hpp                   deterministic benchmark deal generation
  evaluators.hpp               convenience include

tools/check.cpp                random and exhaustive correctness checks
bench/benchmark.cpp            extensive benchmark suite
CMakeLists.txt                 CMake build
Makefile                       simple local build
```

## Build

Simple build:

```sh
make
```

CMake build:

```sh
cmake -S . -B build-cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-cmake
```

Direct compile:

```sh
c++ -O3 -march=native -std=c++17 -Iinclude tools/check.cpp -o pokereval_check
c++ -O3 -march=native -std=c++17 -Iinclude bench/benchmark.cpp -o pokereval_benchmark
```

## Use

```cpp
#include "pokereval/evaluator_nolut.hpp"

#include <array>

int main() {
    pokereval::nolut::Evaluator evaluator;
    std::array<pokereval::Card, 7> cards{{12, 11, 10, 9, 8, 0, 13}};
    pokereval::Score score = evaluator.evaluate(cards);
    return int(pokereval::score_category(score));
}
```

Card IDs are `suit * 13 + rank`.

Ranks are `0..12 == 2,3,4,5,6,7,8,9,T,J,Q,K,A`.

Suits are `0..3`; suit names only matter for display.

## Representation

`Hand` is a `uint64_t` with four 16-bit suit lanes. Rank bits occupy positions `0..12` inside each lane.

`Score` is packed as:

```text
category r0 r1 r2 r3 r4
```

Each rank field uses 4 bits. Categories are:

```text
0 high card
1 one pair
2 two pair
3 trips
4 straight
5 flush
6 full house
7 quads
8 straight flush
```

## Scope And Limits

The current API and tools are built around standard 7-card Hold'em-style evaluation: choose the best 5-card poker
hand from seven distinct cards in a normal 52-card deck.

The core `evaluate(Hand)` shape is more general than the public card-array overloads. The same rank-mask idea should
extend to other hand sizes and poker variants, but that is not a documented contract yet.

Current limitations:

| Case | Status | Reason |
| --- | --- | --- |
| 1-4 cards | Not supported | Scores encode a complete 5-card hand. Incomplete hands need a new comparison convention. |
| 5-6 cards | Likely via `evaluate(Hand)` | Best-5 scoring is complete; helpers and tests target seven cards. |
| 7 cards | Supported target | This is the tested standard use case. |
| 8-9 cards | Likely via `evaluate(Hand)` | Two 5-card flush suits are still impossible. Needs tests and API work. |
| 10+ cards | Not correct as-is | Two suits can both have flushes; ordinary flush selection assumes one. |
| Omaha-style rules | Not supported | "Use exactly N hole cards" needs filtering or a wrapper. |
| Short deck / lowball / wildcards | Not supported | Straight rules, rank order, ace handling, or categories change. |

The first structural limit for larger standard-deck hands is flush selection. Two flush suits require at least 10 cards,
because each flush needs five cards of a suit. The current evaluators are written around the seven-card invariant that
there is at most one flush suit.

## Correctness

Random oracle checks:

```sh
make check
```

Exhaustive checks over all `C(52,7) = 133,784,560` hands:

```sh
./build/pokereval_check --random 0 --exhaustive
```

The checker compares all four evaluators against each other. Random checks also compare against the independent
21-combination five-card oracle.

## Benchmark

Default benchmark:

```sh
make benchmark
```

Short benchmark:

```sh
./build/pokereval_benchmark \
  --hands 2000000 \
  --api-hands 1000000 \
  --stream 250000 \
  --oracle-hands 10000 \
  --check 100000 \
  --reps 3 \
  --cat-target 200 \
  --cat-min-evals 1000000
```

Benchmark sections:

| Section | Meaning |
| --- | --- |
| `stored packed hands` | Times only evaluator core on pre-packed `uint64_t` hands. |
| `stored card-ID deals` | Times seven card IDs -> pack -> evaluate. Also reports pack-only cost. |
| `streaming deal + pack + eval` | Includes RNG, partial shuffle/deal, packing, and evaluation. |
| `21x five-card oracle` | Slow independent baseline. Useful for scale, not intended as an optimized evaluator. |
| `per-category stored packed hands` | Pre-filters hands by final category, then times each evaluator. |

Recent default-run snapshot on an Apple M4 Pro:

```text
stored packed core:
  no-LUT              147.4 M/s
  no-LUT flush-first  165.9 M/s
  rank LUT            163.7 M/s
  packed-rank LUT     165.5 M/s

stored card-ID API:
  no-LUT               85.1 M/s
  no-LUT flush-first   95.1 M/s
  rank LUT             91.0 M/s
  packed-rank LUT      97.3 M/s

stream deal+pack+eval:
  no-LUT               41.2 M/s
  no-LUT flush-first   43.0 M/s
  rank LUT             41.9 M/s
  packed-rank LUT      43.6 M/s
```

The important result is that the no-LUT evaluators are competitive while remaining table-free. The LUT variants make
the cost/benefit explicit; they are not the default design goal.
