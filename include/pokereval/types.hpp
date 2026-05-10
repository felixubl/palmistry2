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
constexpr uint32_t CardSuitStride = 16;
constexpr uint32_t DeckSize = 52;
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

inline Score pack_score(uint32_t category, uint32_t r0, uint32_t r1, uint16_t kickers) noexcept {
    return (category << 21) | (r0 << 17) | (r1 << 13) | uint32_t(kickers);
}

inline uint32_t score_category(Score score) noexcept {
    return (score >> 21) & 0xFu;
}

inline uint32_t score_primary_rank(Score score, uint32_t index) noexcept {
    return (score >> (17u - 4u * index)) & 0xFu;
}

inline uint16_t score_kicker_mask(Score score) noexcept {
    return uint16_t(score & RankMask);
}

inline uint16_t strip_bottom_2(uint16_t mask) noexcept {
    mask &= uint16_t(mask - 1);
    mask &= uint16_t(mask - 1);
    return mask;
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

inline constexpr Card make_card(uint32_t rank, uint32_t suit) noexcept {
    return Card(suit * CardSuitStride + rank);
}

inline uint32_t card_rank(Card card) noexcept {
    return uint32_t(card & 0xFu);
}

inline uint32_t card_suit(Card card) noexcept {
    return uint32_t(card >> 4);
}

inline Hand card_bit(Card card) noexcept {
    return Hand(1) << card;
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
    static constexpr char rank_chars[] = "23456789TJQKA";
    std::ostringstream out;
    const uint32_t cat = score_category(score);
    out << "cat=" << cat << "(" << category_name(cat) << ")";
    const uint32_t r0 = score_primary_rank(score, 0);
    const uint32_t r1 = score_primary_rank(score, 1);
    if (r0 > 0 || r1 > 0) {
        out << ", ranks=[";
        if (r0 < RankCount) out << rank_chars[r0];
        if (r1 > 0 && r1 < RankCount) out << ',' << rank_chars[r1];
        out << ']';
    }
    const uint16_t kickers = score_kicker_mask(score);
    if (kickers != 0) {
        out << ", kickers={";
        bool first = true;
        for (int i = 12; i >= 0; --i) {
            if (kickers & (1u << i)) {
                if (!first) out << ',';
                out << rank_chars[i];
                first = false;
            }
        }
        out << '}';
    }
    out << ", raw=0x";
    out.setf(std::ios::hex, std::ios::basefield);
    out.width(7);
    out.fill('0');
    out << score;
    return out.str();
}

} // namespace pokereval
