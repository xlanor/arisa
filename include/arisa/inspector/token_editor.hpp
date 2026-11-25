#pragma once

#include <arisa/appearance/theme_token.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/inspector/inspector_model.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/hsb.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace arisa::inspector {

class TokenEditor final : public composition::Component {
 public:
  static constexpr std::string_view key = "arisa.inspector.token-editor";
  static constexpr std::string_view hue_key = "arisa.inspector.token.hue";
  static constexpr std::string_view saturation_key =
      "arisa.inspector.token.saturation";
  static constexpr std::string_view brightness_key =
      "arisa.inspector.token.brightness";
  static constexpr std::string_view alpha_key = "arisa.inspector.token.alpha";
  static constexpr std::string_view hex_key = "arisa.inspector.token.hex";
  static constexpr std::string_view number_key = "arisa.inspector.token.number";
  static constexpr std::string_view toggle_key = "arisa.inspector.token.toggle";
  static constexpr float label_width = 96.0F;

  TokenEditor(std::shared_ptr<InspectorModel> model, std::size_t token,
              appearance::TokenValue value) noexcept
      : model_(std::move(model)), token_(token), value_(std::move(value)) {}

  auto operator==(const TokenEditor&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

  static auto choice_key(std::size_t choice) -> std::string;

 private:
  struct Channels {
    composition::StateHandle<double> hue;
    composition::StateHandle<double> saturation;
    composition::StateHandle<double> brightness;
    composition::StateHandle<double> alpha;

    auto hsb() const -> theme::Hsb;

    void show(theme::Colour colour) const;
  };

  static constexpr composition::StateKey<double> hue{"token-hue", 0.0};
  static constexpr composition::StateKey<double> saturation{"token-saturation",
                                                            0.0};
  static constexpr composition::StateKey<double> brightness{"token-brightness",
                                                            0.0};
  static constexpr composition::StateKey<double> alpha{"token-alpha", 1.0};
  static const composition::StateKey<std::string> hex;
  static constexpr composition::StateKey<double> number{"token-number", 0.0};
  static constexpr composition::StateKey<bool> toggle{"token-toggle", false};

  static auto labelled(std::string_view label, composition::Element control)
      -> composition::Element;

  auto colour(composition::Context& context, theme::Colour current) const
      -> composition::Element;

  auto slider(composition::Context& context, float current) const
      -> composition::Element;

  auto switcher(composition::Context& context, bool current) const
      -> composition::Element;

  auto chooser(std::size_t current) const -> composition::Element;

  std::shared_ptr<InspectorModel> model_;
  std::size_t token_;
  appearance::TokenValue value_;
};

}
