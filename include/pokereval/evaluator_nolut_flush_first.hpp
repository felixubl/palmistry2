#pragma once

#include "pokereval/bitops.hpp"
#include "pokereval/rank_masks.hpp"
#include "pokereval/types.hpp"

#include <array>
#include <cstddef>

namespace pokereval::nolut_flush_first_detail {

inline uint32_t popcount13(uint16_t mask) noexcept {
    return popcount32(mask);
}

inline uint32_t top_rank_nonzero(uint32_t mask) noexcept {
#if defined(__GNUC__) || defined(__clang__)
    return uint32_t(31u - uint32_t(__builtin_clz(mask)));
#else
    return uint32_t(high_bit_index(mask));
#endif
}

inline int32_t straight_end(uint16_t mask) noexcept {
    const uint16_t starts = uint16_t(mask & (mask >> 1) & (mask >> 2) & (mask >> 3) & (mask >> 4));
    if (starts != 0) return int32_t(top_rank_nonzero(starts) + 4);
    return (mask & WheelMask) == WheelMask ? 3 : -1;
}

inline uint32_t take_top_rank(uint16_t& mask) noexcept {
    const uint32_t rank = top_rank_nonzero(mask);
    mask = uint16_t(mask ^ uint16_t(1u << rank));
    return rank;
}

inline Score top_five_score(uint32_t category, uint16_t mask) noexcept {
    const uint32_t r0 = take_top_rank(mask);
    const uint32_t r1 = take_top_rank(mask);
    const uint32_t r2 = take_top_rank(mask);
    const uint32_t r3 = take_top_rank(mask);
    const uint32_t r4 = take_top_rank(mask);
    return pack_score(category, r0, r1, r2, r3, r4);
}

inline uint16_t flush_mask(const RankMasks& masks, uint32_t pc0, uint32_t pc1, uint32_t pc2, uint32_t pc3) noexcept {
    if (pc0 >= 5) return masks.suit[0];
    if (pc1 >= 5) return masks.suit[1];
    if (pc2 >= 5) return masks.suit[2];
    if (pc3 >= 5) return masks.suit[3];
    return 0;
}

} // namespace pokereval::nolut_flush_first_detail

namespace pokereval::nolut_flush_first {

struct Evaluator {
    static constexpr size_t table_bytes = 0;
    static constexpr const char* name = "no-LUT flush-first";

    Score evaluate(Hand hand) const noexcept {
        using namespace nolut_flush_first_detail;

        const RankMasks masks = rank_masks(hand);
        const uint32_t pc0 = popcount13(masks.suit[0]);
        const uint32_t pc1 = popcount13(masks.suit[1]);
        const uint32_t pc2 = popcount13(masks.suit[2]);
        const uint32_t pc3 = popcount13(masks.suit[3]);
        const uint16_t flush = flush_mask(masks, pc0, pc1, pc2, pc3);

        if (flush != 0) {
            const int32_t straight_flush = straight_end(flush);
            if (straight_flush >= 0) {
                return pack_score(uint32_t(Category::StraightFlush), uint32_t(straight_flush), 0, 0, 0, 0);
            }
        }

        if (masks.quads != 0) {
            const uint32_t rank = top_rank_nonzero(masks.quads);
            uint16_t kickers = clear_rank(masks.ranks, rank);
            return pack_score(uint32_t(Category::Quads), rank, take_top_rank(kickers), 0, 0, 0);
        }

        const uint16_t trips = exact_trips(masks);
        if (trips != 0) {
            const uint32_t trip_rank = top_rank_nonzero(trips);
            const uint16_t pair_ranks = clear_rank(masks.pairs_or_better, trip_rank);
            if (pair_ranks != 0) {
                return pack_score(uint32_t(Category::FullHouse), trip_rank, top_rank_nonzero(pair_ranks), 0, 0, 0);
            }
        }

        if (flush != 0) {
            return top_five_score(uint32_t(Category::Flush), flush);
        }

        const int32_t straight = straight_end(masks.ranks);
        if (straight >= 0) {
            return pack_score(uint32_t(Category::Straight), uint32_t(straight), 0, 0, 0, 0);
        }

        if (trips != 0) {
            const uint32_t rank = top_rank_nonzero(trips);
            uint16_t kickers = clear_rank(masks.ranks, rank);
            const uint32_t k0 = take_top_rank(kickers);
            const uint32_t k1 = take_top_rank(kickers);
            return pack_score(uint32_t(Category::Trips), rank, k0, k1, 0, 0);
        }

        const uint16_t pairs = exact_pairs(masks);
        if (pairs != 0 && (pairs & uint16_t(pairs - 1)) != 0) {
            const uint32_t p0 = top_rank_nonzero(pairs);
            const uint32_t p1 = top_rank_nonzero(clear_rank(pairs, p0));
            uint16_t kickers = uint16_t(masks.ranks & uint16_t(~uint16_t((1u << p0) | (1u << p1))));
            return pack_score(uint32_t(Category::TwoPair), p0, p1, take_top_rank(kickers), 0, 0);
        }

        if (pairs != 0) {
            const uint32_t rank = top_rank_nonzero(pairs);
            uint16_t kickers = clear_rank(masks.ranks, rank);
            const uint32_t k0 = take_top_rank(kickers);
            const uint32_t k1 = take_top_rank(kickers);
            const uint32_t k2 = take_top_rank(kickers);
            return pack_score(uint32_t(Category::Pair), rank, k0, k1, k2, 0);
        }

        return top_five_score(uint32_t(Category::HighCard), masks.ranks);
    }

    Score evaluate(const std::array<Card, 7>& cards) const noexcept {
        return evaluate(hand_from_cards(cards));
    }
};

} // namespace pokereval::nolut_flush_first
