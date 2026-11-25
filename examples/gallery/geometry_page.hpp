#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace arisa::examples {

class GeometryPage final : public composition::Component {
 public:
  class Link;

  static constexpr int card_count = 9;
  static constexpr std::uint32_t cards_per_page = 3;
  static constexpr int badge_count = 4;
  static constexpr float badge_size = 64.0F;
  static constexpr float badge_step = 40.0F;
  static constexpr float pad_width = 320.0F;
  static constexpr float pad_height = 160.0F;
  static constexpr float knob_size = 44.0F;
  static constexpr float row_width = 280.0F;
  static constexpr std::string_view close_button = "geometry-close";
  static constexpr std::string_view carousel_view = "geometry-carousel";
  static constexpr std::string_view first_card = "geometry-card-0";
  static constexpr std::string_view badge_prefix = "geometry-badge-";
  static constexpr std::string_view kept_price = "geometry-kept-price";
  static constexpr std::string_view plain_price = "geometry-plain-price";
  static constexpr std::string_view fixed_tag = "geometry-fixed-tag";
  static constexpr std::string_view pad_view = "geometry-pad";
  static constexpr std::string_view knob = "geometry-knob";
  static constexpr std::string_view drag_label = "geometry-drag-label";
  static constexpr std::string_view reader_view = "geometry-reader";
  static constexpr std::string_view reader_label = "geometry-reader-label";
  static constexpr std::string_view pad_space = "geometry-pad-space";
  static constexpr composition::StateKey<int> raised_key{"geometry-raised", 0};
  static constexpr composition::StateKey<composition::Point> drag_key{
      "geometry-drag",
      composition::Point{.x = pad_width * 0.5F, .y = pad_height * 0.5F}};

  explicit GeometryPage(composition::StateHandle<bool> shown) noexcept
      : shown_(std::move(shown)) {}

  auto operator==(const GeometryPage& other) const -> bool {
    return shown_.identity() == other.shown_.identity();
  }

  auto body(composition::Context& context) const
      -> composition::Element override;

  static auto badge_key(int index) -> std::string;

 private:
  static auto section(std::string_view title, composition::Element content)
      -> composition::Element;

  static auto carousel() -> composition::Element;

  static auto badges(composition::StateHandle<int> raised)
      -> composition::Element;

  static auto prices() -> composition::Element;

  static auto price_row(std::string_view title, std::string_view price,
                        std::string_view key, bool kept)
      -> composition::Element;

  static auto fixed() -> composition::Element;

  static auto pad(composition::StateHandle<composition::Point> dragged)
      -> composition::Element;

  static auto reader() -> composition::Element;

  composition::StateHandle<bool> shown_;
};

class GeometryPage::Link final : public composition::Component {
 public:
  static constexpr composition::StateKey<bool> shown_key{"geometry-shown",
                                                         false};
  static constexpr std::string_view button = "layout-geometry-button";

  auto operator==(const Link&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;
};

}
