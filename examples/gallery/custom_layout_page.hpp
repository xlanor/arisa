#pragma once

#include <arisa/components/layout_subviews.hpp>
#include <arisa/components/proposed_view_size.hpp>
#include <arisa/composition/alignment_id.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/container_value_key.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/composition/size.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/view_dimensions.hpp>

#include <array>
#include <string>
#include <string_view>
#include <utility>

namespace arisa::examples {

class CustomLayoutPage final : public composition::Component {
 public:
  class Flow {
   public:
    explicit Flow(float spacing) noexcept : spacing_(spacing) {}

    auto size_that_fits(components::ProposedViewSize proposal,
                        const components::LayoutSubviews& subviews) const
        -> composition::Size;

    void place_subviews(composition::Rect bounds,
                        components::ProposedViewSize proposal,
                        const components::LayoutSubviews& subviews) const;

    friend auto operator==(const Flow&, const Flow&) -> bool = default;

   private:
    float spacing_;
  };

  class Link;

  static constexpr std::array<std::string_view, 9> tag_names{
      "Action",   "Adventure", "Co-op", "Platformer", "Puzzle",
      "Strategy", "Racing",    "Indie", "Multiplayer"};
  static constexpr std::array<std::string_view, 3> field_names{
      "Name", "Release year", "Players"};
  static constexpr std::array<std::string_view, 3> field_values{
      "Kingdom of the Lost Lantern", "2026", "1-4"};
  static constexpr float flow_width = 420.0F;
  static constexpr float wide_title = 460.0F;
  static constexpr float narrow_title = 180.0F;
  static constexpr std::string_view close_button = "custom-layout-close";
  static constexpr std::string_view switch_button = "custom-layout-switch";
  static constexpr std::string_view flow_view = "custom-layout-flow";
  static constexpr std::string_view cards_view = "custom-layout-cards";
  static constexpr std::string_view form_view = "custom-layout-form";
  static constexpr std::string_view wide_view = "custom-layout-wide";
  static constexpr std::string_view narrow_view = "custom-layout-narrow";
  static constexpr std::string_view highlight_view = "custom-layout-highlight";
  static constexpr composition::StateKey<bool> flipped_key{
      "custom-layout-flipped", false};
  static constexpr composition::AlignmentID colon{
      "custom-layout-colon", composition::Axis::horizontal,
      [](const composition::ViewDimensions& dimensions) {
        return dimensions[composition::HorizontalAlignment::leading];
      }};
  static constexpr composition::ContainerValueKey<bool> featured{
      "custom-layout-featured", false};

  explicit CustomLayoutPage(composition::StateHandle<bool> shown) noexcept
      : shown_(std::move(shown)) {}

  auto operator==(const CustomLayoutPage& other) const -> bool {
    return shown_.identity() == other.shown_.identity();
  }

  auto body(composition::Context& context) const
      -> composition::Element override;

  static auto tag_key(std::size_t index) -> std::string;

  static auto card_key(std::size_t index) -> std::string;

  static auto field_key(std::size_t index) -> std::string;

  static auto variant_key(std::string_view view, std::string_view variant)
      -> std::string;

 private:
  static auto section(std::string_view title, composition::Element content)
      -> composition::Element;

  static auto tags() -> composition::Element;

  static auto cards(bool across, composition::StateHandle<bool> flipped)
      -> composition::Element;

  static auto title(std::string_view view) -> composition::Element;

  static auto titles() -> composition::Element;

  static auto form() -> composition::Element;

  static auto highlights() -> composition::Element;

  composition::StateHandle<bool> shown_;
};

class CustomLayoutPage::Link final : public composition::Component {
 public:
  static constexpr composition::StateKey<bool> shown_key{"custom-layout-shown",
                                                         false};
  static constexpr std::string_view button = "geometry-custom-layout-button";

  auto operator==(const Link&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;
};

}
