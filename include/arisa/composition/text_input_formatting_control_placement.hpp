#pragma once

#include <cstdint>

namespace arisa::composition {

class TextInputFormattingControlPlacement {
 public:
  class Set {
   public:
    static const Set accessory_bar;
    static const Set context_menu;
    static const Set input_assistant;
    static const Set all;
    static const Set default_;

    constexpr Set() noexcept = default;

    constexpr explicit Set(std::uint8_t raw) noexcept : raw_(raw) {}

    constexpr auto raw_value() const noexcept -> std::uint8_t { return raw_; }

    constexpr auto contains(Set other) const noexcept -> bool {
      return (raw_ & other.raw_) == other.raw_ && other.raw_ != 0;
    }

    constexpr auto operator|(Set other) const noexcept -> Set {
      return Set{static_cast<std::uint8_t>(raw_ | other.raw_)};
    }

    friend constexpr auto operator==(const Set&, const Set&) -> bool = default;

   private:
    std::uint8_t raw_{0};
  };
};

inline constexpr TextInputFormattingControlPlacement::Set
    TextInputFormattingControlPlacement::Set::accessory_bar{1};
inline constexpr TextInputFormattingControlPlacement::Set
    TextInputFormattingControlPlacement::Set::context_menu{2};
inline constexpr TextInputFormattingControlPlacement::Set
    TextInputFormattingControlPlacement::Set::input_assistant{4};
inline constexpr TextInputFormattingControlPlacement::Set
    TextInputFormattingControlPlacement::Set::all{7};
inline constexpr TextInputFormattingControlPlacement::Set
    TextInputFormattingControlPlacement::Set::default_{7};

}
