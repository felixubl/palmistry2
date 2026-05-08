#pragma once

#include "pokereval/bitops.hpp"
#include "pokereval/rank_masks.hpp"
#include "pokereval/types.hpp"

#include <algorithm>
#include <array>
#include <cstddef>

namespace pokereval::packed_rank_lut {

struct Tables {
    std::array<uint8_t, RankMaskTableSize> popcount{};
    std::array<int8_t, RankMaskTableSize> straight_end{};
    std::array<uint32_t, RankMaskTableSize> top_five{};

    Tables() {
        straight_end.fill(int8_t(-1));

        std::array<uint16_t, 9> runs{};
        for (uint32_t start = 0; start < runs.size(); ++start) {
            runs[start] = uint16_t(((1u << 5) - 1u) << start);
        }

        for (uint32_t value = 0; value < popcount.size(); ++value) {
            const uint16_t mask = uint16_t(value);
            popcount[value] = uint8_t(popcount32(mask));

            uint16_t remaining = mask;
            uint32_t packed = 0;
            for (uint32_t i = 0; i < 5 && remaining != 0; ++i) {
                const uint32_t rank = uint32_t(high_bit_index(remaining));
                packed |= rank << (16u - 4u * i);
                remaining = uint16_t(remaining ^ uint16_t(1u << rank));
            }
            top_five[value] = packed;

            int8_t end = -1;
            for (int start = 8; start >= 0; --start) {
                if ((mask & runs[size_t(start)]) == runs[size_t(start)]) {
                    end = int8_t(start + 4);
                    break;
                }
            }
            if (end < 3 && (mask & WheelMask) == WheelMask) end = 3;
            straight_end[value] = end;
        }
    }
};

struct Evaluator {
    static constexpr size_t table_bytes =
        sizeof(Tables::popcount) + sizeof(Tables::straight_end) + sizeof(Tables::top_five);
    static constexpr const char* name = "packed-rank LUT";

    Tables tables;

    uint32_t popcount13(uint16_t mask) const noexcept {
        return tables.popcount[mask];
    }

    int32_t straight(uint16_t mask) const noexcept {
        return tables.straight_end[mask];
    }

    uint32_t packed_top_five(uint16_t mask) const noexcept {
        return tables.top_five[mask];
    }

    int32_t top_rank(uint16_t mask) const noexcept {
        return mask == 0 ? -1 : int32_t(packed_top_five(mask) >> 16);
    }

    Score evaluate(Hand hand) const noexcept {
        const RankMasks masks = rank_masks(hand);
        const uint32_t pc0 = popcount13(masks.suit[0]);
        const uint32_t pc1 = popcount13(masks.suit[1]);
        const uint32_t pc2 = popcount13(masks.suit[2]);
        const uint32_t pc3 = popcount13(masks.suit[3]);

        int32_t best_straight_flush = -1;
        if (pc0 >= 5) best_straight_flush = straight(masks.suit[0]);
        if (pc1 >= 5) best_straight_flush = std::max(best_straight_flush, straight(masks.suit[1]));
        if (pc2 >= 5) best_straight_flush = std::max(best_straight_flush, straight(masks.suit[2]));
        if (pc3 >= 5) best_straight_flush = std::max(best_straight_flush, straight(masks.suit[3]));
        if (best_straight_flush >= 0) {
            return pack_score(uint32_t(Category::StraightFlush), uint32_t(best_straight_flush), 0, 0, 0, 0);
        }

        if (masks.quads != 0) {
            const uint32_t rank = uint32_t(top_rank(masks.quads));
            const uint16_t kickers = clear_rank(masks.ranks, rank);
            return (uint32_t(Category::Quads) << 20) | (rank << 16) | ((packed_top_five(kickers) >> 4) & 0x0F000u);
        }

        const uint16_t trips = exact_trips(masks);
        if (trips != 0) {
            const uint32_t trip_rank = uint32_t(top_rank(trips));
            const uint16_t pair_ranks = clear_rank(masks.pairs_or_better, trip_rank);
            if (pair_ranks != 0) {
                return pack_score(uint32_t(Category::FullHouse), trip_rank, uint32_t(top_rank(pair_ranks)), 0, 0, 0);
            }
        }

        uint16_t flush = 0;
        if (pc0 >= 5) flush = masks.suit[0];
        else if (pc1 >= 5) flush = masks.suit[1];
        else if (pc2 >= 5) flush = masks.suit[2];
        else if (pc3 >= 5) flush = masks.suit[3];
        if (flush != 0) {
            return (uint32_t(Category::Flush) << 20) | packed_top_five(flush);
        }

        const int32_t straight_end = straight(masks.ranks);
        if (straight_end >= 0) {
            return pack_score(uint32_t(Category::Straight), uint32_t(straight_end), 0, 0, 0, 0);
        }

        if (trips != 0) {
            const uint32_t rank = uint32_t(top_rank(trips));
            const uint16_t kickers = clear_rank(masks.ranks, rank);
            return (uint32_t(Category::Trips) << 20) | (rank << 16) | ((packed_top_five(kickers) >> 4) & 0x0FF00u);
        }

        const uint16_t pairs = exact_pairs(masks);
        if (pairs != 0 && (pairs & uint16_t(pairs - 1)) != 0) {
            const uint32_t packed_pairs = packed_top_five(pairs);
            const uint32_t p0 = (packed_pairs >> 16) & 0xFu;
            const uint32_t p1 = (packed_pairs >> 12) & 0xFu;
            const uint16_t kickers = uint16_t(masks.ranks & uint16_t(~uint16_t((1u << p0) | (1u << p1))));
            return (uint32_t(Category::TwoPair) << 20) | (p0 << 16) | (p1 << 12) |
                   ((packed_top_five(kickers) >> 8) & 0x00F00u);
        }

        if (pairs != 0) {
            const uint32_t rank = uint32_t(top_rank(pairs));
            const uint16_t kickers = clear_rank(masks.ranks, rank);
            return (uint32_t(Category::Pair) << 20) | (rank << 16) | ((packed_top_five(kickers) >> 4) & 0x0FFF0u);
        }

        return packed_top_five(masks.ranks);
    }

    Score evaluate(const std::array<Card, 7>& cards) const noexcept {
        return evaluate(hand_from_cards(cards));
    }
};

} // namespace pokereval::packed_rank_lut
