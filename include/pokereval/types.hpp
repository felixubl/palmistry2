#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>

namespace pokereval {

using Card = uint8_t;
using Hand = uint64_t;
using Score = uint32_t;

constexpr uint32_t RankCount = 13;
constexpr uint32_t SuitCount = 4;
constexpr uint32_t SuitLaneBits = 16;
constexpr size_t RankMaskTableSize = size_t(1) << RankCount;
constexpr uint16_t RankMask = uint16_t((1u << RankCount) - 1u);
constexpr uint16_t WheelMask = uint16_t((1u << 12) | (1u << 0) | (1u << 1) | (1u << 2) | (1u << 3));

enum class Category : uint32_t {
    HighCard = 0,
    Pair = 1,
    TwoPair = 2,
    Trips = 3,
    Straight = 4,
    Flush = 5,
    FullHouse = 6,
    Quads = 7,
    StraightFlush = 8
};

inline Score pack_score(uint32_t category, uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3, uint32_t r4) noexcept {
    return (category << 20) | (r0 << 16) | (r1 << 12) | (r2 << 8) | (r3 << 4) | r4;
}

inline uint32_t score_category(Score score) noexcept {
    return (score >> 20) & 0xFu;
}

inline uint32_t score_rank(Score score, uint32_t index) noexcept {
    return (score >> (16u - 4u * index)) & 0xFu;
}

inline const char* category_name(uint32_t category) noexcept {
    static constexpr const char* names[] = {
        "high card",
        "one pair",
        "two pair",
        "trips",
        "straight",
        "flush",
        "full house",
        "quads",
        "straight flush"
    };
    return category < 9 ? names[category] : "unknown";
}

inline uint32_t card_rank(Card card) noexcept {
    return uint32_t(card % RankCount);
}

inline uint32_t card_suit(Card card) noexcept {
    return uint32_t(card / RankCount);
}

inline Hand card_bit(Card card) noexcept {
    return Hand(1) << (card_suit(card) * SuitLaneBits + card_rank(card));
}

inline void add_card(Hand& hand, Card card) noexcept {
    hand |= card_bit(card);
}

inline Hand hand_from_cards(const std::array<Card, 7>& cards) noexcept {
    Hand hand = 0;
    add_card(hand, cards[0]);
    add_card(hand, cards[1]);
    add_card(hand, cards[2]);
    add_card(hand, cards[3]);
    add_card(hand, cards[4]);
    add_card(hand, cards[5]);
    add_card(hand, cards[6]);
    return hand;
}

inline Hand hand_from_cards(Card c0, Card c1, Card c2, Card c3, Card c4, Card c5, Card c6) noexcept {
    Hand hand = 0;
    add_card(hand, c0);
    add_card(hand, c1);
    add_card(hand, c2);
    add_card(hand, c3);
    add_card(hand, c4);
    add_card(hand, c5);
    add_card(hand, c6);
    return hand;
}

inline uint16_t clear_rank(uint16_t mask, uint32_t rank) noexcept {
    return uint16_t(mask & uint16_t(~uint16_t(1u << rank)));
}

inline std::string card_to_string(Card card) {
    static constexpr char ranks[] = "23456789TJQKA";
    static constexpr char suits[] = "cdhs";
    std::string text;
    text.push_back(ranks[card_rank(card)]);
    text.push_back(suits[card_suit(card)]);
    return text;
}

inline std::string cards_to_string(const std::array<Card, 7>& cards) {
    std::string text;
    for (size_t i = 0; i < cards.size(); ++i) {
        if (i != 0) text.push_back(' ');
        text += card_to_string(cards[i]);
    }
    return text;
}

inline std::string score_to_string(Score score) {
    std::ostringstream out;
    out << "cat=" << score_category(score) << "(" << category_name(score_category(score)) << "), ranks=["
        << score_rank(score, 0) << ','
        << score_rank(score, 1) << ','
        << score_rank(score, 2) << ','
        << score_rank(score, 3) << ','
        << score_rank(score, 4) << "], raw=0x";
    out.setf(std::ios::hex, std::ios::basefield);
    out.width(6);
    out.fill('0');
    out << score;
    return out.str();
}

} // namespace pokereval
