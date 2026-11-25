#include <arisa/appearance/theme_file.hpp>
#include <arisa/appearance/theme_token.hpp>
#include <arisa/appearance/theme_vocabulary.hpp>
#include <arisa/theme/dynamic_type.hpp>
#include <arisa/theme/typography.hpp>

#include <toml.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <flat_set>
#include <format>
#include <sstream>
#include <string>
#include <utility>
#include <variant>

namespace arisa::appearance {

class ThemeFile::Reader {
 public:
  static constexpr double minimum_weight = 1.0;
  static constexpr double maximum_weight = 1000.0;

  Reader(std::string_view file, theme::Theme base)
      : file_(file), theme_(std::move(base)) {}

  auto read(const toml::table& document) -> std::expected<void, ThemeError>;

  auto take() -> theme::Theme { return std::move(theme_); }

 private:
  auto fail(ThemeErrorCode code, const toml::node& node, std::string key,
            std::string message) const -> std::unexpected<ThemeError>;

  auto colour(const toml::node& node, const std::string& key) const
      -> std::expected<theme::Colour, ThemeError>;

  auto number(const toml::node& node, const std::string& key) const
      -> std::expected<float, ThemeError>;

  auto table(const toml::node& node, const std::string& key) const
      -> std::expected<std::reference_wrapper<const toml::table>, ThemeError>;

  auto colours(const toml::table& section) -> std::expected<void, ThemeError>;

  auto metrics(const toml::table& section) -> std::expected<void, ThemeError>;

  auto radii(const toml::table& section) -> std::expected<void, ThemeError>;

  auto motion(const toml::table& section) -> std::expected<void, ThemeError>;

  auto spacing(const toml::table& section) -> std::expected<void, ThemeError>;

  auto type(const toml::table& section) -> std::expected<void, ThemeError>;

  auto glass(const toml::table& section) -> std::expected<void, ThemeError>;

  auto focus(const toml::table& section) -> std::expected<void, ThemeError>;

  auto materials(const toml::table& section) -> std::expected<void, ThemeError>;

  auto liquid_glass(const toml::table& section)
      -> std::expected<void, ThemeError>;

  auto adaptive(const toml::node& node, const std::string& key,
                theme::Adaptive<theme::Colour> fallback) const
      -> std::expected<theme::Adaptive<theme::Colour>, ThemeError>;

  auto signed_number(const toml::node& node, const std::string& key) const
      -> std::expected<float, ThemeError>;

  auto weight(const toml::node& node, const std::string& key) const
      -> std::expected<std::uint16_t, ThemeError>;

  auto lift(const toml::node& node, const std::string& key,
            theme::Adaptive<float> fallback) const
      -> std::expected<theme::Adaptive<float>, ThemeError>;

  auto preferences(const toml::table& section)
      -> std::expected<void, ThemeError>;

  auto surface(const toml::node& node, const std::string& key) const
      -> std::expected<std::optional<theme::Glass>, ThemeError>;

