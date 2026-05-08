#include "pokereval/evaluators.hpp"
#include "pokereval/oracle.hpp"
#include "pokereval/random.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>

using namespace pokereval;

struct Args {
    size_t random = 100000;
    uint64_t seed = 1;
    bool exhaustive = false;
};

size_t parse_size(int argc, char** argv, const char* name, size_t fallback) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::strcmp(argv[i], name) == 0) {
            char* end = nullptr;
            const unsigned long long value = std::strtoull(argv[i + 1], &end, 10);
            if (!end || *end != '\0') {
                std::cerr << "bad value for " << name << '\n';
                std::exit(2);
            }
            return size_t(value);
        }
    }
    return fallback;
}

uint64_t parse_u64(int argc, char** argv, const char* name, uint64_t fallback) {
    return uint64_t(parse_size(argc, argv, name, size_t(fallback)));
}

bool has_flag(int argc, char** argv, const char* flag) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], flag) == 0) return true;
    }
    return false;
}

void usage(const char* program) {
    std::cout << "Usage: " << program << " [options]\n"
              << "  --random N       random oracle checks   default: 100000\n"
              << "  --seed N         RNG seed                default: 1\n"
              << "  --exhaustive     check all C(52,7) hands\n"
              << "  --help           show this message\n";
}

template <typename... Evaluators>
void check_random(size_t count, uint64_t seed, const Evaluators&... evaluators) {
    if (count == 0) return;

    SplitMix64 rng(seed);
    uint64_t checksum = 0;
    const auto start = std::chrono::steady_clock::now();

    for (size_t i = 0; i < count; ++i) {
        const auto cards = random_cards7(rng);
        const Hand hand = hand_from_cards(cards);
        const Score expected = oracle::evaluate_seven_by_fives(cards);
        const std::array<Score, sizeof...(Evaluators)> scores{{evaluators.evaluate(hand)...}};
        const bool ok = std::all_of(scores.begin(), scores.end(), [expected](Score score) {
            return score == expected;
        });

        if (!ok) {
            std::cerr << "random mismatch at sample " << i << '\n'
                      << "cards:     " << cards_to_string(cards) << '\n'
                      << "oracle:    " << score_to_string(expected) << '\n';
            for (Score score : scores) std::cerr << "candidate: " << score_to_string(score) << '\n';
            std::exit(1);
        }

        checksum += scores.front();
    }

    const auto end = std::chrono::steady_clock::now();
    const double seconds = std::chrono::duration<double>(end - start).count();
    std::cout << "random checks:      " << std::setw(12) << count << " OK in "
              << std::fixed << std::setprecision(3) << std::setw(8) << seconds
              << "s  checksum=" << checksum << '\n';
}

template <typename... Evaluators>
void check_exhaustive(const Evaluators&... evaluators) {
    static constexpr std::array<uint64_t, 9> expected_counts{{
        23294460ull,
        58627800ull,
        31433400ull,
        6461620ull,
        6180020ull,
        4047644ull,
        3473184ull,
        224848ull,
        41584ull
    }};

    std::array<uint64_t, 9> counts{};
    uint64_t total = 0;
    uint64_t checksum = 0;
    const auto start = std::chrono::steady_clock::now();

    for (Card a = 0; a < 46; ++a) {
        for (Card b = Card(a + 1); b < 47; ++b) {
            for (Card c = Card(b + 1); c < 48; ++c) {
                for (Card d = Card(c + 1); d < 49; ++d) {
                    for (Card e = Card(d + 1); e < 50; ++e) {
                        for (Card f = Card(e + 1); f < 51; ++f) {
                            for (Card g = Card(f + 1); g < 52; ++g) {
                                const Hand hand = hand_from_cards(a, b, c, d, e, f, g);
                                const std::array<Score, sizeof...(Evaluators)> scores{{evaluators.evaluate(hand)...}};
                                const Score score = scores.front();
                                const bool ok = std::all_of(scores.begin(), scores.end(), [score](Score candidate) {
                                    return candidate == score;
                                });
                                if (!ok) {
                                    std::cerr << "exhaustive mismatch\n";
                                    for (Score candidate : scores) {
                                        std::cerr << "candidate: " << score_to_string(candidate) << '\n';
                                    }
                                    std::exit(1);
                                }
                                ++counts[score_category(score)];
                                checksum += score;
                                ++total;
                            }
                        }
                    }
                }
            }
        }
    }

    const auto end = std::chrono::steady_clock::now();
    const double seconds = std::chrono::duration<double>(end - start).count();
    const double mps = (double(total) / seconds) / 1'000'000.0;
    std::cout << "exhaustive checks:  " << std::setw(12) << total << " OK in "
              << std::fixed << std::setprecision(3) << std::setw(8) << seconds
              << "s  " << std::setw(10) << mps << " M hands/s"
              << "  checksum=" << checksum << '\n';

    bool ok = total == 133784560ull;
    for (size_t i = 0; i < counts.size(); ++i) {
        ok = ok && counts[i] == expected_counts[i];
        std::cout << "  cat " << i << ' ' << std::left << std::setw(15) << category_name(uint32_t(i)) << std::right
                  << " count=" << std::setw(10) << counts[i]
                  << " expected=" << std::setw(10) << expected_counts[i]
                  << (counts[i] == expected_counts[i] ? " OK" : " MISMATCH") << '\n';
    }

    if (!ok) std::exit(1);
}

int main(int argc, char** argv) {
    if (has_flag(argc, argv, "--help") || has_flag(argc, argv, "-h")) {
        usage(argv[0]);
        return 0;
    }

    const Args args{
        parse_size(argc, argv, "--random", 100000),
        parse_u64(argc, argv, "--seed", 1),
        has_flag(argc, argv, "--exhaustive")
    };

    const nolut::Evaluator no_lut;
    const nolut_flush_first::Evaluator no_lut_flush_first;
    const rank_lut::Evaluator rank_lut;
    const packed_rank_lut::Evaluator packed_rank_lut;

    std::cout << "table bytes:\n"
              << "  no-LUT:      " << no_lut.table_bytes << '\n'
              << "  flush-first: " << no_lut_flush_first.table_bytes << '\n'
              << "  rank LUT:    " << rank_lut.table_bytes << '\n'
              << "  packed LUT:  " << packed_rank_lut.table_bytes << '\n';

    check_random(args.random, args.seed, no_lut, no_lut_flush_first, rank_lut, packed_rank_lut);
    if (args.exhaustive) check_exhaustive(no_lut, no_lut_flush_first, rank_lut, packed_rank_lut);
    return 0;
}
