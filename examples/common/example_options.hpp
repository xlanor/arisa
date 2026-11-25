#pragma once

#include <arisa/provider/surface/surface_size.hpp>
#include <arisa/theme/color_scheme.hpp>

#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::examples {

class ExampleOptions {
 public:
  [[nodiscard]] static auto parse(std::span<char* const> arguments)
      -> std::expected<ExampleOptions, std::string>;

  auto offscreen() const noexcept
      -> const std::optional<provider::SurfaceSize>& {
    return offscreen_;
  }

  auto frames() const noexcept -> std::optional<std::uint32_t> {
    return frames_;
  }

  auto tour() const noexcept -> bool { return tour_; }

  auto lcd_capture() const noexcept -> const std::optional<std::string>& {
    return lcd_capture_;
  }

  auto capture() const noexcept -> const std::optional<std::string>& {
    return capture_;
  }

  auto script() const noexcept -> std::uint32_t { return script_; }

  auto page() const noexcept -> const std::optional<std::string>& {
    return page_;
  }

  auto activations() const noexcept -> const std::vector<std::string>& {
    return activations_;
  }

  auto motion_milliseconds() const noexcept -> std::optional<std::uint32_t> {
    return motion_milliseconds_;
  }

  auto scheme() const noexcept -> std::optional<theme::ColorScheme> {
    return scheme_;
  }

  auto theme() const noexcept -> const std::optional<std::string>& {
    return theme_;
  }

  auto locale() const noexcept -> const std::optional<std::string>& {
    return locale_;
  }

  auto sidebar() const noexcept -> const std::optional<std::string>& {
    return sidebar_;
  }

  auto text_size() const noexcept -> const std::optional<std::string>& {
    return text_size_;
  }

  auto ui_scale() const noexcept -> std::optional<float> { return ui_scale_; }

  auto inspector() const noexcept -> const std::optional<std::string>& {
    return inspector_;
  }

  auto inspect() const noexcept -> const std::optional<std::string>& {
    return inspect_;
  }

  auto token() const noexcept -> const std::optional<std::string>& {
    return token_;
  }

  auto debug_glass_coverage() const noexcept -> bool {
    return debug_glass_coverage_;
  }

 private:
  ExampleOptions() = default;

  static auto parse_count(std::string_view text)
      -> std::optional<std::uint32_t>;

  static auto parse_scale(std::string_view text) -> std::optional<float>;

  static auto parse_size(std::string_view text)
      -> std::optional<provider::SurfaceSize>;

  static auto parse_scheme(std::string_view text)
      -> std::optional<theme::ColorScheme>;

  std::optional<provider::SurfaceSize> offscreen_;
  std::optional<std::uint32_t> frames_;
  std::optional<std::string> capture_;
  std::optional<std::string> lcd_capture_;
  bool tour_{false};
  std::uint32_t script_{0};
  std::optional<std::string> page_;
  std::vector<std::string> activations_;
  std::optional<std::uint32_t> motion_milliseconds_;
  std::optional<theme::ColorScheme> scheme_;
  std::optional<std::string> theme_;
  std::optional<std::string> locale_;
  std::optional<std::string> sidebar_;
  std::optional<std::string> text_size_;
  std::optional<float> ui_scale_;
  bool debug_glass_coverage_{false};
  std::optional<std::string> inspector_;
  std::optional<std::string> inspect_;
  std::optional<std::string> token_;
};

}
