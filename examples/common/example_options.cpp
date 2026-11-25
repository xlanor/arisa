#include <example_options.hpp>

#include <arisa/appearance/appearance_settings.hpp>

#include <charconv>
#include <cstddef>
#include <format>
#include <system_error>

namespace arisa::examples {

auto ExampleOptions::parse(std::span<char* const> arguments)
    -> std::expected<ExampleOptions, std::string> {
  ExampleOptions options;
  for (std::size_t index = 0; index < arguments.size(); ++index) {
    const std::string_view flag{arguments[index]};
    if (flag == "--debug-glass-coverage") {
      options.debug_glass_coverage_ = true;
      continue;
    }
    if (index + 1 >= arguments.size()) {
      return std::unexpected(std::format("{} needs a value", flag));
    }
    const std::string_view value{arguments[++index]};
    if (flag == "--offscreen") {
      options.offscreen_ = parse_size(value);
      if (!options.offscreen_.has_value()) {
        return std::unexpected(
            std::format("--offscreen expects WxH, got {}", value));
      }
    } else if (flag == "--frames") {
      options.frames_ = parse_count(value);
      if (!options.frames_.has_value() || *options.frames_ == 0) {
        return std::unexpected(
            std::format("--frames expects a positive count, got {}", value));
      }
    } else if (flag == "--capture") {
      options.capture_ = std::string{value};
    } else if (flag == "--tour") {
      options.tour_ = value != "0";
    } else if (flag == "--lcd-capture") {
      options.lcd_capture_ = std::string{value};
    } else if (flag == "--script") {
      const auto count = parse_count(value);
      if (!count.has_value()) {
        return std::unexpected(
            std::format("--script expects a count, got {}", value));
      }
      options.script_ = *count;
    } else if (flag == "--page") {
      options.page_ = std::string{value};
    } else if (flag == "--activate") {
      options.activations_.emplace_back(value);
    } else if (flag == "--motion-ms") {
      options.motion_milliseconds_ = parse_count(value);
      if (!options.motion_milliseconds_.has_value()) {
        return std::unexpected(
            std::format("--motion-ms expects milliseconds, got {}", value));
      }
    } else if (flag == "--scheme") {
      options.scheme_ = parse_scheme(value);
      if (!options.scheme_.has_value()) {
        return std::unexpected(
            std::format("--scheme expects light or dark, got {}", value));
      }
    } else if (flag == "--theme") {
      options.theme_ = std::string{value};
    } else if (flag == "--locale") {
      options.locale_ = std::string{value};
    } else if (flag == "--sidebar") {
      options.sidebar_ = std::string{value};
    } else if (flag == "--text-size") {
      options.text_size_ = std::string{value};
    } else if (flag == "--ui-scale") {
      options.ui_scale_ = parse_scale(value);
      if (!options.ui_scale_.has_value()) {
        return std::unexpected(
            std::format("--ui-scale expects 0.75 to 2, got {}", value));
      }
    } else if (flag == "--inspector") {
      options.inspector_ = std::string{value};
    } else if (flag == "--inspect") {
      options.inspect_ = std::string{value};
    } else if (flag == "--token") {
      options.token_ = std::string{value};
    } else {
      return std::unexpected(std::format("unknown flag {}", flag));
    }
  }
  if (options.offscreen_.has_value() && !options.frames_.has_value()) {
    options.frames_ = 1;
  }
  return options;
}

auto ExampleOptions::parse_count(std::string_view text)
    -> std::optional<std::uint32_t> {
  std::uint32_t value = 0;
  const auto [end, error] =
      std::from_chars(text.data(), text.data() + text.size(), value);
  if (error != std::errc{} || end != text.data() + text.size()) {
    return std::nullopt;
  }
  return value;
}

auto ExampleOptions::parse_scale(std::string_view text)
    -> std::optional<float> {
  float value = 0.0F;
  const auto [end, error] =
      std::from_chars(text.data(), text.data() + text.size(), value);
  if (error != std::errc{} || end != text.data() + text.size() ||
      value < appearance::UiScales::minimum ||
      value > appearance::UiScales::maximum) {
    return std::nullopt;
  }
  return value;
}

auto ExampleOptions::parse_size(std::string_view text)
    -> std::optional<provider::SurfaceSize> {
  const std::size_t separator = text.find('x');
  if (separator == std::string_view::npos) {
    return std::nullopt;
  }
  const auto width = parse_count(text.substr(0, separator));
  const auto height = parse_count(text.substr(separator + 1));
  if (!width.has_value() || !height.has_value() || *width == 0 ||
      *height == 0) {
    return std::nullopt;
  }
  return provider::SurfaceSize{.width = provider::SurfaceWidth{*width},
                               .height = provider::SurfaceHeight{*height}};
}

auto ExampleOptions::parse_scheme(std::string_view text)
    -> std::optional<theme::ColorScheme> {
  if (text == "light") {
    return theme::ColorScheme::light;
  }
  if (text == "dark") {
    return theme::ColorScheme::dark;
  }
  return std::nullopt;
}

}
