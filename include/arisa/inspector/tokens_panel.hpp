#pragma once

#include <arisa/appearance/appearance_model.hpp>
#include <arisa/appearance/theme_token.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/inspector/inspector_model.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace arisa::inspector {

class TokensPanel final : public composition::Component {
 public:
  static constexpr std::string_view reset_key = "arisa.inspector.tokens.reset";
  static constexpr std::string_view export_key =
      "arisa.inspector.tokens.export";
  static constexpr float swatch_size = 18.0F;
  static constexpr float value_width = 120.0F;
  static constexpr float undo_width = 44.0F;
  static constexpr float cell_gap = 8.0F;

  explicit TokensPanel(std::shared_ptr<InspectorModel> model) noexcept
      : model_(std::move(model)) {}

  auto operator==(const TokensPanel&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

  static auto row_key(const appearance::ThemeToken& token) -> std::string;

  static auto group_key(appearance::TokenGroup group) -> std::string;

  static auto swatch(theme::Colour colour) -> composition::Element;

 private:
  static constexpr std::string_view row_prefix = "arisa.inspector.token.";
  static constexpr std::string_view group_prefix = "arisa.inspector.group.";

  auto actions(const appearance::AppearanceModel& appearance) const
      -> composition::Element;

  auto row(const appearance::ThemeToken& token,
           const appearance::AppearanceModel& appearance) const
      -> composition::Element;

  auto group(appearance::TokenGroup group,
             const appearance::AppearanceModel& appearance) const
      -> composition::Element;

  std::shared_ptr<InspectorModel> model_;
};

}
