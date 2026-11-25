#pragma once

#include <cstdint>

namespace arisa::composition {

class SearchSuggestionsPlacement {
 public:
  class Set;

  static const SearchSuggestionsPlacement automatic;
  static const SearchSuggestionsPlacement menu;
  static const SearchSuggestionsPlacement content;

  friend constexpr auto operator==(SearchSuggestionsPlacement,
                                   SearchSuggestionsPlacement)
      -> bool = default;

 private:
  friend class Set;

  constexpr explicit SearchSuggestionsPlacement(std::uint8_t bit) noexcept
      : bit_(bit) {}

  std::uint8_t bit_;
};

class SearchSuggestionsPlacement::Set {
 public:
  static const Set menu;
  static const Set content;
  static const Set all;

  constexpr Set() noexcept = default;

  constexpr explicit Set(std::uint8_t raw_value) noexcept : bits_(raw_value) {}

  constexpr auto raw_value() const noexcept -> std::uint8_t { return bits_; }

  constexpr auto contains(SearchSuggestionsPlacement placement) const noexcept
      -> bool {
    return (bits_ & placement.bit_) != 0U;
  }

  friend constexpr auto operator|(Set left, Set right) noexcept -> Set {
    return Set{static_cast<std::uint8_t>(left.bits_ | right.bits_)};
  }

  friend constexpr auto operator==(Set, Set) -> bool = default;

 private:
  std::uint8_t bits_{0};
};

inline constexpr SearchSuggestionsPlacement
    SearchSuggestionsPlacement::automatic{static_cast<std::uint8_t>(0U)};
inline constexpr SearchSuggestionsPlacement SearchSuggestionsPlacement::menu{
    static_cast<std::uint8_t>(1U)};
inline constexpr SearchSuggestionsPlacement SearchSuggestionsPlacement::content{
    static_cast<std::uint8_t>(2U)};
inline constexpr SearchSuggestionsPlacement::Set
    SearchSuggestionsPlacement::Set::menu{static_cast<std::uint8_t>(1U)};
inline constexpr SearchSuggestionsPlacement::Set
    SearchSuggestionsPlacement::Set::content{static_cast<std::uint8_t>(2U)};
inline constexpr SearchSuggestionsPlacement::Set
    SearchSuggestionsPlacement::Set::all{static_cast<std::uint8_t>(3U)};

}
