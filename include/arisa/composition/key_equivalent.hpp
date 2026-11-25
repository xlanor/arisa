#pragma once

#include <arisa/provider/input/provider.hpp>

#include <algorithm>
#include <array>
#include <optional>

namespace arisa::composition {

class KeyEquivalent {
 public:
  constexpr KeyEquivalent(char32_t character) noexcept
      : character_(character) {}

  constexpr KeyEquivalent(char character) noexcept
      : character_(static_cast<unsigned char>(character)) {}

  constexpr explicit KeyEquivalent(provider::Key key) noexcept
      : character_(plain(key).value_or(U'\0')) {}

  static const KeyEquivalent up_arrow;
  static const KeyEquivalent down_arrow;
  static const KeyEquivalent left_arrow;
  static const KeyEquivalent right_arrow;
  static const KeyEquivalent escape;
  static const KeyEquivalent delete_key;
  static const KeyEquivalent delete_forward;
  static const KeyEquivalent home;
  static const KeyEquivalent end;
  static const KeyEquivalent page_up;
  static const KeyEquivalent page_down;
  static const KeyEquivalent clear;
  static const KeyEquivalent tab;
  static const KeyEquivalent space;
  static const KeyEquivalent return_key;

  constexpr auto character() const noexcept -> char32_t { return character_; }

  constexpr auto key() const noexcept -> std::optional<provider::Key> {
    const auto found =
        std::ranges::find_if(mappings, [this](const Mapping& mapping) {
          return mapping.plain == character_ ||
                 (mapping.shifted != U'\0' && mapping.shifted == character_);
        });
    if (found == mappings.end()) {
      return std::nullopt;
    }
    return found->key;
  }

  constexpr auto shifted() const noexcept -> bool {
    return std::ranges::any_of(mappings, [this](const Mapping& mapping) {
      return mapping.shifted != U'\0' && mapping.shifted == character_ &&
             mapping.plain != character_;
    });
  }

  constexpr auto unshifted() const noexcept -> KeyEquivalent {
    const std::optional<provider::Key> found = key();
    return found.has_value() ? KeyEquivalent{*found} : *this;
  }

  constexpr auto printable() const noexcept -> std::optional<char> {
    if (character_ >= U' ' && character_ < U'\x7F') {
      return static_cast<char>(character_);
    }
    return std::nullopt;
  }

  static constexpr auto pressed(provider::Key key, bool shift) noexcept
      -> KeyEquivalent {
    const auto found = std::ranges::find(mappings, key, &Mapping::key);
    if (found == mappings.end()) {
      return KeyEquivalent{U'\0'};
    }
    const bool letter = found->plain >= U'a' && found->plain <= U'z';
    if (shift && !letter && found->shifted != U'\0') {
      return KeyEquivalent{found->shifted};
    }
    return KeyEquivalent{found->plain};
  }

  static constexpr auto typed(provider::Key key, bool shift) noexcept
      -> std::optional<char> {
    const auto found = std::ranges::find(mappings, key, &Mapping::key);
    if (found == mappings.end()) {
      return std::nullopt;
    }
    const char32_t produced =
        shift && found->shifted != U'\0' ? found->shifted : found->plain;
    return KeyEquivalent{produced}.printable();
  }

  friend constexpr auto operator==(KeyEquivalent, KeyEquivalent)
      -> bool = default;

 private:
  struct Mapping {
    provider::Key key;
    char32_t plain;
    char32_t shifted;
  };

  static constexpr auto plain(provider::Key key) noexcept
      -> std::optional<char32_t> {
    const auto found = std::ranges::find(mappings, key, &Mapping::key);
    if (found == mappings.end()) {
      return std::nullopt;
    }
    return found->plain;
  }

