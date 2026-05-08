#pragma once

#include "pokereval/evaluator_nolut.hpp"
#include "pokereval/types.hpp"

#include <array>
#include <cstddef>

namespace pokereval::oracle {

inline Score evaluate_five(const std::array<Card, 5>& cards) noexcept {
    std::array<uint8_t, RankCount> counts{};
    uint16_t ranks = 0;
    const uint32_t first_suit = card_suit(cards[0]);
    bool flush = true;

    for (Card card : cards) {
        flush = flush && card_suit(card) == first_suit;
        ++counts[card_rank(card)];
        ranks = uint16_t(ranks | uint16_t(1u << card_rank(card)));
    }

    const int32_t straight = nolut::straight_end(ranks);
    if (flush && straight >= 0) {
        return pack_score(uint32_t(Category::StraightFlush), uint32_t(straight), 0, 0, 0, 0);
    }

    int32_t quads = -1;
    std::array<int32_t, 2> trips{{-1, -1}};
    std::array<int32_t, 2> pairs{{-1, -1}};
    std::array<int32_t, 5> singles{{-1, -1, -1, -1, -1}};
    uint32_t trip_count = 0;
    uint32_t pair_count = 0;
    uint32_t single_count = 0;

    for (int32_t rank = 12; rank >= 0; --rank) {
        switch (counts[size_t(rank)]) {
            case 4:
                quads = rank;
                break;
            case 3:
                if (trip_count < trips.size()) trips[trip_count] = rank;
                ++trip_count;
                break;
            case 2:
                if (pair_count < pairs.size()) pairs[pair_count] = rank;
                ++pair_count;
                break;
            case 1:
                singles[single_count++] = rank;
                break;
            default:
                break;
        }
    }

    if (quads >= 0) {
        return pack_score(uint32_t(Category::Quads), uint32_t(quads), uint32_t(singles[0]), 0, 0, 0);
    }
    if (trip_count > 0 && pair_count > 0) {
        return pack_score(uint32_t(Category::FullHouse), uint32_t(trips[0]), uint32_t(pairs[0]), 0, 0, 0);
    }
    if (flush) {
        return nolut::top_five_score(uint32_t(Category::Flush), ranks);
    }
    if (straight >= 0) {
        return pack_score(uint32_t(Category::Straight), uint32_t(straight), 0, 0, 0, 0);
    }
    if (trip_count > 0) {
        return pack_score(
            uint32_t(Category::Trips),
            uint32_t(trips[0]),
            uint32_t(singles[0]),
            uint32_t(singles[1]),
            0,
            0);
    }
    if (pair_count >= 2) {
        return pack_score(
            uint32_t(Category::TwoPair),
            uint32_t(pairs[0]),
            uint32_t(pairs[1]),
            uint32_t(singles[0]),
            0,
            0);
    }
    if (pair_count == 1) {
        return pack_score(
            uint32_t(Category::Pair),
            uint32_t(pairs[0]),
            uint32_t(singles[0]),
            uint32_t(singles[1]),
            uint32_t(singles[2]),
            0);
    }
    return nolut::top_five_score(uint32_t(Category::HighCard), ranks);
}

inline Score evaluate_seven_by_fives(const std::array<Card, 7>& cards) noexcept {
    Score best = 0;
    for (uint32_t omit_a = 0; omit_a < 7; ++omit_a) {
        for (uint32_t omit_b = omit_a + 1; omit_b < 7; ++omit_b) {
            std::array<Card, 5> five{};
            uint32_t out = 0;
            for (uint32_t i = 0; i < 7; ++i) {
                if (i != omit_a && i != omit_b) five[out++] = cards[i];
            }
            const Score score = evaluate_five(five);
            if (score > best) best = score;
        }
    }
    return best;
}

} // namespace pokereval::oracle