  std::string file_;
  theme::Theme theme_;
};

auto ThemeFile::Reader::fail(ThemeErrorCode code, const toml::node& node,
                             std::string key, std::string message) const
    -> std::unexpected<ThemeError> {
  return std::unexpected(ThemeError{
      code, file_, static_cast<std::uint32_t>(node.source().begin.line),
      std::move(key), std::move(message)});
}

auto ThemeFile::Reader::colour(const toml::node& node,
                               const std::string& key) const
    -> std::expected<theme::Colour, ThemeError> {
  const std::optional<std::string_view> text = node.value<std::string_view>();
  if (!text.has_value()) {
    return fail(ThemeErrorCode::invalid_value, node, key,
                "expected a colour string such as \"#RRGGBB\"");
  }
  const std::optional<theme::Colour> parsed = ThemeVocabulary::colour(*text);
  if (!parsed.has_value()) {
    return fail(ThemeErrorCode::invalid_value, node, key,
                std::format("\"{}\" is not #RRGGBB or #RRGGBBAA", *text));
  }
  return *parsed;
}

auto ThemeFile::Reader::number(const toml::node& node,
                               const std::string& key) const
    -> std::expected<float, ThemeError> {
  const std::optional<double> value = node.value<double>();
  if (!value.has_value() || *value < 0.0) {
    return fail(ThemeErrorCode::invalid_value, node, key,
                "expected a non-negative number");
  }
  return static_cast<float>(*value);
}

auto ThemeFile::Reader::table(const toml::node& node,
                              const std::string& key) const
    -> std::expected<std::reference_wrapper<const toml::table>, ThemeError> {
  const toml::table* found = node.as_table();
  if (found == nullptr) {
    return fail(ThemeErrorCode::invalid_value, node, key, "expected a table");
  }
  return std::cref(*found);
}

auto ThemeFile::Reader::colours(const toml::table& section)
    -> std::expected<void, ThemeError> {
  for (const auto& [name, node] : section) {
    const std::string key = std::format("colours.{}", name.str());
    const std::optional<theme::ColourRole> role =
        ThemeVocabulary::colour_role(name.str());
    if (!role.has_value()) {
      return fail(ThemeErrorCode::unknown_key, node, key,
                  "not a semantic colour role");
    }
    theme::Adaptive<theme::Colour> value = theme_.palette(*role);
    if (const toml::table* pair = node.as_table()) {
      for (const auto& [scheme, entry] : *pair) {
        const std::string inner = std::format("{}.{}", key, scheme.str());
        auto parsed = colour(entry, inner);
        if (!parsed.has_value()) {
          return std::unexpected(parsed.error());
        }
        if (scheme.str() == "light") {
          value.light = *parsed;
        } else if (scheme.str() == "dark") {
          value.dark = *parsed;
        } else {
          return fail(ThemeErrorCode::unknown_key, entry, inner,
                      "expected light or dark");
        }
      }
    } else {
      auto parsed = colour(node, key);
      if (!parsed.has_value()) {
        return std::unexpected(parsed.error());
      }
      value = theme::Adaptive<theme::Colour>{*parsed, *parsed};
    }
    theme_ = theme_.with(*role, value);
  }
  return {};
}

auto ThemeFile::Reader::metrics(const toml::table& section)
    -> std::expected<void, ThemeError> {
  for (const auto& [name, node] : section) {
    const std::string key = std::format("metrics.{}", name.str());
    const std::optional<theme::MetricToken> token =
        ThemeVocabulary::metric(name.str());
    if (!token.has_value()) {
      return fail(ThemeErrorCode::unknown_key, node, key, "not a metric token");
    }
    auto value = number(node, key);
    if (!value.has_value()) {
      return std::unexpected(value.error());
    }
    theme_ = theme_.with(*token, *value);
  }
  return {};
}

auto ThemeFile::Reader::radii(const toml::table& section)
    -> std::expected<void, ThemeError> {
  theme::Theme::Radii radii = theme_.radii();
  for (const auto& [name, node] : section) {
    const std::string key = std::format("radii.{}", name.str());
    if (name.str() == "continuous") {
      const std::optional<bool> flag = node.value<bool>();
      if (!flag.has_value()) {
        return fail(ThemeErrorCode::invalid_value, node, key,
                    "expected true or false");
      }
      radii.continuous = *flag;
      continue;
    }
    auto value = number(node, key);
    if (!value.has_value()) {
      return std::unexpected(value.error());
    }
    if (name.str() == "control") {
      radii.control = *value;
    } else if (name.str() == "card") {
      radii.card = *value;
    } else if (name.str() == "sheet") {
      radii.sheet = *value;
    } else {
      return fail(ThemeErrorCode::unknown_key, node, key,
                  "expected control, card, sheet or continuous");
    }
  }
  theme_ = theme_.with(radii);
  return {};
}

auto ThemeFile::Reader::motion(const toml::table& section)
    -> std::expected<void, ThemeError> {
  theme::Motion tokens = theme_.motion();
  for (const auto& [name, node] : section) {
    const std::string key = std::format("motion.{}", name.str());
    theme::SpringToken* spring = nullptr;
    if (name.str() == "focus") {
      spring = &tokens.focus;
    } else if (name.str() == "standard") {
      spring = &tokens.standard;
    } else {
      return fail(ThemeErrorCode::unknown_key, node, key,
                  "expected focus or standard");
    }
    auto entry = table(node, key);
    if (!entry.has_value()) {
      return std::unexpected(entry.error());
    }
    for (const auto& [field, value] : entry->get()) {
      const std::string inner = std::format("{}.{}", key, field.str());
      auto parsed = number(value, inner);
      if (!parsed.has_value()) {
        return std::unexpected(parsed.error());
      }
      if (*parsed <= 0.0F) {
        return fail(ThemeErrorCode::invalid_value, value, inner,
                    "expected a positive number");
      }
      if (field.str() == "response") {
        spring->response = *parsed;
      } else if (field.str() == "damping_ratio" ||
                 field.str() == "damping_fraction") {
        spring->damping_ratio = *parsed;
      } else {
        return fail(ThemeErrorCode::unknown_key, value, inner,
                    "expected response or damping_ratio");
      }
    }
  }
  theme_ = theme_.with(tokens);
  return {};
}

auto ThemeFile::Reader::spacing(const toml::table& section)
    -> std::expected<void, ThemeError> {
  theme::Theme::Spacing spacing = theme_.spacing();
  for (const auto& [name, node] : section) {
    const std::string key = std::format("spacing.{}", name.str());
    const std::optional<theme::SpaceToken> token =
        ThemeVocabulary::space(name.str());
    if (!token.has_value()) {
      return fail(ThemeErrorCode::unknown_key, node, key, "not a space token");
    }
    auto value = number(node, key);
    if (!value.has_value()) {
      return std::unexpected(value.error());
    }
    spacing[std::to_underlying(*token)] = *value;
  }
  theme_ = theme_.with(spacing);
  return {};
}

auto ThemeFile::Reader::type(const toml::table& section)
    -> std::expected<void, ThemeError> {
  for (const auto& [name, node] : section) {
    const std::string key = std::format("type.{}", name.str());
    const std::optional<theme::TextRole> role =
        ThemeVocabulary::text_role(name.str());
    if (!role.has_value()) {
      return fail(ThemeErrorCode::unknown_key, node, key, "not a text style");
    }
    auto fields = table(node, key);
    if (!fields.has_value()) {
      return std::unexpected(fields.error());
    }
    theme::TypeStyle style = theme_.base_type(*role);
    for (const auto& [field, entry] : fields->get()) {
      const std::string inner = std::format("{}.{}", key, field.str());
      if (field.str() == "design") {
        const std::optional<std::string_view> design =
            entry.value<std::string_view>();
        if (design == "text") {
          style.design = theme::TypeDesign::text;
        } else if (design == "display") {
          style.design = theme::TypeDesign::display;
        } else {
          return fail(ThemeErrorCode::invalid_value, entry, inner,
                      "expected text or display");
        }
        continue;
      }
      if (field.str() == "weight") {
        auto weight = this->weight(entry, inner);
        if (!weight.has_value()) {
          return std::unexpected(weight.error());
        }
        style.weight = *weight;
        continue;
      }
      if (field.str() == "tracking") {
        auto tracking = signed_number(entry, inner);
        if (!tracking.has_value()) {
          return std::unexpected(tracking.error());
        }
        style.tracking = *tracking;
        continue;
      }
      auto value = number(entry, inner);
      if (!value.has_value()) {
        return std::unexpected(value.error());
      }
      if (field.str() == "size") {
        style.size = *value;
      } else if (field.str() == "line_height") {
        style.line_height = *value;
      } else {
        return fail(ThemeErrorCode::unknown_key, entry, inner,
                    "expected size, weight, line_height, tracking or design");
      }
    }
    theme_ = theme_.with(*role, style);
  }
  return {};
}

auto ThemeFile::Reader::surface(const toml::node& node,
                                const std::string& key) const
    -> std::expected<std::optional<theme::Glass>, ThemeError> {
  if (const std::optional<bool> flag = node.value<bool>()) {
    if (*flag) {
      return theme::Glass::regular();
    }
    return std::optional<theme::Glass>{};
  }
  auto fields = table(node, key);
  if (!fields.has_value()) {
    return std::unexpected(fields.error());
  }
  theme::Glass glass = theme::Glass::regular();
  for (const auto& [field, entry] : fields->get()) {
    const std::string inner = std::format("{}.{}", key, field.str());
    if (field.str() == "variant") {
      const std::optional<std::string_view> variant =
          entry.value<std::string_view>();
      if (variant == "regular") {
        glass = theme::Glass::regular()
                    .tint(glass.tint_colour())
                    .interactive(glass.is_interactive());
      } else if (variant == "clear") {
        glass = theme::Glass::clear()
                    .tint(glass.tint_colour())
                    .interactive(glass.is_interactive());
      } else if (variant == "identity") {
        glass = theme::Glass::identity();
      } else {
        return fail(ThemeErrorCode::invalid_value, entry, inner,
                    "expected regular, clear or identity");
      }
    } else if (field.str() == "tint") {
      const std::optional<std::string_view> text =
          entry.value<std::string_view>();
      if (text.has_value()) {
        if (const auto role = ThemeVocabulary::colour_role(*text)) {
          glass = glass.tint(theme::ColourValue{*role});
          continue;
        }
      }
      auto parsed = colour(entry, inner);
      if (!parsed.has_value()) {
        return std::unexpected(parsed.error());
      }
      glass = glass.tint(theme::ColourValue{*parsed});
    } else if (field.str() == "interactive") {
      const std::optional<bool> flag = entry.value<bool>();
      if (!flag.has_value()) {
        return fail(ThemeErrorCode::invalid_value, entry, inner,
                    "expected true or false");
      }
      glass = glass.interactive(*flag);
    } else {
      return fail(ThemeErrorCode::unknown_key, entry, inner,
                  "expected variant, tint or interactive");
    }
  }
  return glass;
}

auto ThemeFile::Reader::glass(const toml::table& section)
    -> std::expected<void, ThemeError> {
  theme::GlassDefaults defaults = theme_.glass();
  for (const auto& [name, node] : section) {
    const std::string key = std::format("glass.{}", name.str());
    auto parsed = surface(node, key);
    if (!parsed.has_value()) {
      return std::unexpected(parsed.error());
    }
    if (name.str() == "sidebar") {
      defaults.sidebar = *parsed;
    } else if (name.str() == "toolbar") {
      defaults.toolbar = *parsed;
    } else if (name.str() == "sheet") {
      defaults.sheet = *parsed;
    } else if (name.str() == "control") {
      defaults.control = parsed->value_or(theme::Glass::identity());
    } else if (name.str() == "panel") {
      defaults.panel = parsed->value_or(theme::Glass::identity());
    } else {
      return fail(ThemeErrorCode::unknown_key, node, key,
                  "expected sidebar, toolbar, sheet, control or panel");
    }
  }
  theme_ = theme_.with(defaults);
  return {};
}

auto ThemeFile::Reader::focus(const toml::table& section)
    -> std::expected<void, ThemeError> {
  theme::Theme::FocusRing ring = theme_.focus_ring();
  for (const auto& [name, node] : section) {
    const std::string key = std::format("focus.{}", name.str());
    if (name.str() != "style") {
      return fail(ThemeErrorCode::unknown_key, node, key, "expected style");
    }
    const std::optional<std::string_view> text = node.value<std::string_view>();
    if (text == "ring") {
      ring.style = theme::Theme::FocusRing::Style::ring;
    } else if (text == "highlight") {
      ring.style = theme::Theme::FocusRing::Style::highlight;
    } else {
      return fail(ThemeErrorCode::invalid_value, node, key,
                  "expected ring or highlight");
    }
  }
  theme_ = theme_.with(ring);
  return {};
}

auto ThemeFile::Reader::weight(const toml::node& node,
                               const std::string& key) const
    -> std::expected<std::uint16_t, ThemeError> {
  using Weight = theme::Font::Weight;
  constexpr std::array<std::pair<std::string_view, Weight>, 9> named{{
      {"ultra_light", Weight::ultra_light},
      {"thin", Weight::thin},
      {"light", Weight::light},
      {"regular", Weight::regular},
      {"medium", Weight::medium},
      {"semibold", Weight::semibold},
      {"bold", Weight::bold},
      {"heavy", Weight::heavy},
      {"black", Weight::black},
  }};
  if (const std::optional<std::string_view> text =
          node.value<std::string_view>()) {
    const auto found = std::ranges::find(
        named, *text, &std::pair<std::string_view, Weight>::first);
    if (found == named.end()) {
      return fail(ThemeErrorCode::invalid_value, node, key,
                  "expected a weight name such as \"semibold\" or 1 to 1000");
    }
    return std::to_underlying(found->second);
  }
  const std::optional<double> value = node.value<double>();
  if (!value.has_value() || *value < minimum_weight ||
      *value > maximum_weight) {
    return fail(ThemeErrorCode::invalid_value, node, key,
                "expected a weight name such as \"semibold\" or 1 to 1000");
  }
  return static_cast<std::uint16_t>(*value);
}

auto ThemeFile::Reader::signed_number(const toml::node& node,
                                      const std::string& key) const
    -> std::expected<float, ThemeError> {
  const std::optional<double> value = node.value<double>();
  if (!value.has_value()) {
    return fail(ThemeErrorCode::invalid_value, node, key, "expected a number");
  }
  return static_cast<float>(*value);
}

auto ThemeFile::Reader::adaptive(const toml::node& node, const std::string& key,
                                 theme::Adaptive<theme::Colour> fallback) const
    -> std::expected<theme::Adaptive<theme::Colour>, ThemeError> {
  if (const toml::table* pair = node.as_table()) {
    for (const auto& [scheme, entry] : *pair) {
      const std::string inner = std::format("{}.{}", key, scheme.str());
      auto parsed = colour(entry, inner);
      if (!parsed.has_value()) {
        return std::unexpected(parsed.error());
      }
      if (scheme.str() == "light") {
        fallback.light = *parsed;
      } else if (scheme.str() == "dark") {
        fallback.dark = *parsed;
      } else {
        return fail(ThemeErrorCode::unknown_key, entry, inner,
                    "expected light or dark");
      }
    }
    return fallback;
  }
  auto parsed = colour(node, key);
  if (!parsed.has_value()) {
    return std::unexpected(parsed.error());
  }
  return theme::Adaptive<theme::Colour>{.light = *parsed, .dark = *parsed};
}

auto ThemeFile::Reader::lift(const toml::node& node, const std::string& key,
                             theme::Adaptive<float> fallback) const
    -> std::expected<theme::Adaptive<float>, ThemeError> {
  if (const toml::table* pair = node.as_table()) {
    for (const auto& [scheme, entry] : *pair) {
      const std::string inner = std::format("{}.{}", key, scheme.str());
      auto parsed = signed_number(entry, inner);
      if (!parsed.has_value()) {
        return std::unexpected(parsed.error());
      }
      if (scheme.str() == "light") {
        fallback.light = *parsed;
      } else if (scheme.str() == "dark") {
        fallback.dark = *parsed;
      } else {
        return fail(ThemeErrorCode::unknown_key, entry, inner,
                    "expected light or dark");
      }
    }
    return fallback;
  }
  auto parsed = signed_number(node, key);
  if (!parsed.has_value()) {
    return std::unexpected(parsed.error());
  }
  return theme::Adaptive<float>{.light = *parsed, .dark = *parsed};
}

auto ThemeFile::Reader::materials(const toml::table& section)
    -> std::expected<void, ThemeError> {
  theme::Theme::Materials styles = theme_.materials();
  for (const auto& [name, node] : section) {
    const std::string key = std::format("materials.{}", name.str());
    const std::optional<theme::Material> token =
        theme::Material::parse(name.str());
    if (!token.has_value()) {
      return fail(ThemeErrorCode::unknown_key, node, key,
                  "expected ultra_thin, thin, regular, thick, ultra_thick, "
                  "sidebar, menu, hud or bar");
    }
    auto fields = table(node, key);
    if (!fields.has_value()) {
      return std::unexpected(fields.error());
    }
    theme::MaterialStyle& style = styles[token->index()];
    for (const auto& [field, entry] : fields->get()) {
      const std::string inner = std::format("{}.{}", key, field.str());
      const std::string_view which = field.str();
      if (which == "blur" || which == "saturation" || which == "noise") {
        auto parsed = number(entry, inner);
        if (!parsed.has_value()) {
          return std::unexpected(parsed.error());
        }
        (which == "blur"         ? style.blur
         : which == "saturation" ? style.saturation
                                 : style.noise) = *parsed;
      } else if (which == "tint") {
        auto parsed = adaptive(entry, inner, style.tint);
        if (!parsed.has_value()) {
          return std::unexpected(parsed.error());
        }
        style.tint = *parsed;
      } else if (which == "luminosity") {
        auto parsed = lift(entry, inner, style.luminosity);
        if (!parsed.has_value()) {
          return std::unexpected(parsed.error());
        }
        style.luminosity = *parsed;
      } else {
        return fail(ThemeErrorCode::unknown_key, entry, inner,
                    "expected blur, saturation, tint, luminosity or noise");
      }
    }
  }
  theme_ = theme_.with(styles);
  return {};
}

auto ThemeFile::Reader::liquid_glass(const toml::table& section)
    -> std::expected<void, ThemeError> {
  theme::LiquidGlassStyle style = theme_.liquid_glass();
  for (const auto& [field, entry] : section) {
    const std::string key = std::format("liquid_glass.{}", field.str());
    const std::string_view which = field.str();
    if (which == "tint") {
      auto parsed = adaptive(entry, key, style.tint);
      if (!parsed.has_value()) {
        return std::unexpected(parsed.error());
      }
      style.tint = *parsed;
      continue;
    }
    auto parsed = number(entry, key);
    if (!parsed.has_value()) {
      return std::unexpected(parsed.error());
    }
    if (which == "refraction") {
      style.refraction = *parsed;
    } else if (which == "rim") {
      style.rim = *parsed;
    } else if (which == "blur") {
      style.blur = *parsed;
    } else if (which == "clear_blur") {
      style.clear_blur = *parsed;
    } else if (which == "highlight") {
      style.highlight = *parsed;
    } else if (which == "rim_width") {
      style.rim_width = *parsed;
    } else if (which == "shadow_opacity") {
      style.shadow.key.opacity = *parsed;
    } else if (which == "shadow_blur") {
      style.shadow.key.blur = *parsed;
    } else if (which == "shadow_offset") {
      style.shadow.key.offset_y = *parsed;
    } else if (which == "ambient_opacity") {
      style.shadow.ambient.opacity = *parsed;
    } else if (which == "ambient_blur") {
      style.shadow.ambient.blur = *parsed;
    } else {
      return fail(ThemeErrorCode::unknown_key, entry, key,
                  "expected refraction, rim, rim_width, blur, clear_blur, "
                  "highlight, shadow_opacity, shadow_blur, shadow_offset, "
                  "ambient_opacity, ambient_blur or tint");
    }
  }
  theme_ = theme_.with(style);
  return {};
}

auto ThemeFile::Reader::preferences(const toml::table& section)
    -> std::expected<void, ThemeError> {
  theme::Theme::Preferences preferred = theme_.preferences();
  for (const auto& [name, node] : section) {
    const std::string key = std::format("preferences.{}", name.str());
    if (name.str() == "dynamic_type_size") {
      const std::optional<std::string_view> text =
          node.value<std::string_view>();
      const std::optional<theme::DynamicTypeSize> size =
          text.has_value() ? theme::DynamicType::parse(*text) : std::nullopt;
      if (!size.has_value()) {
        return fail(ThemeErrorCode::invalid_value, node, key,
                    "expected a dynamic type size such as \"large\"");
      }
      preferred.dynamic_type_size = size;
      continue;
    }
    if (name.str() == "color_scheme" || name.str() == "colour_scheme") {
      const std::optional<std::string_view> text =
          node.value<std::string_view>();
      if (text == "light") {
        preferred.color_scheme = theme::ColorScheme::light;
      } else if (text == "dark") {
        preferred.color_scheme = theme::ColorScheme::dark;
      } else {
        return fail(ThemeErrorCode::invalid_value, node, key,
                    "expected light or dark");
      }
      continue;
    }
    auto value = number(node, key);
    if (!value.has_value()) {
      return std::unexpected(value.error());
    }
    if (name.str() == "handheld_scale") {
      preferred.handheld_scale = *value;
    } else if (name.str() == "docked_scale") {
      preferred.docked_scale = *value;
    } else if (name.str() == "desktop_scale") {
      preferred.desktop_scale = *value;
    } else {
      return fail(
          ThemeErrorCode::unknown_key, node, key,
          "expected dynamic_type_size, color_scheme or a display scale");
    }
  }
  theme_ = theme_.with(preferred);
  return {};
}

auto ThemeFile::Reader::read(const toml::table& document)
    -> std::expected<void, ThemeError> {
  for (const auto& [name, node] : document) {
    const std::string key{name.str()};
    std::expected<void, ThemeError> done{};
    if (key == "schema" || key == "extends") {
      continue;
    }
    if (key == "name") {
      const std::optional<std::string_view> text =
          node.value<std::string_view>();
      if (!text.has_value()) {
        return fail(ThemeErrorCode::invalid_value, node, key,
                    "expected a string");
      }
      theme_ = theme_.named(std::string{*text});
      continue;
    }
    if (key == "accent") {
      auto parsed = colour(node, key);
      if (!parsed.has_value()) {
        return std::unexpected(parsed.error());
      }
      theme_ = theme_.with_accent(*parsed);
      continue;
    }
    auto section = table(node, key);
    if (!section.has_value()) {
      return std::unexpected(section.error());
    }
    if (key == "colours" || key == "colors") {
      done = colours(section->get());
    } else if (key == "metrics") {
      done = metrics(section->get());
    } else if (key == "radii") {
      done = radii(section->get());
    } else if (key == "motion") {
      done = motion(section->get());
    } else if (key == "spacing") {
      done = spacing(section->get());
    } else if (key == "type") {
      done = type(section->get());
    } else if (key == "glass") {
      done = glass(section->get());
    } else if (key == "focus") {
      done = focus(section->get());
    } else if (key == "materials") {
      done = materials(section->get());
    } else if (key == "liquid_glass") {
      done = liquid_glass(section->get());
    } else if (key == "preferences") {
      done = preferences(section->get());
    } else {
      return fail(ThemeErrorCode::unknown_key, node, key,
                  "not a theme section");
    }
    if (!done.has_value()) {
      return done;
    }
  }
  return {};
}

auto ThemeFile::base_of(std::string_view text) -> std::optional<std::string> {
  toml::parse_result parsed = toml::parse(text);
  if (!parsed) {
    return std::nullopt;
  }
  const std::optional<std::string_view> base =
      parsed.table()["extends"].value<std::string_view>();
  if (!base.has_value()) {
    return std::nullopt;
  }
  return std::string{*base};
}

auto ThemeFile::parse(std::string_view text, std::string_view file,
                      const Resolver& bases)
    -> std::expected<theme::Theme, ThemeError> {
  toml::parse_result parsed = toml::parse(text, file);
  if (!parsed) {
    const toml::parse_error& error = parsed.error();
    return std::unexpected(
        ThemeError{ThemeErrorCode::syntax,
                   std::string{file},
                   static_cast<std::uint32_t>(error.source().begin.line),
                   {},
                   std::string{error.description()}});
  }
  const toml::table& document = parsed.table();
  const toml::node_view<const toml::node> schema = document["schema"];
  const std::optional<std::int64_t> version = schema.value<std::int64_t>();
  if (!version.has_value() || *version != schema_version) {
    const std::uint32_t line =
        schema.node() != nullptr
            ? static_cast<std::uint32_t>(schema.node()->source().begin.line)
            : 1U;
    return std::unexpected(ThemeError{
        ThemeErrorCode::unsupported_schema, std::string{file}, line, "schema",
        std::format("expected schema = {}", schema_version)});
  }
  theme::Theme base = theme::Theme::defaults();
  if (const toml::node* extends = document.get("extends")) {
    const std::optional<std::string_view> name =
        extends->value<std::string_view>();
    if (!name.has_value()) {
      return std::unexpected(
          ThemeError{ThemeErrorCode::invalid_value, std::string{file},
                     static_cast<std::uint32_t>(extends->source().begin.line),
                     "extends", "expected a theme name"});
    }
    auto resolved = bases(*name);
    if (!resolved.has_value()) {
      return std::unexpected(resolved.error());
    }
    base = std::move(*resolved);
  }
  Reader reader{file, std::move(base)};
  if (auto read = reader.read(document); !read.has_value()) {
    return std::unexpected(read.error());
  }
  return reader.take();
}

class ThemeFile::Writer {
 public:
  Writer(const theme::Theme& edited, std::string_view name,
         std::string_view extends)
      : edited_(edited) {
    document_.insert("schema", schema_version);
    document_.insert("name", std::string{name});
    document_.insert("extends", std::string{extends});
  }

