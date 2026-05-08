#pragma once

#include "pokereval/types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace pokereval {

struct SplitMix64 {
    uint64_t state;

    explicit SplitMix64(uint64_t seed) noexcept : state(seed) {}

    uint64_t next_u64() noexcept {
        state += 0x9E3779B97F4A7C15ull;
        uint64_t z = state;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }

    uint32_t range(uint32_t n) noexcept {
#if defined(__SIZEOF_INT128__)
        return uint32_t((__uint128_t(next_u64()) * n) >> 64);
#else
        return uint32_t(next_u64() % n);
#endif
    }
};

inline std::array<Card, 7> random_cards7(SplitMix64& rng) noexcept {
    std::array<Card, 52> deck{};
    for (Card i = 0; i < deck.size(); ++i) deck[i] = i;

    for (uint32_t i = 0; i < 7; ++i) {
        const uint32_t j = i + rng.range(52u - i);
        const Card tmp = deck[i];
        deck[i] = deck[j];
        deck[j] = tmp;
    }

    return {deck[0], deck[1], deck[2], deck[3], deck[4], deck[5], deck[6]};
}

inline std::vector<std::array<Card, 7>> generate_card_deals(size_t count, uint64_t seed) {
    SplitMix64 rng(seed);
    std::vector<std::array<Card, 7>> deals;
    deals.reserve(count);
    for (size_t i = 0; i < count; ++i) deals.push_back(random_cards7(rng));
    return deals;
}

inline std::vector<Hand> generate_hands(size_t count, uint64_t seed) {
    SplitMix64 rng(seed);
    std::vector<Hand> hands;
    hands.reserve(count);
    for (size_t i = 0; i < count; ++i) hands.push_back(hand_from_cards(random_cards7(rng)));
    return hands;
}

} // namespace pokereval