  static constexpr std::array mappings{
      Mapping{provider::Key::a, U'a', U'A'},
      Mapping{provider::Key::b, U'b', U'B'},
      Mapping{provider::Key::c, U'c', U'C'},
      Mapping{provider::Key::d, U'd', U'D'},
      Mapping{provider::Key::e, U'e', U'E'},
      Mapping{provider::Key::f, U'f', U'F'},
      Mapping{provider::Key::g, U'g', U'G'},
      Mapping{provider::Key::h, U'h', U'H'},
      Mapping{provider::Key::i, U'i', U'I'},
      Mapping{provider::Key::j, U'j', U'J'},
      Mapping{provider::Key::k, U'k', U'K'},
      Mapping{provider::Key::l, U'l', U'L'},
      Mapping{provider::Key::m, U'm', U'M'},
      Mapping{provider::Key::n, U'n', U'N'},
      Mapping{provider::Key::o, U'o', U'O'},
      Mapping{provider::Key::p, U'p', U'P'},
      Mapping{provider::Key::q, U'q', U'Q'},
      Mapping{provider::Key::r, U'r', U'R'},
      Mapping{provider::Key::s, U's', U'S'},
      Mapping{provider::Key::t, U't', U'T'},
      Mapping{provider::Key::u, U'u', U'U'},
      Mapping{provider::Key::v, U'v', U'V'},
      Mapping{provider::Key::w, U'w', U'W'},
      Mapping{provider::Key::x, U'x', U'X'},
      Mapping{provider::Key::y, U'y', U'Y'},
      Mapping{provider::Key::z, U'z', U'Z'},
      Mapping{provider::Key::digit1, U'1', U'!'},
      Mapping{provider::Key::digit2, U'2', U'@'},
      Mapping{provider::Key::digit3, U'3', U'#'},
      Mapping{provider::Key::digit4, U'4', U'$'},
      Mapping{provider::Key::digit5, U'5', U'%'},
      Mapping{provider::Key::digit6, U'6', U'^'},
      Mapping{provider::Key::digit7, U'7', U'&'},
      Mapping{provider::Key::digit8, U'8', U'*'},
      Mapping{provider::Key::digit9, U'9', U'('},
      Mapping{provider::Key::digit0, U'0', U')'},
      Mapping{provider::Key::minus, U'-', U'_'},
      Mapping{provider::Key::equals, U'=', U'+'},
      Mapping{provider::Key::left_bracket, U'[', U'{'},
      Mapping{provider::Key::right_bracket, U']', U'}'},
      Mapping{provider::Key::backslash, U'\\', U'|'},
      Mapping{provider::Key::semicolon, U';', U':'},
      Mapping{provider::Key::apostrophe, U'\'', U'"'},
      Mapping{provider::Key::grave, U'`', U'~'},
      Mapping{provider::Key::comma, U',', U'<'},
      Mapping{provider::Key::period, U'.', U'>'},
      Mapping{provider::Key::slash, U'/', U'?'},
      Mapping{provider::Key::space, U' ', U' '},
      Mapping{provider::Key::tab, U'\t', U'\0'},
      Mapping{provider::Key::enter, U'\r', U'\0'},
      Mapping{provider::Key::escape, U'\x1B', U'\0'},
      Mapping{provider::Key::backspace, U'\x7F', U'\0'},
      Mapping{provider::Key::up, U'', U'\0'},
      Mapping{provider::Key::down, U'', U'\0'},
      Mapping{provider::Key::left, U'', U'\0'},
      Mapping{provider::Key::right, U'', U'\0'},
      Mapping{provider::Key::f1, U'', U'\0'},
      Mapping{provider::Key::f2, U'', U'\0'},
      Mapping{provider::Key::f3, U'', U'\0'},
      Mapping{provider::Key::f4, U'', U'\0'},
      Mapping{provider::Key::f5, U'', U'\0'},
      Mapping{provider::Key::f6, U'', U'\0'},
      Mapping{provider::Key::f7, U'', U'\0'},
      Mapping{provider::Key::f8, U'', U'\0'},
      Mapping{provider::Key::f9, U'', U'\0'},
      Mapping{provider::Key::f10, U'', U'\0'},
      Mapping{provider::Key::f11, U'', U'\0'},
      Mapping{provider::Key::f12, U'', U'\0'},
      Mapping{provider::Key::insert, U'', U'\0'},
      Mapping{provider::Key::delete_forward, U'', U'\0'},
      Mapping{provider::Key::home, U'', U'\0'},
      Mapping{provider::Key::end, U'', U'\0'},
      Mapping{provider::Key::page_up, U'', U'\0'},
      Mapping{provider::Key::page_down, U'', U'\0'},
  };

  char32_t character_;
};

inline constexpr KeyEquivalent KeyEquivalent::up_arrow{U''};
inline constexpr KeyEquivalent KeyEquivalent::down_arrow{U''};
inline constexpr KeyEquivalent KeyEquivalent::left_arrow{U''};
inline constexpr KeyEquivalent KeyEquivalent::right_arrow{U''};
inline constexpr KeyEquivalent KeyEquivalent::escape{U'\x1B'};
inline constexpr KeyEquivalent KeyEquivalent::delete_key{U'\x7F'};
inline constexpr KeyEquivalent KeyEquivalent::delete_forward{U''};
inline constexpr KeyEquivalent KeyEquivalent::home{U''};
inline constexpr KeyEquivalent KeyEquivalent::end{U''};
inline constexpr KeyEquivalent KeyEquivalent::page_up{U''};
inline constexpr KeyEquivalent KeyEquivalent::page_down{U''};
inline constexpr KeyEquivalent KeyEquivalent::clear{U''};
inline constexpr KeyEquivalent KeyEquivalent::tab{U'\t'};
inline constexpr KeyEquivalent KeyEquivalent::space{U' '};
inline constexpr KeyEquivalent KeyEquivalent::return_key{U'\r'};

}
