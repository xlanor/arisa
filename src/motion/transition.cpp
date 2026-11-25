#include <arisa/motion/transition.hpp>
#include <arisa/motion/transition_content.hpp>

#include <algorithm>
#include <cmath>

namespace arisa::motion {

auto Transition::Effect::combined(const Effect& other) const noexcept
    -> Effect {
  const bool other_scales = other.scale_x != 1.0F || other.scale_y != 1.0F;
  const bool other_tilts = other.tilt != 0.0F;
  return Effect{
      .opacity = opacity * other.opacity,
      .scale_x = scale_x * other.scale_x,
      .scale_y = scale_y * other.scale_y,
      .anchor = other_scales ? other.anchor : anchor,
      .offset_x = offset_x + other.offset_x,
      .offset_y = offset_y + other.offset_y,
      .relative_x = relative_x + other.relative_x,
      .relative_y = relative_y + other.relative_y,
      .blur = std::sqrt((blur * blur) + (other.blur * other.blur)),
      .rotation = rotation + other.rotation,
      .brightness = brightness + other.brightness,
      .contrast = contrast * other.contrast,
      .saturation = saturation * other.saturation,
      .grayscale = std::clamp(grayscale + other.grayscale, 0.0F, 1.0F),
      .hue_rotation = hue_rotation + other.hue_rotation,
      .colour_multiply =
          theme::Colour{colour_multiply.red * other.colour_multiply.red,
                        colour_multiply.green * other.colour_multiply.green,
                        colour_multiply.blue * other.colour_multiply.blue,
                        colour_multiply.alpha * other.colour_multiply.alpha},
      .tilt = other_tilts ? other.tilt : tilt,
      .tilt_x = other_tilts ? other.tilt_x : tilt_x,
      .tilt_y = other_tilts ? other.tilt_y : tilt_y,
      .tilt_z = other_tilts ? other.tilt_z : tilt_z,
      .perspective = other_tilts ? other.perspective : perspective};
}

auto Transition::Effect::moves() const noexcept -> bool {
  return scale_x != 1.0F || scale_y != 1.0F || offset_x != 0.0F ||
         offset_y != 0.0F || relative_x != 0.0F || relative_y != 0.0F ||
         rotation != 0.0F || tilt != 0.0F;
}

auto Transition::Effect::recolours() const noexcept -> bool {
  return brightness != 0.0F || contrast != 1.0F || saturation != 1.0F ||
         grayscale != 0.0F || hue_rotation != 0.0F ||
         colour_multiply != theme::Colour{1.0F, 1.0F, 1.0F, 1.0F};
}

auto Transition::Effect::reduced() const noexcept -> Effect {
  return Effect{.opacity = moves() ? 0.0F : opacity,
                .blur = blur,
                .brightness = brightness,
                .contrast = contrast,
                .saturation = saturation,
                .grayscale = grayscale,
                .hue_rotation = hue_rotation,
                .colour_multiply = colour_multiply};
}

auto Transition::Effect::at(double progress) const noexcept -> Effect {
  const auto mix = [progress](float start, float finish) {
    return static_cast<float>(start + ((finish - start) * progress));
  };
  return Effect{
      .opacity = mix(opacity, 1.0F),
      .scale_x = mix(scale_x, 1.0F),
      .scale_y = mix(scale_y, 1.0F),
      .anchor = anchor,
      .offset_x = mix(offset_x, 0.0F),
      .offset_y = mix(offset_y, 0.0F),
      .relative_x = mix(relative_x, 0.0F),
      .relative_y = mix(relative_y, 0.0F),
      .blur = mix(blur, 0.0F),
      .rotation = mix(rotation, 0.0F),
      .brightness = mix(brightness, 0.0F),
      .contrast = mix(contrast, 1.0F),
      .saturation = mix(saturation, 1.0F),
      .grayscale = mix(grayscale, 0.0F),
      .hue_rotation = mix(hue_rotation, 0.0F),
      .colour_multiply = theme::Colour{mix(colour_multiply.red, 1.0F),
                                       mix(colour_multiply.green, 1.0F),
                                       mix(colour_multiply.blue, 1.0F),
                                       mix(colour_multiply.alpha, 1.0F)},
      .tilt = mix(tilt, 0.0F),
      .tilt_x = tilt_x,
      .tilt_y = tilt_y,
      .tilt_z = tilt_z,
      .perspective = perspective};
}

auto Transition::modifier(const TransitionContent& active,
                          const TransitionContent& identity) noexcept
    -> Transition {
  const Effect& shown = active.effect();
  const Effect& rest = identity.effect();
  const auto ratio = [](float value, float base) {
    return base != 0.0F ? value / base : value;
  };
  const Effect relative{
      .opacity = ratio(shown.opacity, rest.opacity),
      .scale_x = ratio(shown.scale_x, rest.scale_x),
      .scale_y = ratio(shown.scale_y, rest.scale_y),
      .anchor = shown.anchor,
      .offset_x = shown.offset_x - rest.offset_x,
      .offset_y = shown.offset_y - rest.offset_y,
      .relative_x = shown.relative_x - rest.relative_x,
      .relative_y = shown.relative_y - rest.relative_y,
      .blur = std::sqrt(
          std::max((shown.blur * shown.blur) - (rest.blur * rest.blur), 0.0F)),
      .rotation = shown.rotation - rest.rotation,
      .brightness = shown.brightness - rest.brightness,
      .contrast = ratio(shown.contrast, rest.contrast),
      .saturation = ratio(shown.saturation, rest.saturation),
      .grayscale = std::clamp(shown.grayscale - rest.grayscale, 0.0F, 1.0F),
      .hue_rotation = shown.hue_rotation - rest.hue_rotation,
      .colour_multiply =
          theme::Colour{
              ratio(shown.colour_multiply.red, rest.colour_multiply.red),
              ratio(shown.colour_multiply.green, rest.colour_multiply.green),
              ratio(shown.colour_multiply.blue, rest.colour_multiply.blue),
              ratio(shown.colour_multiply.alpha, rest.colour_multiply.alpha)},
      .tilt = shown.tilt - rest.tilt,
      .tilt_x = shown.tilt_x,
      .tilt_y = shown.tilt_y,
      .tilt_z = shown.tilt_z,
      .perspective = shown.perspective};
  return Transition{relative, relative};
}

auto Transition::symbol_effect() noexcept -> Transition {
  const Effect hidden{.opacity = 0.0F,
                      .scale_x = symbol_effect_scale,
                      .scale_y = symbol_effect_scale};
  return Transition{hidden, hidden}.animation(
      Animation::smooth(symbol_effect_duration));
}

auto Transition::animation(std::optional<Animation> animation) const
    -> Transition {
  Transition attached = *this;
  attached.animation_ = std::move(animation);
  return attached;
}

auto Transition::with_properties(TransitionProperties properties) const noexcept
    -> Transition {
  Transition declared = *this;
  declared.declared_ = properties;
  return declared;
}

auto Transition::properties() const noexcept -> TransitionProperties {
  if (declared_.has_value()) {
    return *declared_;
  }
  return TransitionProperties{insertion_.moves() || removal_.moves()};
}

auto Transition::reduced() const noexcept -> Transition {
  if (!properties().has_motion()) {
    return *this;
  }
  Transition plain = *this;
  plain.insertion_ = insertion_.reduced();
  plain.removal_ = removal_.reduced();
  return plain;
}

auto Transition::identity() noexcept -> Transition {
  return Transition{Effect{}, Effect{}};
}

auto Transition::blur_replace(BlurReplace configuration) noexcept
    -> Transition {
  const Effect inserted{.opacity = 0.0F,
                        .scale_x = blur_replace_shrink,
                        .scale_y = blur_replace_shrink,
                        .blur = blur_replace_radius};
  const float departing = configuration == BlurReplace::up_up
                              ? blur_replace_grow
                              : blur_replace_shrink;
  const Effect removed{.opacity = 0.0F,
                       .scale_x = departing,
                       .scale_y = departing,
                       .blur = blur_replace_radius};
  return Transition{inserted, removed};
}

auto Transition::effect_of(const TransitionContent& content) noexcept
    -> Effect {
  return content.effect();
}

auto Transition::effect(Effect symmetric) noexcept -> Transition {
  return Transition{symmetric, symmetric};
}

auto Transition::opacity() noexcept -> Transition {
  const Effect faded{.opacity = 0.0F};
  return Transition{faded, faded};
}

auto Transition::scale(float scale, UnitPoint anchor) noexcept -> Transition {
  const Effect scaled{.scale_x = scale, .scale_y = scale, .anchor = anchor};
  return Transition{scaled, scaled};
}

auto Transition::displacement(theme::Edge edge) noexcept -> Effect {
  const auto has = [edge](theme::Edge side) {
    return (static_cast<unsigned>(edge) & static_cast<unsigned>(side)) != 0U;
  };
  Effect moved;
  if (has(theme::Edge::leading)) {
    moved.relative_x -= 1.0F;
  }
  if (has(theme::Edge::trailing)) {
    moved.relative_x += 1.0F;
  }
  if (has(theme::Edge::top)) {
    moved.relative_y -= 1.0F;
  }
  if (has(theme::Edge::bottom)) {
    moved.relative_y += 1.0F;
  }
  return moved;
}

auto Transition::opposite(theme::Edge edge) noexcept -> theme::Edge {
  switch (edge) {
    case theme::Edge::top:
      return theme::Edge::bottom;
    case theme::Edge::bottom:
      return theme::Edge::top;
    case theme::Edge::leading:
      return theme::Edge::trailing;
    case theme::Edge::trailing:
      return theme::Edge::leading;
    case theme::Edge::horizontal:
    case theme::Edge::vertical:
    case theme::Edge::all:
      return edge;
  }
  return edge;
}

auto Transition::move(theme::Edge edge) noexcept -> Transition {
  const Effect moved = displacement(edge);
  Transition made{moved, moved};
  made.edge_ = edge;
  return made;
}

auto Transition::slide() noexcept -> Transition {
  return asymmetric(move(theme::Edge::leading), move(theme::Edge::trailing));
}

auto Transition::push(theme::Edge edge) noexcept -> Transition {
  Transition made = asymmetric(move(edge).combined(opacity()),
                               move(opposite(edge)).combined(opacity()));
  made.edge_ = edge;
  return made;
}

auto Transition::offset(float x, float y) noexcept -> Transition {
  const Effect shifted{.offset_x = x, .offset_y = y};
  return Transition{shifted, shifted};
}

auto Transition::asymmetric(Transition insertion, Transition removal) noexcept
    -> Transition {
  Transition made{insertion.insertion_, removal.removal_};
  if (insertion.declared_.has_value() || removal.declared_.has_value()) {
    made.declared_ = TransitionProperties{insertion.properties().has_motion() ||
                                          removal.properties().has_motion()};
  }
  made.animation_ = insertion.animation_.has_value()
                        ? std::move(insertion.animation_)
                        : std::move(removal.animation_);
  return made;
}

auto Transition::combined(const Transition& other) const noexcept
    -> Transition {
  Transition made{insertion_.combined(other.insertion_),
                  removal_.combined(other.removal_)};
  if (declared_.has_value() || other.declared_.has_value()) {
    made.declared_ = TransitionProperties{properties().has_motion() ||
                                          other.properties().has_motion()};
  }
  made.animation_ = animation_.has_value() ? animation_ : other.animation_;
  return made;
}

auto TransitionContent::opacity(float value) const noexcept
    -> TransitionContent {
  TransitionContent next = *this;
  next.effect_.opacity *= std::clamp(value, 0.0F, 1.0F);
  return next;
}

auto TransitionContent::scale_effect(float scale,
                                     UnitPoint anchor) const noexcept
    -> TransitionContent {
  return scale_effect(scale, scale, anchor);
}

auto TransitionContent::scale_effect(float x, float y,
                                     UnitPoint anchor) const noexcept
    -> TransitionContent {
  TransitionContent next = *this;
  next.effect_.scale_x *= x;
  next.effect_.scale_y *= y;
  next.effect_.anchor = anchor;
  return next;
}

auto TransitionContent::offset(float x, float y) const noexcept
    -> TransitionContent {
  TransitionContent next = *this;
  next.effect_.offset_x += x;
  next.effect_.offset_y += y;
  return next;
}

auto TransitionContent::blur(float radius) const noexcept -> TransitionContent {
  TransitionContent next = *this;
  const float added = std::max(radius, 0.0F);
  next.effect_.blur =
      std::sqrt((effect_.blur * effect_.blur) + (added * added));
  return next;
}

auto TransitionContent::rotation_effect(Angle angle) const noexcept
    -> TransitionContent {
  TransitionContent next = *this;
  next.effect_.rotation += static_cast<float>(angle.radians());
  return next;
}

auto TransitionContent::rotation_3d_effect(Angle angle, float x, float y,
                                           float z,
                                           float perspective) const noexcept
    -> TransitionContent {
  TransitionContent next = *this;
  next.effect_.tilt = static_cast<float>(angle.radians());
  next.effect_.tilt_x = x;
  next.effect_.tilt_y = y;
  next.effect_.tilt_z = z;
  next.effect_.perspective = perspective;
  return next;
}

auto TransitionContent::brightness(float amount) const noexcept
    -> TransitionContent {
  TransitionContent next = *this;
  next.effect_.brightness += amount;
  return next;
}

auto TransitionContent::contrast(float amount) const noexcept
    -> TransitionContent {
  TransitionContent next = *this;
  next.effect_.contrast *= amount;
  return next;
}

auto TransitionContent::saturation(float amount) const noexcept
    -> TransitionContent {
  TransitionContent next = *this;
  next.effect_.saturation *= amount;
  return next;
}

auto TransitionContent::grayscale(float amount) const noexcept
    -> TransitionContent {
  TransitionContent next = *this;
  next.effect_.grayscale = std::clamp(effect_.grayscale + amount, 0.0F, 1.0F);
  return next;
}

auto TransitionContent::hue_rotation(Angle angle) const noexcept
    -> TransitionContent {
  TransitionContent next = *this;
  next.effect_.hue_rotation += static_cast<float>(angle.radians());
  return next;
}

auto TransitionContent::color_multiply(theme::Colour colour) const noexcept
    -> TransitionContent {
  TransitionContent next = *this;
  next.effect_.colour_multiply =
      theme::Colour{effect_.colour_multiply.red * colour.red,
                    effect_.colour_multiply.green * colour.green,
                    effect_.colour_multiply.blue * colour.blue,
                    effect_.colour_multiply.alpha * colour.alpha};
  return next;
}

}
