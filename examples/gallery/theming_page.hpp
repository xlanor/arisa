#pragma once

#include <arisa/appearance/theme_error.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/namespace.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/theme/theme.hpp>

#include <array>
#include <expected>
#include <string_view>
#include <utility>

namespace arisa::examples {

class ThemingPage final : public composition::Component {
 public:
  static constexpr std::string_view sample_file = R"(schema = 1
name = "orchard"
extends = "default"
accent = "#34C759"

[radii]
control = 12
card = 20

[colours]
system_grouped_background = { light = "#DDF2E2", dark = "#173320" }
)";

  static constexpr std::string_view broken_file = R"(schema = 1
extends = "default"

[colours]
labell = "#000000"
)";

  static constexpr std::string_view switch_button = "theming.switch";
  static constexpr std::string_view switch_swatch = "theming.switch-swatch";
  static constexpr std::string_view switch_accent = "theming.switch-accent";
  static constexpr std::string_view morph_button = "theming.morph";
  static constexpr std::string_view sample_card = "theming.sample-card";
  static constexpr std::string_view broken_card = "theming.broken-card";
  static constexpr std::string_view broken_error = "theming.broken-error";
  static constexpr std::string_view sample_name = "orchard.toml";
  static constexpr std::string_view broken_name = "broken.toml";
  static constexpr composition::StateKey<bool> merged{"theming-merged", false};
  static constexpr composition::Namespace glass_space{"theming-glass"};

  explicit ThemingPage(composition::StateHandle<bool> warm) noexcept
      : shown_(warm.get()), warm_(std::move(warm)) {}

  auto operator==(const ThemingPage& other) const -> bool {
    return shown_ == other.shown_ && warm_.identity() == other.warm_.identity();
  }

  auto body(composition::Context& context) const
      -> composition::Element override;

  static auto sample() -> std::expected<theme::Theme, appearance::ThemeError>;

  static auto broken() -> std::expected<theme::Theme, appearance::ThemeError>;

  static auto warm_theme(const theme::Theme& base) -> theme::Theme;

 private:
  static auto card(std::string_view title, composition::Element content)
      -> composition::Element;

  static auto swatch(std::string_view label, std::string_view marker = {})
      -> composition::Element;

  static auto files() -> std::array<composition::Element, 2>;

  static auto scoped(const theme::Theme& base) -> composition::Element;

  static auto switching(composition::StateHandle<bool> warm)
      -> composition::Element;

  static auto buttons() -> composition::Element;

  static auto container(composition::Namespace::ID space)
      -> composition::Element;

  static auto morph(composition::StateHandle<bool> together,
                    composition::Namespace::ID space) -> composition::Element;

  bool shown_{false};
  composition::StateHandle<bool> warm_;
};

}
