# Palmistry

A zero-lookup-table 7-card poker hand evaluator.

Header-only C++17. Zero dependencies. Zero lookup tables. Single header include.

```cpp
#include "pokereval/evaluator.hpp"

pokereval::Evaluator evaluator;
std::array<pokereval::Card, 7> cards{{12, 11, 10, 9, 8, 0, 16}};
pokereval::Score score = evaluator.evaluate(cards);
// higher score = better hand
```

## API

**Card** — `uint8_t`, encoded as `suit * 16 + rank`. Ranks `0..12` map to `2,3,4,5,6,7,8,9,T,J,Q,K,A`. Suits `0..3`.

**Hand** — `uint64_t`, four 16-bit suit lanes with rank bits at positions `0..12`. Constructed via `hand_from_cards()`.

**Score** — `uint32_t`, packed as `category(4b) | r0(4b) | r1(4b) | kickers(13b)`. Higher integer = better hand. Scores are directly comparable with `<`, `>`, `==`. The kicker field is a 13-bit rank bitmask (top N ranks kept).

**Evaluator::evaluate** — two overloads:
- `evaluate(Hand)` — evaluates a pre-packed hand
- `evaluate(std::array<Card, 7>)` — packs and evaluates

**Categories** (from `score_category(score)`):

| Value | Hand |
| ---: | --- |
| 0 | High card |
| 1 | One pair |
| 2 | Two pair |
| 3 | Three of a kind |
| 4 | Straight |
| 5 | Flush |
| 6 | Full house |
| 7 | Four of a kind |
| 8 | Straight flush |

## How it works

1. Pack 7 cards into a `Hand` (four 16-bit suit lanes)
2. Extract per-suit 13-bit rank masks
3. Detect flush (any suit with popcount >= 5)
4. If flush: check for straight flush, otherwise score top-5 flush ranks
5. If no flush: check quads, full house, flush (deferred), straight, trips, two pair, pair, high card

The evaluator uses only arithmetic and bit operations — no precomputed tables, no lookup arrays. The flush suit is detected first and reused, avoiding redundant work in the non-flush path.

## Performance

Benchmarked on Apple M4 Pro, clang 17.0.0, `-O3 -march=native`.

All three evaluators compiled into the same binary with the same flags, same random hands, same timing methodology. Hands are generated from a neutral `(rank, suit)` representation and converted to each library's native format.

### Level 1 — pre-packed `evaluate()` (algorithm only)

Hands are pre-converted to each library's native hand type. Only `evaluate()` is timed.

| Evaluator | Table data | Throughput |
| --- | ---: | ---: |
| Palmistry | 0 bytes | 173 M/s |
| ACE_eval | 0 bytes | 180 M/s |
| OMPEval | 124 KiB | 1,674 M/s |

### Level 2 — cards-to-score pipeline (realistic end-to-end)

Starting from neutral `(rank, suit)` cards. Conversion + packing + evaluation are all timed.

| Evaluator | Table data | Throughput |
| --- | ---: | ---: |
| Palmistry | 0 bytes | 92 M/s |
| ACE_eval | 0 bytes | 61 M/s |
| OMPEval | 124 KiB | 446 M/s |

### Standalone benchmark detail

| Level | Throughput |
| --- | ---: |
| Pre-packed core | 173 M/s |
| Card-array API (pack + eval) | 103 M/s |
| Streaming (RNG + deal + pack + eval) | 48 M/s |

### Summary

Palmistry matches ACE_eval on raw algorithm speed (within 4%) while beating it by 50% on the realistic cards-to-score pipeline thanks to more efficient packing. OMPEval is ~10x faster on raw evaluation but uses 124 KiB of precomputed lookup tables.

## Code size

The core evaluator (`evaluator.hpp` plus its three internal headers) is ~350 lines of C++. In a code-golf minification, palmistry compresses to about 1,328 bytes vs ACE_eval's 577 bytes — roughly 2.3x larger for a more readable, maintainable implementation with comparable throughput.

## Build

```sh
make                # build check + benchmark into build/
make check          # run correctness tests
make benchmark      # run benchmark suite
make clean          # remove build/
```

CMake:

```sh
cmake -S . -B build-cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-cmake
```

Direct compile:

```sh
c++ -O3 -march=native -std=c++17 -Iinclude tools/check.cpp -o pokereval_check
```

## Correctness

Random checks (1M hands by default):

```sh
make check
```

Exhaustive check over all C(52,7) = 133,784,560 hands:

```sh
./build/pokereval_check --random 0 --exhaustive
```

The exhaustive check verifies that every hand produces the correct category count — matching the known combinatorial distribution exactly.

## Files

```
include/pokereval/
  evaluator.hpp        the evaluator (flush-first, zero tables)
  types.hpp            Card, Hand, Score types and packing
  bitops.hpp           popcount, high/low bit index
  rank_masks.hpp       per-suit rank mask extraction
  random.hpp           deterministic xoshiro256++ RNG

tools/check.cpp        correctness checker
bench/benchmark.cpp    benchmark suite
Makefile               simple build
CMakeLists.txt         CMake build
```

## License

GNU General Public License v3. See [LICENSE](LICENSE).
