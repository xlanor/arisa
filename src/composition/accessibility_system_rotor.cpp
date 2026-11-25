#include <arisa/composition/accessibility_system_rotor.hpp>

#include <format>
#include <utility>

namespace arisa::composition {

auto AccessibilitySystemRotor::name() const -> std::string {
  switch (kind_) {
    case Kind::links:
      if (visited_.has_value()) {
        return *visited_ ? "Visited links" : "Unvisited links";
      }
      return "Links";
    case Kind::headings:
      if (level_.has_value() &&
          *level_ != AccessibilityHeadingLevel::unspecified) {
        return std::format("Heading level {}", std::to_underlying(*level_));
      }
      return "Headings";
    case Kind::bold_text:
      return "Bold text";
    case Kind::italic_text:
      return "Italic text";
    case Kind::underline_text:
      return "Underlined text";
    case Kind::misspelled_words:
      return "Misspelled words";
    case Kind::images:
      return "Images";
    case Kind::text_fields:
      return "Text fields";
    case Kind::tables:
      return "Tables";
    case Kind::lists:
      return "Lists";
    case Kind::landmarks:
      return "Landmarks";
  }
  return {};
}

auto AccessibilitySystemRotor::matches(
    AccessibilityTraits traits,
    std::optional<AccessibilityHeadingLevel> heading) const noexcept -> bool {
  switch (kind_) {
    case Kind::links:
      return traits.contains(AccessibilityTrait::is_link);
    case Kind::headings:
      if (!traits.contains(AccessibilityTrait::is_header)) {
        return false;
      }
      return !level_.has_value() ||
             *level_ == AccessibilityHeadingLevel::unspecified ||
             heading == level_;
    case Kind::images:
      return traits.contains(AccessibilityTrait::is_image);
    case Kind::text_fields:
      return traits.contains(AccessibilityTrait::is_search_field) ||
             traits.contains(AccessibilityTrait::is_keyboard_key);
    case Kind::bold_text:
    case Kind::italic_text:
    case Kind::underline_text:
    case Kind::misspelled_words:
    case Kind::tables:
    case Kind::lists:
    case Kind::landmarks:
      return false;
  }
  return false;
}

}
