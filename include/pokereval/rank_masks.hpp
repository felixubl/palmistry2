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

    // 4-input binary adder network: per-rank popcount across 4 suits
    const uint16_t a = uint16_t(s0 ^ s1);
    const uint16_t b = uint16_t(s0 & s1);
    const uint16_t c = uint16_t(s2 ^ s3);
    const uint16_t d = uint16_t(s2 & s3);

    const uint16_t sum0 = uint16_t(a ^ c);
    const uint16_t carry0 = uint16_t(a & c);
    const uint16_t bd_xor = uint16_t(b ^ d);
    const uint16_t sum1 = uint16_t(bd_xor ^ carry0);
    const uint16_t sum2 = uint16_t((b & d) | (carry0 & bd_xor));

    masks.ranks = uint16_t(sum0 | sum1 | sum2);
    masks.quads = sum2;
    masks.pairs_or_better = uint16_t(sum1 | sum2);
    masks.trips_or_better = uint16_t((sum1 & sum0) | sum2);
    return masks;
}

inline uint16_t exact_trips(const RankMasks& masks) noexcept {
    return uint16_t(masks.trips_or_better & uint16_t(~masks.quads));
}

inline uint16_t exact_pairs(const RankMasks& masks) noexcept {
    return uint16_t(masks.pairs_or_better & uint16_t(~masks.trips_or_better));
}

} // namespace pokereval
