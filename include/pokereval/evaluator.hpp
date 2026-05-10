#pragma once

#include "pokereval/bitops.hpp"
#include "pokereval/rank_masks.hpp"
#include "pokereval/types.hpp"

#include <array>
#include <cstddef>

namespace pokereval {

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

namespace detail {

inline uint16_t flush_mask(const RankMasks& masks, uint32_t pc0, uint32_t pc1, uint32_t pc2, uint32_t pc3) noexcept {
    if (pc0 >= 5) return masks.suit[0];
    if (pc1 >= 5) return masks.suit[1];
    if (pc2 >= 5) return masks.suit[2];
    if (pc3 >= 5) return masks.suit[3];
    return 0;
}

} // namespace detail

struct Evaluator {
    static constexpr size_t table_bytes = 0;
    static constexpr const char* name = "no-LUT flush-first";

    Score evaluate(Hand hand) const noexcept {
        const RankMasks masks = rank_masks(hand);
        const uint32_t pc0 = popcount13(masks.suit[0]);
        const uint32_t pc1 = popcount13(masks.suit[1]);
        const uint32_t pc2 = popcount13(masks.suit[2]);
        const uint32_t pc3 = popcount13(masks.suit[3]);
        const uint16_t flush = detail::flush_mask(masks, pc0, pc1, pc2, pc3);

        if (flush != 0) {
            const int32_t sf = straight_end(flush);
            if (sf >= 0) {
                return pack_score(uint32_t(Category::StraightFlush), uint32_t(sf), 0, uint16_t(0));
            }
        }

        if (masks.quads != 0) {
            const uint32_t rank = top_rank_nonzero(masks.quads);
            const uint16_t kickers = clear_rank(masks.ranks, rank);
            return pack_score(uint32_t(Category::Quads), rank, 0,
                              uint16_t(1u << top_rank_nonzero(kickers)));
        }

        const uint16_t trips = exact_trips(masks);
        if (trips != 0) {
            const uint32_t trip_rank = top_rank_nonzero(trips);
            const uint16_t pair_ranks = clear_rank(masks.pairs_or_better, trip_rank);
            if (pair_ranks != 0) {
                return pack_score(uint32_t(Category::FullHouse), trip_rank,
                                  top_rank_nonzero(pair_ranks), uint16_t(0));
            }
        }

        if (flush != 0) {
            uint16_t fmask = flush;
            const uint32_t n = popcount13(fmask);
            if (n > 6) fmask &= uint16_t(fmask - 1);
            if (n > 5) fmask &= uint16_t(fmask - 1);
            return pack_score(uint32_t(Category::Flush), 0, 0, fmask);
        }

        const int32_t straight = straight_end(masks.ranks);
        if (straight >= 0) {
            return pack_score(uint32_t(Category::Straight), uint32_t(straight), 0, uint16_t(0));
        }

        if (trips != 0) {
            const uint32_t rank = top_rank_nonzero(trips);
            return pack_score(uint32_t(Category::Trips), rank, 0,
                              strip_bottom_2(clear_rank(masks.ranks, rank)));
        }

        const uint16_t pairs = exact_pairs(masks);
        if (pairs != 0 && (pairs & uint16_t(pairs - 1)) != 0) {
            const uint32_t p0 = top_rank_nonzero(pairs);
            const uint32_t p1 = top_rank_nonzero(clear_rank(pairs, p0));
            const uint16_t kickers = uint16_t(masks.ranks & uint16_t(~uint16_t((1u << p0) | (1u << p1))));
            return pack_score(uint32_t(Category::TwoPair), p0, p1,
                              uint16_t(1u << top_rank_nonzero(kickers)));
        }

        if (pairs != 0) {
            const uint32_t rank = top_rank_nonzero(pairs);
            return pack_score(uint32_t(Category::Pair), rank, 0,
                              strip_bottom_2(clear_rank(masks.ranks, rank)));
        }

        return pack_score(uint32_t(Category::HighCard), 0, 0, strip_bottom_2(masks.ranks));
    }

    Score evaluate(const std::array<Card, 7>& cards) const noexcept {
        return evaluate(hand_from_cards(cards));
    }
};

} // namespace pokereval
