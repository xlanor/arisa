#pragma once

#include <arisa/components/arrangement_view_style.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/content_margin_placement.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/scroll_dismisses_keyboard_mode.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/theme/colour_value.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace arisa::examples {

class LayoutScrollPage final : public composition::Component {
 public:
  class Link;

  enum class Style : std::uint8_t {
    automatic,
    split,
    overlay,
  };

  enum class Share : std::uint8_t {
    third,
    half,
    two_thirds,
  };

  static constexpr int lazy_rows = 24;
  static constexpr int card_count = 10;
  static constexpr int page_count = 4;
  static constexpr int margin_rows = 12;
  static constexpr int keyboard_rows = 12;
  static constexpr int avatar_count = 5;
  static constexpr float region_inset = 32.0F;
  static constexpr float region_width = 96.0F;
  static constexpr float region_height = 120.0F;
  static constexpr float aligned_chip = 16.0F;
  static constexpr float chip_overlap = -6.0F;
  static constexpr float avatar_spacing = -12.0F;
  static constexpr float avatar_size = 40.0F;
  static constexpr float card_width = 120.0F;
  static constexpr float strip_height = 72.0F;
  static constexpr float margin_inset = 20.0F;
  static constexpr float well_height = 140.0F;
  static constexpr float secondary_ideal = 160.0F;

  static constexpr std::string_view close_button = "layout-scroll-close";
  static constexpr std::string_view lazy_view = "layout-scroll-lazy";
  static constexpr std::string_view lazy_header = "layout-scroll-lazy-header";
  static constexpr std::string_view lazy_footer = "layout-scroll-lazy-footer";
  static constexpr std::string_view ignored_region = "layout-scroll-ignored";
  static constexpr std::string_view respected_region =
      "layout-scroll-respected";
  static constexpr std::string_view aligned_region = "layout-scroll-aligned";
  static constexpr std::string_view ignored_fill = "layout-scroll-ignored-fill";
  static constexpr std::string_view respected_fill =
      "layout-scroll-respected-fill";
  static constexpr std::string_view aligned_fill = "layout-scroll-aligned-fill";
  static constexpr std::string_view chip_prefix = "layout-scroll-chip-";
  static constexpr std::string_view overlapped_chip =
      "layout-scroll-overlapped";
  static constexpr std::string_view avatar_prefix = "layout-scroll-avatar-";
  static constexpr std::string_view style_picker = "layout-scroll-style";
  static constexpr std::string_view share_picker = "layout-scroll-share";
  static constexpr std::string_view arrangement_view =
      "layout-scroll-arrangement";
  static constexpr std::string_view primary_view = "layout-scroll-primary";
  static constexpr std::string_view secondary_view = "layout-scroll-secondary";
  static constexpr std::string_view carousel_view = "layout-scroll-carousel";
  static constexpr std::string_view paging_view = "layout-scroll-paging";
  static constexpr std::string_view content_margin_view =
      "layout-scroll-content-margin";
  static constexpr std::string_view indicator_margin_view =
      "layout-scroll-indicator-margin";
  static constexpr std::string_view immediate_list = "layout-scroll-immediate";
  static constexpr std::string_view never_list = "layout-scroll-never";
  static constexpr std::string_view immediate_field =
      "layout-scroll-immediate-field";
  static constexpr std::string_view never_field = "layout-scroll-never-field";

  static constexpr composition::StateKey<Style> style_key{"layout-scroll-style",
                                                          Style::automatic};
  static constexpr composition::StateKey<Share> share_key{"layout-scroll-share",
                                                          Share::half};
  static const composition::StateKey<std::string> immediate_text;
  static const composition::StateKey<std::string> never_text;

  explicit LayoutScrollPage(composition::StateHandle<bool> shown) noexcept
      : shown_(std::move(shown)) {}

  auto operator==(const LayoutScrollPage& other) const -> bool {
    return shown_.identity() == other.shown_.identity();
  }

  auto body(composition::Context& context) const
      -> composition::Element override;

  static auto lazy_row_key(int index) -> std::string;
  static auto chip_key(int index) -> std::string;
  static auto avatar_key(int index) -> std::string;
  static auto card_key(int index) -> std::string;
  static auto page_key(int index) -> std::string;
  static auto margin_row_key(std::string_view view, int index) -> std::string;
  static auto keyboard_row_key(std::string_view view, int index) -> std::string;
  static auto arrangement_style(Style style)
      -> components::ArrangementViewStyle;
  static auto ratio(Share share) noexcept -> float;

 private:
  static auto section(std::string_view title, composition::Element content)
      -> composition::Element;
  static auto caption(std::string_view text) -> composition::Element;
  static auto lazy() -> composition::Element;
  static auto region(std::string_view key, std::string_view label,
                     composition::Element content) -> composition::Element;
  static auto safe_areas() -> composition::Element;
  static auto chip(int index, std::string_view label) -> composition::Element;
  static auto negative() -> composition::Element;
  static auto pane(std::string_view label, theme::ColourRole role)
      -> composition::Element;
  static auto arrangement(composition::StateHandle<Style> style,
                          composition::StateHandle<Share> share)
      -> composition::Element;
  static auto carousel() -> composition::Element;
  static auto paging() -> composition::Element;
  static auto well(std::string_view key, std::string_view label,
                   composition::ContentMarginPlacement placement)
      -> composition::Element;
  static auto margins() -> composition::Element;
  static auto typing(std::string_view key, std::string_view field,
                     std::string_view label,
                     composition::StateHandle<std::string> text,
                     composition::ScrollDismissesKeyboardMode mode)
      -> composition::Element;
  static auto keyboard(composition::StateHandle<std::string> immediate,
                       composition::StateHandle<std::string> never)
      -> composition::Element;

  composition::StateHandle<bool> shown_;
};

class LayoutScrollPage::Link final : public composition::Component {
 public:
  static constexpr composition::StateKey<bool> shown_key{"layout-scroll-shown",
                                                         false};
  static constexpr std::string_view button = "geometry-layout-scroll-button";

  auto operator==(const Link&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;
};

}
