#pragma once

#include <arisa/composition/accessibility.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace arisa::composition {

class AccessibilitySystemRotor {
 public:
  enum class Kind : std::uint8_t {
    links,
    headings,
    bold_text,
    italic_text,
    underline_text,
    misspelled_words,
    images,
    text_fields,
    tables,
    lists,
    landmarks,
  };

  static const AccessibilitySystemRotor bold_text;
  static const AccessibilitySystemRotor italic_text;
  static const AccessibilitySystemRotor underline_text;
  static const AccessibilitySystemRotor misspelled_words;
  static const AccessibilitySystemRotor images;
  static const AccessibilitySystemRotor text_fields;
  static const AccessibilitySystemRotor tables;
  static const AccessibilitySystemRotor lists;
  static const AccessibilitySystemRotor landmarks;

  static constexpr auto links() noexcept -> AccessibilitySystemRotor {
    return AccessibilitySystemRotor{Kind::links};
  }

  static constexpr auto links(bool visited) noexcept
      -> AccessibilitySystemRotor {
    AccessibilitySystemRotor rotor{Kind::links};
    rotor.visited_ = visited;
    return rotor;
  }

  static constexpr auto headings() noexcept -> AccessibilitySystemRotor {
    return AccessibilitySystemRotor{Kind::headings};
  }

  static constexpr auto headings(AccessibilityHeadingLevel level) noexcept
      -> AccessibilitySystemRotor {
    AccessibilitySystemRotor rotor{Kind::headings};
    rotor.level_ = level;
    return rotor;
  }

  constexpr auto kind() const noexcept -> Kind { return kind_; }

  constexpr auto visited() const noexcept -> const std::optional<bool>& {
    return visited_;
  }

  constexpr auto level() const noexcept
      -> const std::optional<AccessibilityHeadingLevel>& {
    return level_;
  }

  auto name() const -> std::string;

  auto matches(AccessibilityTraits traits,
               std::optional<AccessibilityHeadingLevel> heading) const noexcept
      -> bool;

  friend constexpr auto operator==(const AccessibilitySystemRotor&,
                                   const AccessibilitySystemRotor&)
      -> bool = default;

 private:
  constexpr explicit AccessibilitySystemRotor(Kind kind) noexcept
      : kind_(kind) {}

  Kind kind_;
  std::optional<bool> visited_;
  std::optional<AccessibilityHeadingLevel> level_;
};

inline constexpr AccessibilitySystemRotor AccessibilitySystemRotor::bold_text{
    Kind::bold_text};
inline constexpr AccessibilitySystemRotor AccessibilitySystemRotor::italic_text{
    Kind::italic_text};
inline constexpr AccessibilitySystemRotor
    AccessibilitySystemRotor::underline_text{Kind::underline_text};
inline constexpr AccessibilitySystemRotor
    AccessibilitySystemRotor::misspelled_words{Kind::misspelled_words};
inline constexpr AccessibilitySystemRotor AccessibilitySystemRotor::images{
    Kind::images};
inline constexpr AccessibilitySystemRotor AccessibilitySystemRotor::text_fields{
    Kind::text_fields};
inline constexpr AccessibilitySystemRotor AccessibilitySystemRotor::tables{
    Kind::tables};
inline constexpr AccessibilitySystemRotor AccessibilitySystemRotor::lists{
    Kind::lists};
inline constexpr AccessibilitySystemRotor AccessibilitySystemRotor::landmarks{
    Kind::landmarks};

}
