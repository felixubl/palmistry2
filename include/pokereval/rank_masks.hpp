#pragma once

#include "pokereval/types.hpp"

#include <array>

namespace pokereval {

struct RankMasks {
    std::array<uint16_t, SuitCount> suit;
    uint16_t ranks;
    uint16_t pairs_or_better;
    uint16_t trips_or_better;
    uint16_t quads;
};

inline RankMasks rank_masks(Hand hand) noexcept {
    RankMasks masks{};
    masks.suit = {{
        uint16_t(hand & RankMask),
        uint16_t((hand >> 16) & RankMask),
        uint16_t((hand >> 32) & RankMask),
        uint16_t((hand >> 48) & RankMask),
    }};

    const uint16_t s0 = masks.suit[0];
    const uint16_t s1 = masks.suit[1];
    const uint16_t s2 = masks.suit[2];
    const uint16_t s3 = masks.suit[3];

    masks.ranks = uint16_t(s0 | s1 | s2 | s3);
    masks.quads = uint16_t(s0 & s1 & s2 & s3);
    masks.pairs_or_better = uint16_t((s0 & s1) | (s0 & s2) | (s0 & s3) | (s1 & s2) | (s1 & s3) | (s2 & s3));
    masks.trips_or_better = uint16_t((s0 & s1 & s2) | (s0 & s1 & s3) | (s0 & s2 & s3) | (s1 & s2 & s3));
    return masks;
}

inline uint16_t exact_trips(const RankMasks& masks) noexcept {
    return uint16_t(masks.trips_or_better & uint16_t(~masks.quads));
}

inline uint16_t exact_pairs(const RankMasks& masks) noexcept {
    return uint16_t(masks.pairs_or_better & uint16_t(~masks.trips_or_better));
}

} // namespace pokereval
