#pragma once

#include <cstdint>

namespace pokereval {

inline uint32_t popcount32(uint32_t value) noexcept {
#if defined(__GNUC__) || defined(__clang__)
    return uint32_t(__builtin_popcount(value));
#else
    uint32_t count = 0;
    while (value != 0) {
        value &= value - 1;
        ++count;
    }
    return count;
#endif
}

inline int32_t high_bit_index(uint32_t value) noexcept {
    if (value == 0) return -1;
#if defined(__GNUC__) || defined(__clang__)
    return int32_t(31 - __builtin_clz(value));
#else
    int32_t index = -1;
    while (value != 0) {
        value >>= 1;
        ++index;
    }
    return index;
#endif
}

} // namespace pokereval
