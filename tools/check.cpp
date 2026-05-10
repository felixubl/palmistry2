#include "pokereval/evaluator.hpp"
#include "pokereval/random.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>

using namespace pokereval;

struct Args {
    size_t random = 1000000;
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
              << "  --random N       random hands to evaluate  default: 1000000\n"
              << "  --seed N         RNG seed                  default: 1\n"
              << "  --exhaustive     check all C(52,7) hands\n"
              << "  --help           show this message\n";
}

void check_random(const Evaluator& evaluator, size_t count, uint64_t seed) {
    if (count == 0) return;

    SplitMix64 rng(seed);
    std::array<uint64_t, 9> counts{};
    uint64_t checksum = 0;
    const auto start = std::chrono::steady_clock::now();

    for (size_t i = 0; i < count; ++i) {
        const auto cards = random_cards7(rng);
        const Hand hand = hand_from_cards(cards);
        const Score score = evaluator.evaluate(hand);
        ++counts[score_category(score)];
        checksum += score;
    }

    const auto end = std::chrono::steady_clock::now();
    const double seconds = std::chrono::duration<double>(end - start).count();
    const double mps = (double(count) / seconds) / 1'000'000.0;
    std::cout << "random hands:       " << std::setw(12) << count << " in "
              << std::fixed << std::setprecision(3) << std::setw(8) << seconds
              << "s  " << std::setw(10) << mps << " M hands/s"
              << "  checksum=" << checksum << '\n';
    for (size_t i = 0; i < counts.size(); ++i) {
        std::cout << "  cat " << i << ' ' << std::left << std::setw(15) << category_name(uint32_t(i)) << std::right
                  << " count=" << std::setw(10) << counts[i] << '\n';
    }
}

void check_exhaustive(const Evaluator& evaluator) {
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

    std::array<Card, DeckSize> deck{};
    for (uint32_t s = 0; s < SuitCount; ++s)
        for (uint32_t r = 0; r < RankCount; ++r)
            deck[s * RankCount + r] = make_card(r, s);

    std::array<uint64_t, 9> counts{};
    uint64_t total = 0;
    uint64_t checksum = 0;
    const auto start = std::chrono::steady_clock::now();

    for (uint32_t a = 0; a < DeckSize - 6; ++a) {
        for (uint32_t b = a + 1; b < DeckSize - 5; ++b) {
            for (uint32_t c = b + 1; c < DeckSize - 4; ++c) {
                for (uint32_t d = c + 1; d < DeckSize - 3; ++d) {
                    for (uint32_t e = d + 1; e < DeckSize - 2; ++e) {
                        for (uint32_t f = e + 1; f < DeckSize - 1; ++f) {
                            for (uint32_t g = f + 1; g < DeckSize; ++g) {
                                const Hand hand = hand_from_cards(deck[a], deck[b], deck[c], deck[d], deck[e], deck[f], deck[g]);
                                const Score score = evaluator.evaluate(hand);
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
    std::cout << "exhaustive:         " << std::setw(12) << total << " in "
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
        parse_size(argc, argv, "--random", 1000000),
        parse_u64(argc, argv, "--seed", 1),
        has_flag(argc, argv, "--exhaustive")
    };

    const Evaluator evaluator;

    std::cout << "evaluator:          " << evaluator.name << '\n'
              << "table bytes:        " << evaluator.table_bytes << '\n';

    check_random(evaluator, args.random, args.seed);
    if (args.exhaustive) check_exhaustive(evaluator);
    return 0;
}
