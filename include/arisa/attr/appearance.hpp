#pragma once

#include <arisa/components/environment.hpp>
#include <arisa/components/panel_radius.hpp>
#include <arisa/composition/alignment.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/modifiers.hpp>
#include <arisa/composition/overlay_layer.hpp>
#include <arisa/composition/redaction_reasons.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/theme/color_scheme.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/dynamic_type_size.hpp>
#include <arisa/theme/elevation.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/stroke.hpp>
#include <arisa/theme/theme.hpp>

#include <functional>
#include <optional>
#include <utility>

namespace arisa::attr {

class ForegroundStyle {
 public:
  explicit ForegroundStyle(graphics::ShapeStyle style)
      : style_(std::move(style)) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).foreground_style(style_);
  }

 private:
  graphics::ShapeStyle style_;
};

class Background {
 public:
  explicit Background(graphics::ShapeStyle paint,
                      theme::Shape outline = theme::Rectangle())
      : write_([paint = std::move(paint),
                outline](composition::Element element) mutable {
          return std::move(element).background(std::move(paint), outline);
        }) {}

  explicit Background(theme::Material material,
                      theme::Shape outline = theme::Rectangle())
      : write_([material, outline](composition::Element element) {
          return std::move(element).background(material, outline);
        }) {}

  explicit Background(composition::Element content)
      : Background(composition::Alignment::center, std::move(content)) {}

  Background(composition::Alignment alignment, composition::Element content)
      : write_([alignment, held = std::move(content)](
                   composition::Element element) mutable {
          return std::move(element).background(alignment, std::move(held));
        }) {}

  auto apply(composition::Element element) && -> composition::Element {
    return write_(std::move(element));
  }

 private:
  std::move_only_function<composition::Element(composition::Element)> write_;
};

class Overlay {
 public:
  explicit Overlay(composition::Element content)
      : Overlay(composition::Alignment::center, std::move(content)) {}

  Overlay(composition::Alignment alignment, composition::Element content)
      : write_([alignment, held = std::move(content)](
                   composition::Element element) mutable {
          return std::move(element).overlay(alignment, std::move(held));
        }) {}

  Overlay(bool is_presented, composition::OverlayLayer layer,
          composition::Element content)
      : write_([is_presented, layer, held = std::move(content)](
                   composition::Element element) mutable {
          return std::move(element).overlay(is_presented, layer,
                                            std::move(held));
        }) {}

  auto apply(composition::Element element) && -> composition::Element {
    return write_(std::move(element));
  }

 private:
  std::move_only_function<composition::Element(composition::Element)> write_;
};

class AllowsHitTesting {
 public:
  constexpr explicit AllowsHitTesting(bool enabled) noexcept
      : enabled_(enabled) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).allows_hit_testing(enabled_);
  }

 private:
  bool enabled_;
};

class ClipShape {
 public:
  constexpr explicit ClipShape(theme::Shape outline) noexcept
      : outline_(outline) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).clip_shape(outline_);
  }

 private:
  theme::Shape outline_;
};

class Border {
 public:
  constexpr explicit Border(theme::ColourValue colour,
                            theme::Stroke stroke = 1.0F) noexcept
      : colour_(colour), stroke_(stroke) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).border(colour_, stroke_);
  }

 private:
  theme::ColourValue colour_;
  theme::Stroke stroke_;
};

class Shadow {
 public:
  constexpr Shadow(theme::ColourValue colour, float radius, float x = 0.0F,
                   float y = 0.0F) noexcept
      : cast_(composition::DropShadow{
            .colour = colour, .radius = radius, .x = x, .y = y}) {}

  constexpr explicit Shadow(theme::Elevation elevation) noexcept
      : elevation_(elevation) {}

  auto apply(composition::Element element) const -> composition::Element {
    if (cast_.has_value()) {
      return std::move(element).shadow(cast_->colour, cast_->radius, cast_->x,
                                       cast_->y);
    }
    return std::move(element).shadow(elevation_);
  }

 private:
  std::optional<composition::DropShadow> cast_;
  theme::Elevation elevation_{theme::Elevation::none};
};

class Opacity {
 public:
  constexpr explicit Opacity(float value) noexcept : value_(value) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).opacity(value_);
  }

 private:
  float value_;
};

class Tint {
 public:
  constexpr explicit Tint(std::optional<theme::Colour> colour) noexcept
      : colour_(colour) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).tint(colour_);
  }

 private:
  std::optional<theme::Colour> colour_;
};

class Hidden {
 public:
  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).hidden();
  }
};

class Redacted {
 public:
  constexpr explicit Redacted(composition::RedactionReasons reason) noexcept
      : reason_(reason) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).redacted(reason_);
  }

 private:
  composition::RedactionReasons reason_;
};

class Unredacted {
 public:
  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).unredacted();
  }
};

class PrivacySensitive {
 public:
  constexpr explicit PrivacySensitive(bool sensitive = true) noexcept
      : sensitive_(sensitive) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).privacy_sensitive(sensitive_);
  }

 private:
  bool sensitive_;
};

class PreferredColorScheme {
 public:
  constexpr explicit PreferredColorScheme(
      std::optional<theme::ColorScheme> scheme) noexcept
      : scheme_(scheme) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).preferred_color_scheme(scheme_);
  }

 private:
  std::optional<theme::ColorScheme> scheme_;
};

class GlassEffect {
 public:
  constexpr explicit GlassEffect(
      theme::Glass glass = theme::Glass::regular(),
      theme::Shape shape = theme::Shape::capsule()) noexcept
      : glass_(glass), shape_(shape) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).glass_effect(glass_, shape_);
  }

 private:
  theme::Glass glass_;
  theme::Shape shape_;
};

class GlassPanelAttribute {
 public:
  constexpr explicit GlassPanelAttribute(
      components::PanelRadius radius) noexcept
      : radius_(radius) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).glass_panel(radius_);
  }

 private:
  components::PanelRadius radius_;
};

class ThemeOverride {
 public:
  explicit ThemeOverride(theme::Theme value) : value_(std::move(value)) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).theme_override(value_);
  }

 private:
  theme::Theme value_;
};

class DynamicTypeSize {
 public:
  constexpr explicit DynamicTypeSize(theme::DynamicTypeSize size) noexcept
      : range_(size, size), exact_(true) {}

  constexpr explicit DynamicTypeSize(theme::DynamicTypeRange range) noexcept
      : range_(range), exact_(false) {}

  auto apply(composition::Element element) const -> composition::Element {
    if (exact_) {
      return std::move(element).dynamic_type_size(range_.lower());
    }
    return std::move(element).dynamic_type_size(range_);
  }

 private:
  theme::DynamicTypeRange range_;
  bool exact_;
};

using foreground_style = ForegroundStyle;
using glass_effect = GlassEffect;
using glass_panel = GlassPanelAttribute;
using theme_override = ThemeOverride;
using dynamic_type_size = DynamicTypeSize;
using background = Background;
using overlay = Overlay;
using allows_hit_testing = AllowsHitTesting;
using clip_shape = ClipShape;
using border = Border;
using shadow = Shadow;
using opacity = Opacity;
using tint = Tint;
inline constexpr Hidden hidden{};
using redacted = Redacted;
inline constexpr Unredacted unredacted{};
using privacy_sensitive = PrivacySensitive;
using preferred_color_scheme = PreferredColorScheme;

}
