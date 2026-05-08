#include "pokereval/evaluators.hpp"
#include "pokereval/oracle.hpp"
#include "pokereval/random.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

using namespace pokereval;

struct Args {
    size_t hands = 5000000;
    size_t api_hands = 2000000;
    size_t stream = 1000000;
    size_t oracle_hands = 20000;
    size_t check = 100000;
    size_t reps = 5;
    size_t cat_target = 1000;
    size_t cat_min_evals = 5000000;
    uint64_t seed = 1;
    bool skip_check = false;
    bool skip_category = false;
};

struct BenchStats {
    double best = std::numeric_limits<double>::infinity();
    double mean = 0.0;
    double stdev = 0.0;
    uint64_t checksum = 0;
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

uint64_t mix_checksum(uint64_t acc, uint64_t value) noexcept {
    return acc ^ (value + 0x9E3779B97F4A7C15ull + (acc << 6) + (acc >> 2));
}

template <typename Fn>
BenchStats run_reps(size_t reps, Fn&& fn) {
    if (reps == 0) reps = 1;
    BenchStats stats;
    std::vector<double> samples;
    samples.reserve(reps);

    for (size_t rep = 0; rep < reps; ++rep) {
        const auto start = std::chrono::steady_clock::now();
        const uint64_t checksum = fn(rep);
        const auto end = std::chrono::steady_clock::now();
        const double seconds = std::chrono::duration<double>(end - start).count();
        samples.push_back(seconds);
        stats.best = std::min(stats.best, seconds);
        stats.mean += seconds;
        stats.checksum = mix_checksum(stats.checksum, checksum);
    }

    stats.mean /= double(reps);
    double variance = 0.0;
    for (double sample : samples) {
        const double delta = sample - stats.mean;
        variance += delta * delta;
    }
    stats.stdev = std::sqrt(variance / double(reps));
    return stats;
}

void print_line(const std::string& label, size_t count, const char* unit, const BenchStats& stats) {
    const double best_mps = stats.best > 0.0 ? (double(count) / stats.best) / 1'000'000.0 : 0.0;
    const double mean_mps = stats.mean > 0.0 ? (double(count) / stats.mean) / 1'000'000.0 : 0.0;

    std::cout << std::left << std::setw(34) << label << std::right
              << std::setw(12) << count << ' ' << std::setw(7) << unit
              << "  best=" << std::fixed << std::setprecision(4) << std::setw(8) << stats.best << "s"
              << "  " << std::setw(10) << std::setprecision(3) << best_mps << " M/s"
              << "  mean=" << std::setw(8) << std::setprecision(4) << stats.mean << "s"
              << "  " << std::setw(10) << std::setprecision(3) << mean_mps << " M/s"
              << "  sd=" << std::setw(8) << std::setprecision(3) << stats.stdev * 1000.0 << " ms"
              << "  checksum=" << stats.checksum << '\n';
}

std::string mib(size_t bytes) {
    std::ostringstream out;
    out << (bytes / (1024 * 1024)) << " MiB";
    return out.str();
}

void print_environment() {
#if defined(__clang__)
    std::cout << "compiler:           clang " << __clang_version__ << '\n';
#elif defined(__GNUC__)
    std::cout << "compiler:           gcc " << __VERSION__ << '\n';
#else
    std::cout << "compiler:           unknown\n";
#endif
#if defined(__OPTIMIZE__)
    std::cout << "optimization:       enabled\n";
#else
    std::cout << "optimization:       not detected\n";
#endif
    std::cout << "hand bytes:         " << sizeof(Hand) << " packed, "
              << sizeof(std::array<Card, 7>) << " card-ID deal\n";
}

template <typename A, typename B, typename C>
void check_random(const A& no_lut, const B& rank_lut, const C& packed_rank_lut, size_t count, uint64_t seed) {
    if (count == 0) return;
    SplitMix64 rng(seed);
    uint64_t checksum = 0;
    const auto start = std::chrono::steady_clock::now();

    for (size_t i = 0; i < count; ++i) {
        const auto cards = random_cards7(rng);
        const Hand hand = hand_from_cards(cards);
        const Score expected = oracle::evaluate_seven_by_fives(cards);
        const Score s0 = no_lut.evaluate(hand);
        const Score s1 = rank_lut.evaluate(hand);
        const Score s2 = packed_rank_lut.evaluate(hand);
        if (s0 != expected || s1 != expected || s2 != expected) {
            std::cerr << "mismatch at random sample " << i << '\n'
                      << "cards:     " << cards_to_string(cards) << '\n'
                      << "oracle:    " << score_to_string(expected) << '\n'
                      << "no-LUT:    " << score_to_string(s0) << '\n'
                      << "rank LUT:  " << score_to_string(s1) << '\n'
                      << "packed:    " << score_to_string(s2) << '\n';
            std::exit(1);
        }
        checksum += s0;
    }

    const auto end = std::chrono::steady_clock::now();
    const double seconds = std::chrono::duration<double>(end - start).count();
    std::cout << "random check:       " << std::setw(12) << count << " OK in "
              << std::fixed << std::setprecision(3) << std::setw(8) << seconds
              << "s  checksum=" << checksum << '\n';
}

template <typename Evaluator>
BenchStats bench_core(const Evaluator& evaluator, const std::vector<Hand>& hands, size_t reps) {
    return run_reps(reps, [&](size_t) {
        uint64_t checksum = 0;
        for (Hand hand : hands) checksum += evaluator.evaluate(hand);
        return checksum;
    });
}

template <typename Evaluator>
BenchStats bench_card_api(const Evaluator& evaluator, const std::vector<std::array<Card, 7>>& deals, size_t reps) {
    return run_reps(reps, [&](size_t) {
        uint64_t checksum = 0;
        for (const auto& cards : deals) checksum += evaluator.evaluate(cards);
        return checksum;
    });
}

template <typename Evaluator>
BenchStats bench_stream(const Evaluator& evaluator, size_t count, uint64_t seed, size_t reps) {
    return run_reps(reps, [&](size_t rep) {
        SplitMix64 rng(seed + 0xD00D'BEEFull * (rep + 1));
        uint64_t checksum = 0;
        for (size_t i = 0; i < count; ++i) checksum += evaluator.evaluate(random_cards7(rng));
        return checksum;
    });
}

BenchStats bench_pack_only(const std::vector<std::array<Card, 7>>& deals, size_t reps) {
    return run_reps(reps, [&](size_t) {
        uint64_t checksum = 0;
        for (const auto& cards : deals) checksum += hand_from_cards(cards);
        return checksum;
    });
}

BenchStats bench_oracle(const std::vector<std::array<Card, 7>>& deals, size_t reps) {
    return run_reps(reps, [&](size_t) {
        uint64_t checksum = 0;
        for (const auto& cards : deals) checksum += oracle::evaluate_seven_by_fives(cards);
        return checksum;
    });
}

template <typename Classifier>
std::array<std::vector<Hand>, 9> collect_categories(const Classifier& classifier, size_t target, uint64_t seed) {
    std::array<std::vector<Hand>, 9> buckets;
    for (auto& bucket : buckets) bucket.reserve(target);

    SplitMix64 rng(seed);
    size_t filled = 0;
    size_t trials = 0;
    const size_t max_trials = std::max<size_t>(target * 30000, 1000000);

    while (filled < buckets.size() && trials < max_trials) {
        const auto cards = random_cards7(rng);
        const Hand hand = hand_from_cards(cards);
        const uint32_t category = score_category(classifier.evaluate(hand));
        if (category < buckets.size() && buckets[category].size() < target) {
            buckets[category].push_back(hand);
            if (buckets[category].size() == target) ++filled;
        }
        ++trials;
    }

    std::cout << "category collection trials: " << trials << '\n';
    for (size_t i = 0; i < buckets.size(); ++i) {
        std::cout << "  cat " << i << ' ' << std::left << std::setw(15) << category_name(uint32_t(i)) << std::right
                  << " collected=" << std::setw(8) << buckets[i].size();
        if (buckets[i].size() < target) std::cout << " short";
        std::cout << '\n';
    }
    return buckets;
}

template <typename Evaluator>
BenchStats bench_bucket(const Evaluator& evaluator, const std::vector<Hand>& bucket, size_t min_evals, size_t reps) {
    const size_t passes = std::max<size_t>(1, (min_evals + bucket.size() - 1) / bucket.size());
    return run_reps(reps, [&](size_t) {
        uint64_t checksum = 0;
        for (size_t pass = 0; pass < passes; ++pass) {
            for (Hand hand : bucket) checksum += evaluator.evaluate(hand);
        }
        return checksum;
    });
}

void usage(const char* program) {
    std::cout << "Usage: " << program << " [options]\n"
              << "  --hands N           stored packed-hand benchmark       default: 5000000\n"
              << "  --api-hands N       stored card-ID API benchmark       default: 2000000\n"
              << "  --stream N          stream deal+pack+eval benchmark    default: 1000000\n"
              << "  --oracle-hands N    21x five-card oracle benchmark     default: 20000\n"
              << "  --check N           random correctness checks          default: 100000\n"
              << "  --reps N            timed repetitions                  default: 5\n"
              << "  --cat-target N      random hands per category          default: 1000\n"
              << "  --cat-min-evals N   min timed evals per category       default: 5000000\n"
              << "  --seed N            RNG seed                           default: 1\n"
              << "  --no-check          skip random oracle checks\n"
              << "  --no-category       skip per-category benchmark\n"
              << "  --help              show this message\n";
}

int main(int argc, char** argv) {
    if (has_flag(argc, argv, "--help") || has_flag(argc, argv, "-h")) {
        usage(argv[0]);
        return 0;
    }

    const Args args{
        parse_size(argc, argv, "--hands", 5000000),
        parse_size(argc, argv, "--api-hands", 2000000),
        parse_size(argc, argv, "--stream", 1000000),
        parse_size(argc, argv, "--oracle-hands", 20000),
        parse_size(argc, argv, "--check", 100000),
        parse_size(argc, argv, "--reps", 5),
        parse_size(argc, argv, "--cat-target", 1000),
        parse_size(argc, argv, "--cat-min-evals", 5000000),
        parse_u64(argc, argv, "--seed", 1),
        has_flag(argc, argv, "--no-check"),
        has_flag(argc, argv, "--no-category")
    };

    const nolut::Evaluator no_lut;
    const rank_lut::Evaluator rank_lut;
    const packed_rank_lut::Evaluator packed_rank_lut;

    std::cout << "table bytes:\n"
              << "  no-LUT:      " << no_lut.table_bytes << '\n'
              << "  rank LUT:    " << rank_lut.table_bytes << '\n'
              << "  packed LUT:  " << packed_rank_lut.table_bytes << '\n';
    print_environment();
    std::cout << "repetitions:        " << args.reps << "\n\n";

    if (!args.skip_check) {
        check_random(no_lut, rank_lut, packed_rank_lut, args.check, args.seed ^ 0xABCDEFu);
    }

    if (args.hands != 0) {
        std::cout << "\n[stored packed hands]\n";
        const auto hands = generate_hands(args.hands, args.seed);
        std::cout << "stored:             " << mib(hands.size() * sizeof(Hand)) << '\n';
        print_line("no-LUT core", args.hands, "evals", bench_core(no_lut, hands, args.reps));
        print_line("rank LUT core", args.hands, "evals", bench_core(rank_lut, hands, args.reps));
        print_line("packed-rank LUT core", args.hands, "evals", bench_core(packed_rank_lut, hands, args.reps));
    }

    if (args.api_hands != 0) {
        std::cout << "\n[stored card-ID deals]\n";
        const auto deals = generate_card_deals(args.api_hands, args.seed ^ 0x12345678u);
        std::cout << "stored:             " << mib(deals.size() * sizeof(std::array<Card, 7>)) << '\n';
        print_line("pack only", args.api_hands, "hands", bench_pack_only(deals, args.reps));
        print_line("no-LUT card API", args.api_hands, "evals", bench_card_api(no_lut, deals, args.reps));
        print_line("rank LUT card API", args.api_hands, "evals", bench_card_api(rank_lut, deals, args.reps));
        print_line(
            "packed-rank LUT card API",
            args.api_hands,
            "evals",
            bench_card_api(packed_rank_lut, deals, args.reps));
    }

    if (args.stream != 0) {
        std::cout << "\n[streaming deal + pack + eval]\n";
        const uint64_t stream_seed = args.seed ^ 0xBADC0FFEEull;
        print_line("no-LUT stream", args.stream, "evals", bench_stream(no_lut, args.stream, stream_seed, args.reps));
        print_line(
            "rank LUT stream",
            args.stream,
            "evals",
            bench_stream(rank_lut, args.stream, stream_seed, args.reps));
        print_line(
            "packed-rank LUT stream",
            args.stream,
            "evals",
            bench_stream(packed_rank_lut, args.stream, stream_seed, args.reps));
    }

    if (args.oracle_hands != 0) {
        std::cout << "\n[21x five-card oracle]\n";
        const auto deals = generate_card_deals(args.oracle_hands, args.seed ^ 0xCAFEBABEu);
        print_line("oracle 21x eval5", args.oracle_hands, "evals", bench_oracle(deals, std::min<size_t>(args.reps, 3)));
    }

    if (!args.skip_category && args.cat_target != 0 && args.cat_min_evals != 0) {
        std::cout << "\n[per-category stored packed hands]\n";
        const auto buckets = collect_categories(no_lut, args.cat_target, args.seed ^ 0xFACEFEEDull);
        for (size_t category = 0; category < buckets.size(); ++category) {
            if (buckets[category].empty()) continue;
            const size_t passes =
                std::max<size_t>(1, (args.cat_min_evals + buckets[category].size() - 1) / buckets[category].size());
            const size_t timed = passes * buckets[category].size();
            const std::string prefix =
                "cat " + std::to_string(category) + " " + category_name(uint32_t(category)) + " ";
            print_line(
                prefix + "no-LUT",
                timed,
                "evals",
                bench_bucket(no_lut, buckets[category], args.cat_min_evals, args.reps));
            print_line(
                prefix + "rank LUT",
                timed,
                "evals",
                bench_bucket(rank_lut, buckets[category], args.cat_min_evals, args.reps));
            print_line(
                prefix + "packed-rank LUT",
                timed,
                "evals",
                bench_bucket(packed_rank_lut, buckets[category], args.cat_min_evals, args.reps));
        }
    }

    return 0;
}
