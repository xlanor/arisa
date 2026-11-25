#pragma once

#include <arisa/composition/binding.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/hsb.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::controls {

class ColorPicker : public composition::Element {
 public:
  class Content;
  class Panel;

  static constexpr float swatch_width = 48.0F;
  static constexpr float swatch_height = 24.0F;
  static constexpr float panel_width = 280.0F;
  static constexpr float preview_height = 44.0F;
  static constexpr std::string_view swatch_key = "arisa.color_picker.swatch";
  static constexpr std::string_view hex_key = "arisa.color_picker.hex";
  static constexpr std::string_view hue_key = "arisa.color_picker.hue";
  static constexpr std::string_view saturation_key =
      "arisa.color_picker.saturation";
  static constexpr std::string_view brightness_key =
      "arisa.color_picker.brightness";
  static constexpr std::string_view alpha_key = "arisa.color_picker.alpha";

  ColorPicker(std::string title, composition::Binding<theme::Colour> selection,
              bool supports_opacity = true);

  ColorPicker(composition::Binding<theme::Colour> selection,
              composition::Element label, bool supports_opacity = true);

  static auto swatch(theme::Colour colour) -> composition::Element;
};

class ColorPicker::Content final : public composition::ElementContent {
 public:
  using Source = std::shared_ptr<composition::PlaceholderContentView::Source>;

  static constexpr composition::StateKey<bool> shown{"color-picker-shown",
                                                     false};

  Content(Source label, composition::Binding<theme::Colour> selection,
          bool supports_opacity) noexcept
      : label_(std::move(label)),
        selection_(std::move(selection)),
        supports_opacity_(supports_opacity) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

 private:
  Source label_;
  composition::Binding<theme::Colour> selection_;
  bool supports_opacity_;
};

class ColorPicker::Panel final : public composition::Component {
 public:
  static constexpr composition::StateKey<double> hue{"color-picker-hue", 0.0};
  static constexpr composition::StateKey<double> saturation{
      "color-picker-saturation", 0.0};
  static constexpr composition::StateKey<double> brightness{
      "color-picker-brightness", 0.0};
  static constexpr composition::StateKey<double> alpha{"color-picker-alpha",
                                                       1.0};
  static constexpr composition::StateKey<bool> seeded{"color-picker-seeded",
                                                      false};
  static const composition::StateKey<std::string> hex;

  Panel(composition::Binding<theme::Colour> selection,
        bool supports_opacity) noexcept
      : selection_(std::move(selection)), supports_opacity_(supports_opacity) {}

  auto operator==(const Panel& other) const -> bool {
    return selection_.identity() == other.selection_.identity() &&
           supports_opacity_ == other.supports_opacity_;
  }

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  static auto channel(std::string_view title,
                      composition::StateHandle<double> value, double upper,
                      double step, std::string_view key,
                      std::function<void()> push) -> composition::Element;

  composition::Binding<theme::Colour> selection_;
  bool supports_opacity_;
};

}
