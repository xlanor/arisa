#pragma once

#include <cstdint>

#include <arisa/support/copy_on_write.hpp>
#include <arisa/theme/adaptive.hpp>
#include <arisa/theme/appearance_variants.hpp>
#include <arisa/theme/color_scheme.hpp>
#include <arisa/theme/color_scheme_contrast.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/dynamic_type_size.hpp>
#include <arisa/theme/elevation.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/material.hpp>
#include <arisa/theme/metrics.hpp>
#include <arisa/theme/motion.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/resolved_hdr.hpp>
#include <arisa/theme/scaled_metric.hpp>
#include <arisa/theme/seed.hpp>
#include <arisa/theme/semantic_palette.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <array>
#include <bitset>
#include <optional>
#include <string>
#include <string_view>

namespace arisa::theme {

class Theme {
 public:
  using Palette = std::array<Adaptive<Colour>, colour_role_count>;
  using Typography = std::array<TypeStyle, text_role_count>;
  using Spacing = std::array<float, space_token_count>;
  using Elevations = std::array<ElevationStyle, elevation_count>;
  using Metrics = std::array<float, metric_token_count>;
  using Materials = std::array<MaterialStyle, Material::count>;
  using ColourRoles = std::bitset<colour_role_count>;

  struct Radii {
    float control{metrics::button.corner_radius};
    float card{14.0F};
    float sheet{20.0F};
    bool continuous{true};

    friend constexpr auto operator==(const Radii&, const Radii&)
        -> bool = default;
  };

  struct FocusRing {
    enum class Style : std::uint8_t { ring, highlight };

    struct Highlight {
      float fill{0.18F};
      float edge{0.45F};
      float width{1.5F};

      friend constexpr auto operator==(const Highlight&, const Highlight&)
          -> bool = default;
    };

    float width{metrics::focus_ring.width};
    bool halo{false};
    Style style{Style::ring};
    Highlight highlight{};

    friend constexpr auto operator==(const FocusRing&, const FocusRing&)
        -> bool = default;
  };

  static constexpr std::string_view high_contrast_name = "high-contrast";
  static constexpr float high_contrast_ring_width = 4.0F;
  static constexpr FocusRing::Highlight high_contrast_highlight{
      .fill = 0.3F, .edge = 1.0F, .width = 2.0F};

  struct Preferences {
    std::optional<DynamicTypeSize> dynamic_type_size;
    std::optional<ColorScheme> color_scheme;
    std::optional<float> handheld_scale;
    std::optional<float> docked_scale;
    std::optional<float> desktop_scale;

    friend constexpr auto operator==(const Preferences&, const Preferences&)
        -> bool = default;
  };

  static constexpr float pressed_lightness_step =
      SemanticPalette::pressed_lightness_step;

  static auto defaults() -> Theme;

  static auto from(Seed seed) -> Theme;

  static auto high_contrast() -> Theme;

  auto seed() const noexcept -> const Seed& { return seed_; }

  auto scheme() const noexcept -> ColorScheme { return seed_.scheme; }

  auto palette(ColourRole role) const noexcept -> const Adaptive<Colour>&;

  auto variants(ColourRole role) const noexcept -> const AppearanceVariants&;

  auto colour(ColourValue value, ColorScheme scheme) const noexcept -> Colour;

  auto colour(ColourValue value, ColorScheme scheme,
              ColorSchemeContrast contrast) const noexcept -> Colour;

  auto colour_hdr(ColourValue value, ColorScheme scheme) const noexcept
      -> ResolvedHDR;

  auto type(TextRole role) const noexcept -> TypeStyle;

  auto base_type(TextRole role) const noexcept -> const TypeStyle&;

  auto font_context(bool bold_text = false) const -> Font::Context;

  auto dynamic_type_size() const noexcept -> DynamicTypeSize {
    return dynamic_type_size_;
  }

  auto sized(DynamicTypeSize size) const -> Theme;

  auto metric(MetricToken token) const noexcept -> float;

  auto scaled(ScaledMetric metric) const noexcept -> float;

  auto spacing() const noexcept -> const Spacing& { return tokens_->spacing; }

  auto glass() const noexcept -> const GlassDefaults& { return tokens_->glass; }

  auto material(Material token) const noexcept -> const MaterialStyle& {
    return tokens_->materials[token.index()];
  }

  auto materials() const noexcept -> const Materials& {
    return tokens_->materials;
  }

  auto liquid_glass() const noexcept -> const LiquidGlassStyle& {
    return tokens_->liquid_glass;
  }

  auto preferences() const noexcept -> const Preferences& {
    return tokens_->preferences;
  }

  auto name() const noexcept -> std::string_view { return name_; }

  auto space(Space value) const noexcept -> float;

  auto edges(const Insets& insets) const noexcept -> Edges;

  auto radius(Radius value) const noexcept -> float;

  auto radii() const noexcept -> const Radii& { return tokens_->radii; }

  auto focus_ring() const noexcept -> const FocusRing& {
    return tokens_->focus_ring;
  }

  auto elevation(Elevation level) const noexcept -> const ElevationStyle&;

  auto motion() const noexcept -> const Motion& { return tokens_->motion; }

  auto with(ColourRole role, Adaptive<Colour> colour) const -> Theme;

  auto with(Radii radii) const -> Theme;

  auto with(FocusRing ring) const -> Theme;

  auto with(TextRole role, TypeStyle style) const -> Theme;

  auto with(Motion tokens) const -> Theme;

  auto with(MetricToken token, float points) const -> Theme;

  auto with(Spacing spacing) const -> Theme;

  auto with(GlassDefaults glass) const -> Theme;

  auto with(Preferences preferences) const -> Theme;

  auto with(Elevations elevations) const -> Theme;

  auto with(Materials materials) const -> Theme;

  auto with(LiquidGlassStyle glass) const -> Theme;

  auto with_accent(Colour accent) const -> Theme;

  auto named(std::string name) const -> Theme;

  auto lays_out_like(const Theme& other) const noexcept -> bool;

  auto paints_like(const Theme& other, const ColourRoles& roles) const noexcept
      -> bool;

  friend auto operator==(const Theme&, const Theme&) -> bool = default;

 private:
  auto leaf(const ColourValue::Leaf& leaf, ColorScheme scheme,
            ColorSchemeContrast contrast) const noexcept -> Colour;

  auto composed(const ColourValue& value, Colour base, ColorScheme scheme,
                ColorSchemeContrast contrast) const noexcept -> Colour;

  Theme(Seed seed, SemanticPalette::Variants variants) noexcept;

  static auto adapt(const SemanticPalette::Variants& variants) noexcept
      -> Palette;

  static auto default_typography() noexcept -> Typography;

  static auto default_spacing() noexcept -> Spacing;

  static auto default_elevations() noexcept -> Elevations;

  static auto default_metrics() noexcept -> Metrics;

  static auto default_materials() noexcept -> Materials;

  static auto default_liquid_glass() noexcept -> LiquidGlassStyle;

  struct Tokens {
    SemanticPalette::Variants variants;
    Palette palette;
    Typography typography;
    Spacing spacing;
    Radii radii;
    FocusRing focus_ring;
    Elevations elevations;
    Motion motion;
    Metrics metrics;
    GlassDefaults glass;
    Materials materials;
    LiquidGlassStyle liquid_glass;
    Preferences preferences;

    friend auto operator==(const Tokens&, const Tokens&) -> bool = default;
  };

  Seed seed_;
  support::CopyOnWrite<Tokens> tokens_;
  DynamicTypeSize dynamic_type_size_{DynamicTypeSize::large};
  std::string name_{"default"};
};

}
