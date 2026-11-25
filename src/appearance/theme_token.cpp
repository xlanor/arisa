#include <arisa/appearance/theme_token.hpp>
#include <arisa/appearance/theme_vocabulary.hpp>

#include <algorithm>
#include <cmath>
#include <format>
#include <utility>

namespace arisa::appearance {

ThemeToken::ThemeToken(TokenGroup group, TokenKind kind, std::uint16_t slot,
                       std::uint8_t field, std::string path)
    : group_(group),
      kind_(kind),
      slot_(slot),
      field_(field),
      path_(std::move(path)) {}

auto ThemeToken::name(TokenGroup group) noexcept -> std::string_view {
  switch (group) {
    case TokenGroup::colours:
      return "Colours";
    case TokenGroup::metrics:
      return "Metrics";
    case TokenGroup::radii:
      return "Radii";
    case TokenGroup::spacing:
      return "Spacing";
    case TokenGroup::type:
      return "Type";
    case TokenGroup::materials:
      return "Materials";
    case TokenGroup::liquid_glass:
      return "Liquid glass";
    case TokenGroup::glass:
      return "Glass";
    case TokenGroup::motion:
      return "Motion";
  }
  return "Colours";
}

auto ThemeToken::build() -> std::vector<ThemeToken> {
  static constexpr std::array<std::string_view, 2> sides{"light", "dark"};
  static constexpr std::array<std::string_view, 4> radii{"control", "card",
                                                         "sheet", "continuous"};
  static constexpr std::array<std::string_view, 5> type_fields{
      "size", "weight", "line_height", "tracking", "design"};
  static constexpr std::array<std::string_view, 7> material_fields{
      "blur",      "saturation",       "noise",          "tint.light",
      "tint.dark", "luminosity.light", "luminosity.dark"};
  static constexpr std::array<std::string_view, 13> liquid_fields{
      "refraction",     "rim",
      "rim_width",      "blur",
      "clear_blur",     "highlight",
      "shadow_opacity", "shadow_blur",
      "shadow_offset",  "ambient_opacity",
      "ambient_blur",   "tint.light",
      "tint.dark"};
  static constexpr std::array<std::string_view, 5> surfaces{
      "sidebar", "toolbar", "sheet", "control", "panel"};
  static constexpr std::array<std::string_view, 2> springs{"focus", "standard"};
  static constexpr std::array<std::string_view, 2> spring_fields{
      "response", "damping_ratio"};
  std::vector<ThemeToken> built;
  const auto add = [&built](TokenGroup group, TokenKind kind, std::size_t slot,
                            std::size_t field, std::string path) {
    built.push_back(ThemeToken{group, kind, static_cast<std::uint16_t>(slot),
                               static_cast<std::uint8_t>(field),
                               std::move(path)});
  };
  const std::span<const std::string_view> roles =
      ThemeVocabulary::colour_roles();
  for (std::size_t role = 0; role < roles.size(); ++role) {
    for (std::size_t side = 0; side < sides.size(); ++side) {
      add(TokenGroup::colours, TokenKind::colour, role, side,
          std::format("colours.{}.{}", roles[role], sides.at(side)));
    }
  }
  const std::span<const std::string_view> metrics = ThemeVocabulary::metrics();
  for (std::size_t metric = 0; metric < metrics.size(); ++metric) {
    add(TokenGroup::metrics, TokenKind::number, metric, 0,
        std::format("metrics.{}", metrics[metric]));
  }
  for (std::size_t slot = 0; slot < radii.size(); ++slot) {
    add(TokenGroup::radii,
        slot + 1 == radii.size() ? TokenKind::toggle : TokenKind::number, slot,
        0, std::format("radii.{}", radii.at(slot)));
  }
  const std::span<const std::string_view> spaces = ThemeVocabulary::spaces();
  for (std::size_t space = 0; space < spaces.size(); ++space) {
    add(TokenGroup::spacing, TokenKind::number, space, 0,
        std::format("spacing.{}", spaces[space]));
  }
  const std::span<const std::string_view> texts = ThemeVocabulary::text_roles();
  for (std::size_t role = 0; role < texts.size(); ++role) {
    for (std::size_t field = 0; field < type_fields.size(); ++field) {
      add(TokenGroup::type,
          field + 1 == type_fields.size() ? TokenKind::choice
                                          : TokenKind::number,
          role, field,
          std::format("type.{}.{}", texts[role], type_fields.at(field)));
    }
  }
  for (const theme::Material material : theme::Material::all()) {
    for (std::size_t field = 0; field < material_fields.size(); ++field) {
      add(TokenGroup::materials,
          field == 3 || field == 4 ? TokenKind::colour : TokenKind::number,
          material.index(), field,
          std::format("materials.{}.{}", material.name(),
                      material_fields.at(field)));
    }
  }
  for (std::size_t field = 0; field < liquid_fields.size(); ++field) {
    add(TokenGroup::liquid_glass,
        field >= 11 ? TokenKind::colour : TokenKind::number, 0, field,
        std::format("liquid_glass.{}", liquid_fields.at(field)));
  }
  for (std::size_t slot = 0; slot < surfaces.size(); ++slot) {
    add(TokenGroup::glass, TokenKind::choice, slot, 0,
        std::format("glass.{}.variant", surfaces.at(slot)));
    add(TokenGroup::glass, TokenKind::toggle, slot, 1,
        std::format("glass.{}.interactive", surfaces.at(slot)));
  }
  for (std::size_t slot = 0; slot < springs.size(); ++slot) {
    for (std::size_t field = 0; field < spring_fields.size(); ++field) {
      add(TokenGroup::motion, TokenKind::number, slot, field,
          std::format("motion.{}.{}", springs.at(slot),
                      spring_fields.at(field)));
    }
  }
  for (std::size_t index = 0; index < built.size(); ++index) {
    built[index].index_ = index;
  }
  return built;
}

auto ThemeToken::all() -> std::span<const ThemeToken> {
  static const std::vector<ThemeToken> tokens = build();
  return tokens;
}

auto ThemeToken::at(std::size_t index) -> const ThemeToken& {
  return all()[index];
}

auto ThemeToken::in(TokenGroup group) -> std::span<const ThemeToken> {
  const std::span<const ThemeToken> tokens = all();
  const auto first = std::ranges::find(tokens, group, &ThemeToken::group);
  const auto last = std::find_if(first, tokens.end(), [group](const auto& at) {
    return at.group() != group;
  });
  return std::span<const ThemeToken>{first, last};
}

auto ThemeToken::find(std::string_view path) -> std::optional<Ref> {
  const std::span<const ThemeToken> tokens = all();
  const auto found = std::ranges::find(tokens, path, &ThemeToken::path);
  if (found == tokens.end()) {
    return std::nullopt;
  }
  return std::cref(*found);
}

auto ThemeToken::quantised(theme::Colour colour) -> theme::Colour {
  return ThemeVocabulary::colour(ThemeVocabulary::hex(colour)).value_or(colour);
}

auto ThemeToken::label() const -> std::string_view {
  const std::string_view whole = path_;
  return whole.substr(whole.find('.') + 1);
}

auto ThemeToken::scheme() const noexcept -> std::optional<theme::ColorScheme> {
  if (kind_ != TokenKind::colour) {
    return std::nullopt;
  }
  return path_.ends_with(".dark") ? theme::ColorScheme::dark
                                  : theme::ColorScheme::light;
}

auto ThemeToken::range() const noexcept -> std::optional<TokenRange> {
  if (kind_ != TokenKind::number) {
    return std::nullopt;
  }
  switch (group_) {
    case TokenGroup::metrics:
      return TokenRange{.minimum = 0.0, .maximum = 400.0, .step = 0.5};
    case TokenGroup::radii:
      return TokenRange{.minimum = 0.0, .maximum = 48.0, .step = 0.5};
    case TokenGroup::spacing:
      return TokenRange{.minimum = 0.0, .maximum = 64.0, .step = 0.5};
    case TokenGroup::type: {
      static constexpr std::array<TokenRange, 4> fields{
          TokenRange{.minimum = 6.0, .maximum = 96.0, .step = 0.5},
          TokenRange{.minimum = 100.0, .maximum = 900.0, .step = 100.0},
          TokenRange{.minimum = 6.0, .maximum = 120.0, .step = 0.5},
          TokenRange{.minimum = 0.0, .maximum = 2.0, .step = 0.01}};
      return fields.at(field_);
    }
    case TokenGroup::materials: {
      static constexpr std::array<TokenRange, 7> fields{
          TokenRange{.minimum = 0.0, .maximum = 80.0, .step = 0.5},
          TokenRange{.minimum = 0.0, .maximum = 3.0, .step = 0.01},
          TokenRange{.minimum = 0.0, .maximum = 0.1, .step = 0.001},
          TokenRange{},
          TokenRange{},
          TokenRange{.minimum = -0.5, .maximum = 0.5, .step = 0.005},
          TokenRange{.minimum = -0.5, .maximum = 0.5, .step = 0.005}};
      return fields.at(field_);
    }
    case TokenGroup::liquid_glass: {
      static constexpr std::array<TokenRange, 11> fields{
          TokenRange{.minimum = 0.0, .maximum = 100.0, .step = 0.5},
          TokenRange{.minimum = 0.0, .maximum = 1.0, .step = 0.01},
          TokenRange{.minimum = 0.0, .maximum = 8.0, .step = 0.1},
          TokenRange{.minimum = 0.0, .maximum = 20.0, .step = 0.1},
          TokenRange{.minimum = 0.0, .maximum = 20.0, .step = 0.1},
          TokenRange{.minimum = 0.0, .maximum = 2.0, .step = 0.01},
          TokenRange{.minimum = 0.0, .maximum = 1.0, .step = 0.01},
          TokenRange{.minimum = 0.0, .maximum = 80.0, .step = 0.5},
          TokenRange{.minimum = 0.0, .maximum = 40.0, .step = 0.5},
          TokenRange{.minimum = 0.0, .maximum = 1.0, .step = 0.01},
          TokenRange{.minimum = 0.0, .maximum = 80.0, .step = 0.5}};
      return fields.at(field_);
    }
    case TokenGroup::motion:
      return TokenRange{.minimum = 0.05, .maximum = 2.0, .step = 0.01};
    case TokenGroup::colours:
    case TokenGroup::glass:
      break;
  }
  return std::nullopt;
}

auto ThemeToken::choices() const noexcept -> std::span<const std::string_view> {
  static constexpr std::array<std::string_view, 2> designs{"text", "display"};
  static constexpr std::array<std::string_view, 4> optional_glass{
      "none", "regular", "clear", "identity"};
  static constexpr std::array<std::string_view, 3> glass_variants{
      "regular", "clear", "identity"};
  if (kind_ != TokenKind::choice) {
    return {};
  }
  if (group_ == TokenGroup::type) {
    return designs;
  }
  return slot_ < 3 ? std::span<const std::string_view>{optional_glass}
                   : std::span<const std::string_view>{glass_variants};
}

auto ThemeToken::adaptive(const theme::Adaptive<theme::Colour>& pair,
                          std::uint8_t side) -> theme::Colour {
  return side == 0 ? pair.light : pair.dark;
}

auto ThemeToken::adaptive(theme::Adaptive<theme::Colour> pair,
                          std::uint8_t side, theme::Colour colour)
    -> theme::Adaptive<theme::Colour> {
  (side == 0 ? pair.light : pair.dark) = colour;
  return pair;
}

auto ThemeToken::glass(const theme::GlassDefaults& defaults, std::uint16_t slot)
    -> std::optional<theme::Glass> {
  switch (slot) {
    case 0:
      return defaults.sidebar;
    case 1:
      return defaults.toolbar;
    case 2:
      return defaults.sheet;
    case 3:
      return defaults.control;
    default:
      break;
  }
  return defaults.panel;
}

auto ThemeToken::with_glass(theme::GlassDefaults defaults, std::uint16_t slot,
                            std::optional<theme::Glass> glass)
    -> theme::GlassDefaults {
  switch (slot) {
    case 0:
      defaults.sidebar = glass;
      break;
    case 1:
      defaults.toolbar = glass;
      break;
    case 2:
      defaults.sheet = glass;
      break;
    case 3:
      defaults.control = glass.value_or(theme::Glass::identity());
      break;
    default:
      defaults.panel = glass.value_or(theme::Glass::identity());
      break;
  }
  return defaults;
}

auto ThemeToken::read_colour(const theme::Theme& theme) const -> theme::Colour {
  switch (group_) {
    case TokenGroup::colours:
      return adaptive(theme.palette(static_cast<theme::ColourRole>(slot_)),
                      field_);
    case TokenGroup::materials:
      return adaptive(theme.materials().at(slot_).tint,
                      static_cast<std::uint8_t>(field_ - 3));
    default:
      break;
  }
  return adaptive(theme.liquid_glass().tint,
                  static_cast<std::uint8_t>(field_ - 11));
}

auto ThemeToken::write_colour(const theme::Theme& theme,
                              theme::Colour colour) const -> theme::Theme {
  switch (group_) {
    case TokenGroup::colours: {
      const auto role = static_cast<theme::ColourRole>(slot_);
      return theme.with(role, adaptive(theme.palette(role), field_, colour));
    }
    case TokenGroup::materials: {
      theme::Theme::Materials styles = theme.materials();
      theme::MaterialStyle& style = styles.at(slot_);
      style.tint =
          adaptive(style.tint, static_cast<std::uint8_t>(field_ - 3), colour);
      return theme.with(styles);
    }
    default:
      break;
  }
  theme::LiquidGlassStyle style = theme.liquid_glass();
  style.tint =
      adaptive(style.tint, static_cast<std::uint8_t>(field_ - 11), colour);
  return theme.with(style);
}

auto ThemeToken::read_number(const theme::Theme& theme) const -> float {
  switch (group_) {
    case TokenGroup::metrics:
      return theme.metric(static_cast<theme::MetricToken>(slot_));
    case TokenGroup::radii:
      return slot_ == 0   ? theme.radii().control
             : slot_ == 1 ? theme.radii().card
                          : theme.radii().sheet;
    case TokenGroup::spacing:
      return theme.spacing().at(slot_);
    case TokenGroup::type: {
      const theme::TypeStyle& style =
          theme.base_type(static_cast<theme::TextRole>(slot_));
      switch (field_) {
        case 0:
          return style.size;
        case 1:
          return static_cast<float>(style.weight);
        case 2:
          return style.line_height;
        default:
          return style.tracking;
      }
    }
    case TokenGroup::materials: {
      const theme::MaterialStyle& style = theme.materials().at(slot_);
      switch (field_) {
        case 0:
          return style.blur;
        case 1:
          return style.saturation;
        case 2:
          return style.noise;
        case 5:
          return style.luminosity.light;
        default:
          return style.luminosity.dark;
      }
    }
    case TokenGroup::liquid_glass: {
      const theme::LiquidGlassStyle& style = theme.liquid_glass();
      const std::array<float, 11> values{
          style.refraction,          style.rim,
          style.rim_width,           style.blur,
          style.clear_blur,          style.highlight,
          style.shadow.key.opacity,  style.shadow.key.blur,
          style.shadow.key.offset_y, style.shadow.ambient.opacity,
          style.shadow.ambient.blur};
      return values.at(field_);
    }
    case TokenGroup::motion: {
      const theme::SpringToken& spring =
          slot_ == 0 ? theme.motion().focus : theme.motion().standard;
      return static_cast<float>(field_ == 0 ? spring.response
                                            : spring.damping_ratio);
    }
    case TokenGroup::colours:
    case TokenGroup::glass:
      break;
  }
  return 0.0F;
}

auto ThemeToken::write_number(const theme::Theme& theme, float value) const
    -> theme::Theme {
  switch (group_) {
    case TokenGroup::metrics:
      return theme.with(static_cast<theme::MetricToken>(slot_), value);
    case TokenGroup::radii: {
      theme::Theme::Radii radii = theme.radii();
      (slot_ == 0   ? radii.control
       : slot_ == 1 ? radii.card
                    : radii.sheet) = value;
      return theme.with(radii);
    }
    case TokenGroup::spacing: {
      theme::Theme::Spacing spacing = theme.spacing();
      spacing.at(slot_) = value;
      return theme.with(spacing);
    }
    case TokenGroup::type: {
      const auto role = static_cast<theme::TextRole>(slot_);
      theme::TypeStyle style = theme.base_type(role);
      switch (field_) {
        case 0:
          style.size = value;
          break;
        case 1:
          style.weight = static_cast<std::uint16_t>(value);
          break;
        case 2:
          style.line_height = value;
          break;
        default:
          style.tracking = value;
          break;
      }
      return theme.with(role, style);
    }
    case TokenGroup::materials: {
      theme::Theme::Materials styles = theme.materials();
      theme::MaterialStyle& style = styles.at(slot_);
      switch (field_) {
        case 0:
          style.blur = value;
          break;
        case 1:
          style.saturation = value;
          break;
        case 2:
          style.noise = value;
          break;
        case 5:
          style.luminosity.light = value;
          break;
        default:
          style.luminosity.dark = value;
          break;
      }
      return theme.with(styles);
    }
    case TokenGroup::liquid_glass: {
      theme::LiquidGlassStyle style = theme.liquid_glass();
      const std::array<float*, 11> fields{
          &style.refraction,          &style.rim,
          &style.rim_width,           &style.blur,
          &style.clear_blur,          &style.highlight,
          &style.shadow.key.opacity,  &style.shadow.key.blur,
          &style.shadow.key.offset_y, &style.shadow.ambient.opacity,
          &style.shadow.ambient.blur};
      *fields.at(field_) = value;
      return theme.with(style);
    }
    case TokenGroup::motion: {
      theme::Motion motion = theme.motion();
      theme::SpringToken& spring = slot_ == 0 ? motion.focus : motion.standard;
      (field_ == 0 ? spring.response : spring.damping_ratio) = value;
      return theme.with(motion);
    }
    case TokenGroup::colours:
    case TokenGroup::glass:
      break;
  }
  return theme;
}

auto ThemeToken::read_choice(const theme::Theme& theme) const -> std::size_t {
  if (group_ == TokenGroup::type) {
    return std::to_underlying(
        theme.base_type(static_cast<theme::TextRole>(slot_)).design);
  }
  const std::optional<theme::Glass> found = glass(theme.glass(), slot_);
  if (!found.has_value()) {
    return 0;
  }
  const auto variant =
      static_cast<std::size_t>(std::to_underlying(found->variant()));
  return slot_ < 3 ? variant + 1 : variant;
}

auto ThemeToken::write_choice(const theme::Theme& theme,
                              std::size_t choice) const -> theme::Theme {
  if (group_ == TokenGroup::type) {
    const auto role = static_cast<theme::TextRole>(slot_);
    theme::TypeStyle style = theme.base_type(role);
    style.design =
        choice == 0 ? theme::TypeDesign::text : theme::TypeDesign::display;
    return theme.with(role, style);
  }
  if (slot_ < 3 && choice == 0) {
    return theme.with(with_glass(theme.glass(), slot_, std::nullopt));
  }
  const std::size_t variant = slot_ < 3 ? choice - 1 : choice;
  const theme::Glass current =
      glass(theme.glass(), slot_).value_or(theme::Glass::regular());
  theme::Glass chosen = theme::Glass::identity();
  if (variant == 0) {
    chosen = theme::Glass::regular()
                 .tint(current.tint_colour())
                 .interactive(current.is_interactive());
  } else if (variant == 1) {
    chosen = theme::Glass::clear()
                 .tint(current.tint_colour())
                 .interactive(current.is_interactive());
  }
  return theme.with(with_glass(theme.glass(), slot_, chosen));
}

auto ThemeToken::read_toggle(const theme::Theme& theme) const -> bool {
  if (group_ == TokenGroup::radii) {
    return theme.radii().continuous;
  }
  const std::optional<theme::Glass> found = glass(theme.glass(), slot_);
  return found.has_value() && found->is_interactive();
}

auto ThemeToken::write_toggle(const theme::Theme& theme, bool on) const
    -> theme::Theme {
  if (group_ == TokenGroup::radii) {
    theme::Theme::Radii radii = theme.radii();
    radii.continuous = on;
    return theme.with(radii);
  }
  const std::optional<theme::Glass> found = glass(theme.glass(), slot_);
  if (!found.has_value() ||
      found->variant() == theme::Glass::Variant::identity) {
    return theme;
  }
  return theme.with(with_glass(theme.glass(), slot_, found->interactive(on)));
}

auto ThemeToken::read(const theme::Theme& theme) const -> TokenValue {
  switch (kind_) {
    case TokenKind::colour:
      return read_colour(theme);
    case TokenKind::number:
      return read_number(theme);
    case TokenKind::toggle:
      return read_toggle(theme);
    case TokenKind::choice:
      break;
  }
  return read_choice(theme);
}

auto ThemeToken::write(const theme::Theme& theme, const TokenValue& value) const
    -> theme::Theme {
  const std::optional<TokenValue> clamped = clamp(value);
  if (!clamped.has_value()) {
    return theme;
  }
  switch (kind_) {
    case TokenKind::colour:
      return write_colour(theme, std::get<theme::Colour>(*clamped));
    case TokenKind::number:
      return write_number(theme, std::get<float>(*clamped));
    case TokenKind::toggle:
      return write_toggle(theme, std::get<bool>(*clamped));
    case TokenKind::choice:
      break;
  }
  return write_choice(theme, std::get<std::size_t>(*clamped));
}

auto ThemeToken::clamp(const TokenValue& value) const
    -> std::optional<TokenValue> {
  switch (kind_) {
    case TokenKind::colour:
      if (const auto* colour = std::get_if<theme::Colour>(&value)) {
        return quantised(*colour);
      }
      return std::nullopt;
    case TokenKind::number: {
      const auto* number = std::get_if<float>(&value);
      const std::optional<TokenRange> limits = range();
      if (number == nullptr || !limits.has_value() || !std::isfinite(*number)) {
        return std::nullopt;
      }
      const double steps = std::round(
          (static_cast<double>(*number) - limits->minimum) / limits->step);
      const double snapped =
          std::clamp(limits->minimum + (steps * limits->step), limits->minimum,
                     limits->maximum);
      return static_cast<float>(snapped);
    }
    case TokenKind::toggle:
      if (const auto* on = std::get_if<bool>(&value)) {
        return *on;
      }
      return std::nullopt;
    case TokenKind::choice:
      break;
  }
  const auto* choice = std::get_if<std::size_t>(&value);
  if (choice == nullptr || *choice >= choices().size()) {
    return std::nullopt;
  }
  return *choice;
}

auto ThemeToken::describe(const TokenValue& value) const -> std::string {
  if (const auto* colour = std::get_if<theme::Colour>(&value)) {
    return ThemeVocabulary::hex(*colour);
  }
  if (const auto* number = std::get_if<float>(&value)) {
    return std::format("{:g}", *number);
  }
  if (const auto* on = std::get_if<bool>(&value)) {
    return *on ? "on" : "off";
  }
  const std::size_t choice = std::get<std::size_t>(value);
  const std::span<const std::string_view> names = choices();
  return choice < names.size() ? std::string{names[choice]}
                               : std::format("{}", choice);
}

}