  void add(const ThemeToken& token, const TokenValue& value) {
    if (token.group() == TokenGroup::glass) {
      surfaces_.insert(
          std::string{token.label().substr(0, token.label().find('.'))});
      return;
    }
    if (const auto* colour = std::get_if<theme::Colour>(&value)) {
      place(token.path(), ThemeVocabulary::hex(*colour));
    } else if (const auto* number = std::get_if<float>(&value)) {
      if (token.path().ends_with(".weight")) {
        place(token.path(), static_cast<std::int64_t>(*number));
      } else {
        place(token.path(), decimal(*number));
      }
    } else if (const auto* on = std::get_if<bool>(&value)) {
      place(token.path(), *on);
    } else {
      place(token.path(), token.describe(value));
    }
  }

  auto text() -> std::string {
    write_surfaces();
    std::ostringstream stream;
    stream << document_ << '\n';
    return std::move(stream).str();
  }

 private:
  static auto decimal(float value) -> double {
    std::array<char, 32> buffer{};
    const auto written =
        std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    double parsed = static_cast<double>(value);
    static_cast<void>(std::from_chars(buffer.data(), written.ptr, parsed));
    return parsed;
  }

  auto section(toml::table& parent, std::string_view key) -> toml::table& {
    auto [found, inserted] = parent.emplace<toml::table>(key);
    static_cast<void>(inserted);
    return *found->second.as_table();
  }

