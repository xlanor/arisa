#pragma once

#include <arisa/provider/surface/surface_scale.hpp>
#include <arisa/text/cache_statistics.hpp>
#include <arisa/text/fitted_text.hpp>
#include <arisa/text/font_collection.hpp>
#include <arisa/text/layout_width.hpp>
#include <arisa/text/text_error.hpp>
#include <arisa/text/text_fitting.hpp>
#include <arisa/text/text_layout.hpp>
#include <arisa/text/text_size.hpp>
#include <arisa/text/text_style.hpp>
#include <arisa/text/truncation_mode.hpp>
#include <arisa/text/typesetter_limits.hpp>

#include <cstddef>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace arisa::text {

class Shaper;
class ShapedText;
class TypesetterCache;

class Typesetter {
 public:
  static constexpr float maximum_tightening = 0.04F;
  static constexpr int scale_steps = 6;

  [[nodiscard]] static auto create(const FontCollection& fonts,
                                   TypesetterLimits limits = {})
      -> std::expected<Typesetter, TextError>;

  Typesetter(const Typesetter&) = delete;
  auto operator=(const Typesetter&) -> Typesetter& = delete;
  Typesetter(Typesetter&& other) noexcept;
  auto operator=(Typesetter&& other) noexcept -> Typesetter&;
  ~Typesetter();

  [[nodiscard]] auto layout(std::string_view text, const TextStyle& style,
                            LayoutWidth width, provider::SurfaceScale scale)
      -> std::expected<std::shared_ptr<const TextLayout>, TextError>;

  [[nodiscard]] auto measure(std::string_view text, const TextStyle& style,
                             LayoutWidth width, provider::SurfaceScale scale)
      -> std::expected<TextSize, TextError>;

  [[nodiscard]] auto truncated(std::string_view text, const TextStyle& style,
                               LayoutWidth width, provider::SurfaceScale scale,
                               std::size_t lines,
                               TruncationMode mode = TruncationMode::tail)
      -> std::expected<std::string, TextError>;

  [[nodiscard]] auto fitted(std::string_view text, const TextStyle& style,
                            LayoutWidth width, provider::SurfaceScale scale,
                            const TextFitting& fitting)
      -> std::expected<std::shared_ptr<const FittedText>, TextError>;

  auto statistics() const noexcept -> CacheStatistics;

  void clear() noexcept;

 private:
  Typesetter(const FontCollection& fonts, std::unique_ptr<Shaper> shaper,
             std::unique_ptr<TypesetterCache> cache) noexcept;

  static constexpr std::string_view ellipsis = "\u2026";

  auto fits(std::string_view text, const TextStyle& style, LayoutWidth width,
            provider::SurfaceScale scale, std::size_t lines) -> bool;

  auto fits(std::string_view text, const TextStyle& style, LayoutWidth width,
            provider::SurfaceScale scale, const TextFitting& fitting) -> bool;

  auto tightened(std::string_view text, const TextStyle& style,
                 LayoutWidth width, provider::SurfaceScale scale,
                 const TextFitting& fitting) -> std::optional<TextStyle>;

  auto fit(std::string_view text, const TextStyle& style, LayoutWidth width,
           provider::SurfaceScale scale, const TextFitting& fitting)
      -> std::expected<FittedText, TextError>;

  static auto scaled(const TextStyle& style, float factor) -> TextStyle;

  static auto candidate(std::string_view text, const TextStyle& style,
                        std::span<const std::size_t> boundaries,
                        std::size_t kept, TruncationMode mode) -> FittedText;

  [[nodiscard]] auto cut(std::string_view text, const TextStyle& style,
                         LayoutWidth width, provider::SurfaceScale scale,
                         std::size_t lines, TruncationMode mode)
      -> std::expected<FittedText, TextError>;

  [[nodiscard]] auto shaped(std::string_view text, const TextStyle& style)
      -> std::expected<std::shared_ptr<const ShapedText>, TextError>;

  std::reference_wrapper<const FontCollection> fonts_;
  std::unique_ptr<Shaper> shaper_;
  std::unique_ptr<TypesetterCache> cache_;
  CacheStatistics statistics_;
};

}
