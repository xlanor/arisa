#pragma once

#include <arisa/composition/accessibility_speech.hpp>

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::composition {

enum class AccessibilityTrait : std::uint32_t {
  is_button = 1U << 0U,
  is_header = 1U << 1U,
  is_selected = 1U << 2U,
  is_link = 1U << 3U,
  is_search_field = 1U << 4U,
  is_image = 1U << 5U,
  plays_sound = 1U << 6U,
  is_keyboard_key = 1U << 7U,
  is_static_text = 1U << 8U,
  is_summary_element = 1U << 9U,
  updates_frequently = 1U << 10U,
  starts_media_session = 1U << 11U,
  allows_direct_interaction = 1U << 12U,
  causes_page_turn = 1U << 13U,
  is_modal = 1U << 14U,
  is_toggle = 1U << 15U,
  is_tab_bar = 1U << 16U,
};

class AccessibilityTraits {
 public:
  constexpr AccessibilityTraits() noexcept = default;

  constexpr AccessibilityTraits(AccessibilityTrait trait) noexcept
      : bits_(std::to_underlying(trait)) {}

  constexpr AccessibilityTraits(
      std::initializer_list<AccessibilityTrait> traits) noexcept {
    for (const AccessibilityTrait trait : traits) {
      bits_ = bits_ | std::to_underlying(trait);
    }
  }

  constexpr auto contains(AccessibilityTrait trait) const noexcept -> bool {
    return (bits_ & std::to_underlying(trait)) != 0U;
  }

  constexpr auto empty() const noexcept -> bool { return bits_ == 0U; }

  constexpr auto with(AccessibilityTraits other) const noexcept
      -> AccessibilityTraits {
    return AccessibilityTraits{bits_ | other.bits_};
  }

  constexpr auto without(AccessibilityTraits other) const noexcept
      -> AccessibilityTraits {
    return AccessibilityTraits{bits_ & ~other.bits_};
  }

  constexpr auto intersection(AccessibilityTraits other) const noexcept
      -> AccessibilityTraits {
    return AccessibilityTraits{bits_ & other.bits_};
  }

  constexpr auto symmetric_difference(AccessibilityTraits other) const noexcept
      -> AccessibilityTraits {
    return AccessibilityTraits{bits_ ^ other.bits_};
  }

  constexpr void form_intersection(AccessibilityTraits other) noexcept {
    bits_ = bits_ & other.bits_;
  }

  constexpr void form_symmetric_difference(AccessibilityTraits other) noexcept {
    bits_ = bits_ ^ other.bits_;
  }

  auto names() const -> std::vector<std::string_view>;

  static auto name(AccessibilityTrait trait) noexcept -> std::string_view;

  friend constexpr auto operator==(AccessibilityTraits, AccessibilityTraits)
      -> bool = default;

 private:
  constexpr explicit AccessibilityTraits(std::uint32_t bits) noexcept
      : bits_(bits) {}

  std::uint32_t bits_{0};
};

enum class AccessibilityChildBehavior : std::uint8_t {
  ignore,
  contain,
  combine,
};

enum class AccessibilityHeadingLevel : std::uint8_t {
  unspecified,
  h1,
  h2,
  h3,
  h4,
  h5,
  h6,
};

enum class AccessibilityTextContentType : std::uint8_t {
  plain,
  console,
  file_system,
  messaging,
  narrative,
  source_code,
  spreadsheet,
  word_processing,
};

struct AccessibilityDefaults {
  std::optional<std::string> label{};
  std::optional<std::string> value{};
  AccessibilityTraits traits{};
  std::optional<AccessibilitySpeech> speech{};
  bool adjustable{false};

  friend auto operator==(const AccessibilityDefaults&,
                         const AccessibilityDefaults&) -> bool = default;
};

}
