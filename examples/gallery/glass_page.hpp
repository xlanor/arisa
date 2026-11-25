#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/provider/gpu/image.hpp>
#include <glass_preview.hpp>

#include <array>
#include <cstddef>
#include <memory>
#include <string_view>
#include <utility>

namespace arisa::examples {

class GlassPage final : public composition::Component {
 public:
  using Photo = std::shared_ptr<const provider::Image>;
  using Look = GlassPreview::Look;

  struct Preset {
    std::string_view title;
    std::string_view key;
    Look look;
  };

  explicit GlassPage(Photo photo = nullptr) noexcept
      : photo_(std::move(photo)) {}

  static constexpr std::string_view regular_view = "glass-regular";
  static constexpr std::string_view clear_view = "glass-clear";
  static constexpr std::string_view tinted_view = "glass-tinted";
  static constexpr std::string_view soft_view = "glass-tinted-soft";
  static constexpr std::string_view identity_view = "glass-identity";
  static constexpr std::string_view container_view = "glass-container";
  static constexpr std::string_view union_view = "glass-union";
  static constexpr std::string_view interactive_view = "glass-interactive";
  static constexpr std::string_view panel_view = "glass-panel";
  static constexpr float panel_width = 330.0F;
  static constexpr float label_width = 66.0F;
  static constexpr float swatch_extent = 18.0F;
  static constexpr float swatch_cell = 26.0F;
  static constexpr float swatch_gap = 2.0F;
  static constexpr double spacing_limit = 60.0;
  static constexpr double spacing_step = 4.0;

  static constexpr std::array<Preset, 8> presets{
      Preset{"Regular", regular_view, Look{}},
      Preset{"Clear", clear_view,
             Look{.variant = GlassPreview::Variant::clear}},
      Preset{"Tinted", tinted_view, Look{.tint = GlassPreview::Tint::accent}},
      Preset{"Soft", soft_view, Look{.tint = GlassPreview::Tint::accent_soft}},
      Preset{"Identity", identity_view,
             Look{.variant = GlassPreview::Variant::identity}},
      Preset{"Container", container_view,
             Look{.size = GlassPreview::Size::small,
                  .grouping = GlassPreview::Grouping::container}},
      Preset{"Union", union_view,
             Look{.size = GlassPreview::Size::small,
                  .grouping = GlassPreview::Grouping::united}},
      Preset{"Interactive", interactive_view, Look{.interactive = true}},
  };

  static constexpr std::array<GlassPreview::Tint, 11> tints{
      GlassPreview::Tint::none,        GlassPreview::Tint::accent,
      GlassPreview::Tint::red,         GlassPreview::Tint::green,
      GlassPreview::Tint::orange,      GlassPreview::Tint::purple,
      GlassPreview::Tint::accent_soft, GlassPreview::Tint::red_soft,
      GlassPreview::Tint::green_soft,  GlassPreview::Tint::orange_soft,
      GlassPreview::Tint::purple_soft};

  static constexpr std::array<std::string_view, 11> swatch_views{
      "glass-tint-none",        "glass-tint-accent",
      "glass-tint-red",         "glass-tint-green",
      "glass-tint-orange",      "glass-tint-purple",
      "glass-tint-accent-soft", "glass-tint-red-soft",
      "glass-tint-green-soft",  "glass-tint-orange-soft",
      "glass-tint-purple-soft"};

  static constexpr composition::StateKey<GlassPreview::Variant> variant{
      "glass-variant", GlassPreview::Variant::regular};
  static constexpr composition::StateKey<GlassPreview::Tint> tint{
      "glass-tint", GlassPreview::Tint::none};
  static constexpr composition::StateKey<bool> interactive{"glass-interactive",
                                                           false};
  static constexpr composition::StateKey<GlassPreview::Form> form{
      "glass-form", GlassPreview::Form::capsule};
  static constexpr composition::StateKey<GlassPreview::Size> size{
      "glass-size", GlassPreview::Size::medium};
  static constexpr composition::StateKey<GlassPreview::Grouping> grouping{
      "glass-grouping", GlassPreview::Grouping::single};
  static constexpr composition::StateKey<int> count{"glass-count", 3};
  static constexpr composition::StateKey<double> spacing{"glass-spacing", 24.0};

  auto operator==(const GlassPage&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  struct Choices {
    composition::StateHandle<GlassPreview::Variant> variant;
    composition::StateHandle<GlassPreview::Tint> tint;
    composition::StateHandle<bool> interactive;
    composition::StateHandle<GlassPreview::Form> form;
    composition::StateHandle<GlassPreview::Size> size;
    composition::StateHandle<GlassPreview::Grouping> grouping;
    composition::StateHandle<int> count;
    composition::StateHandle<double> spacing;
  };

  static auto chosen(composition::Context& context) -> Choices;

  static auto look(const Choices& choices) -> Look;

  static void apply(const Choices& choices, const Look& look);

  static auto row(std::string_view title, composition::Element control)
      -> composition::Element;

  static auto preset_buttons(const Choices& choices, const Look& current)
      -> composition::Element;

  static auto swatch(const Choices& choices, std::size_t index)
      -> composition::Element;

  static auto swatches(const Choices& choices) -> composition::Element;

  static auto form_icon(GlassPreview::Form shape) -> composition::Element;

  static auto panel(const Choices& choices, const Look& current)
      -> composition::Element;

  Photo photo_;
};

}
