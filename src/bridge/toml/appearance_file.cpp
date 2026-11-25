#include <arisa/appearance/appearance_file.hpp>
#include <arisa/appearance/theme_vocabulary.hpp>
#include <arisa/theme/dynamic_type.hpp>

#include <toml.hpp>

#include <array>
#include <format>
#include <sstream>
#include <utility>

namespace arisa::appearance {

class AppearanceFile::Reader {
 public:
  explicit Reader(std::string_view file) : file_(file) {}

  auto read(const toml::table& document)
      -> std::expected<AppearanceOverrides, ThemeError>;

  static auto scheme_name(ColorSchemePreference scheme) noexcept
      -> std::string_view;

 private:
  auto fail(const toml::node& node, std::string key, std::string message) const
      -> std::unexpected<ThemeError>;

  auto flag(const toml::node& node, const std::string& key) const
      -> std::expected<bool, ThemeError>;

  auto scales(const toml::node& node, UiScales& into) const
      -> std::expected<void, ThemeError>;

  auto strength(const toml::node& node) const
      -> std::expected<provider::HapticStrength, ThemeError>;

  std::string file_;
};

auto AppearanceFile::Reader::scheme_name(ColorSchemePreference scheme) noexcept
    -> std::string_view {
  switch (scheme) {
    case ColorSchemePreference::light:
      return "light";
    case ColorSchemePreference::dark:
      return "dark";
    case ColorSchemePreference::system:
      break;
  }
  return "system";
}

auto AppearanceFile::Reader::fail(const toml::node& node, std::string key,
                                  std::string message) const
    -> std::unexpected<ThemeError> {
  return std::unexpected(
      ThemeError{ThemeErrorCode::invalid_value, file_,
                 static_cast<std::uint32_t>(node.source().begin.line),
                 std::move(key), std::move(message)});
}

auto AppearanceFile::Reader::flag(const toml::node& node,
                                  const std::string& key) const
    -> std::expected<bool, ThemeError> {
  const std::optional<bool> value = node.value<bool>();
  if (!value.has_value()) {
    return fail(node, key, "expected true or false");
  }
  return *value;
}

auto AppearanceFile::Reader::scales(const toml::node& node,
                                    UiScales& into) const
    -> std::expected<void, ThemeError> {
  const toml::table* table = node.as_table();
  if (table == nullptr) {
    return fail(node, "ui_scale", "expected a table");
  }
  for (const auto& [name, entry] : *table) {
    const std::string key = std::format("ui_scale.{}", name.str());
    const std::optional<double> value = entry.value<double>();
    if (!value.has_value() || *value < UiScales::minimum ||
        *value > UiScales::maximum) {
      return fail(entry, key,
                  std::format("expected a scale from {} to {}",
                              UiScales::minimum, UiScales::maximum));
    }
    const auto scale = static_cast<float>(*value);
    if (name.str() == "handheld") {
      into.handheld = scale;
    } else if (name.str() == "docked") {
      into.docked = scale;
    } else if (name.str() == "desktop") {
      into.desktop = scale;
    } else {
      return fail(entry, key, "expected handheld, docked or desktop");
    }
  }
  return {};
}

auto AppearanceFile::Reader::strength(const toml::node& node) const
    -> std::expected<provider::HapticStrength, ThemeError> {
  const std::optional<double> value = node.value<double>();
  if (!value.has_value() || *value < provider::HapticStrength::minimum ||
      *value > provider::HapticStrength::maximum) {
    return fail(node, "haptic_strength",
                std::format("expected a strength from {} to {}",
                            provider::HapticStrength::minimum,
                            provider::HapticStrength::maximum));
  }
  return provider::HapticStrength{static_cast<float>(*value)};
}

auto AppearanceFile::Reader::read(const toml::table& document)
    -> std::expected<AppearanceOverrides, ThemeError> {
  AppearanceOverrides overrides;
  for (const auto& [name, node] : document) {
    const std::string key{name.str()};
    if (key == "schema") {
      continue;
    }
    const std::optional<std::string_view> text = node.value<std::string_view>();
    if (key == "theme") {
      if (!text.has_value() || text->empty()) {
        return fail(node, key, "expected a theme name");
      }
      overrides.theme = std::string{*text};
    } else if (key == "color_scheme") {
      if (text == "system") {
        overrides.color_scheme = ColorSchemePreference::system;
      } else if (text == "light") {
        overrides.color_scheme = ColorSchemePreference::light;
      } else if (text == "dark") {
        overrides.color_scheme = ColorSchemePreference::dark;
      } else {
        return fail(node, key, "expected system, light or dark");
      }
    } else if (key == "accent") {
      const std::optional<theme::Colour> colour =
          text.has_value() ? ThemeVocabulary::colour(*text) : std::nullopt;
      if (!colour.has_value()) {
        return fail(node, key, "expected #RRGGBB");
      }
      overrides.accent = colour;
    } else if (key == "dynamic_type_size") {
      const std::optional<theme::DynamicTypeSize> size =
          text.has_value() ? theme::DynamicType::parse(*text) : std::nullopt;
      if (!size.has_value()) {
        return fail(node, key, "expected a dynamic type size such as large");
      }
      overrides.dynamic_type_size = size;
    } else if (key == "sidebar") {
      const auto visibility =
          text.has_value() ? ThemeVocabulary::visibility(*text) : std::nullopt;
      if (!visibility.has_value()) {
        return fail(node, key, "expected all, icons_only or detail_only");
      }
      overrides.sidebar = visibility;
    } else if (key == "ui_scale") {
      if (auto read = scales(node, overrides.ui_scale); !read.has_value()) {
        return std::unexpected(read.error());
      }
    } else if (key == "haptic_strength") {
      auto value = strength(node);
      if (!value.has_value()) {
        return std::unexpected(value.error());
      }
      overrides.haptic_strength = *value;
    } else if (key == "reduce_motion" || key == "reduce_transparency" ||
               key == "increase_contrast" || key == "sensory_feedback" ||
               key == "bold_text" || key == "button_shapes" ||
               key == "differentiate_without_color" ||
               key == "dim_flashing_lights" || key == "voice_over" ||
               key == "switch_control" || key == "invert_colors" ||
               key == "play_animated_images" ||
               key == "prefers_cross_fade_transitions" ||
               key == "reduce_highlighting_effects" || key == "show_borders" ||
               key == "assistive_access") {
      auto value = flag(node, key);
      if (!value.has_value()) {
        return std::unexpected(value.error());
      }
      if (key == "reduce_motion") {
        overrides.reduce_motion = *value;
      } else if (key == "reduce_transparency") {
        overrides.reduce_transparency = *value;
      } else if (key == "sensory_feedback") {
        overrides.sensory_feedback = *value;
      } else if (key == "bold_text") {
        overrides.bold_text = *value;
      } else if (key == "button_shapes") {
        overrides.button_shapes = *value;
      } else if (key == "differentiate_without_color") {
        overrides.differentiate_without_color = *value;
      } else if (key == "dim_flashing_lights") {
        overrides.dim_flashing_lights = *value;
      } else if (key == "voice_over") {
        overrides.voice_over = *value;
      } else if (key == "switch_control") {
        overrides.switch_control = *value;
      } else if (key == "invert_colors") {
        overrides.invert_colors = *value;
      } else if (key == "play_animated_images") {
        overrides.play_animated_images = *value;
      } else if (key == "prefers_cross_fade_transitions") {
        overrides.prefers_cross_fade_transitions = *value;
      } else if (key == "reduce_highlighting_effects") {
        overrides.reduce_highlighting_effects = *value;
      } else if (key == "show_borders") {
        overrides.show_borders = *value;
      } else if (key == "assistive_access") {
        overrides.assistive_access = *value;
      } else {
        overrides.increase_contrast = *value;
      }
    } else {
      return std::unexpected(
          ThemeError{ThemeErrorCode::unknown_key, file_,
                     static_cast<std::uint32_t>(node.source().begin.line), key,
                     "not an appearance setting"});
    }
  }
  return overrides;
}

auto AppearanceFile::parse(std::string_view text, std::string_view file)
    -> std::expected<AppearanceOverrides, ThemeError> {
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
  const std::optional<std::int64_t> version =
      document["schema"].value<std::int64_t>();
  if (!version.has_value() || *version != schema_version) {
    return std::unexpected(ThemeError{
        ThemeErrorCode::unsupported_schema, std::string{file}, 1U, "schema",
        std::format("expected schema = {}", schema_version)});
  }
  return Reader{file}.read(document);
}

auto AppearanceFile::serialise(const AppearanceOverrides& overrides)
    -> std::string {
  toml::table document;
  document.insert("schema", schema_version);
  if (overrides.theme.has_value()) {
    document.insert("theme", *overrides.theme);
  }
  if (overrides.color_scheme.has_value()) {
    document.insert("color_scheme",
                    std::string{Reader::scheme_name(*overrides.color_scheme)});
  }
  if (overrides.accent.has_value()) {
    document.insert("accent", ThemeVocabulary::hex(*overrides.accent));
  }
  if (overrides.dynamic_type_size.has_value()) {
    document.insert(
        "dynamic_type_size",
        std::string{theme::DynamicType::name(*overrides.dynamic_type_size)});
  }
  if (overrides.reduce_motion.has_value()) {
    document.insert("reduce_motion", *overrides.reduce_motion);
  }
  if (overrides.reduce_transparency.has_value()) {
    document.insert("reduce_transparency", *overrides.reduce_transparency);
  }
  if (overrides.increase_contrast.has_value()) {
    document.insert("increase_contrast", *overrides.increase_contrast);
  }
  if (overrides.sensory_feedback.has_value()) {
    document.insert("sensory_feedback", *overrides.sensory_feedback);
  }
  if (overrides.bold_text.has_value()) {
    document.insert("bold_text", *overrides.bold_text);
  }
  if (overrides.button_shapes.has_value()) {
    document.insert("button_shapes", *overrides.button_shapes);
  }
  if (overrides.differentiate_without_color.has_value()) {
    document.insert("differentiate_without_color",
                    *overrides.differentiate_without_color);
  }
  if (overrides.dim_flashing_lights.has_value()) {
    document.insert("dim_flashing_lights", *overrides.dim_flashing_lights);
  }
  if (overrides.voice_over.has_value()) {
    document.insert("voice_over", *overrides.voice_over);
  }
  if (overrides.switch_control.has_value()) {
    document.insert("switch_control", *overrides.switch_control);
  }
  if (overrides.invert_colors.has_value()) {
    document.insert("invert_colors", *overrides.invert_colors);
  }
  if (overrides.play_animated_images.has_value()) {
    document.insert("play_animated_images", *overrides.play_animated_images);
  }
  if (overrides.prefers_cross_fade_transitions.has_value()) {
    document.insert("prefers_cross_fade_transitions",
                    *overrides.prefers_cross_fade_transitions);
  }
  if (overrides.reduce_highlighting_effects.has_value()) {
    document.insert("reduce_highlighting_effects",
                    *overrides.reduce_highlighting_effects);
  }
  if (overrides.show_borders.has_value()) {
    document.insert("show_borders", *overrides.show_borders);
  }
  if (overrides.assistive_access.has_value()) {
    document.insert("assistive_access", *overrides.assistive_access);
  }
  if (overrides.haptic_strength.has_value()) {
    document.insert(
        "haptic_strength",
        static_cast<double>(overrides.haptic_strength->percent()) / 100.0);
  }
  if (overrides.sidebar.has_value()) {
    document.insert("sidebar",
                    std::string{ThemeVocabulary::name(*overrides.sidebar)});
  }
  toml::table scales;
  const std::array<std::pair<std::string_view, std::optional<float>>, 3> modes{
      std::pair{std::string_view{"handheld"}, overrides.ui_scale.handheld},
      std::pair{std::string_view{"docked"}, overrides.ui_scale.docked},
      std::pair{std::string_view{"desktop"}, overrides.ui_scale.desktop}};
  for (const auto& [mode, scale] : modes) {
    if (scale.has_value()) {
      scales.insert(mode, static_cast<double>(*scale));
    }
  }
  if (!scales.empty()) {
    document.insert("ui_scale", std::move(scales));
  }
  std::ostringstream stream;
  stream << document << '\n';
  return std::move(stream).str();
}

}