  template <class T>
  void place(std::string_view path, T value) {
    toml::table* table = &document_;
    std::size_t start = 0;
    for (std::size_t dot = path.find('.'); dot != std::string_view::npos;
         dot = path.find('.', start)) {
      table = &section(*table, path.substr(start, dot - start));
      start = dot + 1;
    }
    table->insert_or_assign(path.substr(start), std::move(value));
  }

  void write_surfaces() {
    static constexpr std::array<std::string_view, 3> variants{
        "regular", "clear", "identity"};
    for (const std::string& surface : surfaces_) {
      const theme::GlassDefaults& defaults = edited_.glass();
      std::optional<theme::Glass> glass;
      if (surface == "sidebar") {
        glass = defaults.sidebar;
      } else if (surface == "toolbar") {
        glass = defaults.toolbar;
      } else if (surface == "sheet") {
        glass = defaults.sheet;
      } else if (surface == "control") {
        glass = defaults.control;
      } else {
        glass = defaults.panel;
      }
      toml::table& section_table = section(document_, "glass");
      if (!glass.has_value()) {
        section_table.insert_or_assign(surface, false);
        continue;
      }
      toml::table fields;
      fields.insert(
          "variant",
          std::string{variants.at(std::to_underlying(glass->variant()))});
      if (glass->variant() != theme::Glass::Variant::identity) {
        fields.insert("interactive", glass->is_interactive());
        if (const auto& tint = glass->tint_colour();
            tint.has_value() && tint->opacity() >= 1.0F) {
          if (const theme::ColourRole* role = tint->role()) {
            fields.insert("tint", std::string{ThemeVocabulary::name(*role)});
          } else if (const theme::Colour* literal = tint->literal()) {
            fields.insert("tint", ThemeVocabulary::hex(*literal));
          }
        }
      }
      section_table.insert_or_assign(surface, std::move(fields));
    }
  }

  const theme::Theme& edited_;
  toml::table document_;
  std::flat_set<std::string> surfaces_;
};

auto ThemeFile::serialise(const theme::Theme& edited, const theme::Theme& base,
                          std::string_view name, std::string_view extends)
    -> std::string {
  Writer writer{edited, name, extends};
  for (const ThemeToken& token : ThemeToken::all()) {
    const TokenValue value = token.read(edited);
    if (value != token.read(base)) {
      writer.add(token, value);
    }
  }
  return writer.text();
}

}
