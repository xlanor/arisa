#include <arisa/graphics/shape_style.hpp>
#include <arisa/theme/color.hpp>
#include <arisa/theme/theme.hpp>

#include <type_traits>
#include <utility>
#include <vector>

namespace arisa::graphics {

ShapeStyle::ShapeStyle(const ShapeStyle& other) = default;

ShapeStyle::ShapeStyle(ShapeStyle&& other) noexcept = default;

auto ShapeStyle::operator=(const ShapeStyle& other) -> ShapeStyle& = default;

auto ShapeStyle::operator=(ShapeStyle&& other) noexcept
    -> ShapeStyle& = default;

ShapeStyle::~ShapeStyle() = default;

auto ShapeStyle::gradient() const noexcept -> const Gradient* {
  return std::visit(
      [](const auto& each) -> const Gradient* {
        using T = std::remove_cvref_t<decltype(each)>;
        if constexpr (std::same_as<T, LinearGradient> ||
                      std::same_as<T, RadialGradient> ||
                      std::same_as<T, AngularGradient> ||
                      std::same_as<T, EllipticalGradient>) {
          return &each.gradient;
        } else {
          return nullptr;
        }
      },
      value_);
}

auto ShapeStyle::colour() const noexcept -> std::optional<theme::ColourValue> {
  if (const auto* solid = std::get_if<theme::ColourValue>(&value_)) {
    return solid->opacity(opacity_);
  }
  return std::nullopt;
}

auto ShapeStyle::level() const noexcept
    -> std::optional<HierarchicalShapeStyle> {
  if (const auto* tier = std::get_if<HierarchicalShapeStyle>(&value_)) {
    return *tier;
  }
  return std::nullopt;
}

auto ShapeStyle::material() const noexcept -> std::optional<theme::Material> {
  if (const auto* frosted = std::get_if<theme::Material>(&value_)) {
    return *frosted;
  }
  return std::nullopt;
}

auto ShapeStyle::image_paint() const noexcept -> const ImagePaint* {
  return std::get_if<ImagePaint>(&value_);
}

auto ShapeStyle::opacity(float amount) const -> ShapeStyle {
  ShapeStyle faded = *this;
  if (auto* solid = std::get_if<theme::ColourValue>(&faded.value_)) {
    *solid = solid->opacity(amount);
  } else {
    faded.opacity_ *= amount;
  }
  return faded;
}

auto ShapeStyle::shadow(ShadowStyle style) const -> ShapeStyle {
  ShapeStyle shaded = *this;
  shaded.shadow_ = style;
  return shaded;
}

auto ShapeStyle::substituted(const ShapeStyle& replacement) const
    -> ShapeStyle {
  ShapeStyle resolved = replacement.opacity(opacity_);
  if (shadow_.has_value()) {
    resolved.shadow_ = shadow_;
  }
  if (blend_ != BlendMode::normal) {
    resolved.blend_ = blend_;
  }
  if (bounds_.has_value()) {
    resolved.bounds_ = bounds_;
  }
  return resolved;
}

auto ShapeStyle::black() -> ShapeStyle {
  return ShapeStyle{theme::Color::black};
}

auto ShapeStyle::white() -> ShapeStyle {
  return ShapeStyle{theme::Color::white};
}

auto ShapeStyle::image(std::shared_ptr<const provider::Image> image,
                       ImagePaint::SourceRect source_rect, float scale)
    -> ShapeStyle {
  return ShapeStyle{
      ImagePaint{std::move(image), source_rect, scale, ImagePaint::Origin{}}};
}

auto ShapeStyle::blended(BlendMode mode) -> ShapeStyle {
  return foreground().blend_mode(mode);
}

auto ShapeStyle::blend_mode(BlendMode mode) const -> ShapeStyle {
  ShapeStyle blended = *this;
  blended.blend_ = mode;
  return blended;
}

auto ShapeStyle::in(composition::Rect rect) const -> ShapeStyle {
  ShapeStyle bounded = *this;
  bounded.bounds_ = rect;
  return bounded;
}

auto ShapeStyle::resolve(const theme::Theme& theme,
                         theme::ColorScheme scheme) const -> ShapeStyle {
  const auto literal = [&theme, scheme](const theme::ColourValue& value) {
    return theme::ColourValue{theme.colour(value, scheme)};
  };
  const auto stops = [&literal](const Gradient& gradient) {
    std::vector<Gradient::Stop> resolved;
    for (const Gradient::Stop& stop : gradient.stops()) {
      resolved.push_back(Gradient::Stop{.color = literal(stop.color),
                                        .location = stop.location});
    }
    return Gradient{std::move(resolved)}.color_space(gradient.interpolation());
  };
  ShapeStyle resolved = *this;
  std::visit(
      [&](const auto& each) {
        using T = std::remove_cvref_t<decltype(each)>;
        if constexpr (std::same_as<T, theme::ColourValue>) {
          resolved.value_ = literal(each);
        } else if constexpr (std::same_as<T, LinearGradient> ||
                             std::same_as<T, RadialGradient> ||
                             std::same_as<T, AngularGradient> ||
                             std::same_as<T, EllipticalGradient>) {
          T copy = each;
          copy.gradient = stops(each.gradient);
          resolved.value_ = std::move(copy);
        } else if constexpr (std::same_as<T, HierarchicalShapeStyle>) {
          static constexpr std::array<theme::ColourRole, 5> roles{
              theme::ColourRole::label, theme::ColourRole::secondary_label,
              theme::ColourRole::tertiary_label,
              theme::ColourRole::quaternary_label,
              theme::ColourRole::quinary_label};
          resolved.value_ = literal(roles[static_cast<std::size_t>(each)]);
        } else if constexpr (std::same_as<T, BackgroundStyle>) {
          resolved.value_ = literal(theme::ColourRole::window_background);
        }
      },
      value_);
  return resolved;
}

}
