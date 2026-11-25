#include <arisa/rendering/painter.hpp>

#include <arisa/components/system_symbols.hpp>

#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/divider.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/glass_effect_container.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/mesh_gradient.hpp>
#include <arisa/components/shape_view.hpp>
#include <arisa/components/surface_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/text_layout.hpp>
#include <arisa/composition/decoration.hpp>
#include <arisa/composition/graphic_effects.hpp>
#include <arisa/composition/hover_effect_environment.hpp>
#include <arisa/composition/layer.hpp>
#include <arisa/composition/layer_host.hpp>
#include <arisa/composition/node_lifecycle_phase.hpp>
#include <arisa/composition/paint_order.hpp>
#include <arisa/composition/redaction.hpp>
#include <arisa/composition/scroll_environment.hpp>
#include <arisa/graphics/path.hpp>
#include <arisa/rendering/glass_lens.hpp>
#include <arisa/text/glyph_bitmap.hpp>
#include <arisa/text/glyph_quad.hpp>
#include <arisa/text/glyph_run.hpp>
#include <arisa/text/layout_width.hpp>
#include <arisa/text/positioned_glyph.hpp>
#include <arisa/text/text_layout.hpp>
#include <arisa/text/utf8.hpp>
#include <arisa/theme/oklab.hpp>
#include <arisa/theme/oklch.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <format>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <numbers>
#include <ranges>
#include <utility>
#include <variant>

namespace arisa::rendering {

Painter::Painter(provider::GpuProvider& gpu, text::GlyphAtlas atlas) noexcept
    : gpu_(gpu), atlas_(std::move(atlas)), shapes_(gpu), compositor_(gpu) {}

auto Painter::create(provider::GpuProvider& gpu, text::AtlasOptions options)
    -> std::expected<Painter, PaintError> {
  auto atlas = text::GlyphAtlas::create(gpu, options);
  if (!atlas.has_value()) {
    return std::unexpected(
        PaintError::from(atlas.error(), PaintStage::painter_setup));
  }
  return Painter{gpu, std::move(*atlas)};
}

auto Painter::create(provider::GpuProvider& gpu, text::AtlasOptions options,
                     provider::DiagnosticsProvider& diagnostics)
    -> std::expected<Painter, PaintError> {
  auto created = create(gpu, options);
  if (created.has_value()) {
    created->diagnostics_ = std::ref(diagnostics);
    created->compositor_.report_to(diagnostics);
  }
  return created;
}

auto Painter::contrast_threshold(Contrast kind,
                                 theme::ColorScheme scheme) noexcept -> float {
  const bool dark = scheme == theme::ColorScheme::dark;
  switch (kind) {
    case Contrast::fill:
      return dark ? fill_contrast_dark : fill_contrast_light;
    case Contrast::border:
      return dark ? border_contrast_dark : border_contrast_light;
    case Contrast::text:
      return text_contrast;
  }
  return text_contrast;
}

auto Painter::composite(theme::Colour colour, float opacity,
                        theme::Colour ground) noexcept -> theme::Colour {
  const float share = std::clamp(colour.alpha * opacity, 0.0F, 1.0F);
  const auto mix = [share](float top, float bottom) {
    return (top * share) + (bottom * (1.0F - share));
  };
  return theme::Colour{mix(colour.red, ground.red),
                       mix(colour.green, ground.green),
                       mix(colour.blue, ground.blue), 1.0F};
}

auto Painter::unpack(std::uint32_t colour) noexcept -> theme::Colour {
  const auto channel = [colour](unsigned shift) {
    return static_cast<float>((colour >> shift) & 0xFFU) / 255.0F;
  };
  return theme::Colour{channel(0U), channel(8U), channel(16U), channel(24U)};
}

auto Painter::ground(Box area) const noexcept -> theme::Colour {
  const Box wanted = bounds(area, transform_);
  for (const Quad& quad : std::views::reverse(quads_)) {
    const bool frosted = quad.kind == provider::ShapeKind::material ||
                         quad.kind == provider::ShapeKind::glass ||
                         quad.kind == provider::ShapeKind::glass_union;
    if (!frosted && (quad.kind != provider::ShapeKind::fill ||
                     (quad.colour >> 24U) != 0xFFU)) {
      continue;
    }
    const Box filled = bounds(Box{.left = quad.centre[0] - quad.half_size[0],
                                  .top = quad.centre[1] - quad.half_size[1],
                                  .right = quad.centre[0] + quad.half_size[0],
                                  .bottom = quad.centre[1] + quad.half_size[1]},
                              quad.transform);
    if (filled.left <= wanted.left + 1.0F && filled.top <= wanted.top + 1.0F &&
        filled.right >= wanted.right - 1.0F &&
        filled.bottom >= wanted.bottom - 1.0F) {
      if (frosted) {
        const theme::Colour tint = unpack(quad.colour);
        if (quad.kind == provider::ShapeKind::material) {
          return composite(tint.with_alpha(1.0F), tint.alpha, clear_);
        }
        const float kept = tint.alpha;
        return theme::Colour{std::min((clear_.red * kept) + tint.red, 1.0F),
                             std::min((clear_.green * kept) + tint.green, 1.0F),
                             std::min((clear_.blue * kept) + tint.blue, 1.0F),
                             1.0F};
      }
      return unpack(quad.colour);
    }
  }
  return clear_;
}

void Painter::refuse_picture(std::string_view stage,
                             const provider::Image& source, std::uint8_t level,
                             const provider::ProviderError& error) {
  if (!diagnostics_.has_value()) {
    return;
  }
  diagnostics_->get().report(provider::Diagnostic{
      .severity = provider::Severity::warning,
      .category = "picture",
      .message = std::format("picture {} failed for {}x{} at level {}: {}",
                             stage, source.width(), source.height(), level,
                             error.explanation())});
}

void Painter::missing_symbol(const composition::Node& node) {
  const auto image = node.content_as<components::Image::Content>();
  if (!image.has_value() || !image->get().system() ||
      !diagnostics_.has_value() ||
      components::SystemSymbols::known(image->get().name())) {
    return;
  }
  for (const composition::Node& child : node.children()) {
    if (child.content_as<components::Text::Content>().has_value()) {
      return;
    }
  }
  const std::string name{image->get().name()};
  if (!missing_.insert(name).second) {
    return;
  }
  diagnostics_->get().report(provider::Diagnostic{
      .severity = provider::Severity::warning,
      .category = "symbol",
      .message =
          std::format("system symbol \"{}\" is not in the icon font", name)});
}

void Painter::forget_departed(const composition::Tree& tree) {
  if (reported_.empty() || tree.revision() == reported_revision_) {
    return;
  }
  reported_revision_ = tree.revision();
  std::erase_if(reported_, [&tree](const auto& entry) {
    return !tree.node(entry.first).has_value();
  });
}

void Painter::inspect(const composition::Node& node, const Scope& scope,
                      theme::Colour colour, Contrast kind, Box area) {
  if (!checks_contrast || !diagnostics_.has_value() ||
      colour.alpha <= visible_threshold || shown(scope) < 1.0F ||
      area.width() <= 0.0F || area.height() <= 0.0F) {
    return;
  }
  const theme::Colour beneath = ground(area);
  const theme::Colour shown = composite(colour, 1.0F, beneath);
  const theme::Oklab top = theme::Oklab::from(shown);
  const theme::Oklab bottom = theme::Oklab::from(beneath);
  const float step = std::hypot(top.lightness() - bottom.lightness(),
                                top.a() - bottom.a(), top.b() - bottom.b());
  const float wanted = contrast_threshold(kind, scope.resolved_scheme());
  if (reported_.size() > reported_limit) {
    reported_.clear();
  }
  if (step < same_surface || step >= wanted ||
      !reported_.insert(std::pair{node.id(), kind}).second) {
    return;
  }
  constexpr std::array names{"fill", "border", "text"};
  diagnostics_->get().report(provider::Diagnostic{
      .severity = provider::Severity::warning,
      .category = "contrast",
      .message = std::format(
          "node {} {} Oklab step {:.3f} is under {:.3f} ({}) at "
          "{:.0f},{:.0f} {:.0f}x{:.0f}",
          node.id().value(), names[std::to_underlying(kind)], step, wanted,
          scope.resolved_scheme() == theme::ColorScheme::dark ? "dark"
                                                              : "light",
          node.frame().x, node.frame().y, node.frame().width,
          node.frame().height)});
}

auto Painter::pack(theme::Colour colour, float opacity) noexcept
    -> std::uint32_t {
  const auto channel = [](float value) {
    return static_cast<std::uint8_t>(
        std::lround(std::clamp(value, 0.0F, 1.0F) * 255.0F));
  };
  return provider::Vertex::rgba(channel(colour.red), channel(colour.green),
                                channel(colour.blue),
                                channel(colour.alpha * opacity));
}

auto Painter::snap(Box box) noexcept -> Box {
  return Box{.left = std::round(box.left),
             .top = std::round(box.top),
             .right = std::round(box.right),
             .bottom = std::round(box.bottom)};
}

auto Painter::intersect(provider::ClipRect clip, Box box) noexcept
    -> provider::ClipRect {
  const float left = std::max(static_cast<float>(clip.x), std::floor(box.left));
  const float top = std::max(static_cast<float>(clip.y), std::floor(box.top));
  const float right =
      std::min(static_cast<float>(clip.x + clip.width), std::ceil(box.right));
  const float bottom =
      std::min(static_cast<float>(clip.y + clip.height), std::ceil(box.bottom));
  if (right <= left || bottom <= top) {
    return provider::ClipRect{
        .x = clip.x, .y = clip.y, .width = 0, .height = 0};
  }
  return provider::ClipRect{.x = static_cast<std::uint32_t>(left),
                            .y = static_cast<std::uint32_t>(top),
                            .width = static_cast<std::uint32_t>(right - left),
                            .height = static_cast<std::uint32_t>(bottom - top)};
}

auto Painter::rounding(const theme::Theme& theme, theme::Shape outline,
                       Box area, float scale) noexcept -> Corners {
  const float limit = std::min(area.width(), area.height()) * 0.5F;
  const float radius = std::min(theme.radius(outline.radius) * scale, limit);
  return Corners{.radius = std::max(radius, 0.0F),
                 .smoothing = outline.continuous && theme.radii().continuous
                                  ? continuous_smoothing
                                  : 0.0F};
}

auto Painter::bounded(const graphics::ShapeStyle& style,
                      Box frame) const noexcept -> Box {
  const std::optional<composition::Rect>& bounds = style.bounds();
  if (!bounds.has_value()) {
    return frame;
  }
  const float scale = target_.scale.value();
  return Box{.left = frame.left + (bounds->x * scale),
             .top = frame.top + (bounds->y * scale),
             .right = frame.left + ((bounds->x + bounds->width) * scale),
             .bottom = frame.top + ((bounds->y + bounds->height) * scale)};
}

auto Painter::fitted(const theme::Shape& outline, Box area) noexcept -> Box {
  if (outline.form != theme::ShapeForm::circle) {
    return area;
  }
  const float side = std::max(std::min(area.width(), area.height()), 0.0F);
  const float left = (area.left + area.right - side) * 0.5F;
  const float top = (area.top + area.bottom - side) * 0.5F;
  return Box{
      .left = left, .top = top, .right = left + side, .bottom = top + side};
}

auto Painter::corner_of(const Corners& corners, std::size_t corner) noexcept
    -> float {
  return uneven(corners) ? corners.radii[corner] : corners.radius;
}

auto Painter::plain_corners(const Corners& corners, Box area) noexcept
    -> Corners {
  if (corners.ellipse) {
    return Corners{.radius = (std::min(area.width(), area.height()) * 0.5F) +
                             corners.radius,
                   .smoothing = 0.0F};
  }
  return Corners{.radius = corners.radius, .smoothing = corners.smoothing};
}

auto Painter::outer_radius(const Outline& outline, std::size_t corner) noexcept
    -> float {
  if (outline.corners.ellipse) {
    return (std::min(outline.box.width(), outline.box.height()) * 0.5F) +
           outline.corners.radius;
  }
  return corner_of(outline.corners, corner);
}

auto Painter::uneven(const Corners& corners) noexcept -> bool {
  return !corners.ellipse && corners.radii != std::array<float, 4>{};
}

auto Painter::grown(const Corners& corners, float amount) noexcept -> Corners {
  Corners result = corners;
  result.radius = std::max(corners.radius + amount, 0.0F);
  if (uneven(corners)) {
    for (float& radius : result.radii) {
      radius = std::max(radius + amount, 0.0F);
    }
  }
  return result;
}

auto Painter::detail_of(const Corners& corners) noexcept
    -> std::array<float, 4> {
  if (corners.ellipse) {
    return {ellipse_marker, ellipse_marker, ellipse_marker, ellipse_marker};
  }
  return uneven(corners) ? corners.radii : std::array<float, 4>{};
}

auto Painter::corners_of(std::array<float, 4> radii, float smoothing,
                         float limit) noexcept -> Corners {
  for (float& radius : radii) {
    radius = std::clamp(radius, 0.0F, std::max(limit, 0.0F));
  }
  const float largest = std::ranges::max(radii);
  if (std::ranges::all_of(radii,
                          [&radii](float each) { return each == radii[0]; })) {
    return Corners{.radius = largest, .smoothing = smoothing};
  }
  return Corners{.radius = largest, .smoothing = smoothing, .radii = radii};
}

auto Painter::screen_corners(const theme::RectangleCornerRadii& radii,
                             composition::LayoutDirection direction) noexcept
    -> std::array<float, 4> {
  if (direction == composition::LayoutDirection::right_to_left) {
    return {radii.top_trailing, radii.top_leading, radii.bottom_leading,
            radii.bottom_trailing};
  }
  return {radii.top_leading, radii.top_trailing, radii.bottom_trailing,
          radii.bottom_leading};
}

auto Painter::resolve(const Scope& scope, const theme::Shape& outline,
                      Box area) const -> Corners {
  const theme::Theme& theme = scope.theme.get();
  const float scale = target_.scale.value();
  const float limit =
      std::max(std::min(area.width(), area.height()) * 0.5F, 0.0F);
  const float smoothing = outline.continuous && theme.radii().continuous
                              ? continuous_smoothing
                              : 0.0F;
  switch (outline.form) {
    case theme::ShapeForm::rounded:
      return rounding(theme, outline, area, scale);
    case theme::ShapeForm::circle:
      return Corners{.radius = limit, .smoothing = 0.0F};
    case theme::ShapeForm::ellipse:
      return Corners{.radius = 0.0F, .smoothing = 0.0F, .ellipse = true};
    case theme::ShapeForm::uneven: {
      std::array<float, 4> radii =
          screen_corners(outline.corner_radii, scope.direction);
      for (float& radius : radii) {
        radius *= scale;
      }
      return corners_of(radii, smoothing, limit);
    }
    case theme::ShapeForm::concentric:
    case theme::ShapeForm::container_relative:
      break;
  }
  const bool relative = outline.form == theme::ShapeForm::container_relative;
  const std::optional<Outline> found =
      scope.enclosure.has_value()
          ? relocate(scope.enclosure->outline, scope.enclosure->transform,
                     scope.transform)
          : std::nullopt;
  const std::array<float, 4> fallback = screen_corners(
      theme::RectangleCornerRadii{
          .top_leading = outline.concentric[0].radius(),
          .bottom_leading = outline.concentric[1].radius(),
          .bottom_trailing = outline.concentric[2].radius(),
          .top_trailing = outline.concentric[3].radius()},
      scope.direction);
  if (!found.has_value()) {
    if (relative) {
      return Corners{};
    }
    std::array<float, 4> radii{};
    for (std::size_t corner = 0; corner < radii.size(); ++corner) {
      radii[corner] = fallback[corner] * scale;
    }
    even_out(outline.uniform_edges, scope.direction, radii);
    return corners_of(radii, smoothing, limit);
  }
  const Box& edge = found->box;
  const float left = std::max(area.left - edge.left, 0.0F);
  const float top = std::max(area.top - edge.top, 0.0F);
  const float right = std::max(edge.right - area.right, 0.0F);
  const float bottom = std::max(edge.bottom - area.bottom, 0.0F);
  const std::array<std::array<float, 2>, 4> insets{
      {{left, top}, {right, top}, {right, bottom}, {left, bottom}}};
  const float nearest = std::min({left, top, right, bottom});
  const theme::RectangleCornerRadii fixed_flags{
      .top_leading =
          outline.concentric[0].kind() == theme::ConcentricCorner::Kind::fixed
              ? 1.0F
              : 0.0F,
      .bottom_leading =
          outline.concentric[1].kind() == theme::ConcentricCorner::Kind::fixed
              ? 1.0F
              : 0.0F,
      .bottom_trailing =
          outline.concentric[2].kind() == theme::ConcentricCorner::Kind::fixed
              ? 1.0F
              : 0.0F,
      .top_trailing =
          outline.concentric[3].kind() == theme::ConcentricCorner::Kind::fixed
              ? 1.0F
              : 0.0F};
  const std::array<float, 4> fixed =
      screen_corners(fixed_flags, scope.direction);
  std::array<float, 4> radii{};
  for (std::size_t corner = 0; corner < radii.size(); ++corner) {
    const float outer = outer_radius(*found, corner);
    if (relative) {
      radii[corner] = std::max(outer - nearest, 0.0F);
    } else if (fixed[corner] > 0.5F) {
      radii[corner] = fallback[corner] * scale;
    } else {
      radii[corner] =
          std::max(outer - std::max(insets[corner][0], insets[corner][1]),
                   fallback[corner] * scale);
    }
  }
  if (outline.uniform) {
    radii.fill(std::ranges::max(radii));
  }
  even_out(outline.uniform_edges, scope.direction, radii);
  return corners_of(radii, found->corners.smoothing, limit);
}

void Painter::even_out(std::uint8_t edges,
                       composition::LayoutDirection direction,
                       std::array<float, 4>& radii) noexcept {
  const bool flipped = direction == composition::LayoutDirection::right_to_left;
  const auto pair = [&radii](std::size_t first, std::size_t second) {
    const float larger = std::max(radii[first], radii[second]);
    radii[first] = larger;
    radii[second] = larger;
  };
  if ((edges & theme::Shape::uniform_top) != 0U) {
    pair(0, 1);
  }
  if ((edges & theme::Shape::uniform_bottom) != 0U) {
    pair(2, 3);
  }
  if ((edges & theme::Shape::uniform_leading) != 0U) {
    flipped ? pair(1, 2) : pair(0, 3);
  }
  if ((edges & theme::Shape::uniform_trailing) != 0U) {
    flipped ? pair(0, 3) : pair(1, 2);
  }
}

auto Painter::hierarchy(const Hierarchy* parent,
                        const graphics::StyleHierarchy& source)
    -> const Hierarchy* {
  const auto found =
      std::ranges::find_if(hierarchies_, [&](const Hierarchy& held) {
        return held.parent == parent && held.source == source;
      });
  if (found != hierarchies_.end()) {
    return &*found;
  }
  const std::vector<graphics::ShapeStyle>& inherited =
      parent != nullptr ? parent->levels : default_levels_;
  std::vector<graphics::ShapeStyle> levels;
  levels.reserve(graphics::StyleHierarchy::levels);
  for (std::size_t index = 0; index < graphics::StyleHierarchy::levels;
       ++index) {
    graphics::ShapeStyle level =
        source.level(static_cast<graphics::HierarchicalShapeStyle>(index));
    if (const std::optional<graphics::HierarchicalShapeStyle> tier =
            level.level()) {
      level = level.substituted(inherited[static_cast<std::size_t>(*tier)]);
    }
    levels.push_back(std::move(level));
  }
  return &hierarchies_.emplace_back(Hierarchy{
      .parent = parent, .source = source, .levels = std::move(levels)});
}

auto Painter::canonical_background(const graphics::ShapeStyle& style)
    -> const graphics::ShapeStyle* {
  const auto found = std::ranges::find(backgrounds_, style);
  if (found != backgrounds_.end()) {
    return &*found;
  }
  return &backgrounds_.emplace_back(style);
}

auto Painter::style_of(const Scope& scope,
                       const graphics::ShapeStyle& style) const -> Styled {
  if (const std::optional<graphics::HierarchicalShapeStyle> tier =
          style.level()) {
    const std::vector<graphics::ShapeStyle>& levels =
        scope.foreground != nullptr ? scope.foreground->levels
                                    : default_levels_;
    return Styled{.style = &levels[static_cast<std::size_t>(*tier)],
                  .opacity = style.opacity()};
  }
  if (std::holds_alternative<graphics::BackgroundStyle>(style.value())) {
    const graphics::ShapeStyle* background = scope.background_style != nullptr
                                                 ? scope.background_style
                                                 : &default_background_;
    if (background->level().has_value() ||
        std::holds_alternative<graphics::BackgroundStyle>(
            background->value())) {
      background = &default_background_;
    }
    return Styled{.style = background, .opacity = style.opacity()};
  }
  return Styled{.style = &style, .opacity = 1.0F};
}

auto Painter::ink(const Scope& scope, theme::ColourValue fallback) const
    -> theme::ColourValue {
  if (scope.foreground == nullptr) {
    return fallback;
  }
  const Styled styled = style_of(scope, scope.foreground->levels.front());
  if (const std::optional<theme::ColourValue> solid = styled.style->colour()) {
    return solid->opacity(styled.opacity);
  }
  if (const graphics::Gradient* ramp = styled.style->gradient();
      ramp != nullptr && !ramp->stops().empty()) {
    return ramp->stops().front().color;
  }
  if (const std::optional<theme::Material> material =
          styled.style->material()) {
    return theme::ColourValue{material_colour(scope, *material)};
  }
  return fallback;
}

auto Painter::material_colour(const Scope& scope,
                              theme::Material material) const -> theme::Colour {
  const theme::MaterialStyle& style = scope.theme.get().material(material);
  const theme::Colour tint = style.tint.resolve(scope.resolved_scheme());
  return composite(tint.with_alpha(1.0F),
                   std::max(tint.alpha, solid_glass_alpha), clear_);
}

auto Painter::shade(const Scope& scope, const graphics::ShapeStyle& style,
                    Box frame) -> std::optional<Shade> {
  if (style.gradient() == nullptr) {
    return std::nullopt;
  }
  restyle(scope, style, frame, shade_paint_);
  return shaded_paint(shade_paint_, frame);
}

auto Painter::animated_shade(const Scope& scope,
                             const graphics::ShapeStyle& style, Box frame,
                             composition::NodeId owner,
                             motion::Presentation& presentation,
                             const motion::Presentation::Moment& moment)
    -> std::optional<Shade> {
  if (style.gradient() == nullptr) {
    return std::nullopt;
  }
  restyle(scope, style, frame, shade_paint_);
  animate_paint(shade_paint_, owner, presentation, moment,
                motion::Presentation::Mix::foreground);
  return shaded_paint(shade_paint_, frame);
}

void Painter::animate_paint(ShapePainter::Paint& paint,
                            composition::NodeId owner,
                            motion::Presentation& presentation,
                            const motion::Presentation::Moment& moment,
                            motion::Presentation::Mix mix) {
  std::array<float, motion::Presentation::mix_capacity> values{};
  const std::size_t used = encoded(paint, values);
  if (used == 0) {
    return;
  }
  const std::span<const float> mixed =
      presentation.mix(mix, std::span<const float>{values}.first(used), moment,
                       paint_shape(paint));
  if (presentation.in_flight(mix)) {
    decoded(mixed, paint);
    paint.owner = owner;
    unsettle();
  }
}

auto Painter::encoded(const ShapePainter::Paint& paint,
                      std::span<float> into) noexcept -> std::size_t {
  constexpr std::size_t geometry = 6;
  constexpr std::size_t per_stop = 5;
  const std::size_t needed = geometry + (paint.stops.size() * per_stop);
  if (paint.stops.empty() || needed > into.size()) {
    return 0;
  }
  into[0] = paint.start[0];
  into[1] = paint.start[1];
  into[2] = paint.end[0];
  into[3] = paint.end[1];
  into[4] = paint.first;
  into[5] = paint.second;
  std::size_t at = geometry;
  for (const ShapePainter::Stop& stop : paint.stops) {
    into[at++] = stop.colour.red;
    into[at++] = stop.colour.green;
    into[at++] = stop.colour.blue;
    into[at++] = stop.colour.alpha;
    into[at++] = stop.location;
  }
  return needed;
}

void Painter::decoded(std::span<const float> values,
                      ShapePainter::Paint& into) {
  constexpr std::size_t geometry = 6;
  constexpr std::size_t per_stop = 5;
  if (values.size() < geometry) {
    return;
  }
  into.start = {values[0], values[1]};
  into.end = {values[2], values[3]};
  into.first = values[4];
  into.second = values[5];
  into.stops.clear();
  for (std::size_t at = geometry; at + per_stop <= values.size();
       at += per_stop) {
    into.stops.push_back(ShapePainter::Stop{
        .colour = theme::Colour{values[at], values[at + 1], values[at + 2],
                                values[at + 3]},
        .location = values[at + 4]});
  }
}

auto Painter::paint_shape(const ShapePainter::Paint& paint) noexcept
    -> std::uint64_t {
  const std::array<std::uint64_t, 3> traits{
      static_cast<std::uint64_t>(paint.kind),
      static_cast<std::uint64_t>(paint.space), paint.stops.size()};
  return Compositor::mix(0, std::as_bytes(std::span{traits}));
}

auto Painter::blends(const Scope& scope, const graphics::ShapeStyle& style,
                     Box area, Corners corners, float opacity,
                     const std::optional<Motion>& animated) -> bool {
  if (!animated.has_value() || style.gradient() == nullptr) {
    return false;
  }
  restyle(scope, style, area, shade_paint_);
  return gradient_blend(scope, area, corners, opacity, *animated);
}

auto Painter::gradient_blend(const Scope& scope, Box area, Corners corners,
                             float opacity, const Motion& animated) -> bool {
  using Mix = motion::Presentation::Mix;
  std::array<float, motion::Presentation::mix_capacity> values{};
  const std::size_t used = encoded(shade_paint_, values);
  if (used == 0) {
    return false;
  }
  motion::Presentation& presentation = animated.presentation;
  const std::span<const float> mixed =
      presentation.mix(Mix::fill, std::span<const float>{values}.first(used),
                       animated.moment, paint_shape(shade_paint_));
  const std::optional<motion::Presentation::Blend> blending =
      presentation.blend(Mix::fill);
  if (!blending.has_value()) {
    return false;
  }
  const auto layer = [&](std::span<const float> stops, float alpha) {
    decoded(stops, shade_paint_);
    shade_paint_.start = {mixed[0], mixed[1]};
    shade_paint_.end = {mixed[2], mixed[3]};
    shade_paint_.first = mixed[4];
    shade_paint_.second = mixed[5];
    if (const std::optional<Shade> shaded = shaded_paint(shade_paint_, area)) {
      const std::size_t before = quads_.size();
      shape(provider::ShapeKind::fill, area, corners, 0.0F, 0.0F,
            pack(theme::Colour{1.0F, 1.0F, 1.0F, 1.0F}, alpha), scope.clip);
      stylise(before, *shaded);
    }
  };
  layer(blending->from, opacity);
  layer(blending->to, opacity * blending->progress);
  return true;
}

void Painter::blend_corners(motion::Presentation& presentation,
                            const motion::Presentation::Moment& moment,
                            std::optional<double> previous, Corners& rounded,
                            Box area) const {
  using Mix = motion::Presentation::Mix;
  if (rounded.ellipse ||
      (!uneven(rounded) && !presentation.mixing(Mix::corners))) {
    return;
  }
  const float scale = target_.scale.value();
  std::array<float, 4> wanted{};
  for (std::size_t corner = 0; corner < wanted.size(); ++corner) {
    wanted[corner] =
        (uneven(rounded) ? rounded.radii[corner] : rounded.radius) / scale;
  }
  if (!presentation.mixing(Mix::corners) && previous.has_value()) {
    const auto held = static_cast<float>(*previous);
    const std::array<float, 4> seeded{held, held, held, held};
    presentation.seed(Mix::corners, seeded);
  }
  const std::span<const float> mixed =
      presentation.mix(Mix::corners, wanted, moment);
  if (!presentation.in_flight(Mix::corners)) {
    return;
  }
  const float limit = std::min(area.width(), area.height()) * 0.5F;
  float largest = 0.0F;
  for (std::size_t corner = 0; corner < rounded.radii.size(); ++corner) {
    rounded.radii[corner] = std::clamp(mixed[corner] * scale, 0.0F, limit);
    largest = std::max(largest, rounded.radii[corner]);
  }
  if (rounded.radii == std::array<float, 4>{}) {
    rounded.radii[0] = 1.0e-4F;
  }
  rounded.radius = largest;
}

auto Painter::shaded_paint(const ShapePainter::Paint& paint, Box frame)
    -> std::optional<Shade> {
  using Kind = ShapePainter::PaintKind;
  const std::optional<provider::TextureId> ramp = shapes_.ramp(paint);
  if (!ramp.has_value()) {
    return std::nullopt;
  }
  const float scale = target_.scale.value();
  const float cx = (frame.left + frame.right) * 0.5F;
  const float cy = (frame.top + frame.bottom) * 0.5F;
  const std::array<float, 2> start{paint.start[0] - cx, paint.start[1] - cy};
  switch (paint.kind) {
    case Kind::linear:
      return Shade{
          .mode = 0.0F,
          .points = {start[0], start[1], paint.end[0] - cx, paint.end[1] - cy},
          .centre = {cx, cy},
          .ramp = *ramp};
    case Kind::radial:
      return Shade{.mode = 1.0F,
                   .points = {start[0], start[1], paint.first * scale,
                              paint.second * scale},
                   .centre = {cx, cy},
                   .ramp = *ramp};
    case Kind::angular:
      return Shade{.mode = 2.0F,
                   .points = {start[0], start[1], paint.first, paint.second},
                   .centre = {cx, cy},
                   .ramp = *ramp};
    case Kind::elliptical:
      return Shade{.mode = 3.0F,
                   .points = {start[0], start[1], paint.first, paint.second},
                   .extent = {paint.end[0], paint.end[1]},
                   .centre = {cx, cy},
                   .ramp = *ramp};
    case Kind::solid:
      break;
  }
  return std::nullopt;
}

void Painter::stylise(std::size_t from, const Shade& shade) {
  for (std::size_t index = from; index < quads_.size(); ++index) {
    stylise(quads_[index], shade);
  }
}

void Painter::stylise(Quad& quad, const Shade& shade) {
  {
    if (quad.kind == provider::ShapeKind::fill) {
      quad.kind = provider::ShapeKind::gradient_fill;
      quad.texture = shade.ramp;
      quad.shape[provider::ShapeVertex::stroke] = shade.mode;
      quad.effect = shade.points;
      quad.uv = {shade.extent[0], shade.extent[1], shade.extent[0],
                 shade.extent[1]};
    } else if (quad.kind == provider::ShapeKind::glyph) {
      quad.kind = provider::ShapeKind::glyph_gradient;
      quad.detail_texture = shade.ramp;
      quad.shape = {0.0F, 0.0F, shade.mode, 0.0F};
      quad.effect = shade.points;
      quad.half_size = shade.extent;
      quad.centre = shade.centre;
    }
  }
}

void Painter::style_shadow(const Scope& scope,
                           const graphics::ShapeStyle& style, Box area,
                           Corners corners, graphics::ShadowStyle::Kind kind) {
  if (!style.shadow().has_value() || style.shadow()->kind() != kind) {
    return;
  }
  const graphics::ShadowStyle& cast = *style.shadow();
  const float scale = target_.scale.value();
  const theme::Colour colour = scope.colour(cast.colour());
  if (kind == graphics::ShadowStyle::Kind::drop) {
    drop_shadow(scope, colour, cast.radius(), cast.x(), cast.y(), area, corners,
                ShadowMotion::settled);
    return;
  }
  const std::size_t before = quads_.size();
  shape(provider::ShapeKind::fill, area, corners, 0.0F,
        std::max(cast.radius() * scale * drop_shadow_spread, 0.5F),
        pack(colour, scope.opacity), scope.clip);
  for (std::size_t index = before; index < quads_.size(); ++index) {
    quads_[index].kind = provider::ShapeKind::inner_shadow;
    quads_[index].effect = {cast.x() * scale, cast.y() * scale, 0.0F, 0.0F};
  }
}

void Painter::styled_fill(composition::NodeId key, const Scope& scope,
                          const graphics::ShapeStyle& style, Box area,
                          Corners corners, std::optional<Motion> animated) {
  const Styled styled = style_of(scope, style);
  const graphics::ShapeStyle& shown = *styled.style;
  const graphics::ShapeStyle& shadowed =
      style.shadow().has_value() ? style : shown;
  style_shadow(scope, shadowed, area, corners,
               graphics::ShadowStyle::Kind::drop);
  if (const std::optional<theme::ColourValue> solid = shown.colour()) {
    shape(provider::ShapeKind::fill, area, corners, 0.0F, 0.0F,
          pack(scope.colour(solid->opacity(styled.opacity)), scope.opacity),
          scope.clip);
  } else if (const std::optional<theme::Material> material = shown.material()) {
    Scope faded = scope;
    faded.opacity *= styled.opacity;
    frost(key, faded, *material, area, corners);
  } else if (const graphics::ImagePaint* tiled = shown.image_paint()) {
    paint_image(scope, *tiled, area,
                scope.opacity * styled.opacity * shown.opacity());
  } else if (blends(scope, shown, area, corners,
                    scope.opacity * styled.opacity * shown.opacity(),
                    animated)) {
  } else if (const std::optional<Shade> shaded = shade(scope, shown, area)) {
    const std::size_t before = quads_.size();
    shape(provider::ShapeKind::fill, area, corners, 0.0F, 0.0F,
          pack(theme::Colour{1.0F, 1.0F, 1.0F, 1.0F},
               scope.opacity * styled.opacity * shown.opacity()),
          scope.clip);
    stylise(before, *shaded);
  }
  style_shadow(scope, shadowed, area, corners,
               graphics::ShadowStyle::Kind::inner);
}

auto Painter::focus_outline(const composition::Node& node)
    -> std::optional<theme::Shape> {
  const composition::Node* at = &node;
  while (true) {
    const composition::Modifiers& modifiers = at->modifiers();
    if (const std::optional<theme::Shape>& declared =
            at->interactions().focus_shape) {
      return declared;
    }
    if (modifiers.corners.has_value()) {
      return modifiers.corners;
    }
    if (modifiers.glass.has_value()) {
      return modifiers.glass->shape;
    }
    if (modifiers.background.has_value()) {
      return modifiers.background->shape;
    }
    if (modifiers.material.has_value()) {
      return modifiers.material->shape;
    }
    const composition::Node* only = nullptr;
    for (const composition::Node& child : at->children()) {
      if (child.phase() != composition::NodeLifecyclePhase::live) {
        continue;
      }
      if (only != nullptr) {
        return std::nullopt;
      }
      only = &child;
    }
    if (only == nullptr ||
        std::abs(only->frame().width - at->frame().width) > same_extent ||
        std::abs(only->frame().height - at->frame().height) > same_extent) {
      return std::nullopt;
    }
    at = only;
  }
}

auto Painter::retinted(const theme::Theme& base, theme::Colour accent)
    -> const theme::Theme& {
  const auto key = std::pair{pack(accent, 1.0F), &base};
  auto found = tints_.find(key);
  if (found == tints_.end()) {
    found =
        tints_
            .emplace(
                key,
                base.sized(theme::DynamicTypeSize::large).with_accent(accent))
            .first;
  }
  return found->second;
}

auto Painter::tint_colour(const Scope& scope, const graphics::ShapeStyle& style)
    -> std::optional<theme::Colour> {
  if (const std::optional<theme::ColourValue> colour = style.colour()) {
    return scope.colour(*colour);
  }
  if (const graphics::Gradient* gradient = style.gradient();
      gradient != nullptr && !gradient->stops().empty()) {
    return scope.colour(gradient->stops().front().color);
  }
  return std::nullopt;
}

auto Painter::scoped(const composition::Node& node, Scope scope) -> Scope {
  bool rescale = false;
  scope.inverts_here = false;
  if (const auto invert = node.override_of(
          components::Environment::accessibility_invert_colors)) {
    scope.inverts_here = invert->get() != scope.invert_colors;
    scope.invert_colors = invert->get();
  }
  if (const auto& accessible = node.interactions().accessibility;
      scope.invert_colors && !scope.ignores_invert && accessible != nullptr &&
      accessible->ignores_invert_colors) {
    scope.inverts_here = !scope.inverts_here;
    scope.ignores_invert = true;
  }
  if (const auto highlighting = node.override_of(
          components::Environment::accessibility_reduce_highlighting_effects)) {
    scope.reduce_highlighting = highlighting->get();
  }
  if (const auto borders = node.override_of(
          components::Environment::accessibility_show_borders)) {
    scope.show_borders = borders->get();
  }
  if (const auto fades =
          node.override_of(components::Environment::
                               accessibility_prefers_cross_fade_transitions)) {
    scope.cross_fade = fades->get();
  }
  if (const auto theme = node.override_of(components::Environment::theme)) {
    scope.theme = canonical(theme->get());
    rescale = true;
  }
  if (const auto tint = node.override_of(components::Environment::tint)) {
    if (tint->get().has_value()) {
      if (const std::optional<theme::Colour> accent =
              tint_colour(scope, *tint->get())) {
        scope.theme = retinted(scope.theme.get(), *accent);
        rescale = true;
      }
    }
  }
  if (const auto range =
          node.override_of(components::Environment::dynamic_type_range)) {
    scope.dynamic_type_range = scope.dynamic_type_range.intersect(range->get());
    scope.dynamic_type_size =
        scope.dynamic_type_range.clamp(scope.dynamic_type_size);
    rescale = true;
  }
  if (const auto size =
          node.override_of(components::Environment::dynamic_type_size)) {
    scope.dynamic_type_size = scope.dynamic_type_range.clamp(size->get());
    rescale = true;
  }
  if (const auto font = node.override_of(components::Environment::font)) {
    if (font->get().has_value()) {
      scope.font = font->get();
    }
  }
  scope.text = components::Text::inherited(node, scope.text);
  if (const auto direction =
          node.override_of(components::Environment::layout_direction)) {
    scope.direction = direction->get();
  }
  if (const auto& behavior = node.modifiers().layout_direction_behavior;
      behavior.has_value() && behavior->mirrored_in(scope.direction)) {
    scope.direction = composition::LayoutDirection::left_to_right;
  }
  if (const auto differentiate = node.override_of(
          components::Environment::accessibility_differentiate_without_color)) {
    scope.differentiate = differentiate->get();
  }
  if (highlights_.focused == node.id()) {
    scope.focused = true;
  }
  if (const auto effects =
          node.override_of(components::Environment::is_focus_effect_enabled)) {
    scope.focus_effect_disabled = !effects->get();
  }
  if (const auto enabled =
          node.override_of(components::Environment::is_enabled)) {
    scope.enabled = scope.enabled && enabled->get();
  }
  if (const auto effects = node.override_of(
          composition::ScrollEnvironment::scroll_edge_effects)) {
    scope.edge_effects = &effects->get();
  }
  if (const auto redaction = node.override_of(composition::Redaction::entry)) {
    scope.redaction =
        composition::Redaction::applied(scope.redaction, redaction->get());
  }
  if (const auto sensitive =
          node.override_of(composition::Redaction::privacy_sensitive)) {
    scope.sensitive = sensitive->get();
  }
  if (const auto enabled = node.override_of(
          composition::HoverEffectEnvironment::is_hover_effect_enabled)) {
    scope.hover_disabled = scope.hover_disabled || !enabled->get();
  }
  if (const auto fallback = node.override_of(
          composition::HoverEffectEnvironment::default_hover_effect)) {
    scope.default_hover =
        fallback->get().has_value() ? &*fallback->get() : nullptr;
  }
  if (const auto& container = node.interactions().hover_container) {
    using Kind = composition::Interactions::HoverContainer::Kind;
    scope.hover_container.reset();
    scope.hover_group = nullptr;
    if (container->kind == Kind::implicit) {
      scope.hover_container = node.id();
    } else if (container->kind == Kind::named) {
      scope.hover_group = &*container->group;
    }
  }
  if (rescale &&
      scope.theme.get().dynamic_type_size() != scope.dynamic_type_size) {
    scope.theme = sized(scope.theme.get(), scope.dynamic_type_size);
  }
  if (const auto contrast =
          node.override_of(components::Environment::color_scheme_contrast)) {
    scope.contrast = contrast->get();
  }
  if (const auto transparency = node.override_of(
          components::Environment::accessibility_reduce_transparency)) {
    scope.reduce_transparency = transparency->get();
  }
  if (const auto scheme =
          node.override_of(components::Environment::colour_scheme)) {
    if (scheme->get().has_value()) {
      scope.scheme = scheme->get();
    }
  }
  if (const auto locale = node.override_of(components::Environment::locale)) {
    scope.locale = canonical(locale->get());
  }
  if (const auto reduce =
          node.override_of(components::Environment::reduce_motion)) {
    scope.reduce_motion = reduce->get();
  }
  const composition::Modifiers& modifiers = node.modifiers();
  if (modifiers.foreground.has_value()) {
    scope.foreground = hierarchy(scope.foreground, *modifiers.foreground);
  }
  if (modifiers.background_style.has_value()) {
    scope.background_style = canonical_background(*modifiers.background_style);
  }
  if (modifiers.content_transition.has_value()) {
    scope.content_transition = modifiers.content_transition;
  }
  return scope;
}

auto Painter::sized(const theme::Theme& base, theme::DynamicTypeSize size)
    -> const theme::Theme& {
  const auto found = std::ranges::find_if(sized_, [&](const Sized& entry) {
    return entry.size == size && entry.base == &base;
  });
  if (found != sized_.end()) {
    return found->theme;
  }
  return sized_
      .emplace_back(
          Sized{.base = &base, .size = size, .theme = base.sized(size)})
      .theme;
}

auto Painter::canonical(const theme::Theme& theme) -> const theme::Theme& {
  const auto found =
      std::ranges::find_if(themes_, [&theme](const theme::Theme& held) {
        return &held == &theme || held == theme;
      });
  if (found != themes_.end()) {
    return *found;
  }
  return themes_.emplace_back(theme);
}

auto Painter::canonical(const text::Locale& locale) -> const text::Locale& {
  const auto found = std::ranges::find(locales_, locale);
  if (found != locales_.end()) {
    return *found;
  }
  return locales_.emplace_back(locale);
}

void Painter::forget_canonical() {
  if (hierarchies_.size() + backgrounds_.size() > style_limit) {
    hierarchies_.clear();
    backgrounds_.clear();
    ++generation_;
  }
  if (themes_.size() + locales_.size() + tints_.size() + sized_.size() <=
      canonical_limit) {
    return;
  }
  themes_.clear();
  locales_.clear();
  tints_.clear();
  sized_.clear();
  ++generation_;
}

auto Painter::backdrop(const composition::Node& root, Scope scope) -> Scope {
  scope = scoped(root, scope);
  const composition::Node* node = &root;
  while (node->child_count() > 0) {
    node = &*node->children().begin();
    scope = scoped(*node, scope);
    if (node->child_count() != 1) {
      break;
    }
  }
  return scope;
}

auto Painter::Affine::translation(float x, float y) noexcept -> Affine {
  return Affine{.a = 1.0F, .b = 0.0F, .c = 0.0F, .d = 1.0F, .tx = x, .ty = y};
}

auto Painter::Affine::scaling(float x, float y) noexcept -> Affine {
  return Affine{.a = x, .b = 0.0F, .c = 0.0F, .d = y, .tx = 0.0F, .ty = 0.0F};
}

auto Painter::Affine::rotation(double radians) noexcept -> Affine {
  const auto cosine = static_cast<float>(std::cos(radians));
  const auto sine = static_cast<float>(std::sin(radians));
  return Affine{
      .a = cosine, .b = sine, .c = -sine, .d = cosine, .tx = 0.0F, .ty = 0.0F};
}

auto Painter::Affine::around(float x, float y) const noexcept -> Affine {
  return translation(x, y).then(*this).then(translation(-x, -y));
}

auto Painter::Affine::then(const Affine& inner) const noexcept -> Affine {
  return Affine{.a = (a * inner.a) + (c * inner.b),
                .b = (b * inner.a) + (d * inner.b),
                .c = (a * inner.c) + (c * inner.d),
                .d = (b * inner.c) + (d * inner.d),
                .tx = (a * inner.tx) + (c * inner.ty) + tx,
                .ty = (b * inner.tx) + (d * inner.ty) + ty};
}

auto Painter::Affine::apply(std::array<float, 2> point) const noexcept
    -> std::array<float, 2> {
  return {(a * point[0]) + (c * point[1]) + tx,
          (b * point[0]) + (d * point[1]) + ty};
}

auto Painter::Affine::translating() const noexcept -> bool {
  constexpr float tolerance = 1.0e-4F;
  return std::abs(a - 1.0F) < tolerance && std::abs(d - 1.0F) < tolerance &&
         std::abs(b) < tolerance && std::abs(c) < tolerance;
}

auto Painter::Affine::identity() const noexcept -> bool {
  return a == 1.0F && b == 0.0F && c == 0.0F && d == 1.0F && tx == 0.0F &&
         ty == 0.0F;
}

auto Painter::anchored(motion::UnitPoint anchor, Box frame) noexcept
    -> std::array<float, 2> {
  return {frame.left + (anchor.x * frame.width()),
          frame.top + (anchor.y * frame.height())};
}

auto Painter::adopt(std::uint32_t parent, const Affine& local)
    -> std::uint32_t {
  if (local.identity()) {
    return parent;
  }
  transforms_.push_back(transforms_[parent].then(local));
  adoptions_.push_back(Adoption{.parent = parent, .local = local});
  return static_cast<std::uint32_t>(transforms_.size() - 1);
}

auto Painter::bounds(Box box, std::uint32_t transform) const noexcept -> Box {
  if (transform == 0) {
    return box;
  }
  const Affine& matrix = transforms_[transform];
  Box covered{.left = std::numeric_limits<float>::max(),
              .top = std::numeric_limits<float>::max(),
              .right = std::numeric_limits<float>::lowest(),
              .bottom = std::numeric_limits<float>::lowest()};
  for (const std::array<float, 2> corner :
       {std::array{box.left, box.top}, std::array{box.right, box.top},
        std::array{box.right, box.bottom}, std::array{box.left, box.bottom}}) {
    const std::array<float, 2> moved = matrix.apply(corner);
    covered.left = std::min(covered.left, moved[0]);
    covered.top = std::min(covered.top, moved[1]);
    covered.right = std::max(covered.right, moved[0]);
    covered.bottom = std::max(covered.bottom, moved[1]);
  }
  return covered;
}

auto Painter::mirrors(const composition::Modifiers& modifiers,
                      composition::LayoutDirection outer,
                      composition::LayoutDirection inner) noexcept -> bool {
  if (modifiers.layout_direction_behavior.has_value()) {
    return modifiers.layout_direction_behavior->mirrored_in(outer);
  }
  return modifiers.flips_for_right_to_left &&
         inner == composition::LayoutDirection::right_to_left;
}

auto Painter::transform_of(
    const composition::Modifiers& modifiers, motion::Presentation& presentation,
    const motion::Presentation::Moment& moment,
    const std::optional<motion::Transition::Effect>& effect, const Hover& hover,
    Box frame, float width, composition::LayoutDirection direction) -> Affine {
  using Property = motion::Presentation::Property;
  const composition::GeometryEffects& effects = modifiers.effects;
  const float scale = target_.scale.value();
  const motion::AnimatableVector scaled = presentation.animate(
      Property::scale,
      motion::AnimatableVector{effects.scale_x, effects.scale_y}, moment);
  const motion::AnimatableVector shifted = presentation.animate(
      Property::offset,
      motion::AnimatableVector{effects.offset_x, effects.offset_y}, moment);
  const motion::AnimatableVector turned = presentation.animate(
      Property::rotation, motion::AnimatableVector{effects.rotation.radians()},
      moment);
  auto shift_x = static_cast<float>(shifted[0]) + hover.offset_x;
  auto shift_y = static_cast<float>(shifted[1]) + hover.offset_y;
  if (modifiers.path.has_value()) {
    const motion::AnimatableVector travelled = presentation.animate(
        Property::path, motion::AnimatableVector{modifiers.path->progress},
        moment);
    const motion::MotionPath::Point point =
        modifiers.path->path.point(travelled[0]);
    shift_x += point.x;
    shift_y += point.y;
  }
  const float height = frame.height() / scale;
  if (effect.has_value()) {
    const float side =
        direction == composition::LayoutDirection::right_to_left ? -1.0F : 1.0F;
    shift_x += effect->offset_x + (effect->relative_x * width * side);
    shift_y += effect->offset_y + (effect->relative_y * height);
  }
  Affine local = Affine::translation(shift_x * scale, shift_y * scale);
  if (effect.has_value() &&
      (effect->scale_x != 1.0F || effect->scale_y != 1.0F)) {
    const std::array<float, 2> pivot = anchored(effect->anchor, frame);
    local = local.then(Affine::scaling(effect->scale_x, effect->scale_y)
                           .around(pivot[0], pivot[1]));
  }
  const float hover_x = hover.scale * hover.content_scale_x;
  const float hover_y = hover.scale * hover.content_scale_y;
  if (hover_x != 1.0F || hover_y != 1.0F) {
    const std::array<float, 2> pivot =
        anchored(motion::UnitPoint::center, frame);
    local = local.then(
        Affine::scaling(hover_x, hover_y).around(pivot[0], pivot[1]));
  }
  double rotation = turned[0] + hover.rotation;
  if (effect.has_value()) {
    rotation += effect->rotation;
  }
  if (rotation != 0.0) {
    const std::array<float, 2> pivot = anchored(effects.rotation_anchor, frame);
    local = local.then(Affine::rotation(rotation).around(pivot[0], pivot[1]));
  }
  if (scaled[0] != 1.0 || scaled[1] != 1.0) {
    const std::array<float, 2> pivot = anchored(effects.scale_anchor, frame);
    local = local.then(Affine::scaling(static_cast<float>(scaled[0]),
                                       static_cast<float>(scaled[1]))
                           .around(pivot[0], pivot[1]));
  }
  return local;
}

auto Painter::phased(composition::NodeId node, std::uint32_t index,
                     const composition::HoverEffectPhaseOverride& phase,
                     bool natural) -> bool {
  if (!phase.trigger().has_value()) {
    return phase.applied(natural);
  }
  PhaseMemory& memory = phases_[{node.value(), index}];
  if (!memory.trigger.has_value()) {
    memory.trigger = phase.trigger();
  } else if (*memory.trigger != *phase.trigger()) {
    memory.trigger = phase.trigger();
    memory.flipped = !memory.flipped;
    memory.changed = target_.time;
  }
  if (!phase.temporary()) {
    return memory.flipped ? !natural : natural;
  }
  if (memory.changed.has_value() &&
      target_.time - *memory.changed <
          composition::HoverEffectPhaseOverride::temporary_duration) {
    animating_ = true;
    unsettle();
    return phase.applied(natural);
  }
  return natural;
}

auto Painter::hover_active(const composition::Node& node, const Scope& scope,
                           const composition::CustomHoverEffect::Layer& layer,
                           const composition::CustomHoverEffect::Layer& origin,
                           std::uint32_t index) -> bool {
  const composition::HoverPath* path = highlights_.hovering.get();
  const bool own = path != nullptr && path->contains(node.id());
  const composition::HoverEffectGroup* group =
      layer.group.has_value()    ? &*layer.group
      : origin.group.has_value() ? &*origin.group
                                 : scope.hover_group;
  bool natural = own;
  if (group != nullptr && group->behavior().follows()) {
    natural = path != nullptr && path->active(*group);
  } else if (group == nullptr && scope.hover_container.has_value()) {
    natural = path != nullptr && path->active(*scope.hover_container);
  }
  natural = natural || highlighted(node.id());
  const std::optional<composition::HoverEffectPhaseOverride>& phase =
      layer.phase.has_value() ? layer.phase : origin.phase;
  if (phase.has_value()) {
    natural = phased(node.id(), index, *phase, natural);
  }
  return natural;
}

auto Painter::hover_of(const composition::Node& node, const Scope& scope,
                       Box frame, motion::Presentation& presentation,
                       const motion::Presentation::Moment& moment) -> Hover {
  using Property = motion::Presentation::Property;
  using Layer = composition::CustomHoverEffect::Layer;
  const std::optional<composition::CustomHoverEffect>& declared =
      node.modifiers().hover;
  if (!declared.has_value()) {
    return Hover{};
  }
  hover_nodes_.insert(node.id());
  if (scope.hover_disabled) {
    return Hover{};
  }
  const auto visit = [&](const auto& each) {
    std::uint32_t index = 0;
    for (const Layer& origin : declared->layers()) {
      const bool substitutes =
          scope.default_hover != nullptr &&
          origin.kind == Layer::Kind::system &&
          origin.system.kind() == motion::HoverEffect::Kind::automatic;
      if (!substitutes) {
        each(origin, origin, index++);
        continue;
      }
      for (const Layer& inherited : scope.default_hover->layers()) {
        each(inherited, origin, index++);
      }
    }
  };
  bool present = false;
  bool any = false;
  visit([&](const Layer& layer, const Layer& origin, std::uint32_t index) {
    if (!layer.enabled || !origin.enabled) {
      return;
    }
    if (layer.kind != Layer::Kind::group) {
      present = true;
    }
    any = hover_active(node, scope, layer, origin, index) || any;
  });
  if (!present) {
    return Hover{};
  }
  const theme::SpringToken token = scope.theme.get().motion().focus;
  const motion::Transaction springing{
      motion::Animation::spring(token.response, token.damping_ratio)};
  const motion::Presentation::Moment focusing{
      .transaction = springing,
      .now = moment.now,
      .reduce_motion = moment.reduce_motion};
  const auto lifted = static_cast<float>(presentation.animate(
      Property::focus, motion::AnimatableVector{any ? 1.0 : 0.0}, focusing)[0]);
  const bool focused = highlighted(node.id());
  Hover hover{.lifted = lifted, .present = true};
  std::optional<composition::GeometryProxy> proxy;
  bool tilts = false;
  motion::Angle tilt_angle{};
  float tilt_shift = 0.0F;
  visit([&](const Layer& layer, const Layer& origin, std::uint32_t index) {
    if (!layer.enabled || !origin.enabled || layer.kind == Layer::Kind::group) {
      return;
    }
    const bool active = hover_active(node, scope, layer, origin, index);
    const float weight = active || !any ? lifted : 0.0F;
    if (layer.kind == Layer::Kind::system) {
      if (layer.system.lifts()) {
        hover.lift = std::max(hover.lift, weight);
      } else {
        hover.highlight = std::max(hover.highlight, weight);
      }
      if (layer.system.tilts()) {
        tilts = true;
        tilt_angle = layer.system.tilt_angle();
        tilt_shift = layer.system.tilt_shift();
      }
      return;
    }
    if (layer.body == nullptr) {
      return;
    }
    if (!proxy.has_value()) {
      proxy.emplace(
          scroll_proxy(bounds(frame, scope.transform), scope.viewport));
    }
    const composition::VisualEffect resting =
        (*layer.body)(composition::VisualEffect{}, false, *proxy);
    const composition::VisualEffect raised =
        (*layer.body)(composition::VisualEffect{}, true, *proxy);
    const auto mix = [weight](float from, float to) {
      return from + ((to - from) * weight);
    };
    hover.offset_x += mix(resting.offset_x(), raised.offset_x());
    hover.offset_y += mix(resting.offset_y(), raised.offset_y());
    hover.content_scale_x *= mix(resting.scale_x(), raised.scale_x());
    hover.content_scale_y *= mix(resting.scale_y(), raised.scale_y());
    hover.rotation += static_cast<double>(
        mix(static_cast<float>(resting.rotation().radians()),
            static_cast<float>(raised.rotation().radians())));
    hover.opacity *= std::clamp(
        mix(resting.opacity_value(), raised.opacity_value()), 0.0F, 1.0F);
  });
  hover.scale = 1.0F + (motion::HoverEffect::lift_scale * hover.lift);
  if (tilts) {
    float stick_x = 0.0F;
    float stick_y = 0.0F;
    if (focused) {
      stick_x = highlights_.tilt.x;
      stick_y = highlights_.tilt.y;
      const float length = std::hypot(stick_x, stick_y);
      if (length > 1.0F) {
        stick_x /= length;
        stick_y /= length;
      }
    }
    const motion::AnimatableVector tilted = presentation.animate(
        Property::tilt, motion::AnimatableVector{stick_x, stick_y}, focusing);
    hover.rotation += tilted[0] * tilt_angle.radians();
    hover.offset_x += static_cast<float>(tilted[0]) * tilt_shift;
    hover.offset_y -= static_cast<float>(tilted[1]) * tilt_shift;
  }
  return hover;
}

auto Painter::highlighted(composition::NodeId node) const noexcept -> bool {
  return highlights_.focus_visible && highlights_.focused == node;
}

auto Painter::shaken(composition::NodeId node) -> std::array<float, 2> {
  if (!highlights_.shake.has_value() || highlights_.shake->node != node) {
    return {0.0F, 0.0F};
  }
  const std::chrono::nanoseconds elapsed =
      target_.time - highlights_.shake->started;
  const std::chrono::nanoseconds span = shake_duration;
  if (elapsed < std::chrono::nanoseconds{0} || elapsed >= span) {
    return {0.0F, 0.0F};
  }
  animating_ = true;
  unhint(motion::UnhintedAnimation{
      .route = motion::UnhintedAnimation::Route::shake, .node = node.value()});
  unsettle();
  const double progress = std::chrono::duration<double>(elapsed) /
                          std::chrono::duration<double>(span);
  const double wave =
      std::sin(progress * shake_cycles * 2.0 * std::numbers::pi_v<double>) *
      (1.0 - progress);
  const auto amount = static_cast<float>(wave) * shake_amplitude;
  switch (highlights_.shake->direction) {
    case composition::MoveCommandDirection::left:
      return {-amount, 0.0F};
    case composition::MoveCommandDirection::right:
      return {amount, 0.0F};
    case composition::MoveCommandDirection::up:
      return {0.0F, -amount};
    case composition::MoveCommandDirection::down:
      return {0.0F, amount};
  }
  return {0.0F, 0.0F};
}

auto Painter::points_of(Box box) const noexcept -> composition::Rect {
  const float scale = target_.scale.value();
  return composition::Rect{.x = box.left / scale,
                           .y = box.top / scale,
                           .width = box.width() / scale,
                           .height = box.height() / scale};
}

void Painter::record(composition::Tree& tree, const Presence& presence) {
  presences_.push_back(presence);
  const Box& frame = presence.frame;
  const provider::ClipRect& clip = presence.clip;
  const float scale = target_.scale.value();
  const Affine& matrix = transforms_[presence.transform];
  composition::PresentedFrame presented;
  presented.clip =
      composition::Rect{.x = static_cast<float>(clip.x) / scale,
                        .y = static_cast<float>(clip.y) / scale,
                        .width = static_cast<float>(clip.width) / scale,
                        .height = static_cast<float>(clip.height) / scale};
  presented.hit_radius =
      presence.hit_radius *
      std::sqrt(std::abs((matrix.a * matrix.d) - (matrix.b * matrix.c)));
  float left = std::numeric_limits<float>::max();
  float top = std::numeric_limits<float>::max();
  float right = std::numeric_limits<float>::lowest();
  float bottom = std::numeric_limits<float>::lowest();
  const std::array corners{std::array{frame.left, frame.top},
                           std::array{frame.right, frame.top},
                           std::array{frame.right, frame.bottom},
                           std::array{frame.left, frame.bottom}};
  for (std::size_t index = 0; index < corners.size(); ++index) {
    const std::array<float, 2> moved = matrix.apply(corners[index]);
    const composition::Point point{.x = moved[0] / scale,
                                   .y = moved[1] / scale};
    presented.corners[index] = point;
    left = std::min(left, point.x);
    top = std::min(top, point.y);
    right = std::max(right, point.x);
    bottom = std::max(bottom, point.y);
  }
  presented.bounds = composition::Rect{
      .x = left, .y = top, .width = right - left, .height = bottom - top};
  presented.visible = true;
  presented.hollow = presence.hollow;
  if (presence.drawn.has_value()) {
    const Box& drawn = *presence.drawn;
    const std::array inner{std::array{drawn.left, drawn.top},
                           std::array{drawn.right, drawn.top},
                           std::array{drawn.right, drawn.bottom},
                           std::array{drawn.left, drawn.bottom}};
    std::array<composition::Point, 4> quad{};
    for (std::size_t index = 0; index < inner.size(); ++index) {
      const std::array<float, 2> moved = matrix.apply(inner[index]);
      quad[index] =
          composition::Point{.x = moved[0] / scale, .y = moved[1] / scale};
    }
    presented.drawn = quad;
  }
  presented.opacity = presence.opacity * fades_[presence.fade].value;
  if (presence.shaped.has_value()) {
    presented.clip_shape = presented_clip(tree, presence, presented);
  }
  if (presence.hit_path.has_value()) {
    presented.hit_shape =
        composition::PresentedClip{.corners = presented.corners,
                                   .radii = {},
                                   .ellipse = false,
                                   .shape = presence.hit_path,
                                   .fill = presence.hit_fill};
  }
  tree.present(presence.node, presented);
}

void Painter::conceal(composition::Tree& tree, const composition::Node& node) {
  const composition::Rect& laid = node.frame();
  tree.present(
      node.id(),
      composition::PresentedFrame{
          .corners = {composition::Point{.x = laid.x, .y = laid.y},
                      composition::Point{.x = laid.x + laid.width, .y = laid.y},
                      composition::Point{.x = laid.x + laid.width,
                                         .y = laid.y + laid.height},
                      composition::Point{.x = laid.x,
                                         .y = laid.y + laid.height}},
          .bounds = laid,
          .visible = false,
          .clip = std::nullopt,
          .hit_radius = 0.0F,
          .opacity = 0.0F});
}

auto Painter::phase_of(
    Box rect, const Viewport& viewport,
    const composition::ScrollEffects::ScrollTransition& transition) noexcept
    -> double {
  const bool vertical =
      transition.axis.has_value()
          ? *transition.axis == composition::Axis::vertical
          : viewport.axes.contains(composition::Axis::vertical);
  const float lower = vertical ? viewport.screen.top : viewport.screen.left;
  const float upper = vertical ? viewport.screen.bottom : viewport.screen.right;
  const float start = (vertical ? rect.top : rect.left) - lower;
  const float end = (vertical ? rect.bottom : rect.right) - lower;
  return transition.progress(start, end, upper - lower);
}

auto Painter::scroll_proxy(Box screen,
                           const std::optional<Viewport>& viewport) const
    -> composition::GeometryProxy {
  if (!viewport.has_value()) {
    return composition::GeometryProxy{points_of(screen), std::nullopt};
  }
  const composition::Rect area = points_of(viewport->screen);
  composition::GeometryProxy::ScrollViews along;
  if (viewport->axes.contains(composition::Axis::horizontal)) {
    along.horizontal = area;
  }
  if (viewport->axes.contains(composition::Axis::vertical)) {
    along.vertical = area;
  }
  return composition::GeometryProxy{
      points_of(screen), area,    along,
      nullptr,           nullptr, composition::LayoutDirection::left_to_right};
}

auto Painter::effect_transform(const composition::VisualEffect& effect,
                               Box frame) const -> Affine {
  const float scale = target_.scale.value();
  Affine local =
      Affine::translation(effect.offset_x() * scale, effect.offset_y() * scale);
  if (effect.rotation().radians() != 0.0) {
    const std::array<float, 2> pivot =
        anchored(effect.rotation_anchor(), frame);
    local = local.then(Affine::rotation(effect.rotation().radians())
                           .around(pivot[0], pivot[1]));
  }
  if (effect.scale_x() != 1.0F || effect.scale_y() != 1.0F) {
    const std::array<float, 2> pivot = anchored(effect.scale_anchor(), frame);
    local = local.then(Affine::scaling(effect.scale_x(), effect.scale_y())
                           .around(pivot[0], pivot[1]));
  }
  return local;
}

auto Painter::pinned(const composition::ScrollEffects::Pin& pin,
                     const Scope& scope, Box screen,
                     const composition::Rect& within) const -> float {
  if (!scope.viewport.has_value()) {
    return 0.0F;
  }
  const float scale = target_.scale.value();
  const Box area = bounds(Box{.left = within.x * scale,
                              .top = within.y * scale,
                              .right = (within.x + within.width) * scale,
                              .bottom = (within.y + within.height) * scale},
                          scope.transform);
  const Box& view = scope.viewport->screen;
  const bool vertical = pin.axis == composition::Axis::vertical;
  const float near_view = vertical ? view.top : view.left;
  const float far_view = vertical ? view.bottom : view.right;
  const float near_self = vertical ? screen.top : screen.left;
  const float far_self = vertical ? screen.bottom : screen.right;
  const float near_area = vertical ? area.top : area.left;
  const float far_area = vertical ? area.bottom : area.right;
  if (pin.trailing) {
    return std::max(std::min(far_view - far_self, 0.0F),
                    std::min(near_area - near_self, 0.0F)) /
           scale;
  }
  return std::min(std::max(near_view - near_self, 0.0F),
                  std::max(far_area - far_self, 0.0F)) /
         scale;
}

auto Painter::shading(const composition::Node& node, const Scope& scope,
                      motion::Presentation& presentation,
                      const motion::Presentation::Moment& moment, Box frame,
                      std::optional<composition::Rect> within) const
    -> std::optional<Shading> {
  using Mode = composition::ScrollTransitionConfiguration::Mode;
  const composition::ScrollEffects& effects = node.interactions().effects;
  if (effects.empty()) {
    return std::nullopt;
  }
  const Box screen = bounds(frame, scope.transform);
  composition::VisualEffect effect;
  for (const composition::ScrollEffects::ScrollTransition& transition :
       effects.transitions) {
    double progress = 0.0;
    if (scope.viewport.has_value()) {
      progress = phase_of(screen, *scope.viewport, transition);
    }
    const composition::ScrollTransitionConfiguration& side =
        transition.side(progress);
    double value = transition.shaped(progress);
    if (side.mode() == Mode::animated) {
      const motion::Transaction animated{side.animation()};
      const motion::Presentation::Moment timing{
          .transaction = animated,
          .now = moment.now,
          .reduce_motion = moment.reduce_motion};
      value = presentation.animate(motion::Presentation::Property::scroll_phase,
                                   motion::AnimatableVector{value}, timing)[0];
    }
    effect = transition.transition(
        effect, composition::ScrollTransitionPhase::at(value));
  }
  if (!effects.effects.empty()) {
    const composition::GeometryProxy proxy =
        scroll_proxy(screen, scope.viewport);
    for (const composition::ScrollEffects::Effect& visual : effects.effects) {
      effect = visual(effect, proxy);
    }
  }
  if (effects.pin.has_value() && within.has_value()) {
    const float shift = pinned(*effects.pin, scope, screen, *within);
    effect = effects.pin->axis == composition::Axis::vertical
                 ? effect.offset(0.0F, shift)
                 : effect.offset(shift, 0.0F);
  }
  const composition::Size size{
      .width = frame.width() / target_.scale.value(),
      .height = frame.height() / target_.scale.value()};
  graphics::ProjectionTransform projection =
      effect.rotation_3d().has_value()
          ? effect.rotation_3d()->projection(size)
          : graphics::ProjectionTransform::identity();
  projection = projection.concatenating(effect.projection());
  return Shading{.transform = effect_transform(effect, frame),
                 .opacity = effect.opacity_value(),
                 .colour = effect.colour_matrix(),
                 .blur = effect.blur_radius(),
                 .opaque = effect.blur_opaque(),
                 .blend = effect.blend(),
                 .projection = projection,
                 .shader = effect.shader(),
                 .sample_offset = effect.sample_offset()};
}

void Painter::edge_effects(const composition::Node& node, const Scope& scope,
                           Box viewport) {
  const std::optional<composition::Size>& content = node.content_size();
  if (scope.edge_effects == nullptr || scope.edge_effects->empty() ||
      !content.has_value()) {
    return;
  }
  const float scale = target_.scale.value();
  const composition::Point offset = node.scroll_presentation().offset;
  const float width = viewport.width() / scale;
  const float height = viewport.height() / scale;
  const theme::Colour ground = scope.colour(theme::ColourRole::background);
  const theme::Colour rule = scope.colour(theme::ColourRole::separator);
  const provider::ClipRect clip =
      intersect(scope.clip, bounds(viewport, scope.transform));
  const float spill = std::max(viewport.width(), viewport.height());
  const auto band = [&](theme::Edge edge, bool scrolled) {
    const std::optional<composition::ScrollEdgeEffectStyle> style =
        scope.edge_effects->at(edge);
    if (!style.has_value() || !scrolled) {
      return;
    }
    const bool soft = *style == composition::ScrollEdgeEffectStyle::soft;
    const float depth = (soft ? composition::ScrollEdgeEffects::soft_depth
                              : composition::ScrollEdgeEffects::hard_depth) *
                        scale;
    const float half = depth * 0.5F;
    Box shade = viewport;
    Box line = viewport;
    switch (edge) {
      case theme::Edge::top:
        shade = Box{.left = viewport.left - spill,
                    .top = viewport.top - spill,
                    .right = viewport.right + spill,
                    .bottom = viewport.top + half};
        line.bottom = line.top + 1.0F;
        break;
      case theme::Edge::bottom:
        shade = Box{.left = viewport.left - spill,
                    .top = viewport.bottom - half,
                    .right = viewport.right + spill,
                    .bottom = viewport.bottom + spill};
        line.top = line.bottom - 1.0F;
        break;
      case theme::Edge::leading:
        shade = Box{.left = viewport.left - spill,
                    .top = viewport.top - spill,
                    .right = viewport.left + half,
                    .bottom = viewport.bottom + spill};
        line.right = line.left + 1.0F;
        break;
      default:
        shade = Box{.left = viewport.right - half,
                    .top = viewport.top - spill,
                    .right = viewport.right + spill,
                    .bottom = viewport.bottom + spill};
        line.left = line.right - 1.0F;
        break;
    }
    shape(provider::ShapeKind::shadow, shade, Corners{}, 0.0F,
          std::max(half, 0.5F), pack(ground, scope.opacity), clip);
    if (!soft) {
      shape(provider::ShapeKind::fill, line, Corners{}, 0.0F, 0.0F,
            pack(rule, scope.opacity), clip);
    }
  };
  constexpr float settled = 0.5F;
  band(theme::Edge::top, offset.y > settled);
  band(theme::Edge::bottom, offset.y < content->height - height - settled);
  band(theme::Edge::leading, offset.x > settled);
  band(theme::Edge::trailing, offset.x < content->width - width - settled);
}

void Painter::indicators(const composition::Node& node, const Scope& scope,
                         Box viewport) {
  const auto& container = node.interactions().scrolling.container;
  const std::optional<composition::Size>& content = node.content_size();
  const float alpha = node.scroll_presentation().indicators;
  if (!container.has_value() || !container->shows_indicators ||
      !content.has_value() || alpha <= visible_threshold) {
    return;
  }
  const float scale = target_.scale.value();
  const composition::Point offset = node.scroll_presentation().offset;
  const float thickness = indicator_thickness * scale;
  const float inset = indicator_inset * scale;
  const std::uint32_t colour =
      pack(scope.colour(theme::ColourRole::text_secondary),
           scope.opacity * alpha * indicator_opacity);
  const auto bar = [&](float length, float extent, float travel,
                       float position) -> std::array<float, 2> {
    const float range = std::max(extent - length, 1.0F);
    float size =
        std::max(length * length / std::max(extent, 1.0F), indicator_minimum);
    float overshoot = 0.0F;
    if (position < 0.0F) {
      overshoot = -position;
    } else if (position > range) {
      overshoot = position - range;
    }
    size = std::max(size - overshoot, thickness / scale);
    const float room = travel - size;
    const float start =
        std::clamp(position / range, 0.0F, 1.0F) * std::max(room, 0.0F);
    return {start, size};
  };
  const float width = viewport.width() / scale;
  const float height = viewport.height() / scale;
  const std::optional<theme::Insets>& margins =
      node.interactions().scrolling.indicator_margins;
  const theme::Edges margined =
      margins.has_value() ? scope.theme.get().edges(*margins).positive()
                          : theme::Edges{};
  const bool mirrored =
      scope.direction == composition::LayoutDirection::right_to_left;
  const Box track{
      .left = viewport.left +
              ((mirrored ? margined.trailing : margined.leading) * scale),
      .top = viewport.top + (margined.top * scale),
      .right = viewport.right -
               ((mirrored ? margined.leading : margined.trailing) * scale),
      .bottom = viewport.bottom - (margined.bottom * scale)};
  const composition::Axes shown = node.scroll_presentation().indicator_axes;
  if (container->axes.contains(composition::Axis::vertical) &&
      shown.contains(composition::Axis::vertical) &&
      content->height > height + 0.5F) {
    const std::array<float, 2> placed =
        bar(height, content->height,
            (track.height() / scale) - (2.0F * indicator_inset), offset.y);
    const float top = track.top + inset + (placed[0] * scale);
    shape(provider::ShapeKind::fill,
          Box{.left = track.right - inset - thickness,
              .top = top,
              .right = track.right - inset,
              .bottom = top + (placed[1] * scale)},
          Corners{.radius = thickness * 0.5F, .smoothing = 0.0F}, 0.0F, 0.0F,
          colour, scope.clip);
  }
  if (container->axes.contains(composition::Axis::horizontal) &&
      shown.contains(composition::Axis::horizontal) &&
      content->width > width + 0.5F) {
    const std::array<float, 2> placed =
        bar(width, content->width,
            (track.width() / scale) - (2.0F * indicator_inset), offset.x);
    const float left = track.left + inset + (placed[0] * scale);
    shape(provider::ShapeKind::fill,
          Box{.left = left,
              .top = track.bottom - inset - thickness,
              .right = left + (placed[1] * scale),
              .bottom = track.bottom - inset},
          Corners{.radius = thickness * 0.5F, .smoothing = 0.0F}, 0.0F, 0.0F,
          colour, scope.clip);
  }
}

auto Painter::follow(const composition::Tree& tree,
                     const composition::Node& node)
    -> std::optional<composition::Rect> {
  if (const std::optional<composition::MatchedGeometry::Key>& zoom =
          node.zoom_source();
      zoom.has_value() &&
      node.phase() != composition::NodeLifecyclePhase::live) {
    const std::optional<composition::NodeId> source =
        tree.matched_source(*zoom);
    if (!source.has_value()) {
      return std::nullopt;
    }
    const auto found = tree.node(*source);
    if (!found.has_value()) {
      return std::nullopt;
    }
    return found->get().frame();
  }
  const std::optional<composition::MatchedGeometry>& matched =
      node.matched_geometry();
  if (!matched.has_value() || matched->is_source) {
    return std::nullopt;
  }
  const std::optional<composition::NodeId> source =
      tree.matched_source(matched->key);
  if (!source.has_value()) {
    return std::nullopt;
  }
  const auto found = tree.node(*source);
  if (!found.has_value()) {
    return std::nullopt;
  }
  const composition::Rect& own = node.frame();
  const composition::Rect& theirs = found->get().frame();
  const motion::AnimatableVector placed = motion::Presentation::matched(
      motion::AnimatableVector{theirs.x, theirs.y, theirs.width, theirs.height},
      motion::AnimatableVector{own.x, own.y, own.width, own.height},
      matched->properties, matched->anchor);
  return composition::Rect{.x = static_cast<float>(placed[0]),
                           .y = static_cast<float>(placed[1]),
                           .width = static_cast<float>(placed[2]),
                           .height = static_cast<float>(placed[3])};
}

auto Painter::carry(Box laid, Box shown, motion::UnitPoint anchor) noexcept
    -> Affine {
  if (laid.width() <= 0.0F || laid.height() <= 0.0F) {
    return Affine{};
  }
  const float factor =
      std::min(shown.width() / laid.width(), shown.height() / laid.height());
  const std::array<float, 2> from = anchored(anchor, laid);
  const std::array<float, 2> to = anchored(anchor, shown);
  return Affine::translation(to[0], to[1])
      .then(Affine::scaling(factor, factor))
      .then(Affine::translation(-from[0], -from[1]));
}

auto Painter::tinted(theme::Colour colour, theme::Colour toward,
                     float amount) noexcept -> theme::Colour {
  const theme::Oklab from = theme::Oklab::from(colour);
  const theme::Oklab to = theme::Oklab::from(toward);
  const float kept = 1.0F - amount;
  return theme::Oklab{(from.lightness() * kept) + (to.lightness() * amount),
                      (from.a() * kept) + (to.a() * amount),
                      (from.b() * kept) + (to.b() * amount)}
      .to_colour(colour.alpha);
}

auto Painter::faded(std::uint32_t colour, float alpha) noexcept
    -> std::uint32_t {
  const float kept = std::clamp(alpha, 0.0F, 1.0F);
  const auto level = static_cast<std::uint32_t>(
      std::lround(static_cast<float>(colour >> 24U) * kept));
  return (colour & 0x00FFFFFFU) | (level << 24U);
}

void Painter::settle(composition::Tree& tree, const composition::Node& node) {
  tree.presentation(node).settle(target_.time);
  for (const composition::Node& child : node.children()) {
    settle(tree, child);
  }
}

auto Painter::pixels_of(const composition::Node& node) const noexcept -> Box {
  const float scale = target_.scale.value();
  const composition::Rect& frame = node.frame();
  return Box{.left = frame.x * scale,
             .top = frame.y * scale,
             .right = (frame.x + frame.width) * scale,
             .bottom = (frame.y + frame.height) * scale};
}

void Painter::shape(provider::ShapeKind kind, Box area, Corners shape_corners,
                    float stroke, float softness, std::uint32_t colour,
                    const provider::ClipRect& clip) {
  if (area.width() <= 0.0F || area.height() <= 0.0F || clip.width == 0 ||
      clip.height == 0 || (colour >> 24U) == 0) {
    return;
  }
  const float margin =
      kind == provider::ShapeKind::shadow ? softness + 1.0F : 1.0F;
  quads_.push_back(
      Quad{.kind = kind,
           .bounds = Box{.left = area.left - margin,
                         .top = area.top - margin,
                         .right = area.right + margin,
                         .bottom = area.bottom + margin},
           .uv = {},
           .centre = {(area.left + area.right) * 0.5F,
                      (area.top + area.bottom) * 0.5F},
           .half_size = {area.width() * 0.5F, area.height() * 0.5F},
           .shape = {shape_corners.radius, shape_corners.smoothing, stroke,
                     softness},
           .colour = colour,
           .texture = std::nullopt,
           .clip = clip,
           .transform = transform_,
           .detail = detail_of(shape_corners)});
}

void Painter::picture(const std::shared_ptr<const provider::Image>& pixels,
                      Box area, std::uint32_t colour,
                      const provider::ClipRect& clip, std::array<float, 4> uv,
                      components::Image::Rendering rendering) {
  if (pixels == nullptr || area.width() <= 0.0F || area.height() <= 0.0F ||
      clip.width == 0 || clip.height == 0) {
    return;
  }
  const PictureKey key{
      .image = pixels.get(),
      .mask =
          rendering.mode == components::Image::TemplateRenderingMode::template_,
      .filter =
          rendering.interpolation == components::Image::Interpolation::none
              ? provider::SamplerFilter::nearest
              : provider::SamplerFilter::linear,
      .level = reduction(*pixels, area, uv, rendering.interpolation)};
  auto found = pictures_.find(key);
  if (found != pictures_.end() && found->second.source.lock() != pixels) {
    picture_bytes_ -= found->second.bytes;
    pictures_.erase(found);
    found = pictures_.end();
  }
  const bool created = found == pictures_.end();
  if (created) {
    const std::optional<provider::Image> smaller =
        key.level > 0 ? reduced(*pixels, key.level) : std::nullopt;
    const provider::Image& source = smaller.has_value() ? *smaller : *pixels;
    auto texture = gpu_.get().create_texture(
        provider::TextureDescription{.width = source.width(),
                                     .height = source.height(),
                                     .format = provider::TextureFormat::rgba8,
                                     .usage = provider::TextureUsage::sampled,
                                     .filter = key.filter});
    if (!texture.has_value()) {
      refuse_picture("create", source, key.level, texture.error());
      return;
    }
    const provider::TextureRegion whole{
        .x = 0, .y = 0, .width = source.width(), .height = source.height()};
    const std::vector<std::byte> mask =
        key.mask ? masked(source) : std::vector<std::byte>{};
    if (auto uploaded = gpu_.get().update_texture(
            *texture, whole,
            provider::PixelRows{key.mask ? std::span{mask} : source.pixels()});
        !uploaded.has_value()) {
      refuse_picture("upload", source, key.level, uploaded.error());
      return;
    }
    const std::uint64_t bytes = std::uint64_t{source.width()} *
                                source.height() * provider::Image::channels;
    found = pictures_
                .emplace(key, Picture{.source = pixels,
                                      .texture = std::move(*texture),
                                      .idle = 0,
                                      .bytes = bytes,
                                      .painted = picture_frame_})
                .first;
    picture_bytes_ += bytes;
    ++picture_uploads_;
  }
  found->second.idle = 0;
  found->second.painted = picture_frame_;
  sample(found->second.texture.id(), area, colour, clip, created, uv);
}

auto Painter::reduction(const provider::Image& pixels, Box area,
                        std::array<float, 4> uv,
                        components::Image::Interpolation interpolation) noexcept
    -> std::uint8_t {
  if (interpolation != components::Image::Interpolation::medium &&
      interpolation != components::Image::Interpolation::high) {
    return 0;
  }
  const float wide =
      static_cast<float>(pixels.width()) * std::abs(uv[2] - uv[0]);
  const float tall =
      static_cast<float>(pixels.height()) * std::abs(uv[3] - uv[1]);
  float ratio = std::min(wide / std::max(area.width(), 1.0F),
                         tall / std::max(area.height(), 1.0F));
  std::uint8_t level = 0;
  while (ratio >= 2.0F && level < maximum_reduction) {
    ratio *= 0.5F;
    ++level;
  }
  return level;
}

auto Painter::reduced(const provider::Image& pixels, std::uint8_t level)
    -> std::optional<provider::Image> {
  std::vector<std::array<std::uint32_t, 2>> sizes{
      {pixels.width(), pixels.height()}};
  while (sizes.size() <= level &&
         (sizes.back()[0] > 1 || sizes.back()[1] > 1)) {
    sizes.push_back(
        {std::max(sizes.back()[0] / 2, 1U), std::max(sizes.back()[1] / 2, 1U)});
  }
  const auto [width, height] = sizes.back();
  std::vector<std::byte> straight(std::size_t{width} * height * 4);
  for (std::uint32_t y = 0; y < height; ++y) {
    for (std::uint32_t x = 0; x < width; ++x) {
      const std::array<float, 4> mean =
          averaged(pixels, sizes, sizes.size() - 1, x, y);
      const float alpha = mean[3];
      const std::size_t at = ((std::size_t{y} * width) + x) * 4;
      for (std::size_t channel = 0; channel < 3; ++channel) {
        const float value = alpha > 0.0F ? mean[channel] / alpha : 0.0F;
        straight[at + channel] = static_cast<std::byte>(
            std::lround(std::clamp(value, 0.0F, 1.0F) * 255.0F));
      }
      straight[at + 3] = static_cast<std::byte>(
          std::lround(std::clamp(alpha, 0.0F, 1.0F) * 255.0F));
    }
  }
  auto made = provider::Image::create(width, height, std::move(straight));
  if (!made.has_value()) {
    return std::nullopt;
  }
  return std::move(*made);
}

auto Painter::averaged(const provider::Image& pixels,
                       std::span<const std::array<std::uint32_t, 2>> sizes,
                       std::size_t depth, std::uint32_t x,
                       std::uint32_t y) noexcept -> std::array<float, 4> {
  if (depth == 0) {
    const std::span<const std::byte> bytes = pixels.pixels();
    const std::size_t at = ((std::size_t{y} * sizes[0][0]) + x) * 4;
    const float alpha = std::to_integer<std::uint32_t>(bytes[at + 3]) / 255.0F;
    return {std::to_integer<std::uint32_t>(bytes[at]) / 255.0F * alpha,
            std::to_integer<std::uint32_t>(bytes[at + 1]) / 255.0F * alpha,
            std::to_integer<std::uint32_t>(bytes[at + 2]) / 255.0F * alpha,
            alpha};
  }
  const auto [width, height] = sizes[depth - 1];
  std::array<float, 4> sum{};
  for (const std::uint32_t dy : {0U, 1U}) {
    for (const std::uint32_t dx : {0U, 1U}) {
      const std::array<float, 4> part =
          averaged(pixels, sizes, depth - 1, std::min((x * 2) + dx, width - 1),
                   std::min((y * 2) + dy, height - 1));
      for (std::size_t channel = 0; channel < 4; ++channel) {
        sum[channel] += part[channel];
      }
    }
  }
  for (float& channel : sum) {
    channel /= 4.0F;
  }
  return sum;
}

auto Painter::masked(const provider::Image& pixels) -> std::vector<std::byte> {
  std::vector<std::byte> mask(pixels.pixels().size());
  const std::span<const std::byte> source = pixels.pixels();
  for (std::size_t at = 0; at + provider::Image::channels <= mask.size();
       at += provider::Image::channels) {
    mask[at] = std::byte{0xFF};
    mask[at + 1] = std::byte{0xFF};
    mask[at + 2] = std::byte{0xFF};
    mask[at + 3] = source[at + 3];
  }
  return mask;
}

void Painter::paint_raster(const components::Image::Raster::Content& raster,
                           const Scope& scope, Box area) {
  if (scope.redacts()) {
    redact_box(scope, area);
    return;
  }
  const std::shared_ptr<const provider::Image>& pixels = raster.pixels();
  const bool template_mode =
      raster.rendering().mode ==
      components::Image::TemplateRenderingMode::template_;
  const theme::Colour tint =
      template_mode ? scope.colour(ink(scope, theme::ColourRole::text_primary))
                    : theme::Colour{1.0F, 1.0F, 1.0F, 1.0F};
  const bool fits =
      raster.mode() == components::Image::ContentMode::fit && pixels != nullptr;
  const bool fills = raster.mode() == components::Image::ContentMode::fill &&
                     pixels != nullptr && area.height() > 0.0F;
  const Box shown = raster.rendering().antialiased ? area : pixel_snapped(area);
  if (raster.slicing().has_value() && pixels != nullptr) {
    paint_slices(pixels, *raster.slicing(), shown, pack(tint, scope.opacity),
                 scope.clip, raster.rendering());
    return;
  }
  picture(pixels, fits ? fitted_box(*pixels, shown) : shown,
          pack(tint, scope.opacity), scope.clip,
          fills ? filled_uv(*pixels, shown) : whole_texture,
          raster.rendering());
}

auto Painter::pixel_snapped(Box area) noexcept -> Box {
  return Box{.left = std::round(area.left),
             .top = std::round(area.top),
             .right = std::round(area.right),
             .bottom = std::round(area.bottom)};
}

void Painter::tile_picture(const std::shared_ptr<const provider::Image>& pixels,
                           Box area, std::array<float, 4> uv, float tile_width,
                           float tile_height, std::uint32_t colour,
                           const provider::ClipRect& clip,
                           components::Image::Rendering rendering) {
  if (area.width() <= 0.0F || area.height() <= 0.0F || tile_width < 1.0F ||
      tile_height < 1.0F) {
    return;
  }
  const float across = uv[2] - uv[0];
  const float down = uv[3] - uv[1];
  for (float top = area.top; top < area.bottom; top += tile_height) {
    const float bottom = std::min(top + tile_height, area.bottom);
    const float tall = (bottom - top) / tile_height;
    for (float left = area.left; left < area.right; left += tile_width) {
      const float right = std::min(left + tile_width, area.right);
      const float wide = (right - left) / tile_width;
      picture(pixels,
              Box{.left = left, .top = top, .right = right, .bottom = bottom},
              colour, clip,
              {uv[0], uv[1], uv[0] + (across * wide), uv[1] + (down * tall)},
              rendering);
    }
  }
}

void Painter::paint_slices(const std::shared_ptr<const provider::Image>& pixels,
                           const components::Image::Slicing& slicing, Box area,
                           std::uint32_t colour, const provider::ClipRect& clip,
                           components::Image::Rendering rendering) {
  const auto width = static_cast<float>(pixels->width());
  const auto height = static_cast<float>(pixels->height());
  if (width <= 0.0F || height <= 0.0F) {
    return;
  }
  const float target = target_.scale.value();
  const float density = slicing.scale > 0.0F ? slicing.scale : 1.0F;
  const auto [top, leading, bottom, trailing] = slicing.insets;
  const float source_top = std::clamp(top * density, 0.0F, height);
  const float source_bottom =
      std::clamp(bottom * density, 0.0F, height - source_top);
  const float source_left = std::clamp(leading * density, 0.0F, width);
  const float source_right =
      std::clamp(trailing * density, 0.0F, width - source_left);
  const float to_target = target / density;
  float edge_top = source_top * to_target;
  float edge_bottom = source_bottom * to_target;
  float edge_left = source_left * to_target;
  float edge_right = source_right * to_target;
  if (edge_top + edge_bottom > area.height() && edge_top + edge_bottom > 0.0F) {
    const float shrink = area.height() / (edge_top + edge_bottom);
    edge_top *= shrink;
    edge_bottom *= shrink;
  }
  if (edge_left + edge_right > area.width() && edge_left + edge_right > 0.0F) {
    const float shrink = area.width() / (edge_left + edge_right);
    edge_left *= shrink;
    edge_right *= shrink;
  }
  const std::array<float, 4> xs{area.left, area.left + edge_left,
                                area.right - edge_right, area.right};
  const std::array<float, 4> ys{area.top, area.top + edge_top,
                                area.bottom - edge_bottom, area.bottom};
  const std::array<float, 4> us{0.0F, source_left / width,
                                (width - source_right) / width, 1.0F};
  const std::array<float, 4> vs{0.0F, source_top / height,
                                (height - source_bottom) / height, 1.0F};
  const bool tiles = slicing.mode == components::Image::ResizingMode::tile;
  for (std::size_t row = 0; row < 3; ++row) {
    for (std::size_t column = 0; column < 3; ++column) {
      const Box cell{.left = xs.at(column),
                     .top = ys.at(row),
                     .right = xs.at(column + 1),
                     .bottom = ys.at(row + 1)};
      if (cell.width() <= 0.0F || cell.height() <= 0.0F ||
          us.at(column + 1) <= us.at(column) || vs.at(row + 1) <= vs.at(row)) {
        continue;
      }
      const float half_u = 0.5F / width;
      const float half_v = 0.5F / height;
      const float u_start = us.at(column) + (column > 0 ? half_u : 0.0F);
      const float u_stop = us.at(column + 1) - (column < 2 ? half_u : 0.0F);
      const float v_start = vs.at(row) + (row > 0 ? half_v : 0.0F);
      const float v_stop = vs.at(row + 1) - (row < 2 ? half_v : 0.0F);
      const float u_middle = (us.at(column) + us.at(column + 1)) * 0.5F;
      const float v_middle = (vs.at(row) + vs.at(row + 1)) * 0.5F;
      const std::array<float, 4> uv{
          std::min(u_start, u_middle), std::min(v_start, v_middle),
          std::max(u_stop, u_middle), std::max(v_stop, v_middle)};
      const bool stretches_x = column == 1;
      const bool stretches_y = row == 1;
      if (!tiles || (!stretches_x && !stretches_y)) {
        picture(pixels, cell, colour, clip, uv, rendering);
        continue;
      }
      const float tile_width =
          stretches_x ? (us.at(column + 1) - us.at(column)) * width * to_target
                      : cell.width();
      const float tile_height =
          stretches_y ? (vs.at(row + 1) - vs.at(row)) * height * to_target
                      : cell.height();
      tile_picture(pixels, cell, uv, tile_width, tile_height, colour, clip,
                   rendering);
    }
  }
}

void Painter::paint_image(const Scope& scope, const graphics::ImagePaint& paint,
                          Box area, float opacity) {
  const std::shared_ptr<const provider::Image>& pixels = paint.image();
  if (pixels == nullptr || pixels->width() == 0 || pixels->height() == 0) {
    return;
  }
  const graphics::ImagePaint::SourceRect source = paint.source_rect();
  if (source.width <= 0.0F || source.height <= 0.0F) {
    return;
  }
  const float target = target_.scale.value();
  const float tile_width = static_cast<float>(pixels->width()) * source.width *
                           paint.scale() * target;
  const float tile_height = static_cast<float>(pixels->height()) *
                            source.height * paint.scale() * target;
  if (tile_width < 1.0F || tile_height < 1.0F) {
    return;
  }
  const auto phase = [](float offset, float tile) {
    const float wrapped = std::fmod(offset, tile);
    return wrapped > 0.0F ? wrapped - tile : wrapped;
  };
  const Box aligned{
      .left = area.left + phase(paint.origin().x * target, tile_width),
      .top = area.top + phase(paint.origin().y * target, tile_height),
      .right = area.right,
      .bottom = area.bottom};
  tile_picture(
      pixels, aligned,
      {source.x, source.y, source.x + source.width, source.y + source.height},
      tile_width, tile_height,
      pack(theme::Colour{1.0F, 1.0F, 1.0F, 1.0F}, opacity),
      intersect(scope.clip, area), components::Image::Rendering{});
}

auto Painter::pictures() const noexcept -> Pictures {
  return Pictures{.count = pictures_.size(),
                  .bytes = picture_bytes_,
                  .budget = picture_budget_.value(),
                  .uploads = picture_uploads_,
                  .evictions = picture_evictions_};
}

void Painter::note_used(std::span<const Quad> quads) {
  const auto note = [this](const std::optional<provider::TextureId>& texture) {
    if (!texture.has_value() || texture->value() == 0 ||
        (!painted_textures_.empty() &&
         painted_textures_.back() == texture->value())) {
      return;
    }
    painted_textures_.push_back(texture->value());
  };
  for (const Quad& quad : quads) {
    note(quad.texture);
    note(quad.detail_texture);
  }
}

void Painter::mark_used() {
  note_used(quads_);
  if (painted_textures_.empty()) {
    return;
  }
  std::ranges::sort(painted_textures_);
  const auto [last, end] = std::ranges::unique(painted_textures_);
  painted_textures_.erase(last, end);
  shapes_.retain(std::span<const std::uint64_t>{painted_textures_});
  for (auto& [key, held] : pictures_) {
    if (std::ranges::binary_search(painted_textures_,
                                   held.texture.id().value())) {
      held.painted = picture_frame_;
    }
  }
}

void Painter::evict_pictures() {
  if (picture_bytes_ <= picture_budget_.value()) {
    return;
  }
  picture_victims_.clear();
  for (auto entry = pictures_.begin(); entry != pictures_.end(); ++entry) {
    if (entry->second.painted + picture_grace_frames < picture_frame_) {
      picture_victims_.push_back(entry);
    }
  }
  std::ranges::sort(picture_victims_, {},
                    [](const auto& entry) { return entry->second.painted; });
  for (const auto& victim : picture_victims_) {
    if (picture_bytes_ <= picture_budget_.value()) {
      break;
    }
    picture_bytes_ -= victim->second.bytes;
    pictures_.erase(victim);
    ++picture_evictions_;
  }
  picture_victims_.clear();
}

auto Painter::redaction_colour(const Scope& scope) const noexcept
    -> std::uint32_t {
  theme::Colour shown =
      scope.colour(ink(scope, theme::ColourRole::text_primary));
  shown.alpha *= redaction_opacity;
  return pack(shown, scope.opacity);
}

void Painter::redact_box(const Scope& scope, Box area) {
  shape(provider::ShapeKind::fill, area, Corners{}, 0.0F, 0.0F,
        redaction_colour(scope), scope.clip);
}

auto Painter::symbolic(std::string_view content) noexcept -> bool {
  if (content.empty()) {
    return false;
  }
  for (std::size_t offset = 0; offset < content.size();) {
    const text::Utf8::Decoded decoded = text::Utf8::decode(content, offset);
    if (decoded.length == 0 ||
        decoded.codepoint < components::SystemSymbols::first_codepoint) {
      return false;
    }
    offset += decoded.length;
  }
  return true;
}

auto Painter::redact_text(std::string_view content,
                          const text::TextStyle& style, Box area, bool centred,
                          const Scope& scope)
    -> std::expected<void, PaintError> {
  const std::optional<Arranged> arranged =
      arrange(content, style, area, centred, !centred);
  if (!arranged.has_value()) {
    return {};
  }
  const float scale = target_.scale.value();
  const std::uint32_t colour = redaction_colour(scope);
  const bool glyphs_only = symbolic(content);
  const float bar = style.size.value() * redaction_bar_height;
  const Corners corners{.radius = glyphs_only ? 0.0F : redaction_radius * scale,
                        .smoothing = 0.0F};
  for (const text::TextLine& line : arranged->layout->lines()) {
    if (line.width <= 0.0F) {
      continue;
    }
    const float left = (arranged->origin_x + line.x) * scale;
    const float right = left + (line.width * scale);
    const float bottom =
        glyphs_only ? (arranged->origin_y + line.top + line.height) * scale
                    : (arranged->origin_y + line.baseline) * scale;
    const float top = glyphs_only ? (arranged->origin_y + line.top) * scale
                                  : bottom - (bar * scale);
    shape(provider::ShapeKind::fill,
          Box{.left = left, .top = top, .right = right, .bottom = bottom},
          corners, 0.0F, 0.0F, colour, scope.clip);
  }
  return {};
}

auto Painter::filled_uv(const provider::Image& pixels, Box area) noexcept
    -> std::array<float, 4> {
  const float shown = area.width() / area.height();
  const float native =
      static_cast<float>(pixels.width()) / static_cast<float>(pixels.height());
  if (shown > native) {
    const float kept = native / shown;
    return {0.0F, (1.0F - kept) * 0.5F, 1.0F, (1.0F + kept) * 0.5F};
  }
  const float kept = shown / native;
  return {(1.0F - kept) * 0.5F, 0.0F, (1.0F + kept) * 0.5F, 1.0F};
}

auto Painter::fitted_box(const provider::Image& pixels, Box area) noexcept
    -> Box {
  if (pixels.height() == 0 || area.height() <= 0.0F) {
    return area;
  }
  const float shown = area.width() / area.height();
  const float native =
      static_cast<float>(pixels.width()) / static_cast<float>(pixels.height());
  if (shown > native) {
    const float width = area.height() * native;
    const float left = area.left + ((area.width() - width) * 0.5F);
    return Box{.left = left,
               .top = area.top,
               .right = left + width,
               .bottom = area.bottom};
  }
  const float height = area.width() / native;
  const float top = area.top + ((area.height() - height) * 0.5F);
  return Box{.left = area.left,
             .top = top,
             .right = area.right,
             .bottom = top + height};
}

void Painter::sample(provider::TextureId texture, Box area,
                     std::uint32_t colour, const provider::ClipRect& clip,
                     bool fresh, std::array<float, 4> uv) {
  if (area.width() <= 0.0F || area.height() <= 0.0F || clip.width == 0 ||
      clip.height == 0) {
    return;
  }
  quads_.push_back(
      Quad{.kind = provider::ShapeKind::image,
           .bounds = area,
           .uv = uv,
           .centre = {(area.left + area.right) * 0.5F,
                      (area.top + area.bottom) * 0.5F},
           .half_size = {area.width() * 0.5F, area.height() * 0.5F},
           .shape = {},
           .colour = colour,
           .texture = texture,
           .clip = clip,
           .transform = transform_,
           .fresh = fresh});
}

void Painter::sample_external(provider::TextureId texture, Box area,
                              std::uint32_t colour,
                              const provider::ClipRect& clip) {
  const auto published = publication(texture);
  if (!published.has_value()) {
    unsettle();
    sample(texture, area, colour, clip, true);
    return;
  }
  const std::size_t before = quads_.size();
  sample(texture, area, colour, clip, false);
  if (quads_.size() == before) {
    return;
  }
  Quad& quad = quads_.back();
  quad.external = true;
  if (published->planar()) {
    quad.kind = provider::ShapeKind::yuv;
    quad.detail_texture = published->chroma;
    quad.effect = published->conversion.levels();
    quad.detail = published->conversion.weights();
  } else if (published->premultiplied) {
    quad.kind = provider::ShapeKind::layer;
  }
}

void Painter::paint_surface(provider::SurfaceId surface, Box area,
                            const Scope& scope) {
  const auto directory = gpu_.get().surfaces();
  if (!directory.has_value() || scope.opacity <= 0.0F || area.width() <= 0.0F ||
      area.height() <= 0.0F) {
    return;
  }
  const provider::ClipRect visible =
      intersect(scope.clip, bounds(area, transform_));
  if (visible.width == 0 || visible.height == 0) {
    return;
  }
  const std::array<float, 2> origin =
      transforms_[transform_].apply({area.left, area.top});
  const Box placed{.left = std::round(origin[0]),
                   .top = std::round(origin[1]),
                   .right = std::round(origin[0]) + area.width(),
                   .bottom = std::round(origin[1]) + area.height()};
  const auto placement = directory->get().place(
      surface, provider::PassRegion{.x = placed.left,
                                    .y = placed.top,
                                    .width = area.width(),
                                    .height = area.height()});
  if (!placement.has_value()) {
    return;
  }
  const theme::Colour white{1.0F, 1.0F, 1.0F, 1.0F};
  if (placement->mode == provider::SurfaceMode::texture) {
    sample_external(placement->texture, area, pack(white, scope.opacity),
                    scope.clip);
    return;
  }
  const provider::ClipRect hole = intersect(scope.clip, placed);
  if (hole.width == 0 || hole.height == 0) {
    return;
  }
  const std::size_t before = quads_.size();
  sample_external(placement->texture, area, pack(white, 1.0F), hole);
  if (quads_.size() > before) {
    quads_.back().detail_texture = placement->texture;
  }
}

auto Painter::publication(provider::TextureId texture) const
    -> std::optional<provider::TexturePublications::Publication> {
  const auto publications = gpu_.get().publications();
  if (!publications.has_value()) {
    return std::nullopt;
  }
  return publications->get().find(texture);
}

auto Painter::republished(provider::TextureId texture) -> bool {
  const auto published = publication(texture);
  if (!published.has_value()) {
    return true;
  }
  next_publications_.insert_or_assign(texture.value(), published->generation);
  const auto shown = shown_publications_.find(texture.value());
  return shown == shown_publications_.end() ||
         shown->second != published->generation;
}

void Painter::retire_pictures() {
  ++picture_frame_;
  std::erase_if(pictures_, [this](auto& entry) {
    Picture& held = entry.second;
    if (!held.source.expired()) {
      return false;
    }
    ++held.idle;
    if (held.idle <= picture_grace_frames) {
      return false;
    }
    picture_bytes_ -= held.bytes;
    return true;
  });
  evict_pictures();
}

void Painter::shadows(const Scope& scope, theme::Elevation level, Box area,
                      Corners shape_corners, float weight,
                      ShadowMotion movement) {
  if (level == theme::Elevation::none || weight <= 0.0F) {
    return;
  }
  const float scale = target_.scale.value();
  const float boost = scope.resolved_scheme() == theme::ColorScheme::dark
                          ? dark_shadow_boost
                          : 1.0F;
  const theme::ElevationStyle& style = scope.theme.get().elevation(level);
  for (const theme::Shadow& layer : {style.ambient, style.key}) {
    if (layer.opacity <= 0.0F) {
      continue;
    }
    const float offset = layer.offset_y * scale;
    const float softness = std::max(layer.blur * scale * 0.5F, 0.5F);
    soft(Box{.left = area.left,
             .top = area.top + offset,
             .right = area.right,
             .bottom = area.bottom + offset},
         shape_corners, softness,
         pack(theme::Colour{0.0F, 0.0F, 0.0F,
                            std::min(layer.opacity * boost * weight, 1.0F)},
              scope.opacity),
         scope.clip, movement);
  }
}

void Painter::drop_shadow(const Scope& scope, theme::Colour colour,
                          float radius, float x, float y, Box area,
                          Corners shape_corners, ShadowMotion movement) {
  const float scale = target_.scale.value();
  const float offset_x = x * scale;
  const float offset_y = y * scale;
  soft(Box{.left = area.left + offset_x,
           .top = area.top + offset_y,
           .right = area.right + offset_x,
           .bottom = area.bottom + offset_y},
       shape_corners, std::max(radius * scale * drop_shadow_spread, 0.5F),
       pack(colour, scope.opacity), scope.clip, movement);
}

auto Painter::penumbra(std::uint32_t extent, float outward, float radius,
                       float softness) -> text::GlyphBitmap {
  std::vector<std::byte> coverage(static_cast<std::size_t>(extent) * extent);
  const auto ease = [](float edge0, float edge1, float value) {
    const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0F, 1.0F);
    return t * t * (3.0F - (2.0F * t));
  };
  for (std::uint32_t row = 0; row < extent; ++row) {
    for (std::uint32_t column = 0; column < extent; ++column) {
      const float inward_x = static_cast<float>(column) + 0.5F - outward;
      const float inward_y = static_cast<float>(row) + 0.5F - outward;
      const float qx = radius - inward_x;
      const float qy = radius - inward_y;
      const float distance =
          std::hypot(std::max(qx, 0.0F), std::max(qy, 0.0F)) +
          std::min(std::max(qx, qy), 0.0F) - radius;
      const float shade = 1.0F - ease(-softness, softness, distance);
      coverage[(static_cast<std::size_t>(row) * extent) + column] =
          std::byte{static_cast<unsigned char>(
              std::lround(std::clamp(shade, 0.0F, 1.0F) * 255.0F))};
    }
  }
  return text::GlyphBitmap{extent, extent, 0, 0, std::move(coverage)};
}

void Painter::patch(Box bounds, std::array<float, 4> uv, std::uint32_t colour,
                    provider::TextureId texture,
                    const provider::ClipRect& clip) {
  if (bounds.width() <= 0.0F || bounds.height() <= 0.0F) {
    return;
  }
  quads_.push_back(
      Quad{.kind = provider::ShapeKind::glyph,
           .bounds = bounds,
           .uv = uv,
           .centre = {(bounds.left + bounds.right) * 0.5F,
                      (bounds.top + bounds.bottom) * 0.5F},
           .half_size = {bounds.width() * 0.5F, bounds.height() * 0.5F},
           .shape = {},
           .colour = colour,
           .texture = texture,
           .clip = clip,
           .transform = transform_});
}

void Painter::soft(Box area, Corners corners, float softness,
                   std::uint32_t colour, const provider::ClipRect& clip,
                   ShadowMotion movement) {
  const float radius = corners.radius;
  const auto fallback = [&] {
    Corners hard = corners;
    hard.smoothing = 0.0F;
    shape(provider::ShapeKind::shadow, area, hard, 0.0F, softness, colour,
          clip);
  };
  if (area.width() <= 0.0F || area.height() <= 0.0F || clip.width == 0 ||
      clip.height == 0 || (colour >> 24U) == 0) {
    return;
  }
  if (movement == ShadowMotion::moving || corners.ellipse || uneven(corners)) {
    fallback();
    return;
  }
  const float rounded =
      std::round(std::max(radius, 0.0F) * shadow_quantum) / shadow_quantum;
  const float blur =
      std::max(std::round(softness * shadow_quantum) / shadow_quantum, 0.5F);
  const float outward = std::ceil(blur + 1.0F);
  const float inward = std::ceil(rounded + blur);
  const float extent = outward + inward;
  if (extent > maximum_patch || area.width() < 2.0F * inward ||
      area.height() < 2.0F * inward) {
    fallback();
    return;
  }
  const auto size = static_cast<std::uint32_t>(extent) + 1;
  const std::uint64_t key =
      (static_cast<std::uint64_t>(std::lround(rounded * shadow_quantum))
       << 32U) |
      static_cast<std::uint64_t>(std::lround(blur * shadow_quantum));
  const std::uint64_t before = atlas_.statistics().rasterised;
  auto placed = atlas_.mask(key, [size, outward, rounded, blur] {
    return penumbra(size, outward, rounded, blur);
  });
  if (!placed.has_value() || !placed->has_value()) {
    fallback();
    return;
  }
  if (atlas_.statistics().rasterised > before) {
    ++shadow_misses_;
  } else {
    ++shadow_hits_;
  }
  const text::GlyphQuad& cell = **placed;
  use_page(cell.page);
  const float texel_u = (cell.u1 - cell.u0) / static_cast<float>(size);
  const float texel_v = (cell.v1 - cell.v0) / static_cast<float>(size);
  const float edge_u = cell.u1 - texel_u;
  const float edge_v = cell.v1 - texel_v;
  const float inner_u = edge_u - (texel_u * 0.5F);
  const float inner_v = edge_v - (texel_v * 0.5F);
  const float left = area.left - outward;
  const float top = area.top - outward;
  const float right = area.right + outward;
  const float bottom = area.bottom + outward;
  const provider::TextureId page = cell.texture;
  patch(Box{.left = left,
            .top = top,
            .right = left + extent,
            .bottom = top + extent},
        {cell.u0, cell.v0, edge_u, edge_v}, colour, page, clip);
  patch(Box{.left = right - extent,
            .top = top,
            .right = right,
            .bottom = top + extent},
        {edge_u, cell.v0, cell.u0, edge_v}, colour, page, clip);
  patch(Box{.left = left,
            .top = bottom - extent,
            .right = left + extent,
            .bottom = bottom},
        {cell.u0, edge_v, edge_u, cell.v0}, colour, page, clip);
  patch(Box{.left = right - extent,
            .top = bottom - extent,
            .right = right,
            .bottom = bottom},
        {edge_u, edge_v, cell.u0, cell.v0}, colour, page, clip);
  patch(Box{.left = left + extent,
            .top = top,
            .right = right - extent,
            .bottom = top + extent},
        {inner_u, cell.v0, inner_u, edge_v}, colour, page, clip);
  patch(Box{.left = left + extent,
            .top = bottom - extent,
            .right = right - extent,
            .bottom = bottom},
        {inner_u, edge_v, inner_u, cell.v0}, colour, page, clip);
  patch(Box{.left = left,
            .top = top + extent,
            .right = left + extent,
            .bottom = bottom - extent},
        {cell.u0, inner_v, edge_u, inner_v}, colour, page, clip);
  patch(Box{.left = right - extent,
            .top = top + extent,
            .right = right,
            .bottom = bottom - extent},
        {edge_u, inner_v, cell.u0, inner_v}, colour, page, clip);
  patch(Box{.left = left + extent,
            .top = top + extent,
            .right = right - extent,
            .bottom = bottom - extent},
        {inner_u, inner_v, inner_u, inner_v}, colour, page, clip);
}

auto Painter::arrange(std::string_view content, const text::TextStyle& style,
                      Box area, bool centred, bool wraps)
    -> std::optional<Arranged> {
  if (!typesetter_.has_value() || content.empty()) {
    return std::nullopt;
  }
  text::Typesetter& typesetter = typesetter_->get();
  const float scale = target_.scale.value();
  const float available = std::max(area.width() / scale, 0.0F);
  const auto natural = typesetter.measure(
      content, style, text::LayoutWidth::unbounded(), target_.scale);
  if (!natural.has_value()) {
    return std::nullopt;
  }
  text::LayoutWidth width = text::LayoutWidth::unbounded();
  if (wraps && available + 0.01F < natural->width) {
    width = text::LayoutWidth::points(available).value_or(width);
  }
  auto laid = typesetter.layout(content, style, width, target_.scale);
  if (!laid.has_value()) {
    return std::nullopt;
  }
  const text::TextLayout& layout = **laid;
  float origin_x = area.left / scale;
  float origin_y = area.top / scale;
  const float spare = std::max(available - layout.size().width, 0.0F);
  if (centred) {
    origin_x += (available - layout.size().width) * 0.5F;
    origin_y += ((area.height() / scale) - layout.size().height) * 0.5F;
  } else {
    switch (style.alignment) {
      case text::TextAlignment::natural:
        if (!layout.lines().empty() && layout.lines().front().right_to_left) {
          origin_x += spare;
        }
        break;
      case text::TextAlignment::left:
        break;
      case text::TextAlignment::center:
        origin_x += spare * 0.5F;
        break;
      case text::TextAlignment::right:
        origin_x += spare;
        break;
      case text::TextAlignment::opposite:
        if (layout.lines().empty() || !layout.lines().front().right_to_left) {
          origin_x += spare;
        }
        break;
    }
  }
  return Arranged{
      .layout = std::move(*laid), .origin_x = origin_x, .origin_y = origin_y};
}

template <class Inking>
auto Painter::draw(const Arranged& arranged, std::uint32_t colour,
                   const provider::ClipRect& clip, Inking&& ink)
    -> std::expected<void, PaintError> {
  std::size_t index = 0;
  const text::TextLayout& layout = *arranged.layout;
  const text::TextStyle& style = layout.style();
  const bool styled = !style.runs.empty();
  const bool decorated = styled || style.underline || style.strikethrough;
  if (styled && !glyph_window_.has_value()) {
    for (const text::TextLine& line : layout.lines()) {
      underlay(layout, line, arranged, colour >> 24U, clip);
    }
  }
  underlaid_ = quads_.size();
  for (const text::TextLine& line : layout.lines()) {
    Decoration underline{};
    Decoration struck{};
    for (const text::GlyphRun& run :
         layout.runs().subspan(line.first_run, line.run_count)) {
      for (const text::PositionedGlyph& glyph : layout.glyphs(run)) {
        const std::size_t position = index;
        const Ink style_ink = ink(glyph, index);
        ++index;
        if (glyph_window_.has_value() && (position < glyph_window_->first ||
                                          position >= glyph_window_->second)) {
          continue;
        }
        const std::optional<std::reference_wrapper<const text::TextRun>> span =
            styled ? style.runs.find(glyph.cluster) : std::nullopt;
        const std::uint32_t base =
            span.has_value() ? inked(span->get(), colour) : colour;
        const std::uint32_t tinted = faded(base, style_ink.alpha);
        if (decorated) {
          const bool run_lined = span.has_value() && span->get().underline;
          const bool run_crossed =
              span.has_value() && span->get().strikethrough;
          const bool lined = style.underline || run_lined;
          const bool crossed = style.strikethrough || run_crossed;
          const float left = arranged.origin_x + glyph.x;
          const float baseline = arranged.origin_y + line.baseline;
          const text::FaceMetrics metrics = run.face->metrics(run.size);
          const auto run_ink = [&](std::uint16_t chosen,
                                   std::optional<std::uint32_t> view,
                                   bool from_run) -> std::uint32_t {
            if (from_run && chosen > 0 && chosen <= inks_.runs.size()) {
              return inks_.runs[chosen - 1];
            }
            if (!from_run && view.has_value()) {
              return *view;
            }
            return base;
          };
          const bool run_line_owned = run_lined && !style.underline;
          const bool run_cross_owned = run_crossed && !style.strikethrough;
          const std::uint32_t lined_ink =
              run_ink(span.has_value() ? span->get().underline_ink : 0,
                      inks_.underline, run_line_owned);
          const std::uint32_t crossed_ink =
              run_ink(span.has_value() ? span->get().strikethrough_ink : 0,
                      inks_.strikethrough, run_cross_owned);
          const text::LinePattern line_pattern =
              run_line_owned ? span->get().underline_pattern
                             : style.underline_pattern;
          const text::LinePattern cross_pattern =
              run_cross_owned ? span->get().strikethrough_pattern
                              : style.strikethrough_pattern;
          extend(underline, lined, lined_ink, left, glyph.advance,
                 baseline + metrics.underline_position -
                     (metrics.underline_thickness * 0.5F),
                 metrics.underline_thickness, line_pattern, run.size.value(),
                 clip);
          extend(struck, crossed, crossed_ink, left, glyph.advance,
                 baseline - metrics.strikeout_position,
                 metrics.strikeout_thickness, cross_pattern, run.size.value(),
                 clip);
        }
        if ((tinted >> 24U) == 0) {
          continue;
        }
        if (span.has_value() && span->get().attachment.has_value()) {
          attach(span->get(), glyph, arranged, line,
                 style_ink.alpha * static_cast<float>(tinted >> 24U) / 255.0F,
                 clip);
          continue;
        }
        auto placed = atlas_.place(
            *run.face, glyph.glyph, run.size, target_.scale,
            arranged.origin_x + glyph.x + style_ink.dx,
            arranged.origin_y + glyph.y + style_ink.dy,
            !style.drawing.contains(
                text::DrawingOptions::disables_subpixel_quantization));
        if (!placed.has_value()) {
          return std::unexpected(
              PaintError::from(placed.error(), PaintStage::glyph_atlas));
        }
        if (!placed->has_value()) {
          continue;
        }
        const text::GlyphQuad& quad = **placed;
        use_page(quad.page);
        if (span.has_value() && span->get().shade != 0) {
          shaded_glyphs_.push_back(
              ShadedGlyph{.quad = quads_.size(), .shade = span->get().shade});
        }
        quads_.push_back(Quad{.kind = provider::ShapeKind::glyph,
                              .bounds = Box{.left = quad.left,
                                            .top = quad.top,
                                            .right = quad.right,
                                            .bottom = quad.bottom},
                              .uv = {quad.u0, quad.v0, quad.u1, quad.v1},
                              .centre = {(quad.left + quad.right) * 0.5F,
                                         (quad.top + quad.bottom) * 0.5F},
                              .half_size = {(quad.right - quad.left) * 0.5F,
                                            (quad.bottom - quad.top) * 0.5F},
                              .shape = {},
                              .colour = tinted,
                              .texture = quad.texture,
                              .clip = clip,
                              .transform = transform_});
      }
    }
    rule(underline, clip);
    rule(struck, clip);
  }
  return {};
}

void Painter::underlay(const text::TextLayout& layout,
                       const text::TextLine& line, const Arranged& arranged,
                       std::uint32_t alpha, const provider::ClipRect& clip) {
  const text::TextStyle& style = layout.style();
  const float scale = target_.scale.value();
  std::optional<Underlay> open;
  const auto flush = [&]() {
    if (!open.has_value() || open->right <= open->left) {
      open.reset();
      return;
    }
    const float top = arranged.origin_y + line.top;
    shape(provider::ShapeKind::fill,
          Box{.left = std::round(open->left * scale),
              .top = std::round(top * scale),
              .right = std::round(open->right * scale),
              .bottom = std::round((top + line.height) * scale)},
          Corners{}, 0.0F, 0.0F, open->colour, clip);
    open.reset();
  };
  for (const text::GlyphRun& run :
       layout.runs().subspan(line.first_run, line.run_count)) {
    for (const text::PositionedGlyph& glyph : layout.glyphs(run)) {
      const auto span = style.runs.find(glyph.cluster);
      const std::uint16_t chosen =
          span.has_value() ? span->get().background : 0;
      if (chosen == 0 || chosen > inks_.runs.size()) {
        flush();
        continue;
      }
      const std::uint32_t colour =
          faded(inks_.runs[chosen - 1], static_cast<float>(alpha) / 255.0F);
      const float left = arranged.origin_x + glyph.x;
      if (open.has_value() && open->colour == colour &&
          std::abs(open->right - left) < 0.5F) {
        open->right = left + glyph.advance;
        continue;
      }
      flush();
      open = Underlay{
          .colour = colour, .left = left, .right = left + glyph.advance};
    }
  }
  flush();
}

void Painter::attach(const text::TextRun& run,
                     const text::PositionedGlyph& glyph,
                     const Arranged& arranged, const text::TextLine& line,
                     float alpha, const provider::ClipRect& clip) {
  std::shared_ptr<const provider::Image> pixels;
  if (run.image > 0 && run.image <= inks_.images.size()) {
    pixels = inks_.images[run.image - 1].pixels;
  } else if (run.glyph > 0 && run.glyph <= inks_.glyphs.size()) {
    pixels = inks_.glyphs[run.glyph - 1].image;
  }
  if (pixels == nullptr || !run.attachment.has_value()) {
    return;
  }
  const text::TextAttachment& attached = *run.attachment;
  const float scale = target_.scale.value();
  const float left = arranged.origin_x + glyph.x;
  const float bottom = arranged.origin_y + line.baseline + attached.descent;
  picture(pixels,
          Box{.left = left * scale,
              .top = (bottom - attached.height) * scale,
              .right = (left + attached.width) * scale,
              .bottom = bottom * scale},
          pack(theme::Colour{1.0F, 1.0F, 1.0F, 1.0F}, alpha), clip,
          {0.0F, 0.0F, 1.0F, 1.0F});
}

void Painter::link_frames(composition::Tree& tree,
                          const composition::Node& node,
                          const Arranged& arranged, Box frame) {
  const float scale = target_.scale.value();
  const text::TextLayout& layout = *arranged.layout;
  const std::span<const text::TextRun> runs = layout.style().runs.runs();
  link_scratch_.clear();
  for (const text::TextLine& line : layout.lines()) {
    for (const text::GlyphRun& run :
         layout.runs().subspan(line.first_run, line.run_count)) {
      for (const text::PositionedGlyph& glyph : layout.glyphs(run)) {
        const auto span = layout.style().runs.find(glyph.cluster);
        if (!span.has_value() || span->get().link == 0) {
          continue;
        }
        const auto at = static_cast<std::size_t>(&span->get() - runs.data());
        std::size_t starts = 0;
        for (std::size_t index = 0; index <= at; ++index) {
          const bool joins = index > 0 &&
                             runs[index - 1].link == runs[index].link &&
                             runs[index - 1].end == runs[index].start;
          if (runs[index].link != 0 && !joins) {
            ++starts;
          }
        }
        const std::size_t ordinal = starts - 1;
        const float left = arranged.origin_x + glyph.x - (frame.left / scale);
        const float top = arranged.origin_y + line.top - (frame.top / scale);
        if (!link_scratch_.empty() && link_scratch_.back().link == ordinal &&
            link_scratch_.back().frame.y == top &&
            std::abs(link_scratch_.back().frame.x +
                     link_scratch_.back().frame.width - left) < 0.5F) {
          link_scratch_.back().frame.width += glyph.advance;
          continue;
        }
        link_scratch_.push_back(composition::LinkFrame{
            .link = ordinal,
            .frame = composition::Rect{.x = left,
                                       .y = top,
                                       .width = glyph.advance,
                                       .height = line.height}});
      }
    }
  }
  tree.assign_link_frames(node.id(), link_scratch_);
}

auto Painter::inked(const text::TextRun& run,
                    std::uint32_t colour) const noexcept -> std::uint32_t {
  if (run.ink > 0 && run.ink <= inks_.runs.size()) {
    return inks_.runs[run.ink - 1];
  }
  if (run.link > 0 && inks_.link.has_value()) {
    return *inks_.link;
  }
  return colour;
}

void Painter::extend(Decoration& decoration, bool active, std::uint32_t colour,
                     float left, float advance, float top, float thickness,
                     text::LinePattern pattern, float size,
                     const provider::ClipRect& clip) {
  const bool continues =
      decoration.active && active && decoration.colour == colour &&
      decoration.pattern == pattern && std::abs(decoration.right - left) < 0.5F;
  if (continues) {
    decoration.right = left + advance;
    return;
  }
  rule(decoration, clip);
  decoration = Decoration{.active = active,
                          .colour = colour,
                          .left = left,
                          .right = left + advance,
                          .top = top,
                          .thickness = thickness,
                          .pattern = pattern,
                          .size = size};
}

void Painter::rule(Decoration& decoration, const provider::ClipRect& clip) {
  if (!decoration.active || decoration.right <= decoration.left ||
      (decoration.colour >> 24U) == 0) {
    decoration.active = false;
    return;
  }
  const float scale = target_.scale.value();
  const float height = std::max(std::round(decoration.thickness * scale), 1.0F);
  const float top = std::round(decoration.top * scale);
  const std::span<const float> dashes =
      text::LineDashes::of(decoration.pattern);
  if (dashes.empty()) {
    shape(provider::ShapeKind::fill,
          Box{.left = decoration.left * scale,
              .top = top,
              .right = decoration.right * scale,
              .bottom = top + height},
          Corners{}, 0.0F, 0.0F, decoration.colour, clip);
    decoration.active = false;
    return;
  }
  float at = decoration.left;
  std::size_t step = 0;
  const float unit = std::max(decoration.size, 1.0F);
  while (at < decoration.right) {
    const float length = dashes[step % dashes.size()] * unit;
    if (step % 2 == 0) {
      const float end = std::min(at + length, decoration.right);
      shape(provider::ShapeKind::fill,
            Box{.left = std::round(at * scale),
                .top = top,
                .right = std::max(std::round(end * scale),
                                  std::round(at * scale) + 1.0F),
                .bottom = top + height},
            Corners{}, 0.0F, 0.0F, decoration.colour, clip);
    }
    at += length;
    ++step;
  }
  decoration.active = false;
}

auto Painter::glyph_slice(const components::Text::Glyphs::Content& glyphs,
                          Box frame, const Scope& scope)
    -> std::expected<void, PaintError> {
  const std::shared_ptr<const components::Text::Layout::Source>& source =
      glyphs.source();
  if (source == nullptr || source->layout == nullptr || scope.redacts() ||
      glyphs.first() >= glyphs.last() || scope.clip.width == 0 ||
      scope.clip.height == 0) {
    return {};
  }
  const float scale = target_.scale.value();
  const theme::ColourValue ink = source->foreground.value_or(
      this->ink(scope, theme::ColourRole::text_primary));
  run_inks_.clear();
  for (const theme::ColourValue& value : source->attributed.inks()) {
    run_inks_.push_back(pack(scope.colour(value), scope.opacity));
  }
  inks_ = TextInks{
      .runs = run_inks_,
      .link = source->attributed.destinations().empty()
                  ? std::nullopt
                  : std::optional{pack(scope.colour(theme::ColourRole::link),
                                       scope.opacity)},
      .underline = std::nullopt,
      .strikethrough = std::nullopt,
      .images = source->attributed.images(),
      .glyphs = source->attributed.glyphs()};
  glyph_window_ = std::pair{glyphs.first(), glyphs.last()};
  const Arranged arranged{.layout = source->layout,
                          .origin_x = frame.left / scale,
                          .origin_y = frame.top / scale};
  auto drawn =
      draw(arranged, pack(scope.colour(ink), scope.opacity), scope.clip,
           [](const text::PositionedGlyph&, std::size_t) { return Ink{}; });
  glyph_window_.reset();
  inks_ = TextInks{};
  return drawn;
}

auto Painter::typeset(std::string_view content, const text::TextStyle& style,
                      Box area, bool centred, std::uint32_t colour,
                      const provider::ClipRect& clip)
    -> std::expected<void, PaintError> {
  if ((colour >> 24U) == 0 || clip.width == 0 || clip.height == 0) {
    return {};
  }
  const std::optional<Arranged> arranged =
      arrange(content, style, area, centred, !centred);
  if (!arranged.has_value()) {
    return {};
  }
  return draw(*arranged, colour, clip,
              [](const text::PositionedGlyph&, std::size_t) { return Ink{}; });
}

auto Painter::fitted(std::string_view content, const text::TextStyle& style,
                     Box area, const text::TextFitting& fitting)
    -> std::shared_ptr<const text::FittedText> {
  if (!typesetter_.has_value()) {
    return nullptr;
  }
  const float scale = target_.scale.value();
  const float available = std::max(area.width() / scale, 0.0F);
  const text::LayoutWidth width = text::LayoutWidth::points(available).value_or(
      text::LayoutWidth::unbounded());
  text::TextFitting wanted = fitting;
  if (!fitting.lines.has_value() && fitting.minimum_scale_factor >= 1.0F) {
    const std::optional<std::size_t> lines =
        clipped_lines(content, style, width, area.height() / scale);
    if (!lines.has_value()) {
      return nullptr;
    }
    wanted.lines = lines;
  }
  if (!wanted.lines.has_value()) {
    wanted.height = std::max(area.height() / scale, 0.0F) + 0.5F;
  }
  auto made =
      typesetter_->get().fitted(content, style, width, target_.scale, wanted);
  if (!made.has_value()) {
    return nullptr;
  }
  return std::move(*made);
}

auto Painter::clipped_lines(std::string_view content,
                            const text::TextStyle& style,
                            text::LayoutWidth width, float height)
    -> std::optional<std::size_t> {
  static constexpr float slack = 0.5F;
  static constexpr float reach = 0.01F;
  const text::TextSize natural =
      typesetter_->get()
          .measure(content, style, text::LayoutWidth::unbounded(),
                   target_.scale)
          .value_or(text::TextSize{});
  const bool wraps = width.bounded() && natural.width > width.value() + reach;
  const text::TextSize laid =
      wraps ? typesetter_->get()
                  .measure(content, style, width, target_.scale)
                  .value_or(text::TextSize{})
            : natural;
  if (laid.height <= height + slack) {
    return std::nullopt;
  }
  text::TextStyle single = style;
  single.line_spacing = 0.0F;
  const float line =
      typesetter_->get()
          .measure("A", single, text::LayoutWidth::unbounded(), target_.scale)
          .value_or(text::TextSize{})
          .height;
  if (!(line > 0.0F)) {
    return std::nullopt;
  }
  return std::max<std::size_t>(
      1, static_cast<std::size_t>(std::floor((height + slack) / line)));
}

auto Painter::marquee(const composition::Node& node, const Scope& scope,
                      std::string_view content, const text::TextStyle& style,
                      Box area, std::uint32_t colour)
    -> std::optional<std::expected<void, PaintError>> {
  const std::uint64_t key = node.id().value();
  if (!scope.focused || !typesetter_.has_value() || content.empty()) {
    marquees_.erase(key);
    return std::nullopt;
  }
  const float scale = target_.scale.value();
  const auto natural = typesetter_->get().measure(
      content, style, text::LayoutWidth::unbounded(), target_.scale);
  if (!natural.has_value() ||
      natural->width <= (area.width() / scale) + 0.01F) {
    marquees_.erase(key);
    return std::nullopt;
  }
  const auto [found, inserted] = marquees_.try_emplace(key, target_.time);
  const float distance = natural->width + components::Text::marquee_gap;
  const motion::Animation cycle =
      motion::Animation::linear(distance / components::Text::marquee_speed)
          .delay(components::Text::marquee_delay)
          .repeat_forever(false)
          .preferred_frame_rate(components::Text::marquee_frame_rate);
  const motion::AnimationTrack track{
      cycle,
      motion::Animation::Span{.from = motion::AnimatableVector{0.0, 0.0},
                              .to = motion::AnimatableVector{-distance, 0.0},
                              .velocity = {}},
      found->second, nullptr};
  const motion::AnimatableVector shift = track.sample(target_.time).value;
  animating_ = true;
  hinted_rate_ = std::max(hinted_rate_, components::Text::marquee_frame_rate);
  unsettled_ = true;
  const provider::ClipRect clip =
      intersect(scope.clip, bounds(area, scope.transform));
  if (clip.width == 0 || clip.height == 0) {
    return std::expected<void, PaintError>{};
  }
  const Affine& outer = transforms_[scope.transform];
  ReplayBinding& made = bindings_.emplace_back();
  made.parent = binding_;
  made.linear = {outer.a, outer.b, outer.c, outer.d};
  made.scale = scale;
  made.offset = track;
  made.painted_offset = shift;
  const auto bound = static_cast<std::uint32_t>(bindings_.size());
  const std::size_t from = quads_.size();
  const auto offset = static_cast<float>(shift[0]) * scale;
  for (const float lead : {0.0F, distance * scale}) {
    const Box moved{
        .left = area.left + offset + lead,
        .top = area.top,
        .right = area.left + offset + lead + (natural->width * scale),
        .bottom = area.bottom};
    const std::optional<Arranged> arranged =
        arrange(content, style, moved, false, false);
    if (!arranged.has_value()) {
      continue;
    }
    if (auto drawn = draw(
            *arranged, colour, clip,
            [](const text::PositionedGlyph&, std::size_t) { return Ink{}; });
        !drawn.has_value()) {
      return drawn;
    }
  }
  claim(from, bound);
  return std::expected<void, PaintError>{};
}

auto Painter::positions(const Arranged& arranged)
    -> std::vector<std::array<float, 2>> {
  std::vector<std::array<float, 2>> found;
  const text::TextLayout& layout = *arranged.layout;
  for (const text::GlyphRun& run : layout.runs()) {
    for (const text::PositionedGlyph& glyph : layout.glyphs(run)) {
      found.push_back(
          {arranged.origin_x + glyph.x, arranged.origin_y + glyph.y});
    }
  }
  return found;
}

auto Painter::transition_text(const motion::ContentTransition& transition,
                              const motion::Presentation::Swap& swap,
                              std::string_view current,
                              const text::TextStyle& style, Box area,
                              std::uint32_t colour,
                              const provider::ClipRect& clip, bool reduce)
    -> std::expected<void, PaintError> {
  using Kind = motion::ContentTransition::Kind;
  if (clip.width == 0 || clip.height == 0) {
    return {};
  }
  const std::optional<Arranged> before =
      arrange(swap.outgoing, style, area, false, false);
  const std::optional<Arranged> after =
      arrange(current, style, area, false, false);
  const auto progress = static_cast<float>(swap.progress);
  const float shown = std::clamp(progress, 0.0F, 1.0F);
  const float hidden = 1.0F - shown;
  Kind kind = transition.kind();
  if (reduce && transition.moves()) {
    kind = Kind::opacity;
  }
  std::expected<void, PaintError> drawn{};
  if (kind == Kind::numeric_text && before.has_value() && after.has_value()) {
    const std::string_view outgoing = swap.outgoing;
    const float rise = after->layout->size().height * numeric_rise;
    const bool down = transition.value().has_value() ? swap.descending
                                                     : transition.counts_down();
    const float direction = down ? -1.0F : 1.0F;
    const auto shift = static_cast<std::ptrdiff_t>(current.size()) -
                       static_cast<std::ptrdiff_t>(outgoing.size());
    const auto same = [](std::uint32_t cluster, std::ptrdiff_t offset,
                         std::string_view own, std::string_view other) {
      const std::ptrdiff_t mapped =
          static_cast<std::ptrdiff_t>(cluster) - offset;
      return cluster < own.size() && mapped >= 0 &&
             mapped < static_cast<std::ptrdiff_t>(other.size()) &&
             own[cluster] == other[static_cast<std::size_t>(mapped)];
    };
    drawn = draw(*before, colour, clip,
                 [&](const text::PositionedGlyph& glyph, std::size_t) {
                   if (same(glyph.cluster, -shift, outgoing, current)) {
                     return Ink{.dx = 0.0F, .dy = 0.0F, .alpha = 0.0F};
                   }
                   return Ink{.dx = 0.0F,
                              .dy = -direction * progress * rise,
                              .alpha = hidden};
                 });
    if (drawn.has_value()) {
      drawn = draw(*after, colour, clip,
                   [&](const text::PositionedGlyph& glyph, std::size_t) {
                     if (same(glyph.cluster, shift, current, outgoing)) {
                       return Ink{};
                     }
                     return Ink{.dx = 0.0F,
                                .dy = direction * (1.0F - progress) * rise,
                                .alpha = shown};
                   });
    }
    return drawn;
  }
  if (kind == Kind::symbol_effect && before.has_value() && after.has_value()) {
    using Replace = motion::ContentTransition::SymbolReplace;
    const float rise = after->layout->size().height * symbol_rise;
    const float paced = std::clamp(
        progress * static_cast<float>(transition.speed()), 0.0F, 1.0F);
    const float out_dy = transition.replace() == Replace::up_up    ? -rise
                         : transition.replace() == Replace::off_up ? 0.0F
                                                                   : rise;
    drawn = draw(
        *before, colour, clip, [&](const text::PositionedGlyph&, std::size_t) {
          return Ink{.dx = 0.0F, .dy = out_dy * paced, .alpha = 1.0F - paced};
        });
    if (drawn.has_value()) {
      drawn = draw(
          *after, colour, clip, [&](const text::PositionedGlyph&, std::size_t) {
            return Ink{.dx = 0.0F, .dy = rise * (1.0F - paced), .alpha = paced};
          });
    }
    return drawn;
  }
  if (kind == Kind::interpolate && before.has_value() && after.has_value()) {
    const std::vector<std::array<float, 2>> leaving = positions(*before);
    const std::vector<std::array<float, 2>> arriving = positions(*after);
    drawn = draw(
        *before, colour, clip,
        [&](const text::PositionedGlyph&, std::size_t index) {
          if (index >= arriving.size()) {
            return Ink{.dx = 0.0F, .dy = 0.0F, .alpha = hidden};
          }
          return Ink{.dx = (arriving[index][0] - leaving[index][0]) * progress,
                     .dy = (arriving[index][1] - leaving[index][1]) * progress,
                     .alpha = hidden};
        });
    if (drawn.has_value()) {
      drawn =
          draw(*after, colour, clip,
               [&](const text::PositionedGlyph&, std::size_t index) {
                 if (index >= leaving.size()) {
                   return Ink{.dx = 0.0F, .dy = 0.0F, .alpha = shown};
                 }
                 const float remaining = 1.0F - progress;
                 return Ink{
                     .dx = (leaving[index][0] - arriving[index][0]) * remaining,
                     .dy = (leaving[index][1] - arriving[index][1]) * remaining,
                     .alpha = shown};
               });
    }
    return drawn;
  }
  if (before.has_value()) {
    drawn = draw(*before, colour, clip,
                 [hidden](const text::PositionedGlyph&, std::size_t) {
                   return Ink{.dx = 0.0F, .dy = 0.0F, .alpha = hidden};
                 });
  }
  if (drawn.has_value() && after.has_value()) {
    drawn = draw(*after, colour, clip,
                 [shown](const text::PositionedGlyph&, std::size_t) {
                   return Ink{.dx = 0.0F, .dy = 0.0F, .alpha = shown};
                 });
  }
  return drawn;
}

auto Painter::layered(const composition::Node& node) noexcept -> bool {
  return node.content_as<composition::Layer::Content>().has_value();
}

void Painter::defer(const composition::Node& node, const Scope& scope) {
  place();
  Scope layer = scope;
  layer.opacity = 1.0F;
  layer.fade = 0;
  layer.clip = screen_;
  layer.transform = 0;
  layer.viewport.reset();
  layer.content_transition.reset();
  layer.arrow.reset();
  layer.enclosure.reset();
  const auto content = node.content_as<composition::Layer::Content>();
  deferred_.push_back(
      Deferred{.node = node,
               .scope = layer,
               .z = content.has_value() ? content->get().layer().z() : 0,
               .order = deferred_order_++});
  deferrals_.push_back(deferred_.back());
}

auto Painter::draws(const composition::ExclusiveScopes& exclusive,
                    std::int32_t z) noexcept -> bool {
  return !exclusive.active() || exclusive.keeps(composition::OverlayLayer{z});
}

auto Painter::shows(const composition::Tree& tree,
                    const composition::Node& node) -> bool {
  std::optional<std::reference_wrapper<const composition::Node>> at = node;
  while (at.has_value()) {
    const composition::Node& current = at->get();
    if (current.phase() == composition::NodeLifecyclePhase::retired ||
        current.interactions().covered || current.presentation().concealed()) {
      return false;
    }
    at = current.parent().has_value() ? tree.node(*current.parent())
                                      : std::nullopt;
  }
  return true;
}

auto Painter::nested(const composition::Tree& tree,
                     const composition::Node& node) -> bool {
  std::optional<composition::NodeId> parent = node.parent();
  while (parent.has_value()) {
    const auto found = tree.node(*parent);
    if (!found.has_value()) {
      return false;
    }
    if (layered(found->get())) {
      return true;
    }
    parent = found->get().parent();
  }
  return false;
}

void Painter::hoist(const composition::Tree& tree) {
  hoisted_.clear();
  const composition::ExclusiveScopes& exclusive = tree.exclusive();
  for (const composition::NodeId id : tree.layers()) {
    const auto found = tree.node(id);
    if (!found.has_value()) {
      continue;
    }
    const composition::Node& node = found->get();
    const auto content = node.content_as<composition::Layer::Content>();
    if (!content.has_value()) {
      continue;
    }
    const std::int32_t z = content->get().layer().z();
    const bool base = z < composition::OverlayLayer::content.z();
    if (!draws(exclusive, z) || (!base && !exclusive.active()) ||
        (!base && nested(tree, node)) || !shows(tree, node)) {
      continue;
    }
    hoisted_.push_back(Hoisted{.node = node, .z = z});
  }
  std::ranges::sort(hoisted_, {}, [](const Hoisted& entry) {
    return std::pair{entry.z, entry.node.get().id().value()};
  });
}

auto Painter::layer_scope(const composition::Tree& tree,
                          const composition::Node& node, Scope scope) -> Scope {
  ancestry_.clear();
  std::optional<composition::NodeId> parent = node.parent();
  while (parent.has_value()) {
    const auto found = tree.node(*parent);
    if (!found.has_value()) {
      break;
    }
    ancestry_.push_back(found->get());
    parent = found->get().parent();
  }
  for (const composition::Node& ancestor : std::views::reverse(ancestry_)) {
    scope = scoped(ancestor, scope);
  }
  scope.opacity = 1.0F;
  scope.fade = 0;
  scope.clip = screen_;
  scope.transform = 0;
  scope.viewport.reset();
  scope.content_transition.reset();
  scope.arrow.reset();
  scope.enclosure.reset();
  return scope;
}

void Painter::harvest(composition::Tree& tree, const composition::Node& node,
                      const Scope& scope) {
  Harvester harvester{*this, tree};
  gleaner_.run(node, Gleaning{.scope = scope}, harvester);
}

auto Painter::Harvester::enter(const composition::Node& node,
                               const Gleaning& above) -> Gleaner::Step {
  Painter& painter = painter_;
  Gleaner::Step step{.state =
                         Gleaning{.scope = painter.scoped(node, above.scope)}};
  painter.conceal(tree_, node);
  return step;
}

auto Painter::Harvester::next(const composition::Node& node, Gleaning& gleaning)
    -> const composition::Node* {
  Painter& painter = painter_;
  const auto children = node.children();
  while (gleaning.position < std::ranges::size(children)) {
    const composition::Node& child = children[gleaning.position++];
    if (!layered(child)) {
      return &child;
    }
    painter.defer(child, gleaning.scope);
  }
  return nullptr;
}

void Painter::Harvester::leave(const composition::Node&, Gleaning&) {}

auto Painter::popover(const composition::Tree& tree,
                      const composition::Node& layer) const
    -> std::optional<Popover> {
  const auto content = layer.content_as<composition::Layer::Content>();
  if (!content.has_value() || !content->get().anchoring().has_value() ||
      !layer.parent().has_value() || layer.child_count() < 2) {
    return std::nullopt;
  }
  const composition::Layer::Anchoring& anchoring = *content->get().anchoring();
  auto host = tree.node(*layer.parent());
  while (
      host.has_value() &&
      !host->get().content_as<composition::LayerHost::Content>().has_value() &&
      host->get().content().composes() && host->get().parent().has_value()) {
    host = tree.node(*host->get().parent());
  }
  if (!host.has_value()) {
    return std::nullopt;
  }
  std::optional<composition::Rect> anchor;
  std::optional<composition::Rect> region;
  for (const composition::Node& child : host->get().children()) {
    if (!layered(child)) {
      anchor = child.presented().has_value() ? child.presented()->bounds
                                             : child.frame();
      if (child.presented().has_value()) {
        region = child.presented()->clip;
      }
      break;
    }
  }
  if (!anchor.has_value()) {
    return std::nullopt;
  }
  if (anchoring.attachment.is_point()) {
    const motion::UnitPoint at = anchoring.attachment.unit_point();
    anchor = composition::Rect{.x = anchor->x + (at.x * anchor->width),
                               .y = anchor->y + (at.y * anchor->height),
                               .width = 0.0F,
                               .height = 0.0F};
  }
  const composition::Rect panel =
      (*std::next(layer.children().begin())).frame();
  const composition::Rect screen = layer.frame();
  composition::Rect view = screen;
  if (region.has_value()) {
    const float left = std::max(region->x, screen.x);
    const float top = std::max(region->y, screen.y);
    const float right =
        std::min(region->x + region->width, screen.x + screen.width);
    const float bottom =
        std::min(region->y + region->height, screen.y + screen.height);
    const float room = composition::Layer::anchor_gap +
                       composition::Layer::arrow_size + panel.height +
                       composition::Layer::safe_margin;
    const bool roomy = std::max(anchor->y - top,
                                bottom - (anchor->y + anchor->height)) >= room;
    if (right > left && bottom > top && roomy) {
      view = composition::Rect{
          .x = left, .y = top, .width = right - left, .height = bottom - top};
    }
  }
  const float reach =
      composition::Layer::anchor_gap + composition::Layer::arrow_size;
  const float margin = composition::Layer::safe_margin;
  const float middle_x = anchor->x + (anchor->width * 0.5F);
  const float middle_y = anchor->y + (anchor->height * 0.5F);
  float x = middle_x - (panel.width * 0.5F);
  float y = anchor->y + anchor->height + reach;
  theme::Edge edge = anchoring.arrow_edge;
  const auto fits_below = [&] {
    return anchor->y + anchor->height + reach + panel.height <=
           view.y + view.height - margin;
  };
  const auto fits_above = [&] {
    return anchor->y - reach - panel.height >= view.y + margin;
  };
  const auto fits_after = [&] {
    return anchor->x + anchor->width + reach + panel.width <=
           view.x + view.width - margin;
  };
  const auto fits_before = [&] {
    return anchor->x - reach - panel.width >= view.x + margin;
  };
  if (edge == theme::Edge::bottom) {
    edge = !fits_above() && fits_below() ? theme::Edge::top : edge;
  } else if (edge == theme::Edge::leading) {
    edge = !fits_after() && fits_before() ? theme::Edge::trailing : edge;
  } else if (edge == theme::Edge::trailing) {
    edge = !fits_before() && fits_after() ? theme::Edge::leading : edge;
  } else {
    edge =
        !fits_below() && fits_above() ? theme::Edge::bottom : theme::Edge::top;
  }
  if (const std::optional<composition::PopoverPlacement>& placement =
          content->get().placement()) {
    placement->place(edge);
  }
  switch (edge) {
    case theme::Edge::bottom:
      y = anchor->y - reach - panel.height;
      break;
    case theme::Edge::leading:
      x = anchor->x + anchor->width + reach;
      y = middle_y - (panel.height * 0.5F);
      break;
    case theme::Edge::trailing:
      x = anchor->x - reach - panel.width;
      y = middle_y - (panel.height * 0.5F);
      break;
    default:
      break;
  }
  x = std::max(std::min(x, view.x + view.width - margin - panel.width),
               view.x + margin);
  y = std::max(std::min(y, view.y + view.height - margin - panel.height),
               view.y + margin);
  const float inset = composition::Layer::arrow_size * 2.0F;
  const auto along = [inset](float wanted, float start, float length) {
    return std::clamp(wanted, start + inset,
                      std::max(start + length - inset, start + inset));
  };
  Arrow pointer;
  switch (edge) {
    case theme::Edge::bottom:
      pointer =
          Arrow{.x = along(middle_x, x, panel.width) - x, .y = panel.height};
      break;
    case theme::Edge::leading:
      pointer = Arrow{.x = 0.0F, .y = along(middle_y, y, panel.height) - y};
      break;
    case theme::Edge::trailing:
      pointer =
          Arrow{.x = panel.width, .y = along(middle_y, y, panel.height) - y};
      break;
    default:
      pointer = Arrow{.x = along(middle_x, x, panel.width) - x, .y = 0.0F};
      break;
  }
  return Popover{
      .dx = x - panel.x,
      .dy = y - panel.y,
      .arrow = anchoring.arrow ? std::optional<Arrow>{pointer} : std::nullopt};
}

void Painter::arrow(const Scope& scope, Box frame, std::uint32_t colour) {
  if (!scope.arrow.has_value()) {
    return;
  }
  const float scale = target_.scale.value();
  const float half = composition::Layer::arrow_size * scale;
  const float cx = frame.left + (scope.arrow->x * scale);
  const float cy = frame.top + (scope.arrow->y * scale);
  const std::uint32_t turned = adopt(
      scope.transform, Affine::rotation(std::numbers::pi / 4.0).around(cx, cy));
  transform_ = turned;
  const float side = half * std::numbers::sqrt2_v<float>;
  shape(provider::ShapeKind::fill,
        Box{.left = cx - (side * 0.5F),
            .top = cy - (side * 0.5F),
            .right = cx + (side * 0.5F),
            .bottom = cy + (side * 0.5F)},
        Corners{.radius = 2.0F * scale, .smoothing = 0.0F}, 0.0F, 0.0F, colour,
        scope.clip);
  transform_ = scope.transform;
}

auto Painter::relocate(Outline outline, std::uint32_t from,
                       std::uint32_t to) const noexcept
    -> std::optional<Outline> {
  if (from == to) {
    return outline;
  }
  const Affine& source = transforms_[from];
  const Affine& target = transforms_[to];
  const auto aligned = [](const Affine& matrix) {
    return matrix.b == 0.0F && matrix.c == 0.0F && matrix.a > 0.0F &&
           matrix.d > 0.0F;
  };
  if (!aligned(source) || !aligned(target)) {
    return std::nullopt;
  }
  const std::array<float, 2> near =
      source.apply({outline.box.left, outline.box.top});
  const std::array<float, 2> far =
      source.apply({outline.box.right, outline.box.bottom});
  const float factor = std::min(source.a / target.a, source.d / target.d);
  Corners corners = outline.corners;
  corners.radius *= factor;
  for (float& radius : corners.radii) {
    radius *= factor;
  }
  return Outline{.box = Box{.left = (near[0] - target.tx) / target.a,
                            .top = (near[1] - target.ty) / target.d,
                            .right = (far[0] - target.tx) / target.a,
                            .bottom = (far[1] - target.ty) / target.d},
                 .corners = corners};
}

auto Painter::confine(Outline ring, Box frame,
                      const Scope& scope) const noexcept -> Outline {
  if (!scope.enclosure.has_value()) {
    return ring;
  }
  const std::optional<Outline> found = relocate(
      scope.enclosure->outline, scope.enclosure->transform, scope.transform);
  if (!found.has_value()) {
    return ring;
  }
  constexpr float slack = 0.5F;
  const Box& edge = found->box;
  const bool inside =
      frame.left >= edge.left - slack && frame.top >= edge.top - slack &&
      frame.right <= edge.right + slack && frame.bottom <= edge.bottom + slack;
  const bool hugs =
      frame.left <= edge.left + slack && frame.top <= edge.top + slack &&
      frame.right >= edge.right - slack && frame.bottom >= edge.bottom - slack;
  if (!inside || hugs) {
    return ring;
  }
  const Box box{.left = std::max(ring.box.left, edge.left),
                .top = std::max(ring.box.top, edge.top),
                .right = std::min(ring.box.right, edge.right),
                .bottom = std::min(ring.box.bottom, edge.bottom)};
  if (uneven(found->corners) || uneven(ring.corners) ||
      found->corners.ellipse || ring.corners.ellipse) {
    const std::array<std::array<float, 2>, 4> insets{
        {{box.left - edge.left, box.top - edge.top},
         {edge.right - box.right, box.top - edge.top},
         {edge.right - box.right, edge.bottom - box.bottom},
         {box.left - edge.left, edge.bottom - box.bottom}}};
    std::array<float, 4> radii{};
    float smoothing = ring.corners.smoothing;
    bool changed = false;
    for (std::size_t corner = 0; corner < radii.size(); ++corner) {
      const float outer = outer_radius(*found, corner);
      radii[corner] = ring.corners.ellipse ? outer_radius(ring, corner)
                                           : corner_of(ring.corners, corner);
      const auto [across, down] = insets[corner];
      if (across < outer && down < outer &&
          outer - std::min(across, down) > radii[corner]) {
        radii[corner] = outer - std::min(across, down);
        smoothing = found->corners.smoothing;
        changed = true;
      }
    }
    if (!changed && box == ring.box) {
      return ring;
    }
    return Outline{
        .box = box,
        .corners = corners_of(radii, smoothing,
                              std::min(box.width(), box.height()) * 0.5F)};
  }
  const float outer = found->corners.radius;
  float needed = 0.0F;
  for (const float across : {box.left - edge.left, edge.right - box.right}) {
    for (const float down : {box.top - edge.top, edge.bottom - box.bottom}) {
      if (across < outer && down < outer) {
        needed = std::max(needed, outer - std::min(across, down));
      }
    }
  }
  Corners corners = ring.corners;
  if (needed > corners.radius) {
    corners = Corners{.radius = needed, .smoothing = found->corners.smoothing};
  }
  corners.radius =
      std::min(corners.radius, std::min(box.width(), box.height()) * 0.5F);
  return Outline{.box = box, .corners = corners};
}

auto Painter::enclosure_of(const composition::Modifiers& modifiers,
                           const Scope& scope, Box area, Corners painted,
                           bool glassed, bool filled) const
    -> std::optional<Outline> {
  if (modifiers.container_shape.has_value()) {
    const Box fit = fitted(*modifiers.container_shape, area);
    return Outline{.box = fit,
                   .corners = resolve(scope, *modifiers.container_shape, fit)};
  }
  if (glassed) {
    const Box fit = fitted(modifiers.glass->shape, area);
    return Outline{.box = fit,
                   .corners = resolve(scope, modifiers.glass->shape, fit)};
  }
  if (modifiers.material.has_value()) {
    const Box fit = fitted(modifiers.material->shape, area);
    return Outline{.box = fit,
                   .corners = resolve(scope, modifiers.material->shape, fit)};
  }
  if (filled || modifiers.border.has_value() || modifiers.clip) {
    return Outline{.box = area, .corners = painted};
  }
  return std::nullopt;
}

auto Painter::offscreen(const composition::Node& node, const Scope& parent,
                        const motion::Presentation& presentation) const noexcept
    -> bool {
  using Property = motion::Presentation::Property;
  const composition::Rect& laid = node.frame();
  if (laid.width <= 0.0F || laid.height <= 0.0F ||
      node.interactions().effects.pin.has_value() ||
      node.modifiers().effects != composition::GeometryEffects{} ||
      presentation.in_flight(Property::frame) || presentation.morphing()) {
    return false;
  }
  const float scale = target_.scale.value();
  const Box box = bounds(Box{.left = laid.x * scale,
                             .top = laid.y * scale,
                             .right = (laid.x + laid.width) * scale,
                             .bottom = (laid.y + laid.height) * scale},
                         parent.transform);
  const auto left = static_cast<float>(parent.clip.x);
  const auto top = static_cast<float>(parent.clip.y);
  const auto width = static_cast<float>(parent.clip.width);
  const auto height = static_cast<float>(parent.clip.height);
  const float margin_x =
      static_cast<float>(target_.pixels.width.value()) * cull_margin;
  const float margin_y =
      static_cast<float>(target_.pixels.height.value()) * cull_margin;
  return box.right < left - margin_x || box.left > left + width + margin_x ||
         box.bottom < top - margin_y || box.top > top + height + margin_y;
}

void Painter::place_culled(composition::Tree& tree,
                           const composition::Node& node, const Scope& parent) {
  const float scale = target_.scale.value();
  const composition::Rect& laid = node.frame();
  record(tree, Presence{.node = node.id(),
                        .frame = Box{.left = laid.x * scale,
                                     .top = laid.y * scale,
                                     .right = (laid.x + laid.width) * scale,
                                     .bottom = (laid.y + laid.height) * scale},
                        .transform = parent.transform,
                        .clip = parent.clip,
                        .hit_radius = 0.0F,
                        .opacity = parent.opacity,
                        .fade = parent.fade});
  for (const composition::Node& child : node.children()) {
    if (child.phase() == composition::NodeLifecyclePhase::live) {
      place_culled(tree, child, parent);
    }
  }
}

void Painter::suspend(composition::Tree& tree, const composition::Node& node) {
  tree.presentation(node).suspend(target_.time);
  for (const composition::Node& child : node.children()) {
    suspend(tree, child);
  }
}

auto Painter::covers(composition::Tree& tree, const composition::Node& cover,
                     const composition::Node& host, const Scope& scope)
    -> bool {
  constexpr float slack = 0.5F;
  const composition::Modifiers& modifiers = cover.modifiers();
  if (cover.ghost() || layered(cover) ||
      cover.phase() != composition::NodeLifecyclePhase::live ||
      !modifiers.background.has_value() || modifiers.opacity < 1.0F ||
      modifiers.effects != composition::GeometryEffects{} ||
      scope.theme.get().radius(modifiers.background->shape.radius) > 0.0F ||
      modifiers.background->shape.form != theme::ShapeForm::rounded ||
      !modifiers.background->fill.colour().has_value() ||
      modifiers.background->fill.shadow().has_value() ||
      scope.colour(*modifiers.background->fill.colour()).alpha < 1.0F) {
    return false;
  }
  const motion::Presentation& presentation = tree.presentation(cover);
  if (presentation.animating() || !presentation.observed()) {
    return false;
  }
  const composition::Rect& shown = cover.frame();
  const composition::Rect& under = host.frame();
  return shown.x <= under.x + slack && shown.y <= under.y + slack &&
         shown.x + shown.width >= under.x + under.width - slack &&
         shown.y + shown.height >= under.y + under.height - slack;
}

auto Painter::unobserved(composition::Tree& tree, const composition::Node& node)
    -> bool {
  if (!tree.presentation(node).observed()) {
    return true;
  }
  return std::ranges::any_of(node.children(),
                             [&tree](const composition::Node& child) {
                               return unobserved(tree, child);
                             });
}

void Painter::pace(const composition::Node& node,
                   const motion::Presentation& presentation) noexcept {
  if (!presentation.animating()) {
    return;
  }
  animating_ = true;
  if (const std::optional<double> rate = presentation.frame_rate()) {
    hinted_rate_ = std::max(hinted_rate_, *rate);
  } else {
    unhint(motion::UnhintedAnimation{
        .route = motion::UnhintedAnimation::Route::animation,
        .node = node.id().value(),
        .channel = presentation.unhinted_channel().value_or("")});
  }
}

void Painter::unhint(motion::UnhintedAnimation cause) noexcept {
  unhinted_ = true;
  if (!unhinted_cause_.has_value()) {
    unhinted_cause_ = cause;
  }
}

auto Painter::signature(const Scope& scope) const noexcept -> Signature {
  return Signature{.theme = &scope.theme.get(),
                   .scheme = scope.scheme,
                   .locale = &scope.locale.get(),
                   .foreground = scope.foreground,
                   .background_style = scope.background_style,
                   .opacity = scope.opacity,
                   .clip = scope.clip,
                   .reduce_motion = scope.reduce_motion,
                   .dynamic_type_size = scope.dynamic_type_size,
                   .dynamic_type_range = scope.dynamic_type_range,
                   .font = scope.font,
                   .text = scope.text,
                   .direction = scope.direction,
                   .focused = scope.focused,
                   .contrast = scope.contrast,
                   .reduce_transparency = scope.reduce_transparency,
                   .content_transition = scope.content_transition,
                   .viewport = scope.viewport,
                   .arrow = scope.arrow,
                   .focus_effect_disabled = scope.focus_effect_disabled,
                   .enclosure = scope.enclosure.has_value()
                                    ? std::optional{scope.enclosure->outline}
                                    : std::nullopt,
                   .redacted = scope.redacts(),
                   .hover_disabled = scope.hover_disabled,
                   .default_hover = scope.default_hover,
                   .hover_container = scope.hover_container,
                   .hover_group = scope.hover_group};
}

auto Painter::matches(const Signature& held, const Signature& wanted,
                      const theme::Theme::ColourRoles& roles) noexcept -> bool {
  if (held.theme == wanted.theme) {
    return held == wanted;
  }
  Signature retinted = held;
  retinted.theme = wanted.theme;
  return retinted == wanted && held.theme != nullptr &&
         wanted.theme != nullptr &&
         held.theme->paints_like(*wanted.theme, roles);
}

auto Painter::span() const noexcept -> Span {
  return Span{.quads = static_cast<std::uint32_t>(quads_.size()),
              .rings = static_cast<std::uint32_t>(rings_.size()),
              .transforms = static_cast<std::uint32_t>(transforms_.size()),
              .meshes = static_cast<std::uint32_t>(mesh_count_),
              .backdrops = static_cast<std::uint32_t>(backdrop_calls_.size()),
              .deferrals = static_cast<std::uint32_t>(deferrals_.size()),
              .presences = static_cast<std::uint32_t>(presences_.size()),
              .fades = static_cast<std::uint32_t>(fades_.size()),
              .fields = static_cast<std::uint32_t>(field_calls_.size())};
}

auto Painter::held_record(const composition::Node& node) noexcept
    -> std::optional<std::reference_wrapper<Record>> {
  const std::uint32_t slot = node.paint_slot();
  if (slot == 0 || slot > records_.size()) {
    return std::nullopt;
  }
  Record& record = records_[slot - 1];
  if (record.node != node.id() || record.generation != generation_) {
    return std::nullopt;
  }
  return record;
}

auto Painter::spliceable(const Record& record, Span from) const noexcept
    -> bool {
  return from.quads + record.extent.quads <= previous_main_ &&
         from.rings + record.extent.rings <= previous_rings_.size() &&
         from.transforms + record.extent.transforms <=
             previous_transforms_.size() &&
         from.meshes + record.extent.meshes <= previous_mesh_count_ &&
         from.backdrops + record.extent.backdrops <=
             previous_backdrop_calls_.size() &&
         from.deferrals + record.extent.deferrals <=
             previous_deferrals_.size() &&
         from.presences + record.extent.presences <=
             previous_presences_.size() &&
         from.fades + record.extent.fades <= previous_fades_.size() &&
         from.fields + record.extent.fields <= previous_field_calls_.size();
}

void Painter::reclaim_records(const composition::Tree& tree) {
  for (std::size_t index = 0; index < records_.size(); ++index) {
    Record& record = records_[index];
    if (record.node == vacant_record) {
      continue;
    }
    const auto slot = static_cast<std::uint32_t>(index + 1);
    const auto owner = tree.node(record.node);
    if (!owner.has_value() || owner->get().paint_slot() != slot) {
      record.node = vacant_record;
      free_records_.push_back(slot);
    }
  }
}

auto Painter::vacant_slot(composition::Tree& tree) -> std::uint32_t {
  if (free_records_.empty() && records_.size() == records_.capacity()) {
    reclaim_records(tree);
  }
  while (!free_records_.empty()) {
    const std::uint32_t slot = free_records_.back();
    free_records_.pop_back();
    if (records_[slot - 1].node == vacant_record) {
      return slot;
    }
  }
  records_.emplace_back();
  free_records_.reserve(records_.capacity());
  return static_cast<std::uint32_t>(records_.size());
}

auto Painter::slot_for(composition::Tree& tree, const composition::Node& node)
    -> Record& {
  std::uint32_t slot = node.paint_slot();
  if (slot != 0 && slot <= records_.size()) {
    const composition::NodeId holder = records_[slot - 1].node;
    if (holder == node.id()) {
      return records_[slot - 1];
    }
    const auto other =
        holder == vacant_record ? std::nullopt : tree.node(holder);
    if (!other.has_value() || other->get().paint_slot() != slot) {
      records_[slot - 1].node = node.id();
      return records_[slot - 1];
    }
  }
  slot = vacant_slot(tree);
  records_[slot - 1].node = node.id();
  tree.assign_paint_slot(node.id(), slot);
  return records_[slot - 1];
}

void Painter::use_page(std::uint32_t page) noexcept {
  if (page >= std::numeric_limits<std::uint64_t>::digits) {
    unsettle();
    return;
  }
  pages_ |= std::uint64_t{1} << page;
}

auto Painter::backdrop_for(std::uint64_t key, provider::PassRegion wanted,
                           provider::PassRegion core, float radius,
                           std::uint32_t behind)
    -> std::expected<Compositor::Surface, Compositor::BackdropRefusal> {
  place();
  auto surface = compositor_.backdrop(key, wanted, core, radius, behind);
  if (surface.has_value()) {
    backdrop_calls_.push_back(BackdropCall{.key = surface->key,
                                           .wanted = wanted,
                                           .core = core,
                                           .radius = radius,
                                           .behind = behind,
                                           .surface = *surface});
  } else {
    unsettle();
  }
  return surface;
}

void Painter::replay_backdrop(const BackdropCall& call, std::uint32_t behind) {
  auto surface = compositor_.backdrop(call.key, call.wanted, call.core,
                                      call.radius, behind);
  if (!surface.has_value()) {
    unsettle();
    return;
  }
  if (surface->texture != call.surface.texture ||
      surface->region != call.surface.region) {
    unsettle();
  }
  backdrop_calls_.push_back(BackdropCall{.key = surface->key,
                                         .wanted = call.wanted,
                                         .core = call.core,
                                         .radius = call.radius,
                                         .behind = behind,
                                         .surface = *surface});
  ++backdrops_;
}

auto Painter::slides(const Record& record, const Scope& parent) const noexcept
    -> bool {
  const Affine& now = transforms_[parent.transform];
  const Affine& was = record.transform;
  if (!record.translatable ||
      std::abs((was.a * was.d) - (was.b * was.c)) < 1.0e-12F) {
    return false;
  }
  const float margin_x =
      static_cast<float>(target_.pixels.width.value()) * cull_margin;
  const float margin_y =
      static_cast<float>(target_.pixels.height.value()) * cull_margin;
  const auto left = static_cast<float>(parent.clip.x);
  const auto top = static_cast<float>(parent.clip.y);
  const auto right = left + static_cast<float>(parent.clip.width);
  const auto bottom = top + static_cast<float>(parent.clip.height);
  if (record.reach.right < record.reach.left) {
    return true;
  }
  const Box box = carried(record.reach, was, now);
  return box.left >= left - margin_x && box.right <= right + margin_x &&
         box.top >= top - margin_y && box.bottom <= bottom + margin_y;
}

auto Painter::carried(Box box, const Affine& was,
                      const Affine& now) const noexcept -> Box {
  if (box.right < box.left) {
    return box;
  }
  if (now.a == was.a && now.b == was.b && now.c == was.c && now.d == was.d) {
    const float shift_x = now.tx - was.tx;
    const float shift_y = now.ty - was.ty;
    return Box{.left = box.left + shift_x,
               .top = box.top + shift_y,
               .right = box.right + shift_x,
               .bottom = box.bottom + shift_y};
  }
  const Affine delta = now.then(inverse(was));
  Box moved{.left = std::numeric_limits<float>::max(),
            .top = std::numeric_limits<float>::max(),
            .right = std::numeric_limits<float>::lowest(),
            .bottom = std::numeric_limits<float>::lowest()};
  for (const std::array<float, 2> corner :
       {std::array{box.left, box.top}, std::array{box.right, box.top},
        std::array{box.right, box.bottom}, std::array{box.left, box.bottom}}) {
    const std::array<float, 2> point = delta.apply(corner);
    moved.left = std::min(moved.left, point[0]);
    moved.top = std::min(moved.top, point[1]);
    moved.right = std::max(moved.right, point[0]);
    moved.bottom = std::max(moved.bottom, point[1]);
  }
  return moved;
}

auto Painter::inherited(const Record& record, Span from) const noexcept
    -> std::optional<Affine> {
  if (record.extent.transforms > 0) {
    return previous_transforms_[previous_adoptions_[from.transforms].parent];
  }
  if (record.extent.quads > 0) {
    return previous_transforms_[previous_quads_[from.quads].transform];
  }
  if (record.extent.rings > 0) {
    return previous_transforms_[previous_rings_[from.rings].transform];
  }
  return record.transform;
}

void Painter::reach(Box box) noexcept {
  reach_.left = std::min(reach_.left, box.left);
  reach_.top = std::min(reach_.top, box.top);
  reach_.right = std::max(reach_.right, box.right);
  reach_.bottom = std::max(reach_.bottom, box.bottom);
}

auto Painter::splice(composition::Tree& tree, const Record& held, Span from,
                     const Scope& parent) -> bool {
  const bool exact = held.transform == transforms_[parent.transform];
  if ((held.pages & atlas_.evicted_pages()) != 0 ||
      (!exact && !slides(held, parent))) {
    return false;
  }
  const std::span<const BackdropCall> calls =
      std::span{previous_backdrop_calls_}.subspan(from.backdrops,
                                                  held.extent.backdrops);
  if (!std::ranges::all_of(calls, [this](const BackdropCall& call) {
        return compositor_.predicts(call.key, call.wanted, call.radius,
                                    call.surface);
      })) {
    return false;
  }
  const std::span<const FieldCall> fields =
      std::span{previous_field_calls_}.subspan(from.fields, held.extent.fields);
  if (!std::ranges::all_of(fields, [this](const FieldCall& call) {
        return compositor_.holds_field(call.key, call.texture);
      })) {
    return false;
  }
  const Span at = span();
  const std::uint32_t first = from.transforms;
  const std::uint32_t last = from.transforms + held.extent.transforms;
  const auto moved = [&](std::uint32_t index) {
    if (index >= first && index < last) {
      return index - first + at.transforms;
    }
    return index == 0 && held.parent_transform != 0 ? 0U : parent.transform;
  };
  const auto remeshed = [&](std::uint32_t mesh) {
    return mesh == 0 ? 0U : mesh - from.meshes + at.meshes;
  };
  const std::uint32_t fade_first = from.fades;
  const std::uint32_t fade_last = from.fades + held.extent.fades;
  const auto refaded = [&](std::uint32_t index) {
    return index >= fade_first && index < fade_last
               ? index - fade_first + at.fades
               : parent.fade;
  };
  for (std::uint32_t index = fade_first; index < fade_last; ++index) {
    const Fade& was = previous_fades_[index];
    const std::uint32_t outer = refaded(was.parent);
    fades_.push_back(Fade{.parent = outer,
                          .local = was.local,
                          .value = fades_[outer].value * was.local});
  }
  const bool refades = held.faded != fades_[parent.fade].value;
  for (std::uint32_t index = first; index < last; ++index) {
    const Adoption& adoption = previous_adoptions_[index];
    const std::uint32_t outer = moved(adoption.parent);
    adoptions_.push_back(Adoption{.parent = outer, .local = adoption.local});
    transforms_.push_back(exact ? previous_transforms_[index]
                                : transforms_[outer].then(adoption.local));
  }
  for (std::uint32_t index = 0; index < held.extent.meshes; ++index) {
    const ShapePainter::Geometry& mesh = previous_meshes_[from.meshes + index];
    if (mesh.texture.has_value()) {
      shapes_.retain(*mesh.texture);
    }
    stage_mesh(mesh, mesh.revision);
  }
  for (std::uint32_t index = 0; index < held.extent.quads; ++index) {
    Quad& quad = quads_.emplace_back(previous_quads_[from.quads + index]);
    quad.transform = moved(quad.transform);
    if (quad.mesh != 0) {
      quad.mesh = remeshed(quad.mesh);
      shapes_.retain(composition::NodeId{quad.owner});
    }
    quad.binding = 0;
    quad.fresh = false;
    quad.fade = refaded(quad.fade);
  }
  for (std::uint32_t index = 0; index < held.extent.rings; ++index) {
    Quad& quad = rings_.emplace_back(previous_rings_[from.rings + index]);
    quad.transform = moved(quad.transform);
    quad.mesh = remeshed(quad.mesh);
    quad.binding = 0;
    quad.fresh = false;
    quad.fade = refaded(quad.fade);
  }
  for (const BackdropCall& call : calls) {
    replay_backdrop(call, call.behind - from.quads + at.quads);
  }
  for (const FieldCall& call : fields) {
    compositor_.retain_field(call.key);
    field_calls_.push_back(call);
  }
  for (const Deferred& deferral : std::span{previous_deferrals_}.subspan(
           from.deferrals, held.extent.deferrals)) {
    Deferred moved_deferral = deferral;
    moved_deferral.order = deferred_order_++;
    deferred_.push_back(moved_deferral);
    deferrals_.push_back(moved_deferral);
  }
  if (exact && held.extent.quads > 0) {
    splices_.push_back(
        Splice{.from = from.quads, .to = at.quads, .count = held.extent.quads});
  }
  if (exact && held.extent.rings > 0) {
    ring_splices_.push_back(
        Splice{.from = from.rings, .to = at.rings, .count = held.extent.rings});
  }
  for (const Presence& presence : std::span{previous_presences_}.subspan(
           from.presences, held.extent.presences)) {
    Presence moved_presence = presence;
    moved_presence.transform = moved(presence.transform);
    moved_presence.fade = refaded(presence.fade);
    if (exact && !refades) {
      presences_.push_back(moved_presence);
    } else {
      record(tree, moved_presence);
    }
  }
  pages_ |= held.pages;
  atlas_.retain(held.pages);
  ++spliced_;
  return true;
}

auto Painter::repriseable(composition::Tree& tree,
                          const composition::Node& node, const Record& record,
                          Span from, const Scope& parent) const -> bool {
  const Own& own = *record.own;
  if ((own.pages & atlas_.evicted_pages()) != 0 ||
      record.transform != transforms_[parent.transform] ||
      record.faded != fades_[parent.fade].value ||
      tree.presentation(node).animating()) {
    return false;
  }
  if (own.head.rings != 0 || own.head.deferrals != 0 || own.head.fields != 0 ||
      own.tail.transforms != 0 || own.tail.fades != 0 ||
      own.tail.backdrops != 0 || own.tail.deferrals != 0 ||
      own.tail.presences != 0 || own.tail.fields != 0) {
    return false;
  }
  const std::span<const BackdropCall> calls =
      std::span{previous_backdrop_calls_}.subspan(from.backdrops,
                                                  own.head.backdrops);
  return std::ranges::all_of(calls, [this](const BackdropCall& call) {
    return compositor_.predicts(call.key, call.wanted, call.radius,
                                call.surface);
  });
}

void Painter::reprise_head(Own own, std::uint32_t parent_transform, Span from,
                           Span extent, const Scope& parent, Visit& visit) {
  Reprise& reprise =
      reprises_.emplace_back(Reprise{.own = std::move(own),
                                     .from = from,
                                     .extent = extent,
                                     .at = span(),
                                     .parent_transform = parent_transform,
                                     .outer_transform = parent.transform,
                                     .outer_fade = parent.fade});
  const Span& at = reprise.at;
  const Span& head = reprise.own.head;
  for (std::uint32_t index = from.fades; index < from.fades + head.fades;
       ++index) {
    const Fade& was = previous_fades_[index];
    const std::uint32_t outer = reprised_fade(reprise, was.parent);
    fades_.push_back(Fade{.parent = outer,
                          .local = was.local,
                          .value = fades_[outer].value * was.local});
  }
  for (std::uint32_t index = from.transforms;
       index < from.transforms + head.transforms; ++index) {
    const Adoption& adoption = previous_adoptions_[index];
    adoptions_.push_back(
        Adoption{.parent = reprised_transform(reprise, adoption.parent),
                 .local = adoption.local});
    transforms_.push_back(previous_transforms_[index]);
  }
  reprise_copy(reprise, from, head);
  for (const BackdropCall& call : std::span{previous_backdrop_calls_}.subspan(
           from.backdrops, head.backdrops)) {
    replay_backdrop(call, call.behind - from.quads + at.quads);
  }
  for (const Presence& presence :
       std::span{previous_presences_}.subspan(from.presences, head.presences)) {
    Presence moved_presence = presence;
    moved_presence.transform = reprised_transform(reprise, presence.transform);
    moved_presence.fade = reprised_fade(reprise, presence.fade);
    presences_.push_back(moved_presence);
  }
  if (reprise.own.reach.right >= reprise.own.reach.left) {
    reach(reprise.own.reach);
  }
  positional_ = positional_ || !reprise.own.translatable;
  pages_ |= reprise.own.pages;
  atlas_.retain(reprise.own.pages);
  reprise.own.inner.transform =
      reprise.own.inner_transform.has_value()
          ? at.transforms + *reprise.own.inner_transform
          : reprise.outer_transform;
  reprise.own.inner.fade = reprise.own.inner_fade.has_value()
                               ? at.fades + *reprise.own.inner_fade
                               : reprise.outer_fade;
  if (reprise.own.inner.enclosure.has_value()) {
    reprise.own.inner.enclosure =
        reprise.own.encloses
            ? std::optional{Enclosure{
                  .outline = reprise.own.inner.enclosure->outline,
                  .transform = reprise.own.inner.transform}}
            : parent.enclosure;
  }
  visit.inner = reprise.own.inner;
  visit.stage = Stage::reprised;
}

void Painter::reprise_tail() {
  Reprise& reprise = reprises_.back();
  reprise_copy(reprise, reprise.from + reprise.extent - reprise.own.tail,
               reprise.own.tail);
  staged_ = std::move(reprise.own);
}

auto Painter::reprised_transform(const Reprise& reprise,
                                 std::uint32_t index) const noexcept
    -> std::uint32_t {
  const Span& from = reprise.from;
  if (index >= from.transforms &&
      index < from.transforms + reprise.own.head.transforms) {
    return index - from.transforms + reprise.at.transforms;
  }
  return index == 0 && reprise.parent_transform != 0 ? 0U
                                                     : reprise.outer_transform;
}

auto Painter::reprised_fade(const Reprise& reprise,
                            std::uint32_t index) const noexcept
    -> std::uint32_t {
  const Span& from = reprise.from;
  return index >= from.fades && index < from.fades + reprise.own.head.fades
             ? index - from.fades + reprise.at.fades
             : reprise.outer_fade;
}

void Painter::reprise_copy(const Reprise& reprise, Span source, Span count) {
  const Span target = span();
  for (std::uint32_t index = 0; index < count.meshes; ++index) {
    const ShapePainter::Geometry& mesh =
        previous_meshes_[source.meshes + index];
    if (mesh.texture.has_value()) {
      shapes_.retain(*mesh.texture);
    }
    stage_mesh(mesh, mesh.revision);
  }
  const auto remeshed = [&](std::uint32_t mesh) {
    return mesh == 0 ? 0U : mesh - source.meshes + target.meshes;
  };
  for (std::uint32_t index = 0; index < count.quads; ++index) {
    Quad& quad = quads_.emplace_back(previous_quads_[source.quads + index]);
    quad.transform = reprised_transform(reprise, quad.transform);
    if (quad.mesh != 0) {
      quad.mesh = remeshed(quad.mesh);
      shapes_.retain(composition::NodeId{quad.owner});
    }
    quad.binding = 0;
    quad.fresh = false;
    quad.fade = reprised_fade(reprise, quad.fade);
  }
  for (std::uint32_t index = 0; index < count.rings; ++index) {
    Quad& quad = rings_.emplace_back(previous_rings_[source.rings + index]);
    quad.transform = reprised_transform(reprise, quad.transform);
    quad.mesh = remeshed(quad.mesh);
    quad.binding = 0;
    quad.fresh = false;
    quad.fade = reprised_fade(reprise, quad.fade);
  }
  if (count.quads > 0) {
    splices_.push_back(
        Splice{.from = source.quads, .to = target.quads, .count = count.quads});
  }
  if (count.rings > 0) {
    ring_splices_.push_back(
        Splice{.from = source.rings, .to = target.rings, .count = count.rings});
  }
}

void Painter::forget_since(std::size_t first) {
  for (const std::uint32_t slot : std::span{commits_}.subspan(commit_mark_)) {
    records_[slot - 1].reached = 0;
    records_[slot - 1].reusable = false;
  }
  commits_.resize(commit_mark_);
  std::erase_if(splices_,
                [first](const Splice& splice) { return splice.to >= first; });
}

void Painter::rehighlight(composition::Tree& tree, const Highlights& next) {
  const Highlights& was = highlights_;
  const auto touch = [&tree](std::optional<composition::NodeId> node) {
    if (node.has_value()) {
      tree.repaint(*node);
    }
  };
  if (was.focused != next.focused || was.focus_visible != next.focus_visible ||
      was.tilt != next.tilt) {
    touch(was.focused);
    touch(next.focused);
  }
  if (was.pressed != next.pressed || was.touched != next.touched ||
      was.contact != next.contact) {
    touch(was.pressed);
    touch(next.pressed);
    touch(was.touched);
    touch(next.touched);
  }
  if (was.shake != next.shake) {
    if (was.shake.has_value()) {
      touch(was.shake->node);
    }
    if (next.shake.has_value()) {
      touch(next.shake->node);
    }
  }
  if (was.hovering != next.hovering) {
    std::erase_if(hover_nodes_, [&tree](composition::NodeId id) {
      if (!tree.node(id).has_value()) {
        return true;
      }
      tree.repaint(id);
      return false;
    });
  }
}

void Painter::forget_phases(const composition::Tree& tree) {
  if (phases_.empty() || tree.revision() == phases_revision_) {
    return;
  }
  phases_revision_ = tree.revision();
  std::erase_if(phases_, [&tree](const auto& entry) {
    return !tree.node(composition::NodeId{entry.first.first}).has_value();
  });
}

void Painter::begin_frame(composition::Tree& tree, const Target& target,
                          const Highlights& highlights) {
  ++frame_;
  const text::Typesetter* typesetter =
      typesetter_.has_value() ? &typesetter_->get() : nullptr;
  retaining_ = !full_walk_ && target.zoom == 1.0F && !glass_probe_;
  forget_canonical();
  if (!retaining_ || painted_tree_ != &tree ||
      painted_scale_ != target.scale.value() ||
      recorded_pixels_ != target.pixels || painted_typesetter_ != typesetter) {
    ++generation_;
  }
  painted_tree_ = &tree;
  painted_scale_ = target.scale.value();
  recorded_pixels_ = target.pixels;
  painted_typesetter_ = typesetter;
  rehighlight(tree, highlights);
  exchange(quads_, previous_quads_);
  exchange(rings_, previous_rings_);
  exchange(transforms_, previous_transforms_);
  exchange(adoptions_, previous_adoptions_);
  exchange(fades_, previous_fades_);
  exchange(presences_, previous_presences_);
  presences_.clear();
  std::erase_if(clip_sources_, [this](const auto& entry) {
    return entry.second.used + clip_source_frames < frame_;
  });
  exchange(replays_, previous_replays_);
  exchange(quad_hashes_, previous_hashes_);
  exchange(backdrop_calls_, previous_backdrop_calls_);
  exchange(field_calls_, previous_field_calls_);
  exchange(deferrals_, previous_deferrals_);
  exchange(meshes_, previous_meshes_);
  if (meshes_.size() < previous_meshes_.size()) {
    meshes_.resize(previous_meshes_.size());
  }
  for (std::size_t index = 0; index < previous_meshes_.size(); ++index) {
    ShapePainter::Geometry& slot = meshes_[index];
    const ShapePainter::Geometry& other = previous_meshes_[index];
    slot.vertices.reserve(other.vertices.capacity());
    slot.indices.reserve(other.indices.capacity());
  }
  backdrop_calls_.clear();
  field_calls_.clear();
  deferrals_.clear();
  previous_mesh_count_ = mesh_count_;
  previous_main_ = main_count_;
  splices_.clear();
  ring_splices_.clear();
  commits_.clear();
  commit_mark_ = 0;
  recorded_ = 0;
  spliced_ = 0;
  pages_ = 0;
  unsettled_ = false;
}

auto Painter::walk(composition::Tree& tree, const composition::Node& node,
                   const Scope& parent) -> std::expected<void, PaintError> {
  failure_.reset();
  Walker walker{*this, tree};
  walk_.run(node, Visit{.inner = parent}, walker);
  if (failure_.has_value()) {
    return std::unexpected(*std::exchange(failure_, std::nullopt));
  }
  return {};
}

auto Painter::Walker::enter(const composition::Node& node, const Visit& above)
    -> Walk::Step {
  return painter_.get().open(tree_, node, above);
}

auto Painter::Walker::next(const composition::Node& node, Visit& visit)
    -> const composition::Node* {
  return painter_.get().advance(tree_, node, visit);
}

void Painter::Walker::leave(const composition::Node& node, Visit& visit) {
  painter_.get().close(tree_, node, visit);
}

auto Painter::open(composition::Tree& tree, const composition::Node& node,
                   const Visit& above) -> Walk::Step {
  Scope parent = above.inner;
  if (above.cursor.offset) {
    const float scale = target_.scale.value();
    const Popover& anchored = *above.cursor.anchored;
    parent.transform =
        adopt(above.inner.transform,
              Affine::translation(anchored.dx * scale, anchored.dy * scale));
    parent.arrow = anchored.arrow;
    parent.enclosure.reset();
  }
  Walk::Step step{.state = Visit{.inner = parent}};
  Visit& visit = step.state;
  const auto painted = [&]() {
    auto head = paint_head(tree, node, parent, visit);
    if (!head.has_value()) {
      failure_ = head.error();
      step.descend = false;
    } else {
      step.descend = *head;
    }
    visit.descended = step.descend;
  };
  if (!retaining_) {
    ++recorded_;
    node.cost().recorded();
    painted();
    return step;
  }
  const Lineage outer = lineage_;
  const Span begin = span();
  const Signature wanted = signature(parent);
  std::optional<Span> previous;
  std::uint64_t walked = 0;
  std::optional<Reprisal> reprised;
  if (const auto held = held_record(node)) {
    Record& record = held->get();
    if (outer.previous.has_value() && record.reached == outer.walked) {
      previous = *outer.previous + record.offset;
    }
    const bool candidate = previous.has_value() && record.reusable &&
                           record.stamp == node.paint_stamp() &&
                           matches(record.signature, wanted, record.roles) &&
                           record.united == united_.contains(node.id()) &&
                           spliceable(record, *previous);
    const std::optional<Affine> was =
        candidate ? inherited(record, *previous) : std::nullopt;
    if (was.has_value()) {
      record.reach = carried(record.reach, record.transform, *was);
      record.transform = *was;
    }
    if (was.has_value() && splice(tree, record, *previous, parent)) {
      record.signature = wanted;
      roles_ |= record.roles;
      const Affine& now = transforms_[parent.transform];
      record.reach = carried(record.reach, record.transform, now);
      record.transform = now;
      record.parent_transform = parent.transform;
      record.faded = fades_[parent.fade].value;
      record.offset = begin - outer.begin;
      record.reached = frame_;
      commits_.push_back(node.paint_slot());
      reach(record.reach);
      positional_ = positional_ || !record.translatable;
      visit.stage = Stage::spliced;
      step.descend = false;
      return step;
    }
    walked = record.walked;
    if (previous.has_value() && record.own.has_value() &&
        record.own_stamp == node.own_paint_stamp() &&
        matches(record.signature, wanted, record.roles) &&
        record.united == united_.contains(node.id()) &&
        spliceable(record, *previous) &&
        repriseable(tree, node, record, *previous, parent)) {
      reprised = Reprisal{.own = *record.own,
                          .parent_transform = record.parent_transform,
                          .extent = record.extent,
                          .roles = record.roles};
    }
  }
  Retention& retention = visit.retention;
  retention.active = true;
  retention.outer = outer;
  retention.begin = begin;
  retention.wanted = wanted;
  retention.parent_transform = parent.transform;
  retention.parent_fade = parent.fade;
  retention.outer_roles =
      std::exchange(roles_, reprised.has_value() ? reprised->roles
                                                 : theme::Theme::ColourRoles{});
  lineage_ = Lineage{.previous = previous, .walked = walked, .begin = begin};
  retention.outer_unsettled = std::exchange(unsettled_, false);
  retention.outer_positional = std::exchange(positional_, false);
  retention.outer_reach = std::exchange(
      reach_, Box{.left = std::numeric_limits<float>::max(),
                  .top = std::numeric_limits<float>::max(),
                  .right = std::numeric_limits<float>::lowest(),
                  .bottom = std::numeric_limits<float>::lowest()});
  retention.outer_pages = std::exchange(pages_, 0);
  retention.outer_mark = std::exchange(commit_mark_, commits_.size());
  retention.stamp = node.paint_stamp();
  retention.own_stamp = node.own_paint_stamp();
  staged_.reset();
  if (reprised.has_value()) {
    ++spliced_;
  } else {
    ++recorded_;
    node.cost().recorded();
  }
  if (reprised.has_value()) {
    reprise_head(std::move(reprised->own), reprised->parent_transform,
                 *previous, reprised->extent, parent, visit);
    visit.descended = true;
    begin_children(tree, node, visit, std::nullopt);
    return step;
  }
  painted();
  return step;
}

void Painter::close(composition::Tree& tree, const composition::Node& node,
                    Visit& visit) {
  if (visit.stage == Stage::spliced) {
    return;
  }
  if (visit.descended) {
    if (visit.stage == Stage::reprised) {
      if (!failure_.has_value()) {
        reprise_tail();
      }
      reprises_.pop_back();
    } else {
      if (failure_.has_value()) {
        paint_unwind();
      } else {
        paint_tail(node, visit);
      }
      paintings_.pop_back();
    }
  }
  if (visit.retention.active) {
    commit_visit(tree, node, visit);
  }
}

void Painter::commit_visit(composition::Tree& tree,
                           const composition::Node& node, Visit& visit) {
  const Retention& retention = visit.retention;
  std::optional<Own> own = std::exchange(staged_, std::nullopt);
  const bool unsettled =
      unsettled_ || failure_.has_value() || tree.presentation(node).animating();
  Record& record = slot_for(tree, node);
  record = Record{.node = node.id(),
                  .stamp = retention.stamp,
                  .own_stamp = retention.own_stamp,
                  .own = std::move(own),
                  .generation = generation_,
                  .walked = frame_,
                  .reached = frame_,
                  .reusable = !unsettled,
                  .translatable = !positional_,
                  .united = united_.contains(node.id()),
                  .signature = retention.wanted,
                  .roles = roles_,
                  .transform = transforms_[retention.parent_transform],
                  .parent_transform = retention.parent_transform,
                  .faded = fades_[retention.parent_fade].value,
                  .reach = reach_,
                  .offset = retention.begin - retention.outer.begin,
                  .extent = span() - retention.begin,
                  .pages = pages_};
  commits_.push_back(node.paint_slot());
  roles_ |= retention.outer_roles;
  lineage_ = retention.outer;
  unsettled_ = retention.outer_unsettled || unsettled;
  positional_ = retention.outer_positional || positional_;
  const Box inner_reach = std::exchange(reach_, retention.outer_reach);
  if (inner_reach.right >= inner_reach.left) {
    reach(inner_reach);
  }
  pages_ |= retention.outer_pages;
  commit_mark_ = retention.outer_mark;
}

auto Painter::paint_head(composition::Tree& tree, const composition::Node& node,
                         const Scope& parent, Visit& visit)
    -> std::expected<bool, PaintError> {
  using Property = motion::Presentation::Property;
  using Tint = motion::Presentation::Tint;
  const Span entry = span();
  if (node.ghost()) {
    harvest(tree, node, parent);
    return false;
  }
  Scope scope = scoped(node, parent);
  missing_symbol(node);
  motion::Presentation& presentation = tree.presentation(node);
  if (presentation.concealed()) {
    conceal(tree, node);
    return false;
  }
  if (presentation.withheld()) {
    harvest(tree, node, parent);
    return false;
  }
  ++visited_;
  if (offscreen(node, parent, presentation) && !unobserved(tree, node)) {
    ++culled_;
    place();
    suspend(tree, node);
    place_culled(tree, node, parent);
    return false;
  }
  reach(bounds(pixels_of(node), parent.transform));
  if (!node.interactions().effects.empty()) {
    place();
  }
  presentation.resume(target_.time);
  const motion::Presentation::Moment moment{
      .transaction = tree.transaction(node),
      .now = target_.time,
      .reduce_motion = scope.reduce_motion,
      .cross_fade = scope.cross_fade,
      .environment = [&tree, &node] { return tree.environment_values(node); }};
  const composition::Modifiers& modifiers = node.modifiers();
  const composition::Rect& laid = node.frame();
  const std::optional<composition::Rect> followed = follow(tree, node);
  const composition::Rect& wanted = followed.value_or(laid);
  const bool seen = presentation.observed();
  if (!seen || followed.has_value()) {
    unsettle();
  }
  const composition::Point origin =
      parent.group_origin.value_or(composition::Point{});
  motion::AnimatableVector placed = presentation.animate(
      Property::frame,
      motion::AnimatableVector{wanted.x - origin.x, wanted.y - origin.y,
                               wanted.width, wanted.height},
      moment);
  placed = placed + motion::AnimatableVector{origin.x, origin.y};
  const bool grouping =
      modifiers.geometry_group && presentation.in_flight(Property::frame);
  const bool carries = followed.has_value() || presentation.morphing();
  const std::optional<motion::Transition::Effect> effect =
      presentation.appearance(moment);
  const auto pressable = node.content_as<components::Button::Content>();
  const float dimming =
      !scope.enabled && pressable.has_value() && pressable->get().automatic()
          ? components::ButtonStyle::disabled_opacity
          : 1.0F;
  const auto animated = static_cast<float>(presentation.animate(
      Property::opacity, motion::AnimatableVector{modifiers.opacity * dimming},
      moment)[0]);
  const bool fading = presentation.in_flight(Property::opacity);
  float transient = fading ? animated : 1.0F;
  if (effect.has_value()) {
    transient *= effect->opacity;
  }
  const theme::Theme& theme = scope.theme;
  const float scale = target_.scale.value();
  const Box frame{.left = static_cast<float>(placed[0]) * scale,
                  .top = static_cast<float>(placed[1]) * scale,
                  .right = static_cast<float>(placed[0] + placed[2]) * scale,
                  .bottom = static_cast<float>(placed[1] + placed[3]) * scale};
  std::optional<composition::Rect> holder_frame;
  if (node.interactions().effects.pin.has_value() &&
      node.parent().has_value()) {
    if (const auto holder = tree.node(*node.parent())) {
      holder_frame = holder->get().frame();
    }
  }
  const std::optional<Shading> shaded =
      shading(node, parent, presentation, moment, frame, holder_frame);
  if (shaded.has_value()) {
    transient *= shaded->opacity;
  }
  Hover hover = hover_of(node, scope, frame, presentation, moment);
  transient *= hover.opacity;
  if (!node.interactions().effects.effects.empty()) {
    unsettle();
  }
  const float resting = fading ? 1.0F : animated;
  const composition::Interactions& declared = node.interactions();
  const auto& container = declared.scrolling.container;
  const std::optional<Popover> anchored =
      layered(node) ? popover(tree, node) : std::nullopt;
  EffectPlan plan =
      effect_plan(node, scope, laid, shaded, effect, presentation, moment);
  const float effect_factor = std::clamp(resting * transient, 0.0F, 1.0F);
  const bool effecting =
      plan.layered() && effect_factor > 0.0F && !anchored.has_value();
  const bool promoting =
      !effecting && !anchored.has_value() && !container.has_value() &&
      promotable(node, presentation, effect, parent, carries);
  if (effecting) {
    scope.opacity = parent.opacity;
    scope.fade = parent.fade;
  } else if (promoting) {
    scope.opacity =
        parent.opacity * std::clamp(resting * transient, 0.0F, 1.0F);
  } else {
    const float passing = std::clamp(transient, 0.0F, 1.0F);
    scope.opacity = parent.opacity * std::clamp(resting, 0.0F, 1.0F) *
                    (passing > 0.0F ? 1.0F : 0.0F);
    scope.fade = passing > 0.0F ? fade(parent.fade, passing) : parent.fade;
  }
  if (shown(scope) <= 0.0F && seen && !unobserved(tree, node)) {
    if (presentation.animating()) {
      record(tree, Presence{.node = node.id(),
                            .frame = frame,
                            .transform = parent.transform,
                            .clip = parent.clip,
                            .hit_radius = 0.0F,
                            .opacity = 0.0F});
    } else {
      conceal(tree, node);
    }
    if (presentation.animating()) {
      animating_ = true;
      unhint(motion::UnhintedAnimation{
          .route = motion::UnhintedAnimation::Route::hidden,
          .node = node.id().value(),
          .channel = presentation.unhinted_channel().value_or("hinted")});
    } else {
      settle(tree, node);
    }
    return false;
  }
  const std::array<float, 2> shake = shaken(node.id());
  hover.offset_x += shake[0];
  hover.offset_y += shake[1];
  const Box snapped = snap(frame);
  Affine local =
      transform_of(modifiers, presentation, moment, effect, hover, frame,
                   static_cast<float>(placed[2]), parent.direction);
  if (mirrors(modifiers, parent.direction, scope.direction)) {
    local = local.then(Affine::scaling(-1.0F, 1.0F)
                           .around((frame.left + frame.right) * 0.5F,
                                   (frame.top + frame.bottom) * 0.5F));
  }
  if (shaded.has_value()) {
    local = shaded->transform.then(local);
  }
  if (plan.affine.has_value()) {
    const graphics::AffineTransform& applied = *plan.affine;
    local = local.then(Affine{.a = applied.a(),
                              .b = applied.b(),
                              .c = applied.c(),
                              .d = applied.d(),
                              .tx = applied.tx() * scale,
                              .ty = applied.ty() * scale}
                           .around(frame.left, frame.top));
  }
  scope.transform = adopt(parent.transform, local);
  transform_ = scope.transform;
  const std::uint32_t enclosing = binding_;
  binding_ = bind(presentation, parent.transform);
  const bool binds = binding_ != enclosing;
  float hit_radius = 0.0F;
  if (declared.hit_shape.has_value()) {
    hit_radius =
        rounding(theme, *declared.hit_shape, snapped, scale).radius / scale;
  }
  const std::optional<Corners> clip_corners =
      shaped_clip(modifiers, scope, snapped);
  if (shown(scope) > 0.0F) {
    const HitRegion region = hit_region(node, theme, frame, scale);
    record(tree, Presence{.node = node.id(),
                          .frame = frame,
                          .transform = scope.transform,
                          .clip = parent.clip,
                          .hit_radius = hit_radius,
                          .opacity = scope.opacity,
                          .fade = scope.fade,
                          .shaped = clip_corners,
                          .hit_path = declared.hit_path,
                          .hit_fill = declared.hit_fill,
                          .drawn = region.drawn,
                          .hollow = region.hollow});
  } else {
    conceal(tree, node);
  }
  const std::size_t own_from = quads_.size();
  const std::size_t own_backdrops = compositor_.pending_backdrops();
  const auto button = node.content_as<components::Button::Content>();
  const bool automatic = button.has_value() && button->get().automatic();
  const std::optional<theme::Glass> glass = glass_of(modifiers, theme);
  const auto shaped = node.content_as<components::ShapeView::Content>();
  const bool glassed = glass.has_value() &&
                       !(shaped.has_value() && shaped->get().follows_glass());
  const bool glazes = glassed && !united_.contains(node.id());
  const theme::Shape outline =
      painted_outline(modifiers, button.has_value(), glassed);
  const float corner_target =
      std::min(theme.radius(outline.radius),
               std::max(std::min(laid.width, laid.height) * 0.5F, 0.0F));
  const std::optional<double> previous_corner =
      presentation.snapshot().corner_radius;
  const motion::AnimatableVector corner = presentation.animate(
      Property::corner_radius, motion::AnimatableVector{corner_target}, moment);
  const Box outlined = fitted(outline, snapped);
  Corners rounded = resolve(scope, outline, outlined);
  if (presentation.in_flight(Property::corner_radius) && !uneven(rounded) &&
      !rounded.ellipse) {
    rounded.radius =
        std::clamp(static_cast<float>(corner[0]) * scale, 0.0F,
                   std::min(outlined.width(), outlined.height()) * 0.5F);
  }
  blend_corners(presentation, moment, previous_corner, rounded, outlined);
  const ShadowMotion reshaping =
      presentation.in_flight(Property::corner_radius) ||
              presentation.in_flight(motion::Presentation::Mix::corners)
          ? ShadowMotion::moving
          : ShadowMotion::settled;
  const bool hovering = hover.present;
  const float lifted = std::clamp(hover.lift, 0.0F, 1.0F);
  const bool lifting = hovering && lifted > 0.0F;
  const float weight = glazes ? GlassLens::cast(*glass) : 1.0F;
  const Corners hover_corners =
      hovering && node.interactions().hover_shape.has_value()
          ? resolve(scope, *node.interactions().hover_shape, snapped)
          : rounded;
  if (lifting && lifted > visible_threshold) {
    drop_shadow(scope,
                theme::Colour{
                    0.0F, 0.0F, 0.0F,
                    motion::HoverEffect::lift_shadow_opacity * lifted * weight},
                motion::HoverEffect::lift_shadow_radius * lifted, 0.0F,
                motion::HoverEffect::lift_shadow_drop * lifted, outlined,
                hover_corners, reshaping);
    shape(provider::ShapeKind::shadow, outlined,
          Corners{.radius = hover_corners.radius, .smoothing = 0.0F}, 0.0F,
          std::max(motion::HoverEffect::glow_softness * scale, 0.5F),
          pack(scope.colour(theme::ColourRole::focus_ring),
               scope.opacity * motion::HoverEffect::glow_opacity * lifted),
          scope.clip);
  }
  shadows(scope, modifiers.shadow, outlined, rounded, weight, reshaping);
  if (modifiers.drop_shadow.has_value()) {
    const composition::DropShadow& cast = *modifiers.drop_shadow;
    const motion::AnimatableVector geometry = presentation.animate(
        Property::shadow_geometry,
        motion::AnimatableVector{cast.radius, cast.x, cast.y}, moment);
    drop_shadow(
        scope,
        presentation.animate(Tint::shadow, scope.colour(cast.colour), moment),
        static_cast<float>(geometry[0]), static_cast<float>(geometry[1]),
        static_cast<float>(geometry[2]), outlined, rounded,
        presentation.in_flight(Property::shadow_geometry) ? ShadowMotion::moving
                                                          : reshaping);
  }
  if (modifiers.material.has_value()) {
    const Box frosted = fitted(modifiers.material->shape, snapped);
    frost(node.id(), scope, modifiers.material->material, frosted,
          resolve(scope, modifiers.material->shape, frosted), own_from);
  }
  std::optional<theme::ColourValue> fill;
  if (node.content_as<components::GlassEffectContainer::Content>()
          .has_value()) {
    unite(tree, node, scope);
  }
  if (glazes) {
    const bool cast_own = modifiers.shadow != theme::Elevation::none ||
                          modifiers.drop_shadow.has_value();
    const theme::Shape surface = modifiers.glass->shape;
    const Box glazed_box = fitted(surface, snapped);
    glaze(
        node.id(), scope, *glass, glazed_box,
        resolve(scope, surface, glazed_box), held(node.id()),
        Backing{.behind = own_from,
                .frost = modifiers.glass->frost,
                .shadow = cast_own ? 0.0F : 1.0F - (lifting ? lifted : 0.0F)});
  }
  const bool glazed = glass.has_value() && modifiers.glass->slot.has_value() &&
                      !modifiers.glass->glass.has_value();
  const graphics::ShapeStyle* styled_fill_style = nullptr;
  if (glazed) {
    fill.reset();
  } else if (modifiers.background.has_value()) {
    const graphics::ShapeStyle& painted_style = modifiers.background->fill;
    const Styled styled = style_of(scope, painted_style);
    if (const std::optional<theme::ColourValue> solid = styled.style->colour();
        solid.has_value() && !painted_style.shadow().has_value()) {
      fill = solid->opacity(styled.opacity);
    } else {
      styled_fill_style = &painted_style;
    }
  } else if (automatic) {
    fill = highlights_.pressed == node.id()
               ? components::ButtonStyle::pressed_fill_role
               : components::ButtonStyle::fill_role;
  }
  if (fill.has_value()) {
    theme::Colour painted =
        presentation.animate(Tint::background, scope.colour(*fill), moment);
    if (hovering && hover.highlight > visible_threshold) {
      painted = tinted(painted, scope.colour(theme::ColourRole::accent),
                       motion::HoverEffect::highlight_tint * hover.highlight);
    }
    if (!presentation.in_flight(Tint::background)) {
      inspect(node, scope, painted, Contrast::fill, snapped);
    }
    arrow(scope, frame, pack(painted, scope.opacity));
    Box filled = outlined;
    if (filled.height() < 1.0F && frame.height() > 0.0F) {
      filled.bottom = filled.top + 1.0F;
    }
    if (filled.width() < 1.0F && frame.width() > 0.0F) {
      filled.right = filled.left + 1.0F;
    }
    shape(provider::ShapeKind::fill, filled, rounded, 0.0F, 0.0F,
          pack(painted, scope.opacity), scope.clip);
  }
  if (styled_fill_style != nullptr) {
    styled_fill(node.id(), scope, *styled_fill_style, outlined, rounded,
                Motion{.presentation = presentation, .moment = moment});
  }
  if (node.content_as<components::Divider::Content>().has_value()) {
    Box line = snapped;
    if (line.width() < 1.0F) {
      line.right = line.left + 1.0F;
    }
    if (line.height() < 1.0F) {
      line.bottom = line.top + 1.0F;
    }
    const theme::Colour rule = presentation.animate(
        Tint::background, scope.colour(theme::ColourRole::separator), moment);
    inspect(node, scope, rule, Contrast::border, line);
    shape(provider::ShapeKind::fill, line, Corners{}, 0.0F, 0.0F,
          pack(rule, scope.opacity), scope.clip);
  }
  if (const auto raster =
          node.content_as<components::Image::Raster::Content>()) {
    paint_raster(raster->get(), scope, snapped);
  }
  if (const auto external =
          node.content_as<components::Image::External::Content>()) {
    if (scope.redacts()) {
      redact_box(scope, snapped);
    } else {
      sample_external(
          external->get().texture(), snapped,
          pack(theme::Colour{1.0F, 1.0F, 1.0F, 1.0F}, scope.opacity),
          scope.clip);
    }
  }
  if (const auto surface =
          node.content_as<components::SurfaceView::Content>()) {
    paint_surface(surface->get().surface(), snapped, scope);
  }
  if (node.content_as<components::ShapeView::Content>().has_value()) {
    draw_shape(node, scope, presentation, moment, frame, rounded);
  }
  if (node.content_as<components::MeshGradient::Content>().has_value()) {
    draw_mesh(node, scope, presentation, moment, snapped);
  }
  if (automatic) {
    const theme::Edges edges =
        theme
            .edges(modifiers.padding.value_or(components::Button::label_insets))
            .positive();
    const Box label{.left = frame.left + (edges.leading * scale),
                    .top = frame.top + (edges.top * scale),
                    .right = frame.right - (edges.trailing * scale),
                    .bottom = frame.bottom - (edges.bottom * scale)};
    const theme::ColourValue ink =
        modifiers.foreground.has_value()
            ? this->ink(scope, theme::ColourRole::control_text)
            : theme::ColourValue{theme::ColourRole::control_text};
    if (!button->get().label().empty()) {
      inspect(node, scope, scope.colour(ink), Contrast::text, label);
    }
    const text::TextStyle label_style = components::Text::text_style(
        theme, theme::Font{components::Button::label_role}, scope.locale);
    const std::string_view written =
        button->get().localized_label(scope.text.catalog, scope.locale);
    if (auto drawn =
            scope.redacts()
                ? redact_text(written, label_style, label, true, scope)
                : typeset(written, label_style, label, true,
                          pack(presentation.animate(Tint::foreground,
                                                    scope.colour(ink), moment),
                               scope.opacity),
                          scope.clip);
        !drawn.has_value()) {
      return std::unexpected(drawn.error());
    }
  }
  std::optional<Box> link_ring;
  if (const auto slice = node.content_as<components::Text::Glyphs::Content>()) {
    if (auto drawn = glyph_slice(slice->get(), frame, scope);
        !drawn.has_value()) {
      return std::unexpected(drawn.error());
    }
  }
  if (const auto written = node.content_as<components::Text::Content>();
      written.has_value() && !scope.text.custom_rendered) {
    const theme::Edges edges =
        theme.edges(modifiers.padding.value_or(theme::Insets{})).positive();
    const Box settled{.left = frame.left,
                      .top = frame.top,
                      .right = frame.left + (laid.width * scale),
                      .bottom = frame.top + (laid.height * scale)};
    const std::optional<composition::Rect>& aligned = node.content_frame();
    const Box content =
        aligned.has_value()
            ? Box{.left = settled.left + (aligned->x * scale),
                  .top = settled.top + (aligned->y * scale),
                  .right =
                      settled.left + ((aligned->x + aligned->width) * scale),
                  .bottom =
                      settled.top + ((aligned->y + aligned->height) * scale)}
            : Box{.left = settled.left + (edges.leading * scale),
                  .top = settled.top + (edges.top * scale),
                  .right = settled.right - (edges.trailing * scale),
                  .bottom = settled.bottom - (edges.bottom * scale)};
    const theme::ColourValue ink = written->get().foreground().value_or(
        this->ink(scope, theme::ColourRole::text_primary));
    const composition::TextAttributes attributes =
        written->get().attributes_in(scope.text);
    const components::AttributedString& attributed =
        written->get().resolved(attributes, scope.locale);
    std::string_view source = attributed.text();
    text::TextStyle style = components::Text::text_style(
        theme, written->get().font_in(scope.font), scope.locale,
        written->get().weight(), attributes, scope.direction);
    style.runs = components::Text::styled_runs(theme, style, attributed);
    variants_ = written->get().variants(attributes, scope.locale);
    if (!variants_.empty() && typesetter_.has_value()) {
      const std::optional<std::size_t> chosen = components::Text::variant_for(
          variants_, (content.right - content.left) / scale,
          [&](const std::string& variant) {
            const auto natural = typesetter_->get().measure(
                variant, style, text::LayoutWidth::unbounded(), target_.scale);
            return natural.has_value() ? natural->width : 0.0F;
          });
      if (chosen.has_value()) {
        source = variants_[*chosen];
        style.runs = text::TextRuns{};
      }
    }
    run_inks_.clear();
    for (const theme::ColourValue& value : attributed.inks()) {
      run_inks_.push_back(pack(scope.colour(value), scope.opacity));
    }
    const auto packed = [&](const theme::ColourValue& value) {
      return pack(scope.colour(value), scope.opacity);
    };
    inks_ = TextInks{
        .runs = run_inks_,
        .link = attributed.destinations().empty()
                    ? std::nullopt
                    : std::optional{packed(theme::ColourRole::link)},
        .underline = attributes.underline.colour.transform(packed),
        .strikethrough = attributes.strikethrough.colour.transform(packed),
        .images = attributed.images(),
        .glyphs = attributed.glyphs()};
    shaded_glyphs_.clear();
    if (!presentation.in_flight(Tint::foreground)) {
      inspect(node, scope, scope.colour(ink), Contrast::text, content);
    }
    const std::uint32_t colour =
        pack(presentation.animate(Tint::foreground, scope.colour(ink), moment),
             scope.opacity);
    std::optional<std::expected<void, PaintError>> scrolled;
    if (written->get().marquee() && !scope.redacts()) {
      scrolled = marquee(node, scope, source, style, content, colour);
    }
    if (scrolled.has_value()) {
      if (!scrolled->has_value()) {
        return std::unexpected(scrolled->error());
      }
    } else {
      const std::shared_ptr<const text::FittedText> fit = fitted(
          source, style, content,
          components::Text::fitting(attributes, written->get().marquee()));
      const std::string_view shown =
          fit != nullptr ? std::string_view{fit->text} : source;
      const text::TextStyle& drawn_style = fit != nullptr ? fit->style : style;
      std::optional<motion::Presentation::Swap> swap;
      if (scope.content_transition.has_value() &&
          scope.content_transition != motion::ContentTransition::identity) {
        swap = presentation.exchange(shown, moment,
                                     scope.content_transition->value());
      }
      const std::size_t glyphs_from = quads_.size();
      auto drawn =
          scope.redacts()
              ? redact_text(shown, drawn_style, content, false, scope)
          : swap.has_value()
              ? transition_text(*scope.content_transition, *swap, shown,
                                drawn_style, content, colour, scope.clip,
                                scope.reduce_motion)
              : typeset(shown, drawn_style, content, false, colour, scope.clip);
      if (!drawn.has_value()) {
        return std::unexpected(drawn.error());
      }
      if (written->get().link_count() > 0 && !scope.redacts()) {
        if (const std::optional<Arranged> linked =
                arrange(shown, drawn_style, content, false, true)) {
          link_frames(tree, node, *linked, frame);
          if (highlighted(node.id())) {
            const std::size_t selected = node.selected_link();
            const auto chosen = std::ranges::find(
                link_scratch_, selected, &composition::LinkFrame::link);
            if (chosen != link_scratch_.end()) {
              link_ring = Box{
                  .left = frame.left + (chosen->frame.x * scale),
                  .top = frame.top + (chosen->frame.y * scale),
                  .right = frame.left +
                           ((chosen->frame.x + chosen->frame.width) * scale),
                  .bottom = frame.top +
                            ((chosen->frame.y + chosen->frame.height) * scale)};
            }
          }
        }
      }
      if (!shaded_glyphs_.empty() && !scope.redacts()) {
        for (std::size_t shade_index = 0;
             shade_index < attributed.shadings().size(); ++shade_index) {
          const Styled styled =
              style_of(scope, attributed.shadings()[shade_index]);
          const std::optional<Shade> ramped =
              shade(scope, *styled.style, settled);
          if (!ramped.has_value()) {
            continue;
          }
          const std::uint32_t white =
              pack(theme::Colour{1.0F, 1.0F, 1.0F, 1.0F},
                   scope.opacity * styled.opacity * styled.style->opacity());
          for (const ShadedGlyph& glyph_shade : shaded_glyphs_) {
            if (glyph_shade.shade == shade_index + 1 &&
                glyph_shade.quad < quads_.size() &&
                quads_[glyph_shade.quad].kind == provider::ShapeKind::glyph) {
              quads_[glyph_shade.quad].colour = white;
              stylise(quads_[glyph_shade.quad], *ramped);
            }
          }
        }
        shaded_glyphs_.clear();
      }
      const graphics::ShapeStyle* whole =
          written->get().style().has_value() ? &*written->get().style()
          : scope.foreground != nullptr      ? &scope.foreground->levels.front()
                                             : nullptr;
      if (whole != nullptr && !scope.redacts()) {
        const Styled styled = style_of(scope, *whole);
        if (const std::optional<Shade> ramped =
                animated_shade(scope, *styled.style, settled, node.id(),
                               presentation, moment)) {
          const std::uint32_t white =
              pack(theme::Colour{1.0F, 1.0F, 1.0F, 1.0F},
                   scope.opacity * styled.opacity * styled.style->opacity());
          const std::size_t first = std::max(glyphs_from, underlaid_);
          for (std::size_t index = first; index < quads_.size(); ++index) {
            if (quads_[index].kind == provider::ShapeKind::glyph) {
              quads_[index].colour = white;
            }
          }
          stylise(first, *ramped);
        }
      }
    }
    inks_ = TextInks{};
  }
  if (modifiers.border.has_value()) {
    const theme::Stroke stroke = modifiers.border->width;
    const float width = stroke.is_hairline()
                            ? 1.0F
                            : std::max(stroke.device_pixels(scale), 1.0F);
    const theme::Colour edge = presentation.animate(
        Tint::border, scope.colour(modifiers.border->colour), moment);
    shape(provider::ShapeKind::stroke, snapped, rounded, width, 0.0F,
          pack(edge, scope.opacity), scope.clip);
    if (!presentation.in_flight(Tint::border)) {
      inspect(node, scope, edge, Contrast::border, snapped);
    }
  }
  pace(node, presentation);
  Scope inner = scope;
  inner.arrow.reset();
  const std::optional<Outline> drawn =
      enclosure_of(modifiers, scope, snapped, rounded, glassed,
                   fill.has_value() || styled_fill_style != nullptr);
  if (drawn.has_value()) {
    inner.enclosure =
        Enclosure{.outline = *drawn, .transform = scope.transform};
  }
  if (automatic && !modifiers.foreground.has_value()) {
    inner.foreground = hierarchy(scope.foreground, control_text_);
  }
  if (modifiers.clip) {
    place();
    inner.clip = intersect(scope.clip, bounds(snapped, scope.transform));
  }
  const Box viewport_screen = bounds(snapped, scope.transform);
  if (container.has_value()) {
    place();
    if (!declared.scrolling.clip_disabled) {
      inner.clip = intersect(inner.clip, viewport_screen);
    }
    const composition::Point offset = node.scroll_presentation().offset;
    inner.transform =
        adopt(inner.transform,
              Affine::translation(-offset.x * scale, -offset.y * scale));
    inner.viewport =
        Viewport{.screen = viewport_screen, .axes = container->axes};
    inner.enclosure.reset();
  }
  if (modifiers.geometry_group) {
    inner.group_origin = composition::Point{.x = laid.x, .y = laid.y};
  }
  if (grouping && !carries) {
    inner.transform = adopt(scope.transform,
                            Affine::translation(frame.left - (laid.x * scale),
                                                frame.top - (laid.y * scale)));
    inner.enclosure.reset();
    unsettle();
  }
  if (carries) {
    const Box own{.left = laid.x * scale,
                  .top = laid.y * scale,
                  .right = (laid.x + laid.width) * scale,
                  .bottom = (laid.y + laid.height) * scale};
    inner.transform =
        adopt(scope.transform, carry(own, frame,
                                     node.matched_geometry().has_value()
                                         ? node.matched_geometry()->anchor
                                         : motion::UnitPoint::center));
    inner.enclosure.reset();
  }
  if (anchored.has_value() || carries) {
    unsettle();
  }
  const float factor =
      parent.opacity > 0.0F ? scope.opacity / parent.opacity : 1.0F;
  if (promoting) {
    inner.opacity = parent.opacity;
  }
  own(own_from, node.id(), scope.fade);
  const std::size_t promoted_from = quads_.size();
  const std::size_t backdrops_before = compositor_.pending_backdrops();
  const Span head_end = span();
  const Gathered head = gather();
  if (effecting) {
    ++effect_depth_;
  }
  visit.inner = inner;
  const bool masks = plan.masks;
  paintings_.push_back(Painting{.parent = parent,
                                .scope = scope,
                                .presentation = &presentation,
                                .plan = std::move(plan),
                                .head = head,
                                .local = local,
                                .frame = frame,
                                .snapped = snapped,
                                .rounded = rounded,
                                .hover_corners = hover_corners,
                                .hover = hover,
                                .clip_corners = clip_corners,
                                .link_ring = link_ring,
                                .entry = entry,
                                .head_end = head_end,
                                .effect_factor = effect_factor,
                                .factor = factor,
                                .own_from = own_from,
                                .own_backdrops = own_backdrops,
                                .promoted_from = promoted_from,
                                .backdrops_before = backdrops_before,
                                .enclosing = enclosing,
                                .effecting = effecting,
                                .promoting = promoting,
                                .anchored = anchored.has_value(),
                                .carries = carries,
                                .binds = binds,
                                .encloses = drawn.has_value()});
  if (masks) {
    visit.cursor = Cursor{.order = Order::masked, .masks = effecting};
  } else {
    begin_children(tree, node, visit, anchored);
  }
  return true;
}

void Painter::paint_unwind() {
  const Painting& painting = paintings_.back();
  if (painting.effecting) {
    --effect_depth_;
  }
  fold(painting.head);
  staged_.reset();
}

void Painter::paint_tail(const composition::Node& node, Visit& visit) {
  Painting& painting = paintings_.back();
  const Scope& parent = painting.parent;
  const Scope& scope = painting.scope;
  const Scope& inner = visit.inner;
  motion::Presentation& presentation = *painting.presentation;
  const EffectPlan& plan = painting.plan;
  const Gathered& head = painting.head;
  const Affine& local = painting.local;
  const Box& frame = painting.frame;
  const Box& snapped = painting.snapped;
  Corners& rounded = painting.rounded;
  const Corners& hover_corners = painting.hover_corners;
  const Hover& hover = painting.hover;
  const std::optional<Corners>& clip_corners = painting.clip_corners;
  const std::optional<Box>& link_ring = painting.link_ring;
  const Span entry = painting.entry;
  const Span head_end = painting.head_end;
  const float effect_factor = painting.effect_factor;
  const float factor = painting.factor;
  const std::size_t own_from = painting.own_from;
  const std::size_t own_backdrops = painting.own_backdrops;
  const std::size_t promoted_from = painting.promoted_from;
  const std::size_t backdrops_before = painting.backdrops_before;
  const std::uint32_t enclosing = painting.enclosing;
  const bool effecting = painting.effecting;
  const bool promoting = painting.promoting;
  const bool carries = painting.carries;
  const bool binds = painting.binds;
  const bool hovering = hover.present;
  const std::optional<std::size_t>& split = visit.split;
  const composition::Modifiers& modifiers = node.modifiers();
  const composition::Interactions& declared = node.interactions();
  const auto& container = declared.scrolling.container;
  const float scale = target_.scale.value();
  if (effecting) {
    --effect_depth_;
  }
  const Gathered children = gather();
  if (promoting) {
    promote(node, scope, parent, local, factor, promoted_from,
            backdrops_before);
  }
  std::optional<std::size_t> clip_split;
  if (clip_corners.has_value() && !plan.masks && !promoting &&
      (effecting ||
       intrudes(own_from, snapped, *clip_corners, scope.transform) ||
       modifiers.clip_path.has_value()) &&
      quads_.size() > own_from &&
      (effecting || compositor_.pending_backdrops() == own_backdrops)) {
    clip_split = quads_.size();
    transform_ = scope.transform;
    clip_mask(node, scope, snapped, *clip_corners);
    if (quads_.size() == *clip_split) {
      clip_split.reset();
    }
  } else if (clip_corners.has_value() && !plan.masks && !promoting &&
             !effecting && !modifiers.clip_path.has_value() &&
             quads_.size() > own_from &&
             compositor_.pending_backdrops() != own_backdrops) {
    round_images(own_from, snapped, *clip_corners, scope.transform);
  }
  if (effecting) {
    composite(node, scope, parent, plan, effect_factor, own_from,
              split.has_value() ? split : clip_split, own_backdrops, frame);
    unsettle();
  } else if (clip_split.has_value()) {
    composite(node, scope, parent, EffectPlan{}, 1.0F, own_from, clip_split,
              own_backdrops, frame);
    unsettle();
  }
  const Span tail_begin = span();
  transform_ = scope.transform;
  const std::size_t tail_from = quads_.size();
  if (modifiers.overlay.has_value()) {
    const Box overlaid = fitted(modifiers.overlay->shape, snapped);
    styled_fill(node.id(), scope, modifiers.overlay->fill, overlaid,
                resolve(scope, modifiers.overlay->shape, overlaid));
  }
  if (container.has_value()) {
    edge_effects(node, scope, snapped);
    indicators(node, scope, snapped);
  }
  own(tail_from, node.id(), scope.fade);
  if (binding_ != enclosing) {
    claim(own_from, binding_);
  }
  binding_ = enclosing;
  const float ring = scope.focus_effect_disabled
                         ? 0.0F
                         : (hovering ? std::clamp(hover.lifted, 0.0F, 1.0F)
                                     : (highlighted(node.id()) ? 1.0F : 0.0F));
  if (ring > visible_threshold) {
    if (hovering && node.interactions().hover_shape.has_value()) {
      rounded = hover_corners;
    } else if (const std::optional<theme::Shape> own = focus_outline(node)) {
      rounded = resolve(scope, *own, snapped);
    }
    const theme::Theme::FocusRing& style = scope.theme.get().focus_ring();
    if (style.style == theme::Theme::FocusRing::Style::highlight &&
        !scope.differentiate) {
      const Box lit = link_ring.value_or(frame);
      const std::size_t before = quads_.size();
      const theme::Colour tone = scope.colour(theme::ColourRole::accent);
      shape(provider::ShapeKind::fill, lit, rounded, 0.0F, 0.0F,
            pack(tone.with_alpha(style.highlight.fill), scope.opacity * ring),
            parent.clip);
      shape(provider::ShapeKind::stroke, lit, rounded,
            std::max(style.highlight.width * scale, 1.0F), 0.0F,
            pack(tone.with_alpha(style.highlight.edge), scope.opacity * ring),
            parent.clip);
      own(before, node.id(), scope.fade);
      rings_.insert(rings_.end(),
                    std::make_move_iterator(
                        quads_.begin() + static_cast<std::ptrdiff_t>(before)),
                    std::make_move_iterator(quads_.end()));
      quads_.resize(before);
    }
  }
  if (ring > visible_threshold &&
      !(scope.theme.get().focus_ring().style ==
            theme::Theme::FocusRing::Style::highlight &&
        !scope.differentiate)) {
    const theme::Theme::FocusRing& style = scope.theme.get().focus_ring();
    const float gap = focus_ring_gap * scale;
    const float width = std::round(style.width * scale);
    const float reach = gap + width - (declared.focus_ring_inset * scale);
    const Box ringed = link_ring.value_or(frame);
    const Outline confined =
        confine(Outline{.box = snap(Box{.left = ringed.left - reach,
                                        .top = ringed.top - reach,
                                        .right = ringed.right + reach,
                                        .bottom = ringed.bottom + reach}),
                        .corners = grown(rounded, reach)},
                frame, scope);
    const std::size_t before = quads_.size();
    if (style.halo || scope.differentiate) {
      const float halo = std::max(std::round(focus_ring_halo * scale), 1.0F);
      const Outline widened{.box = Box{.left = confined.box.left - halo,
                                       .top = confined.box.top - halo,
                                       .right = confined.box.right + halo,
                                       .bottom = confined.box.bottom + halo},
                            .corners = grown(confined.corners, halo)};
      shape(provider::ShapeKind::stroke, widened.box, widened.corners,
            width + (halo * 2.0F), focus_ring_seal,
            pack(scope.colour(theme::ColourRole::system_background),
                 scope.opacity * ring),
            parent.clip);
    }
    shape(
        provider::ShapeKind::stroke, confined.box, confined.corners, width,
        focus_ring_seal,
        pack(scope.colour(theme::ColourRole::focus_ring), scope.opacity * ring),
        parent.clip);
    own(before, node.id(), scope.fade);
    rings_.insert(rings_.end(),
                  std::make_move_iterator(quads_.begin() +
                                          static_cast<std::ptrdiff_t>(before)),
                  std::make_move_iterator(quads_.end()));
    quads_.resize(before);
  }
  const Gathered tail = gather();
  fold(head);
  fold(children);
  fold(tail);
  const bool alone =
      !head.unsettled && !tail.unsettled && !promoting && !effecting &&
      !painting.anchored && !carries && !binds && !presentation.animating() &&
      !node.content_as<components::GlassEffectContainer::Content>().has_value();
  const auto within = [](std::uint32_t index, std::uint32_t first,
                         std::uint32_t last) -> std::optional<std::uint32_t> {
    if (index >= first && index < last) {
      return index - first;
    }
    return std::nullopt;
  };
  const std::optional<std::uint32_t> inner_transform =
      within(inner.transform, entry.transforms, head_end.transforms);
  const std::optional<std::uint32_t> inner_fade =
      within(inner.fade, entry.fades, head_end.fades);
  if (!alone ||
      (!inner_transform.has_value() && inner.transform != parent.transform) ||
      (!inner_fade.has_value() && inner.fade != parent.fade)) {
    staged_.reset();
    return;
  }
  staged_ = Own{
      .head = head_end - entry,
      .tail = span() - tail_begin,
      .reach = Box{.left = std::min(head.reach.left, tail.reach.left),
                   .top = std::min(head.reach.top, tail.reach.top),
                   .right = std::max(head.reach.right, tail.reach.right),
                   .bottom = std::max(head.reach.bottom, tail.reach.bottom)},
      .translatable = !head.positional && !tail.positional,
      .pages = head.pages | tail.pages,
      .inner = inner,
      .inner_transform = inner_transform,
      .inner_fade = inner_fade,
      .encloses = painting.encloses};
}

void Painter::begin_children(composition::Tree& tree,
                             const composition::Node& node, Visit& visit,
                             const std::optional<Popover>& anchored) {
  Cursor& cursor = visit.cursor;
  cursor = Cursor{.order = Order::plain, .anchored = anchored};
  if (!anchored.has_value()) {
    std::size_t position = 0;
    for (const composition::Node& child : node.children()) {
      if (covers(tree, child, node, visit.inner)) {
        cursor.hidden = position;
      }
      ++position;
    }
  }
  if (composition::PaintOrder::reorders(node)) {
    cursor.order = Order::arranged;
    cursor.base = ordered_.size();
    composition::PaintOrder::arrange(node, ordered_);
    cursor.end = ordered_.size();
    cursor.position = cursor.base;
  }
}

auto Painter::admit(const composition::Node& child, Visit& visit) -> bool {
  Cursor& cursor = visit.cursor;
  if (layered(child)) {
    defer(child, visit.inner);
    return false;
  }
  if (child.interactions().effects.pin.has_value()) {
    cursor.lifting = true;
    return false;
  }
  cursor.offset = cursor.anchored.has_value() && cursor.index > 0;
  ++cursor.index;
  return true;
}

auto Painter::advance(composition::Tree& tree, const composition::Node& node,
                      Visit& visit) -> const composition::Node* {
  Cursor& cursor = visit.cursor;
  cursor.offset = false;
  if (failure_.has_value()) {
    if (cursor.order == Order::arranged) {
      ordered_.resize(cursor.base);
    }
    cursor.order = Order::done;
    return nullptr;
  }
  const auto children = node.children();
  const std::size_t count = std::ranges::size(children);
  if (cursor.order == Order::masked) {
    if (cursor.index >= count || (cursor.index == 1 && !cursor.masks)) {
      cursor.order = Order::done;
      return nullptr;
    }
    if (cursor.index == 1) {
      visit.split = quads_.size();
    }
    return &children[cursor.index++];
  }
  if (cursor.order == Order::arranged) {
    while (cursor.position < cursor.end) {
      const composition::Node& child = *ordered_[cursor.position++];
      if (admit(child, visit)) {
        return &child;
      }
    }
    ordered_.resize(cursor.base);
  }
  if (cursor.order == Order::plain) {
    while (cursor.position < count) {
      const composition::Node& child = children[cursor.position];
      if (cursor.position++ < cursor.hidden) {
        conceal(tree, child);
        suspend(tree, child);
        ++occluded_;
        continue;
      }
      if (admit(child, visit)) {
        return &child;
      }
    }
  }
  if (cursor.order == Order::arranged || cursor.order == Order::plain) {
    cursor.order = cursor.lifting ? Order::pinned : Order::done;
    cursor.position = 0;
  }
  if (cursor.order == Order::pinned) {
    while (cursor.position < count) {
      const composition::Node& child = children[cursor.position++];
      if (child.interactions().effects.pin.has_value() && !layered(child)) {
        return &child;
      }
    }
    cursor.order = Order::done;
  }
  return nullptr;
}

auto Painter::gather() noexcept -> Gathered {
  return Gathered{.unsettled = std::exchange(unsettled_, false),
                  .positional = std::exchange(positional_, false),
                  .reach = std::exchange(reach_, unreached),
                  .pages = std::exchange(pages_, 0)};
}

void Painter::fold(const Gathered& gathered) noexcept {
  unsettled_ = unsettled_ || gathered.unsettled;
  positional_ = positional_ || gathered.positional;
  if (gathered.reach.right >= gathered.reach.left) {
    reach(gathered.reach);
  }
  pages_ |= gathered.pages;
}

auto Painter::paint(composition::Tree& tree, provider::GpuFrame& frame,
                    Target target, TypesetterRef typesetter)
    -> std::expected<Statistics, PaintError> {
  return paint(tree, frame, target, typesetter, Highlights{});
}

auto Painter::paint(composition::Tree& tree, provider::GpuFrame& frame,
                    Target target, TypesetterRef typesetter,
                    Highlights highlights)
    -> std::expected<Statistics, PaintError> {
  const std::chrono::nanoseconds started = stamp(target);
  const composition::Node& root = tree.root();
  typesetter_ = typesetter;
  inks_ = TextInks{};
  begin_frame(tree, target, highlights);
  if (const auto surfaces = gpu_.get().surfaces()) {
    surfaces->get().advance();
  }
  quads_.clear();
  painted_textures_.clear();
  commands_.clear();
  owners_.clear();
  bindings_.clear();
  binding_ = 0;
  transforms_.assign(1, Affine{});
  adoptions_.assign(1, Adoption{});
  fades_.assign(1, Fade{});
  transform_ = 0;
  animating_ = false;
  unhinted_ = false;
  unhinted_cause_.reset();
  hinted_rate_ = 0.0;
  culled_ = 0;
  visited_ = 0;
  occluded_ = 0;
  shadow_hits_ = 0;
  shadow_misses_ = 0;
  target_ = target;
  highlights_ = highlights;
  deferred_.clear();
  deferred_order_ = 0;
  rings_.clear();
  united_.clear();
  forget_departed(tree);
  forget_phases(tree);
  compositor_.advance_frame();
  layer_count_ = 0;
  backdrops_ = 0;
  screen_ = provider::ClipRect{.x = 0,
                               .y = 0,
                               .width = target.pixels.width.value(),
                               .height = target.pixels.height.value()};
  atlas_.advance_frame();
  retire_pictures();
  shapes_.advance_frame([&tree](composition::NodeId id) {
    const auto node = tree.node(id);
    return node.has_value() && tree.presentation(node->get()).animating();
  });
  mesh_count_ = 0;
  const Scope scope{
      .theme = canonical(components::Environment::theme.fallback()),
      .scheme = std::nullopt,
      .locale = canonical(components::Environment::locale.fallback()),
      .foreground = nullptr,
      .opacity = 1.0F,
      .clip = provider::ClipRect{.x = 0,
                                 .y = 0,
                                 .width = target.pixels.width.value(),
                                 .height = target.pixels.height.value()},
      .reduce_motion = false,
      .transform = 0,
      .roles = &roles_};
  const theme::Colour clear = target.clear.value_or(
      backdrop(root, scope).colour(theme::ColourRole::background));
  frame.clear(
      provider::Colour{.red = clear.red,
                       .green = clear.green,
                       .blue = clear.blue,
                       .alpha = target.clear.has_value() ? clear.alpha : 1.0F});
  clear_ = theme::Colour{clear.red, clear.green, clear.blue, 1.0F};
  if (clear_ != recorded_clear_) {
    recorded_clear_ = clear_;
    ++generation_;
  }
  const Lineage origin{
      .previous = Span{}, .walked = frame_ - 1, .begin = Span{}};
  const auto spoiled = [&](PaintError error) {
    typesetter_.reset();
    deferred_.clear();
    whole_next_ = true;
    ++generation_;
    main_count_ = 0;
    tree.seal_paint();
    return std::unexpected(error);
  };
  const composition::ExclusiveScopes& exclusive = tree.exclusive();
  if (exclusive_revision_ != exclusive.revision()) {
    exclusive_revision_ = exclusive.revision();
    whole_next_ = true;
  }
  hoist(tree);
  for (const Hoisted& hoisted : hoisted_) {
    lineage_ = origin;
    if (auto walked = walk(tree, hoisted.node.get(),
                           layer_scope(tree, hoisted.node.get(), scope));
        !walked.has_value()) {
      return spoiled(walked.error());
    }
  }
  if (!exclusive.active()) {
    lineage_ = origin;
    if (auto walked = walk(tree, root, scope); !walked.has_value()) {
      return spoiled(walked.error());
    }
  }
  while (!deferred_.empty()) {
    const auto next = std::ranges::min_element(
        deferred_, {},
        [](const Deferred& entry) { return std::pair{entry.z, entry.order}; });
    const Deferred chosen = *next;
    deferred_.erase(next);
    if (chosen.z < composition::OverlayLayer::content.z() ||
        !draws(exclusive, chosen.z)) {
      continue;
    }
    lineage_ = origin;
    if (auto walked = walk(tree, chosen.node.get(), chosen.scope);
        !walked.has_value()) {
      return spoiled(walked.error());
    }
  }
  typesetter_.reset();
  main_count_ = quads_.size();
  quads_.insert(quads_.end(), rings_.begin(), rings_.end());
  mark_used();
  const std::chrono::nanoseconds recorded = stamp(target);
  if (auto flushed = atlas_.flush(); !flushed.has_value()) {
    return spoiled(PaintError::from(flushed.error(), PaintStage::atlas_upload));
  }
  const std::chrono::nanoseconds uploaded = stamp(target);
  auto emitted = emit(frame);
  tree.seal_paint();
  if (!emitted.has_value()) {
    ++generation_;
  }
  if (emitted.has_value()) {
    emitted->walk = recorded - started;
    emitted->upload = uploaded - recorded;
    emitted->emit = stamp(target) - uploaded;
    emitted->animating = animating_;
    emitted->culled = culled_;
    emitted->visited = visited_;
    emitted->recorded = recorded_;
    emitted->spliced = spliced_;
    emitted->occluded = occluded_;
    emitted->shadow_hits = shadow_hits_;
    emitted->shadow_misses = shadow_misses_;
    emitted->tessellations = shapes_.counters().tessellations;
    emitted->mesh_builds = shapes_.counters().mesh_builds;
    const Compositor::Statistics composed = compositor_.statistics();
    emitted->backdrops = backdrops_;
    emitted->blurs = composed.blurs;
    emitted->blur_hits = composed.blur_hits;
    emitted->shared_backdrops = composed.shared;
    emitted->effect_passes = composed.passes;
    emitted->layer_renders = composed.layer_renders;
    emitted->layer_hits = composed.layer_hits;
    emitted->pictures = static_cast<std::uint32_t>(pictures_.size());
    report_targets(*emitted);
    emitted->picture_bytes = picture_bytes_;
    if (animating_ && !unhinted_ && hinted_rate_ > 0.0) {
      emitted->frame_rate = hinted_rate_;
    } else if (animating_) {
      emitted->unhinted = unhinted_cause_.value_or(motion::UnhintedAnimation{
          .route = motion::UnhintedAnimation::Route::regroup});
    }
    if (target_.replay.has_value()) {
      target_.replay->get().frame_rate = emitted->frame_rate;
    }
  }
  return emitted;
}

auto Painter::stamp(const Target& target) noexcept -> std::chrono::nanoseconds {
  return target.clock.has_value() ? target.clock->get().now().since_origin()
                                  : std::chrono::nanoseconds{0};
}

auto Painter::cost(const Quad& quad) const noexcept
    -> std::array<std::size_t, 2> {
  constexpr std::size_t vertices_per_quad = 4;
  constexpr std::size_t indices_per_quad = 6;
  if (quad.mesh != 0) {
    const ShapePainter::Geometry& geometry = meshes_[quad.mesh - 1];
    return {(geometry.vertices.size() + vertices_per_quad - 1) /
                vertices_per_quad * vertices_per_quad,
            geometry.indices.size()};
  }
  return {vertices_per_quad, indices_per_quad};
}

auto Painter::write(const Quad& quad, const Replay& replayed,
                    const provider::ShapeGeometry& geometry,
                    std::uint32_t& base, std::uint32_t& first) const
    -> std::uint32_t {
  const auto run = static_cast<std::uint32_t>(cost(quad)[0]);
  write_vertices(quad, replayed.baked, geometry.vertices.subspan(base, run));
  const std::uint32_t used =
      write_indices(quad, geometry.indices.subspan(first), base);
  base += run;
  return used;
}

void Painter::write_vertices(const Quad& quad, const Affine& matrix,
                             std::span<provider::ShapeVertex> out) const {
  if (quad.mesh != 0) {
    const ShapePainter::Geometry& mesh = meshes_[quad.mesh - 1];
    const bool moves = !matrix.identity();
    for (std::size_t index = 0; index < out.size(); ++index) {
      provider::ShapeVertex vertex =
          mesh.vertices[std::min(index, mesh.vertices.size() - 1)];
      if (moves) {
        vertex.position = matrix.apply(vertex.position);
      }
      out[index] = vertex;
    }
    return;
  }
  place(quad, matrix, out);
}

auto Painter::replay(const Quad& quad) const noexcept -> Replay {
  Affine matrix = transforms_[quad.transform];
  const float opacity = fades_[quad.fade].value;
  if (quad.transform == 0 || quad.backdrop || samples_backdrop(quad.kind) ||
      target_.zoom != 1.0F) {
    return Replay{.baked = matrix, .translation = {}, .opacity = opacity};
  }
  const std::array<float, 2> translation{matrix.tx, matrix.ty};
  if (quad.kind == provider::ShapeKind::glyph && !matrix.translating()) {
    matrix.tx -= std::round(matrix.tx);
    matrix.ty -= std::round(matrix.ty);
  } else {
    matrix.tx = 0.0F;
    matrix.ty = 0.0F;
  }
  return Replay{
      .baked = matrix, .translation = translation, .opacity = opacity};
}

auto Painter::write_indices(const Quad& quad, std::span<provider::Index> out,
                            std::uint32_t base) const -> std::uint32_t {
  constexpr std::uint32_t indices_per_quad = 6;
  if (quad.mesh != 0) {
    const ShapePainter::Geometry& mesh = meshes_[quad.mesh - 1];
    for (std::size_t index = 0; index < mesh.indices.size(); ++index) {
      out[index] = base + mesh.indices[index];
    }
    return static_cast<std::uint32_t>(mesh.indices.size());
  }
  out[0] = base;
  out[1] = base + 1;
  out[2] = base + 2;
  out[3] = base + 2;
  out[4] = base + 3;
  out[5] = base;
  return indices_per_quad;
}

void Painter::batch(std::vector<provider::DrawCommand>& commands,
                    std::vector<std::uint32_t>& owners, const Quad& quad,
                    provider::TextureId& current, std::uint32_t first,
                    std::uint32_t used, const Replay& replayed) {
  const provider::TextureId texture = quad.texture.value_or(current);
  if (!quad.backdrop) {
    current = texture;
  }
  const provider::TextureId detail =
      quad.detail_texture.value_or(provider::TextureId{0});
  if (!commands.empty() && commands.back().texture == texture &&
      commands.back().detail == detail && commands.back().clip == quad.clip &&
      commands.back().trim == quad.trim &&
      commands.back().translation == replayed.translation &&
      commands.back().opacity == replayed.opacity &&
      owners.back() == quad.binding &&
      commands.back().index_offset + commands.back().index_count == first) {
    commands.back().index_count += used;
    return;
  }
  commands.push_back(provider::DrawCommand{.texture = texture,
                                           .clip = quad.clip,
                                           .index_offset = first,
                                           .index_count = used,
                                           .vertex_offset = 0,
                                           .detail = detail,
                                           .trim = quad.trim,
                                           .translation = replayed.translation,
                                           .opacity = replayed.opacity});
  owners.push_back(quad.binding);
}

void Painter::emit_layers(provider::GpuFrame& frame,
                          const provider::ShapeGeometry& geometry,
                          std::uint32_t& base, std::uint32_t& first) {
  for (const LayerPass& pass : std::span{layer_passes_}.first(layer_count_)) {
    std::vector<provider::DrawCommand>& commands = layer_commands_;
    commands.clear();
    layer_owners_.clear();
    provider::TextureId current = atlas_.pages().front().texture();
    for (const Quad& quad : pass.quads) {
      Replay replayed = replay(quad);
      replayed.translation = {replayed.translation[0] - pass.origin[0],
                              replayed.translation[1] - pass.origin[1]};
      const std::uint32_t used = write(quad, replayed, geometry, base, first);
      batch(commands, layer_owners_, quad, current, first, used, replayed);
      first += used;
    }
    if (auto drawn = frame.draw(
            provider::ShapeDrawData{.geometry = geometry, .commands = commands},
            provider::RenderPass{.target = pass.target,
                                 .region = pass.region,
                                 .clear = provider::Colour{.red = 0.0F,
                                                           .green = 0.0F,
                                                           .blue = 0.0F,
                                                           .alpha = 0.0F}});
        !drawn.has_value()) {
      compositor_.refuse(
          pass.effect, static_cast<std::uint32_t>(std::ceil(pass.region.width)),
          static_cast<std::uint32_t>(std::ceil(pass.region.height)),
          PaintError{PaintErrorCode::draw_rejected, PaintStage::layer_pass,
                     drawn.error()},
          "the subtree draws without the effect", pass.owner);
      compositor_.withdraw(pass.target);
      compositor_.spoil_layer(pass.spoils);
      animating_ = true;
    }
  }
}

auto Painter::add_layer_pass(const Compositor::LayerPlan& plan,
                             std::uint64_t owner, std::uint64_t spoils,
                             std::string_view effect) -> LayerPass& {
  if (layer_count_ == layer_passes_.size()) {
    layer_passes_.emplace_back();
  }
  LayerPass& pass = layer_passes_[layer_count_++];
  pass.target = plan.surface.texture;
  pass.origin = {plan.surface.region.x, plan.surface.region.y};
  pass.region = provider::PassRegion{.x = 0.0F,
                                     .y = 0.0F,
                                     .width = plan.surface.region.width,
                                     .height = plan.surface.region.height};
  pass.owner = owner;
  pass.spoils = spoils;
  pass.effect = effect;
  return pass;
}

auto Painter::effect_key(std::uint64_t key) noexcept -> std::uint64_t {
  constexpr std::uint32_t content_stage = 0;
  return Compositor::mix(key, std::as_bytes(std::span{&content_stage, 1}));
}

auto Painter::effect_name(provider::ShapeKind kind) noexcept
    -> std::string_view {
  switch (kind) {
    case provider::ShapeKind::mask:
      return "mask pass";
    case provider::ShapeKind::shader:
      return "shader pass";
    case provider::ShapeKind::gaussian:
      return "blur pass";
    case provider::ShapeKind::filter:
      return "colour filter pass";
    case provider::ShapeKind::layer:
      return "blur reduction pass";
    default:
      return "effect stage pass";
  }
}

void Painter::fall_back(std::size_t first, float factor) {
  quads_.resize(first);
  quads_.insert(quads_.end(), effect_content_.begin(), effect_content_.end());
  fade_range(first, factor);
}

auto Painter::bind(const motion::Presentation& presentation,
                   std::uint32_t parent_transform) -> std::uint32_t {
  using Property = motion::Presentation::Property;
  const auto offset = presentation.track(Property::offset);
  const auto opacity = presentation.track(Property::opacity);
  const auto from = presentation.track(Property::trim_start);
  const auto to = presentation.track(Property::trim_end);
  if (!offset.has_value() && !opacity.has_value() && !from.has_value() &&
      !to.has_value()) {
    return binding_;
  }
  const std::chrono::nanoseconds now = target_.time;
  const Affine& outer = transforms_[parent_transform];
  ReplayBinding& made = bindings_.emplace_back();
  made.parent = binding_;
  made.linear = {outer.a, outer.b, outer.c, outer.d};
  made.scale = target_.scale.value();
  if (offset.has_value()) {
    made.offset = offset->get().detached();
    made.painted_offset = offset->get().sample(now).value;
  }
  if (opacity.has_value()) {
    const double painted = opacity->get().sample(now).value[0];
    if (painted > visible_threshold) {
      made.opacity = opacity->get().detached();
      made.painted_opacity = painted;
    }
  }
  if (from.has_value()) {
    made.trim_start = from->get().detached();
    made.painted_trim_start = from->get().sample(now).value[0];
  }
  if (to.has_value()) {
    made.trim_end = to->get().detached();
    made.painted_trim_end = to->get().sample(now).value[0];
  }
  return static_cast<std::uint32_t>(bindings_.size());
}

void Painter::claim(std::size_t from, std::uint32_t binding) {
  for (std::size_t index = from; index < quads_.size(); ++index) {
    if (quads_[index].binding == 0) {
      quads_[index].binding = binding;
    }
  }
}

void Painter::capture(const provider::ShapeGeometry& geometry,
                      std::uint32_t vertex_count, std::uint64_t generation,
                      bool replayable) {
  if (!target_.replay.has_value()) {
    return;
  }
  ReplayScene& scene = target_.replay->get();
  scene.replayable = replayable && !bindings_.empty();
  scene.bindings.clear();
  scene.commands.clear();
  scene.owners.clear();
  scene.indices.clear();
  if (!scene.replayable) {
    return;
  }
  grow(scene.bindings, bindings_.capacity());
  grow(scene.commands, commands_.capacity());
  grow(scene.owners, owners_.capacity());
  grow(scene.indices, geometry.indices.size());
  scene.bindings.insert(scene.bindings.end(), bindings_.begin(),
                        bindings_.end());
  scene.commands.insert(scene.commands.end(), commands_.begin(),
                        commands_.end());
  scene.owners.insert(scene.owners.end(), owners_.begin(), owners_.end());
  scene.indices.insert(scene.indices.end(), geometry.indices.begin(),
                       geometry.indices.end());
  scene.vertex_count = vertex_count;
  scene.generation = generation;
  scene.clear = provider::Colour{.red = clear_.red,
                                 .green = clear_.green,
                                 .blue = clear_.blue,
                                 .alpha = 1.0F};
  scene.painted = target_.time;
  scene.frame_rate.reset();
}

auto Painter::samples_backdrop(provider::ShapeKind kind) noexcept -> bool {
  return kind == provider::ShapeKind::material ||
         kind == provider::ShapeKind::blend ||
         kind == provider::ShapeKind::glass ||
         kind == provider::ShapeKind::glass_path ||
         kind == provider::ShapeKind::glass_union;
}

void Painter::own(std::size_t from, composition::NodeId node,
                  std::uint32_t fade) {
  for (std::size_t index = from; index < quads_.size(); ++index) {
    quads_[index].owner = node.value();
    quads_[index].fade = fade;
  }
}

auto Painter::fade(std::uint32_t parent, float local) -> std::uint32_t {
  if (local == 1.0F) {
    return parent;
  }
  fades_.push_back(Fade{
      .parent = parent, .local = local, .value = fades_[parent].value * local});
  return static_cast<std::uint32_t>(fades_.size() - 1);
}

auto Painter::shown(const Scope& scope) const noexcept -> float {
  return scope.opacity * fades_[scope.fade].value;
}

auto Painter::quad_hash(std::uint64_t state, const Quad& quad,
                        const Affine& matrix,
                        const ShapePainter::Geometry* mesh) noexcept
    -> std::uint64_t {
  const std::array<std::uint32_t, 36> packed{
      static_cast<std::uint32_t>(quad.kind),
      quad.colour,
      quad.clip.x,
      quad.clip.y,
      quad.clip.width,
      quad.clip.height,
      std::bit_cast<std::uint32_t>(quad.bounds.left),
      std::bit_cast<std::uint32_t>(quad.bounds.top),
      std::bit_cast<std::uint32_t>(quad.bounds.right),
      std::bit_cast<std::uint32_t>(quad.bounds.bottom),
      std::bit_cast<std::uint32_t>(quad.uv[0]),
      std::bit_cast<std::uint32_t>(quad.uv[1]),
      std::bit_cast<std::uint32_t>(quad.uv[2]),
      std::bit_cast<std::uint32_t>(quad.uv[3]),
      std::bit_cast<std::uint32_t>(quad.centre[0]),
      std::bit_cast<std::uint32_t>(quad.centre[1]),
      std::bit_cast<std::uint32_t>(quad.half_size[0]),
      std::bit_cast<std::uint32_t>(quad.half_size[1]),
      std::bit_cast<std::uint32_t>(quad.shape[0]),
      std::bit_cast<std::uint32_t>(quad.shape[1]),
      std::bit_cast<std::uint32_t>(quad.shape[2]),
      std::bit_cast<std::uint32_t>(quad.shape[3]),
      std::bit_cast<std::uint32_t>(quad.effect[0]),
      std::bit_cast<std::uint32_t>(quad.effect[1]),
      std::bit_cast<std::uint32_t>(quad.effect[2]),
      std::bit_cast<std::uint32_t>(quad.effect[3]),
      std::bit_cast<std::uint32_t>(quad.detail[0]),
      std::bit_cast<std::uint32_t>(quad.detail[1]),
      std::bit_cast<std::uint32_t>(quad.detail[2]),
      std::bit_cast<std::uint32_t>(quad.detail[3]),
      std::bit_cast<std::uint32_t>(matrix.a),
      std::bit_cast<std::uint32_t>(matrix.b),
      std::bit_cast<std::uint32_t>(matrix.c),
      std::bit_cast<std::uint32_t>(matrix.d),
      std::bit_cast<std::uint32_t>(matrix.tx),
      std::bit_cast<std::uint32_t>(matrix.ty)};
  state = DrawListCache::hash_value(state, packed);
  if (mesh != nullptr) {
    state = DrawListCache::hash_value(
        state,
        std::array<std::uint64_t, 3>{mesh->revision, mesh->vertices.size(),
                                     mesh->indices.size()});
  }
  return state;
}

auto Painter::whole_damage() noexcept -> bool {
  const bool whole =
      std::exchange(whole_next_, false) || target_.zoom != 1.0F || zoomed_ ||
      target_.pixels != painted_pixels_ || clear_ != painted_clear_;
  zoomed_ = target_.zoom != 1.0F;
  painted_pixels_ = target_.pixels;
  painted_clear_ = clear_;
  return whole;
}

void Painter::gather_runs() {
  const std::size_t ceiling = quads_.capacity();
  draw_lists_.reserve(ceiling);
  runs_.reserve(ceiling);
  if (ordinals_.keys().capacity() < ceiling) {
    auto containers = std::move(ordinals_).extract();
    containers.keys.reserve(ceiling);
    containers.values.reserve(ceiling);
    ordinals_.replace(std::move(containers.keys), std::move(containers.values));
  }
  runs_.clear();
  ordinals_.clear();
  next_publications_.clear();
  replays_.reserve(ceiling);
  quad_hashes_.reserve(ceiling);
  replays_.resize(quads_.size());
  quad_hashes_.resize(quads_.size());
  const std::uint64_t zoomed =
      DrawListCache::hash_value(DrawListCache::seed, target_.zoom);
  std::size_t filled = 0;
  const auto fill = [&](std::size_t until) {
    for (; filled < until; ++filled) {
      const Quad& quad = quads_[filled];
      replays_[filled] = replay(quad);
      quad_hashes_[filled] =
          quad_hash(zoomed, quad, replays_[filled].baked,
                    quad.mesh != 0 ? &meshes_[quad.mesh - 1] : nullptr);
    }
  };
  const auto reuse = [&](std::size_t to, std::size_t from, std::size_t count) {
    fill(to);
    const auto source = static_cast<std::ptrdiff_t>(from);
    const auto length = static_cast<std::ptrdiff_t>(count);
    std::copy_n(previous_replays_.begin() + source, length,
                replays_.begin() + static_cast<std::ptrdiff_t>(to));
    std::copy_n(previous_hashes_.begin() + source, length,
                quad_hashes_.begin() + static_cast<std::ptrdiff_t>(to));
    filled = to + count;
    for (std::size_t index = to; index < filled; ++index) {
      replays_[index].opacity = fades_[quads_[index].fade].value;
    }
  };
  for (const Splice& splice : splices_) {
    reuse(splice.to, splice.from, splice.count);
  }
  fill(main_count_);
  for (const Splice& splice : ring_splices_) {
    reuse(main_count_ + splice.to, previous_main_ + splice.from, splice.count);
  }
  fill(quads_.size());
  std::size_t begin = 0;
  while (begin < quads_.size()) {
    const std::uint64_t owner = quads_[begin].owner;
    const std::array<float, 2> offset = replays_[begin].translation;
    const float opacity = replays_[begin].opacity;
    std::size_t end = begin + 1;
    while (end < quads_.size() && quads_[end].owner == owner &&
           replays_[end].translation == offset &&
           replays_[end].opacity == opacity) {
      ++end;
    }
    std::uint32_t& ordinal = ordinals_[owner];
    const DrawListCache::Key key{.owner = owner, .ordinal = ordinal++};
    std::uint64_t content =
        DrawListCache::hash_value(DrawListCache::seed, target_.zoom);
    std::uint64_t order = DrawListCache::seed;
    std::uint32_t count = 0;
    bool fresh = false;
    bool samples = false;
    for (std::size_t index = begin; index < end; ++index) {
      const Quad& quad = quads_[index];
      content = DrawListCache::hash_value(content, quad_hashes_[index]);
      if (quad.texture.has_value()) {
        content = DrawListCache::hash_value(
            content, shapes_.ramp_revision(*quad.texture));
      }
      order = DrawListCache::hash_value(
          order,
          std::array<std::uint64_t, 3>{
              quad.texture.value_or(provider::TextureId{0}).value(),
              quad.detail_texture.value_or(provider::TextureId{0}).value(),
              quad.backdrop ? 1U : 0U});
      count += static_cast<std::uint32_t>(cost(quad)[0]);
      const bool renewed = quad.external && quad.texture.has_value() &&
                           republished(*quad.texture);
      fresh = fresh || quad.fresh || renewed;
      samples = samples || samples_backdrop(quad.kind);
    }
    const std::size_t placed =
        draw_lists_.place(DrawListCache::Run{.key = key,
                                             .content = content,
                                             .order = order,
                                             .count = count,
                                             .fresh = fresh,
                                             .offset = offset,
                                             .opacity = opacity});
    runs_.push_back(RunSpan{
        .begin = begin, .end = end, .placed = placed, .samples = samples});
    begin = end;
  }
  std::swap(shown_publications_, next_publications_);
}

auto Painter::vertex_box(
    std::span<const provider::ShapeVertex> vertices) noexcept
    -> DrawListCache::Bounds {
  if (vertices.empty()) {
    return DrawListCache::Bounds{
        .left = 0.0F, .top = 0.0F, .right = -1.0F, .bottom = -1.0F};
  }
  DrawListCache::Bounds box{.left = vertices.front().position[0],
                            .top = vertices.front().position[1],
                            .right = vertices.front().position[0],
                            .bottom = vertices.front().position[1]};
  for (const provider::ShapeVertex& vertex : vertices) {
    box.left = std::min(box.left, vertex.position[0]);
    box.top = std::min(box.top, vertex.position[1]);
    box.right = std::max(box.right, vertex.position[0]);
    box.bottom = std::max(box.bottom, vertex.position[1]);
  }
  return box;
}

auto Painter::drawn_bounds(const DrawListCache::Bounds& box,
                           const provider::ClipRect& clip,
                           std::array<float, 2> shift) noexcept
    -> DrawListCache::Bounds {
  if (box.right < box.left || box.bottom < box.top) {
    return {};
  }
  return DrawListCache::Bounds{
      .left = std::max(box.left + shift[0] - 1.0F, static_cast<float>(clip.x)),
      .top = std::max(box.top + shift[1] - 1.0F, static_cast<float>(clip.y)),
      .right = std::min(box.right + shift[0] + 1.0F,
                        static_cast<float>(clip.x + clip.width)),
      .bottom = std::min(box.bottom + shift[1] + 1.0F,
                         static_cast<float>(clip.y + clip.height))};
}

auto Painter::clip_bounds(const provider::ClipRect& clip) noexcept
    -> DrawListCache::Bounds {
  return DrawListCache::Bounds{
      .left = static_cast<float>(clip.x),
      .top = static_cast<float>(clip.y),
      .right = static_cast<float>(clip.x + clip.width),
      .bottom = static_cast<float>(clip.y + clip.height)};
}

auto Painter::damage_rect(const DrawListCache::Bounds& bounds) const noexcept
    -> provider::ClipRect {
  if (bounds.empty()) {
    return provider::ClipRect{};
  }
  const auto width = static_cast<float>(target_.pixels.width.value());
  const auto height = static_cast<float>(target_.pixels.height.value());
  const float left = std::clamp(std::floor(bounds.left), 0.0F, width);
  const float top = std::clamp(std::floor(bounds.top), 0.0F, height);
  const float right = std::clamp(std::ceil(bounds.right), 0.0F, width);
  const float bottom = std::clamp(std::ceil(bounds.bottom), 0.0F, height);
  if (right <= left || bottom <= top) {
    return provider::ClipRect{};
  }
  return provider::ClipRect{.x = static_cast<std::uint32_t>(left),
                            .y = static_cast<std::uint32_t>(top),
                            .width = static_cast<std::uint32_t>(right - left),
                            .height = static_cast<std::uint32_t>(bottom - top)};
}

auto Painter::emit(provider::GpuFrame& frame)
    -> std::expected<Statistics, PaintError> {
  constexpr std::size_t vertices_per_quad = 4;
  constexpr std::size_t indices_per_quad = 6;
  if (target_.replay.has_value()) {
    target_.replay->get().replayable = false;
  }
  const bool whole = whole_damage();
  draw_lists_.begin(whole);
  gather_runs();
  const std::uint32_t stable = draw_lists_.extent();
  std::size_t vertex_total = stable;
  std::size_t index_total = 0;
  for (const Quad& quad : quads_) {
    index_total += cost(quad)[1];
  }
  for (const LayerPass& pass : std::span{layer_passes_}.first(layer_count_)) {
    for (const Quad& quad : pass.quads) {
      const auto [vertices, indices] = cost(quad);
      vertex_total += vertices;
      index_total += indices;
    }
  }
  const std::size_t extra = compositor_.extra_quads();
  vertex_total += extra * vertices_per_quad;
  index_total += extra * indices_per_quad;
  Statistics statistics{.quads = static_cast<std::uint32_t>(quads_.size()),
                        .commands = 0,
                        .animating = false};
  const auto settle_damage = [&] {
    const std::optional<DrawListCache::Bounds> damaged = draw_lists_.finish();
    const auto full = static_cast<std::uint64_t>(target_.pixels.width.value()) *
                      target_.pixels.height.value();
    if (!damaged.has_value()) {
      statistics.damaged_pixels = full;
      return;
    }
    const provider::ClipRect region = damage_rect(*damaged);
    frame.damage(region);
    statistics.damage = region;
    statistics.damaged_pixels =
        static_cast<std::uint64_t>(region.width) * region.height;
  };
  if (index_total == 0) {
    settle_damage();
    return statistics;
  }
  if (vertex_total > std::numeric_limits<std::uint32_t>::max() ||
      index_total > std::numeric_limits<std::uint32_t>::max()) {
    whole_next_ = true;
    return std::unexpected(PaintError{PaintErrorCode::geometry_unavailable,
                                      PaintStage::resident_geometry});
  }
  auto resident =
      frame.allocate_resident(static_cast<std::uint32_t>(vertex_total),
                              static_cast<std::uint32_t>(index_total));
  if (!resident.has_value()) {
    whole_next_ = true;
    return std::unexpected(PaintError{PaintErrorCode::geometry_unavailable,
                                      PaintStage::resident_geometry,
                                      resident.error()});
  }
  draw_lists_.adopt(resident->generation);
  const provider::ShapeGeometry& geometry = resident->geometry;
  const provider::TextureId neutral = atlas_.pages().front().texture();
  provider::TextureId current = neutral;
  for (const Quad& quad : quads_) {
    if (quad.texture.has_value() && !quad.backdrop) {
      current = *quad.texture;
      break;
    }
  }
  std::uint32_t base = stable;
  std::uint32_t first = 0;
  emit_layers(frame, geometry, base, first);
  std::uint32_t uploaded = base - stable;
  frame.touch(provider::VertexRange{.first = stable, .count = base - stable});
  const bool sensing = compositor_.pending_backdrops() > 0;
  std::vector<Compositor::QuadSpan>& spans = spans_;
  spans.clear();
  spans.reserve(quads_.size());
  for (const RunSpan& run : runs_) {
    const DrawListCache::Placement placement =
        draw_lists_.placement(run.placed);
    std::uint32_t at = placement.first;
    DrawListCache::Extent extent{};
    std::optional<float> sampling_reach;
    for (std::size_t index = run.begin; index < run.end; ++index) {
      const Quad& quad = quads_[index];
      const auto count = static_cast<std::uint32_t>(cost(quad)[0]);
      const Replay& replayed = replays_[index];
      const std::array<float, 2>& translation = replayed.translation;
      Compositor::QuadSpan span{.vertex_begin = at,
                                .vertex_end = at + count,
                                .index_begin = first,
                                .translation = translation,
                                .content = quad_hashes_[index],
                                .damaged = placement.damaged};
      if (placement.write) {
        const std::span<provider::ShapeVertex> out =
            geometry.vertices.subspan(at, count);
        write_vertices(quad, replayed.baked, out);
        if (target_.zoom != 1.0F) {
          zoom_vertices(out);
        }
        const DrawListCache::Bounds box = vertex_box(out);
        const provider::DrawCommand shifted{.translation = translation};
        extent.drawn = extent.drawn.united(
            drawn_bounds(box, quad.clip, shifted.translation_for(quad.kind)));
        extent.local = extent.local.united(box.grown(replay_margin));
        extent.clip = extent.clip.united(clip_bounds(quad.clip));
        if (samples_backdrop(quad.kind)) {
          const std::array<float, 4>& region = out.front().effect;
          extent.sense = extent.sense.united(
              DrawListCache::Bounds{.left = region[0],
                                    .top = region[1],
                                    .right = region[0] + region[2],
                                    .bottom = region[1] + region[3]});
          sampling_reach = std::max(sampling_reach.value_or(0.0F), quad.reach);
        }
      }
      const std::uint32_t used =
          write_indices(quad, geometry.indices.subspan(first), at);
      batch(commands_, owners_, quad, current, first, used, replayed);
      if (!glass_pending_.empty() &&
          quad.kind == provider::ShapeKind::glass_path &&
          std::ranges::contains(glass_pending_, quad.owner)) {
        report_glass(quad.owner, geometry.vertices.subspan(at, count), geometry,
                     commands_.back());
      }
      first += used;
      at += count;
      if (sensing) {
        spans.push_back(span);
      }
    }
    if (placement.write) {
      frame.touch(provider::VertexRange{.first = placement.first,
                                        .count = placement.count});
      uploaded += placement.count;
      extent.reach = sampling_reach.value_or(extent.reach);
      draw_lists_.bound(run.placed, extent);
    }
    if (sensing) {
      const DrawListCache::Bounds reach = draw_lists_.local(run.placed);
      for (Compositor::QuadSpan& span :
           std::span{spans}.subspan(spans.size() - (run.end - run.begin))) {
        span.reach = reach;
      }
    }
  }
  if (target_.zoom != 1.0F) {
    zoom_commands();
  }
  const std::uint32_t composed_from = base;
  compositor_.emit(
      frame, geometry, base, first, commands_, spans,
      sensing ? draw_lists_.changes() : std::nullopt,
      provider::Colour{
          .red = clear_.red,
          .green = clear_.green,
          .blue = clear_.blue,
          .alpha = target_.clear.has_value() ? target_.clear->alpha : 1.0F});
  if (compositor_.unsettled()) {
    animating_ = true;
  }
  const auto composed_count =
      static_cast<std::uint32_t>(extra * vertices_per_quad);
  frame.touch(
      provider::VertexRange{.first = composed_from, .count = composed_count});
  uploaded += composed_count;
  for (const RunSpan& run : runs_) {
    if (run.samples && compositor_.repaints(quads_[run.begin].owner)) {
      draw_lists_.refresh(run.placed);
    }
  }
  settle_damage();
  drop_withdrawn();
  if (auto drawn = frame.draw(
          provider::ShapeDrawData{.geometry = geometry, .commands = commands_});
      !drawn.has_value()) {
    whole_next_ = true;
    return std::unexpected(PaintError{PaintErrorCode::draw_rejected,
                                      PaintStage::main_draw, drawn.error()});
  }
  capture(geometry, static_cast<std::uint32_t>(vertex_total),
          resident->generation,
          extra == 0 && layer_count_ == 0 && target_.zoom == 1.0F);
  statistics.commands = static_cast<std::uint32_t>(commands_.size());
  statistics.uploaded_vertices = uploaded;
  statistics.uploaded_bytes =
      (static_cast<std::uint64_t>(uploaded) * sizeof(provider::ShapeVertex)) +
      (static_cast<std::uint64_t>(index_total) * sizeof(provider::Index));
  return statistics;
}

void Painter::report_targets(Statistics& statistics) {
  const RenderTargetPool::Statistics now = compositor_.pool().statistics();
  const auto delta = [](std::size_t current, std::size_t before) {
    return static_cast<std::uint32_t>(current - before);
  };
  statistics.targets_created = delta(now.created, reported_targets_.created);
  statistics.targets_reused = delta(now.reused, reported_targets_.reused);
  statistics.targets_released = delta(now.released, reported_targets_.released);
  statistics.targets_evicted = delta(now.evicted, reported_targets_.evicted);
  statistics.targets_refused = delta(now.refused, reported_targets_.refused);
  reported_targets_ = now;
}

void Painter::drop_withdrawn() {
  const std::span<const provider::TextureId> withdrawn =
      compositor_.withdrawn();
  if (withdrawn.empty()) {
    return;
  }
  const auto samples = [withdrawn](const provider::DrawCommand& command) {
    return std::ranges::contains(withdrawn, command.texture) ||
           std::ranges::contains(withdrawn, command.detail);
  };
  std::size_t kept = 0;
  for (std::size_t index = 0; index < commands_.size(); ++index) {
    if (samples(commands_[index])) {
      continue;
    }
    commands_[kept] = commands_[index];
    owners_[kept] = owners_[index];
    ++kept;
  }
  if (kept != commands_.size()) {
    commands_.resize(kept);
    owners_.resize(kept);
    animating_ = true;
    whole_next_ = true;
  }
}

void Painter::zoom_vertices(std::span<provider::ShapeVertex> vertices) const {
  const float factor = target_.zoom;
  const float cx = static_cast<float>(target_.pixels.width.value()) * 0.5F;
  const float cy = static_cast<float>(target_.pixels.height.value()) * 0.5F;
  for (provider::ShapeVertex& vertex : vertices) {
    vertex.position = {cx + ((vertex.position[0] - cx) * factor),
                       cy + ((vertex.position[1] - cy) * factor)};
    if (samples_backdrop(vertex.kind)) {
      vertex.effect = {cx + ((vertex.effect[0] - cx) * factor),
                       cy + ((vertex.effect[1] - cy) * factor),
                       vertex.effect[2] * factor, vertex.effect[3] * factor};
    }
  }
}

void Painter::zoom_commands() {
  const float factor = target_.zoom;
  const float cx = static_cast<float>(target_.pixels.width.value()) * 0.5F;
  const float cy = static_cast<float>(target_.pixels.height.value()) * 0.5F;
  compositor_.zoom(factor, cx, cy);
  const auto width = static_cast<float>(target_.pixels.width.value());
  const auto height = static_cast<float>(target_.pixels.height.value());
  for (provider::DrawCommand& command : commands_) {
    const float left = std::clamp(
        cx + ((static_cast<float>(command.clip.x) - cx) * factor), 0.0F, width);
    const float top =
        std::clamp(cy + ((static_cast<float>(command.clip.y) - cy) * factor),
                   0.0F, height);
    const float right = std::clamp(
        cx + ((static_cast<float>(command.clip.x + command.clip.width) - cx) *
              factor),
        0.0F, width);
    const float bottom = std::clamp(
        cy + ((static_cast<float>(command.clip.y + command.clip.height) - cy) *
              factor),
        0.0F, height);
    command.clip = provider::ClipRect{
        .x = static_cast<std::uint32_t>(std::floor(left)),
        .y = static_cast<std::uint32_t>(std::floor(top)),
        .width =
            static_cast<std::uint32_t>(std::ceil(right) - std::floor(left)),
        .height =
            static_cast<std::uint32_t>(std::ceil(bottom) - std::floor(top))};
  }
}

void Painter::push_geometry(const ShapePainter::Geometry& geometry,
                            const provider::ClipRect& clip) {
  if (geometry.indices.empty() || geometry.vertices.empty() ||
      clip.width == 0 || clip.height == 0) {
    return;
  }
  push_mesh(stage_mesh(geometry, geometry.revision), clip,
            provider::ShapeKind::path, false);
}

auto Painter::stage_mesh(const ShapePainter::Geometry& geometry,
                         std::uint64_t revision) -> ShapePainter::Geometry& {
  if (mesh_count_ == meshes_.size()) {
    meshes_.emplace_back();
  }
  ShapePainter::Geometry& slot = meshes_[mesh_count_++];
  slot.restaged = revision == 0 || slot.revision != revision;
  if (slot.restaged) {
    slot.vertices.assign(geometry.vertices.begin(), geometry.vertices.end());
    slot.indices.assign(geometry.indices.begin(), geometry.indices.end());
    slot.revision = revision;
  }
  slot.texture = geometry.texture;
  slot.length = geometry.length;
  slot.alpha = geometry.alpha;
  slot.trims = geometry.trims;
  slot.trim = geometry.trim;
  slot.retrimmed = geometry.retrimmed;
  slot.extent = geometry.extent;
  return slot;
}

void Painter::push_mesh(const ShapePainter::Geometry& geometry,
                        const provider::ClipRect& clip,
                        provider::ShapeKind kind, bool backdrop) {
  quads_.push_back(Quad{.kind = kind,
                        .bounds = {},
                        .uv = {},
                        .centre = {},
                        .half_size = {},
                        .shape = {},
                        .colour = 0,
                        .texture = geometry.texture,
                        .clip = clip,
                        .transform = transform_,
                        .mesh = static_cast<std::uint32_t>(mesh_count_),
                        .effect = {},
                        .detail = {},
                        .detail_texture = std::nullopt,
                        .backdrop = backdrop,
                        .owner = 0,
                        .fresh = geometry.retrimmed,
                        .trim = geometry.trim});
}

void Painter::restyle(const Scope& scope, const graphics::ShapeStyle& style,
                      Box frame, ShapePainter::Paint& paint) {
  using Kind = ShapePainter::PaintKind;
  frame = bounded(style, frame);
  const auto stops = [&scope, &paint](const graphics::Gradient& gradient) {
    paint.stops.clear();
    for (const graphics::Gradient::Stop& stop : gradient.stops()) {
      paint.stops.push_back(ShapePainter::Stop{
          .colour = scope.colour(stop.color), .location = stop.location});
    }
    paint.space = gradient.interpolation();
  };
  const auto at = [&frame](motion::UnitPoint point) {
    return std::array<float, 2>{frame.left + (point.x * frame.width()),
                                frame.top + (point.y * frame.height())};
  };
  paint.colour = {};
  paint.owner.reset();
  paint.first = 0.0F;
  paint.second = 0.0F;
  paint.start = {};
  paint.end = {};
  std::visit(
      [&](const auto& each) {
        using T = std::remove_cvref_t<decltype(each)>;
        if constexpr (std::same_as<T, theme::ColourValue>) {
          paint.kind = Kind::solid;
          paint.colour = scope.colour(each);
          paint.stops.clear();
          paint.space = graphics::Gradient::ColorSpace::device;
        } else if constexpr (std::same_as<T, theme::Material>) {
          paint.kind = Kind::solid;
          paint.colour = material_colour(scope, each);
          paint.stops.clear();
          paint.space = graphics::Gradient::ColorSpace::device;
        } else if constexpr (std::same_as<T,
                                          graphics::HierarchicalShapeStyle> ||
                             std::same_as<T, graphics::BackgroundStyle>) {
          paint.kind = Kind::solid;
          paint.colour = scope.colour(theme::ColourRole::label);
          paint.stops.clear();
          paint.space = graphics::Gradient::ColorSpace::device;
        } else if constexpr (std::same_as<T, graphics::EllipticalGradient>) {
          paint.kind = Kind::elliptical;
          stops(each.gradient);
          paint.start = at(each.center);
          paint.end = {frame.width(), frame.height()};
          paint.first = each.start_radius_fraction;
          paint.second = each.end_radius_fraction;
        } else if constexpr (std::same_as<T, graphics::LinearGradient>) {
          paint.kind = Kind::linear;
          stops(each.gradient);
          paint.start = at(each.start_point);
          paint.end = at(each.end_point);
        } else if constexpr (std::same_as<T, graphics::RadialGradient>) {
          paint.kind = Kind::radial;
          stops(each.gradient);
          paint.start = at(each.center);
          paint.end = at(each.center);
          paint.first = each.start_radius;
          paint.second = each.end_radius;
        } else if constexpr (std::same_as<T, graphics::AngularGradient>) {
          paint.kind = Kind::angular;
          stops(each.gradient);
          paint.start = at(each.center);
          paint.end = at(each.center);
          paint.first = static_cast<float>(each.start_angle.radians());
          paint.second = static_cast<float>(each.end_angle.radians());
        }
      },
      style.value());
}

void Painter::draw_shape(const composition::Node& node, const Scope& scope,
                         motion::Presentation& presentation,
                         const motion::Presentation::Moment& moment, Box frame,
                         Corners corners) {
  const auto found = node.content_as<components::ShapeView::Content>();
  const components::ShapeView::Content& content = found->get();
  std::vector<const components::ShapeView::Content*> passes;
  for (const components::ShapeView::Content* under = content.under();
       under != nullptr; under = under->under()) {
    passes.push_back(under);
  }
  for (std::size_t depth = passes.size(); depth > 0; --depth) {
    const std::uint64_t key =
        node.id().value() ^
        (shape_pass_salt * static_cast<std::uint64_t>(depth));
    draw_pass(node, *passes[depth - 1], composition::NodeId{key}, scope,
              presentation, moment, frame, corners);
  }
  draw_pass(node, content, node.id(), scope, presentation, moment, frame,
            corners);
}

void Painter::draw_pass(const composition::Node& node,
                        const components::ShapeView::Content& content,
                        composition::NodeId key, const Scope& scope,
                        motion::Presentation& presentation,
                        const motion::Presentation::Moment& moment, Box frame,
                        Corners corners) {
  using Property = motion::Presentation::Property;
  const bool top = key == node.id();
  const float scale = target_.scale.value();
  const double from =
      top ? presentation.animate(Property::trim_start,
                                 motion::AnimatableVector{content.trim_from()},
                                 moment)[0]
          : content.trim_from();
  const double to =
      top ? presentation.animate(Property::trim_end,
                                 motion::AnimatableVector{content.trim_to()},
                                 moment)[0]
          : content.trim_to();
  ShapePainter::Request& request = shape_request_;
  if (const auto& declared = content.stroke_style(); declared.has_value()) {
    if (!request.stroke.has_value()) {
      request.stroke.emplace(std::move(spare_stroke_));
    }
    *request.stroke = *declared;
    if (top) {
      const motion::AnimatableVector animated = presentation.animate(
          Property::stroke,
          motion::AnimatableVector{declared->line_width, declared->dash_phase,
                                   declared->miter_limit},
          moment);
      request.stroke->line_width = static_cast<float>(animated[0]);
      request.stroke->dash_phase = static_cast<float>(animated[1]);
      request.stroke->miter_limit = static_cast<float>(animated[2]);
    }
  } else if (request.stroke.has_value()) {
    spare_stroke_ = std::move(*request.stroke);
    request.stroke.reset();
  }
  const graphics::ShapeStyle& declared_style =
      content.style().has_value() ? *content.style() : foreground_style_;
  const Styled styled = style_of(scope, declared_style);
  const graphics::ShapeStyle& style = *styled.style;
  const graphics::ShapeStyle& shadowed =
      declared_style.shadow().has_value() ? declared_style : style;
  if (const graphics::ImagePaint* tiled = style.image_paint();
      tiled != nullptr && !request.stroke.has_value()) {
    paint_image(scope, *tiled, frame,
                scope.opacity * styled.opacity * style.opacity());
    return;
  }
  std::optional<theme::Shape> leaf;
  bool mirrored = false;
  if (const auto* outline = std::get_if<theme::Shape>(&content.source())) {
    leaf = *outline;
  } else if (const auto* any =
                 std::get_if<graphics::AnyShape>(&content.source())) {
    leaf = any->leaf();
    mirrored = any->path_mirrors_in(scope.direction);
  } else {
    mirrored = scope.direction == composition::LayoutDirection::right_to_left;
  }
  Corners resolved = corners;
  if (leaf.has_value() && mirrored) {
    Scope unmirrored = scope;
    unmirrored.direction = composition::LayoutDirection::left_to_right;
    resolved = resolve(unmirrored, *leaf, frame);
  } else if (leaf.has_value()) {
    resolved = resolve(scope, *leaf, frame);
  }
  request.node = key;
  request.source = content.shared_source();
  request.area = ShapePainter::Area{.left = frame.left,
                                    .top = frame.top,
                                    .right = frame.right,
                                    .bottom = frame.bottom,
                                    .scale = scale};
  request.corner_radius = resolved.radius / scale;
  request.corners = theme::RectangleCornerRadii{
      .top_leading = corner_of(resolved, 0) / scale,
      .bottom_leading = corner_of(resolved, 3) / scale,
      .bottom_trailing = corner_of(resolved, 2) / scale,
      .top_trailing = corner_of(resolved, 1) / scale};
  request.inset =
      content.inset() + (content.strokes_border() && request.stroke.has_value()
                             ? request.stroke->line_width * 0.5F
                             : 0.0F);
  request.fill = content.fill_style();
  request.mirrored = mirrored;
  request.trim_from = from;
  request.trim_to = to;
  restyle(scope, style, frame, request.paint);
  if (top && style.gradient() != nullptr) {
    animate_paint(request.paint, key, presentation, moment,
                  motion::Presentation::Mix::shape);
  }
  request.opacity = scope.opacity * styled.opacity *
                    (style.gradient() != nullptr ? style.opacity() : 1.0F);
  request.guide = content.guide();
  request.reach = content.reach();
  if (top) {
    pace(node, presentation);
  }
  if (scope.opacity <= 0.0F) {
    return;
  }
  const float inset = request.inset * scale;
  const Box inset_frame{.left = frame.left + inset,
                        .top = frame.top + inset,
                        .right = frame.right - inset,
                        .bottom = frame.bottom - inset};
  const Box outlined =
      leaf.has_value() ? fitted(*leaf, inset_frame) : inset_frame;
  const Corners shrunk = grown(resolved, -inset);
  const bool sdf = leaf.has_value() &&
                   std::holds_alternative<theme::Shape>(content.source()) &&
                   !request.stroke.has_value() && from <= 0.0 && to >= 1.0;
  if (sdf) {
    if (const std::optional<theme::Material> material = style.material()) {
      Scope faded = scope;
      faded.opacity *= styled.opacity;
      frost(node.id(), faded, *material, snap(outlined), shrunk);
      style_shadow(scope, shadowed, outlined, shrunk,
                   graphics::ShadowStyle::Kind::inner);
      return;
    }
    style_shadow(scope, shadowed, outlined, shrunk,
                 graphics::ShadowStyle::Kind::drop);
  }
  if (const ShapePainter::Geometry* geometry = shapes_.shape(request)) {
    if (top && binding_ != 0 && geometry->trims) {
      ReplayBinding& bound = bindings_[binding_ - 1];
      if (bound.trim_start.has_value() || bound.trim_end.has_value()) {
        bound.trim_length = geometry->length;
        bound.trims = true;
      }
    }
    if (top && content.follows_glass()) {
      if (const std::optional<theme::Glass> glass =
              glass_of(node.modifiers(), scope.theme)) {
        glass_stroke(node, scope, *glass, frame, *geometry);
        return;
      }
    }
    push_geometry(*geometry, scope.clip);
  }
  if (sdf) {
    style_shadow(scope, shadowed, outlined, shrunk,
                 graphics::ShadowStyle::Kind::inner);
  }
}

void Painter::draw_mesh(const composition::Node& node, const Scope& scope,
                        motion::Presentation& presentation,
                        const motion::Presentation::Moment& moment, Box frame) {
  using Property = motion::Presentation::Property;
  const auto found = node.content_as<components::MeshGradient::Content>();
  const components::MeshGradient::Content& content = found->get();
  if (!content.well_formed()) {
    return;
  }
  const theme::Colour background = scope.colour(content.background());
  if (background.alpha > 0.0F) {
    shape(provider::ShapeKind::fill, frame, Corners{}, 0.0F, 0.0F,
          pack(background, scope.opacity), scope.clip);
  }
  ShapePainter::MeshState& state = mesh_request_.state;
  state.width = content.width();
  state.height = content.height();
  state.points.assign(content.points().begin(), content.points().end());
  state.colours.clear();
  for (const theme::ColourValue& colour : content.colors()) {
    state.colours.push_back(scope.colour(colour));
  }
  if (!content.bezier_points().empty()) {
    components::MeshGradient::Grid grid = content.expanded(state.colours);
    state.width = grid.width;
    state.height = grid.height;
    state.points = std::move(grid.points);
    state.colours = std::move(grid.colours);
  }
  ShapePainter::Memory& memory = shapes_.remember(node.id(), state);
  const auto generation = static_cast<double>(memory.generation);
  motion::Presentation::Moment stepped = moment;
  if (const std::optional<double> rate = presentation.frame_rate();
      rate.has_value() && *rate > 0.0) {
    const auto period =
        std::chrono::nanoseconds{static_cast<std::int64_t>(1.0e9 / *rate)};
    stepped.now = (moment.now / period) * period;
  }
  const double reached = presentation.animate(
      Property::mesh, motion::AnimatableVector{generation}, stepped)[0];
  const double amount = std::clamp(reached - (generation - 1.0), 0.0, 1.0);
  ShapePainter::blend(memory, amount);
  pace(node, presentation);
  ShapePainter::MeshRequest& request = mesh_request_;
  request.state = memory.shown;
  request.smooth = content.smooths_colors();
  request.space = content.color_space();
  request.grain = content.grain();
  request.area = ShapePainter::Area{.left = frame.left,
                                    .top = frame.top,
                                    .right = frame.right,
                                    .bottom = frame.bottom,
                                    .scale = target_.scale.value()};
  request.opacity = scope.opacity;
  push_geometry(shapes_.mesh(node.id(), request), scope.clip);
}

void Painter::place(const Quad& quad, const Affine& matrix,
                    std::span<provider::ShapeVertex> vertices) const {
  constexpr std::size_t corners = 4;
  std::array<std::array<float, 2>, corners> points{
      {{quad.bounds.left, quad.bounds.top},
       {quad.bounds.right, quad.bounds.top},
       {quad.bounds.right, quad.bounds.bottom},
       {quad.bounds.left, quad.bounds.bottom}}};
  std::array<std::array<float, 2>, corners> uvs{{{quad.uv[0], quad.uv[1]},
                                                 {quad.uv[2], quad.uv[1]},
                                                 {quad.uv[2], quad.uv[3]},
                                                 {quad.uv[0], quad.uv[3]}}};
  std::array<std::array<float, 2>, corners> locals{};
  for (std::size_t corner = 0; corner < corners; ++corner) {
    locals[corner] = {points[corner][0] - quad.centre[0],
                      points[corner][1] - quad.centre[1]};
  }
  std::array<float, 2> half_size = quad.half_size;
  std::array<float, 4> outline = quad.shape;
  std::array<float, 4> detail = quad.detail;
  std::array<float, 4> effect = quad.effect;
  if (!matrix.identity()) {
    if (quad.kind == provider::ShapeKind::glyph && matrix.translating()) {
      const float dx = std::round(matrix.tx);
      const float dy = std::round(matrix.ty);
      for (std::array<float, 2>& point : points) {
        point = {point[0] + dx, point[1] + dy};
      }
    } else if (quad.kind == provider::ShapeKind::glyph ||
               quad.kind == provider::ShapeKind::glyph_gradient ||
               quad.kind == provider::ShapeKind::image ||
               quad.kind == provider::ShapeKind::yuv ||
               quad.kind == provider::ShapeKind::layer ||
               quad.kind == provider::ShapeKind::filter ||
               quad.kind == provider::ShapeKind::blend ||
               quad.kind == provider::ShapeKind::mask ||
               quad.kind == provider::ShapeKind::gaussian) {
      for (std::array<float, 2>& point : points) {
        point = matrix.apply(point);
      }
    } else {
      const float stretch_x = std::hypot(matrix.a, matrix.b);
      const float stretch_y = std::hypot(matrix.c, matrix.d);
      const float cosine = stretch_x > 0.0F ? matrix.a / stretch_x : 1.0F;
      const float sine = stretch_x > 0.0F ? matrix.b / stretch_x : 0.0F;
      const std::array<float, 2> centre = matrix.apply(quad.centre);
      for (std::size_t corner = 0; corner < corners; ++corner) {
        const float local_x = locals[corner][0] * stretch_x;
        const float local_y = locals[corner][1] * stretch_y;
        locals[corner] = {local_x, local_y};
        points[corner] = {centre[0] + (cosine * local_x) - (sine * local_y),
                          centre[1] + (sine * local_x) + (cosine * local_y)};
      }
      const float uniform = std::min(stretch_x, stretch_y);
      half_size = {half_size[0] * stretch_x, half_size[1] * stretch_y};
      outline[provider::ShapeVertex::radius] *= uniform;
      if (quad.kind == provider::ShapeKind::fill ||
          quad.kind == provider::ShapeKind::stroke ||
          quad.kind == provider::ShapeKind::shadow ||
          quad.kind == provider::ShapeKind::inner_shadow ||
          quad.kind == provider::ShapeKind::gradient_fill) {
        for (float& corner : detail) {
          corner = corner > 0.0F ? corner * uniform : corner;
        }
      }
      if (quad.kind == provider::ShapeKind::inner_shadow) {
        effect = {effect[0] * stretch_x, effect[1] * stretch_y, effect[2],
                  effect[3]};
        outline[provider::ShapeVertex::softness] *= uniform;
      } else if (quad.kind == provider::ShapeKind::gradient_fill) {
        const float mode = outline[provider::ShapeVertex::stroke];
        effect[0] *= stretch_x;
        effect[1] *= stretch_y;
        if (mode < 0.5F) {
          effect[2] *= stretch_x;
          effect[3] *= stretch_y;
        } else if (mode < 1.5F) {
          effect[2] *= uniform;
          effect[3] *= uniform;
        }
        if (mode > 2.5F) {
          for (std::array<float, 2>& extent : uvs) {
            extent = {extent[0] * stretch_x, extent[1] * stretch_y};
          }
        }
      } else if (quad.kind == provider::ShapeKind::fill ||
                 quad.kind == provider::ShapeKind::stroke ||
                 quad.kind == provider::ShapeKind::shadow) {
        outline[provider::ShapeVertex::stroke] *= uniform;
        outline[provider::ShapeVertex::softness] *= uniform;
      } else if (quad.kind == provider::ShapeKind::glass ||
                 quad.kind == provider::ShapeKind::glass_union) {
        outline[provider::ShapeVertex::stroke] *= uniform;
      }
    }
  }
  for (std::size_t corner = 0; corner < corners; ++corner) {
    vertices[corner] = provider::ShapeVertex{.position = points[corner],
                                             .uv = uvs[corner],
                                             .local = locals[corner],
                                             .half_size = half_size,
                                             .shape = outline,
                                             .colour = quad.colour,
                                             .kind = quad.kind,
                                             .effect = effect,
                                             .detail = detail};
  }
}

auto Painter::hit_region(const composition::Node& node,
                         const theme::Theme& theme, const Box& frame,
                         float scale) -> HitRegion {
  const composition::Modifiers& modifiers = node.modifiers();
  const composition::Interactions& declared = node.interactions();
  const bool decorated =
      modifiers.background.has_value() || modifiers.material.has_value() ||
      modifiers.border.has_value() || modifiers.overlay.has_value() ||
      modifiers.glass.has_value() ||
      modifiers.shadow != theme::Elevation::none ||
      modifiers.drop_shadow.has_value();
  const bool interactive =
      !declared.gestures.empty() || declared.focusable.value_or(false) ||
      declared.drag != nullptr || declared.drop != nullptr ||
      declared.hover != nullptr || declared.continuous_hover != nullptr ||
      declared.pointer_style.has_value() ||
      declared.scrolling.container.has_value() ||
      node.content().activatable() ||
      node.content_as<composition::Layer::Content>().has_value();
  if (decorated || interactive || declared.hit_shape.has_value() ||
      declared.hit_path.has_value()) {
    return {};
  }
  if (node.child_count() > 0 || !node.content().draws()) {
    return HitRegion{.drawn = std::nullopt, .hollow = true};
  }
  if (const std::optional<composition::Rect>& aligned = node.content_frame();
      aligned.has_value()) {
    return HitRegion{
        .drawn =
            Box{.left = frame.left + (aligned->x * scale),
                .top = frame.top + (aligned->y * scale),
                .right = frame.left + ((aligned->x + aligned->width) * scale),
                .bottom = frame.top + ((aligned->y + aligned->height) * scale)},
        .hollow = false};
  }
  if (!modifiers.padding.has_value()) {
    return {};
  }
  const theme::Edges edges = theme.edges(*modifiers.padding).positive();
  return HitRegion{
      .drawn = Box{.left = frame.left + (edges.leading * scale),
                   .top = frame.top + (edges.top * scale),
                   .right = frame.right - (edges.trailing * scale),
                   .bottom = frame.bottom - (edges.bottom * scale)},
      .hollow = false};
}

auto Painter::glass_of(const composition::Modifiers& modifiers,
                       const theme::Theme& theme) noexcept
    -> std::optional<theme::Glass> {
  if (!modifiers.glass.has_value()) {
    return std::nullopt;
  }
  const composition::GlassEffect& effect = *modifiers.glass;
  std::optional<theme::Glass> chosen = effect.glass;
  if (!chosen.has_value() && effect.slot.has_value()) {
    chosen = theme.glass().slot(*effect.slot);
  }
  if (chosen.has_value() &&
      chosen->variant() == theme::Glass::Variant::identity) {
    return std::nullopt;
  }
  return chosen;
}

auto Painter::glass_fill(const Scope& scope, const theme::Glass& glass,
                         bool lit) noexcept -> theme::Colour {
  const bool clear = glass.variant() == theme::Glass::Variant::clear;
  theme::Colour base =
      scope.colour(clear ? theme::ColourRole::system_background
                         : theme::ColourRole::secondary_system_background);
  if (const auto& tint = glass.tint_colour()) {
    const theme::Colour toward = scope.colour(*tint);
    base = tinted(base, toward.with_alpha(1.0F),
                  std::clamp(toward.alpha, 0.0F, 1.0F));
  }
  if (lit && glass.is_interactive() && !scope.reduce_highlighting) {
    base =
        tinted(base, scope.colour(theme::ColourRole::accent), glass_highlight);
  }
  if (scope.reduce_transparency) {
    return base.with_alpha(1.0F);
  }
  return base.with_alpha(clear ? clear_glass_alpha : regular_glass_alpha);
}

auto Painter::lens_of(const Scope& scope, const theme::Glass& glass,
                      Box area) const noexcept -> GlassLens {
  const float scale = target_.scale.value();
  std::optional<theme::Colour> tint;
  if (const auto& wanted = glass.tint_colour()) {
    tint = scope.colour(*wanted);
  }
  return GlassLens{GlassLens::Surface{.style = scope.theme.get().liquid_glass(),
                                      .glass = glass,
                                      .scheme = scope.resolved_scheme(),
                                      .tint = tint},
                   std::min(area.width(), area.height()) / scale, scale};
}

void Painter::flat_glass(const Scope& scope, const theme::Glass& glass,
                         Box area, Corners corners, bool lit) {
  shape(provider::ShapeKind::fill, area, corners, 0.0F, 0.0F,
        pack(glass_fill(scope, glass, lit), scope.opacity), scope.clip);
  shape(provider::ShapeKind::stroke, area, corners, 1.0F, 0.0F,
        pack(scope.colour(theme::ColourRole::separator), scope.opacity),
        scope.clip);
}

void Painter::see_through_glass(const Scope& scope, const theme::Glass& glass,
                                Box area, Corners corners, bool lit) {
  const bool clear = glass.variant() == theme::Glass::Variant::clear &&
                     !glass.tint_colour().has_value();
  const theme::Colour fill = glass_fill(scope, glass, lit).with_alpha(
      clear ? see_through_clear_alpha : see_through_regular_alpha);
  shape(provider::ShapeKind::fill, area, corners, 0.0F, 0.0F,
        pack(fill, scope.opacity), scope.clip);
  shape(provider::ShapeKind::stroke, area, corners, 1.0F, 0.0F,
        pack(scope.colour(theme::ColourRole::separator), scope.opacity),
        scope.clip);
}

void Painter::glass_shadow(const Scope& scope, Box area, Corners corners,
                           float strength) {
  if (strength <= 0.0F) {
    return;
  }
  const float scale = target_.scale.value();
  const float boost = scope.resolved_scheme() == theme::ColorScheme::dark
                          ? dark_shadow_boost
                          : 1.0F;
  const theme::ElevationStyle& style = scope.theme.get().liquid_glass().shadow;
  for (const theme::Shadow& layer : {style.ambient, style.key}) {
    if (layer.opacity <= 0.0F) {
      continue;
    }
    const float offset = layer.offset_y * scale;
    soft(Box{.left = area.left,
             .top = area.top + offset,
             .right = area.right,
             .bottom = area.bottom + offset},
         Corners{.radius = corners.radius, .smoothing = corners.smoothing},
         std::max(layer.blur * scale * 0.5F, 0.5F),
         pack(theme::Colour{0.0F, 0.0F, 0.0F,
                            std::min(layer.opacity * boost * strength, 1.0F)},
              scope.opacity),
         scope.clip, ShadowMotion::settled);
  }
}

auto Painter::painted_outline(const composition::Modifiers& modifiers,
                              bool button, bool glassed) -> theme::Shape {
  if (modifiers.corners.has_value()) {
    return *modifiers.corners;
  }
  if (modifiers.background.has_value()) {
    return modifiers.background->shape;
  }
  if (glassed) {
    return modifiers.glass->shape;
  }
  if (modifiers.material.has_value()) {
    return modifiers.material->shape;
  }
  return button ? theme::Shape::rounded(theme::radius::control)
                : theme::Shape::rectangle();
}

auto Painter::highlight_spot(composition::NodeId node, Box area) const noexcept
    -> std::array<float, 2> {
  if ((highlights_.pressed == node || highlights_.touched == node) &&
      highlights_.contact.has_value()) {
    const Box screen = bounds(area, transform_);
    const provider::PixelPoint& at = *highlights_.contact;
    if (at.x >= screen.left && at.x <= screen.right && at.y >= screen.top &&
        at.y <= screen.bottom) {
      return {at.x - ((screen.left + screen.right) * 0.5F),
              at.y - ((screen.top + screen.bottom) * 0.5F)};
    }
  }
  return {0.0F, -area.height() * 0.5F * highlight_rise};
}

auto Painter::backdrop_region(Box area, float reach) const noexcept
    -> provider::PassRegion {
  const Box screen = bounds(area, transform_);
  const auto width = static_cast<float>(target_.pixels.width.value());
  const auto height = static_cast<float>(target_.pixels.height.value());
  const float left = std::clamp(std::floor(screen.left - reach), 0.0F, width);
  const float top = std::clamp(std::floor(screen.top - reach), 0.0F, height);
  const float right = std::clamp(std::ceil(screen.right + reach), 0.0F, width);
  const float bottom =
      std::clamp(std::ceil(screen.bottom + reach), 0.0F, height);
  return provider::PassRegion{.x = left,
                              .y = top,
                              .width = std::max(right - left, 0.0F),
                              .height = std::max(bottom - top, 0.0F)};
}

auto Painter::region_effect(const provider::PassRegion& region) noexcept
    -> std::array<float, 4> {
  return {region.x, region.y, region.width, region.height};
}

void Painter::glaze(composition::NodeId key, const Scope& scope,
                    const theme::Glass& glass, Box area, Corners outline,
                    bool lit, Backing backing) {
  const Corners corners = plain_corners(outline, area);
  const GlassLens lens = lens_of(scope, glass, area);
  if (scope.reduce_transparency) {
    glass_shadow(scope, area, corners,
                 std::max(lens.shadow(), 0.5F) * backing.shadow);
    flat_glass(scope, glass, area, corners, lit);
    return;
  }
  if (see_through_) {
    see_through_glass(scope, glass, area, corners, lit);
    return;
  }
  const theme::LiquidGlassStyle& style = scope.theme.get().liquid_glass();
  const float scale = target_.scale.value();
  const std::optional<theme::MaterialStyle> frosting =
      backing.frost.has_value()
          ? std::optional{scope.theme.get().material(*backing.frost)}
          : std::nullopt;
  const float blur =
      frosting.has_value() ? frosting->blur * scale : lens.blur();
  const std::expected<Compositor::Surface, Compositor::BackdropRefusal>
      surface = backdrop_for(
          key.value(), backdrop_region(area, blur + lens.amplitude()),
          backdrop_region(area, 0.0F), blur,
          static_cast<std::uint32_t>(backing.behind.value_or(quads_.size())));
  glass_shadow(scope, area, corners, lens.shadow() * backing.shadow);
  if (!surface.has_value()) {
    flat_glass(scope, glass, area, corners, lit);
    return;
  }
  const std::array<float, 2> spot = highlight_spot(key, area);
  const float highlight =
      lit && glass.is_interactive() && !scope.reduce_highlighting
          ? style.highlight
          : 0.0F;
  const GlassLens shading =
      frosting.has_value()
          ? GlassLens::frosted(frosting->tint.resolve(scope.resolved_scheme()),
                               frosting->saturation)
          : lens;
  quads_.push_back(
      Quad{.kind = provider::ShapeKind::glass,
           .bounds = Box{.left = area.left - 1.0F,
                         .top = area.top - 1.0F,
                         .right = area.right + 1.0F,
                         .bottom = area.bottom + 1.0F},
           .uv = {scale, shading.response(), scale, shading.response()},
           .centre = {(area.left + area.right) * 0.5F,
                      (area.top + area.bottom) * 0.5F},
           .half_size = {area.width() * 0.5F, area.height() * 0.5F},
           .shape = {corners.radius, corners.smoothing, lens.amplitude(),
                     scope.opacity},
           .colour = pack(shading.veil(), 1.0F),
           .texture = surface->texture,
           .clip = scope.clip,
           .transform = transform_,
           .mesh = 0,
           .effect = region_effect(surface->region),
           .detail = {spot[0], spot[1], highlight, lens.rim()},
           .detail_texture = std::nullopt,
           .backdrop = true,
           .reach = blur + lens.amplitude()});
  ++backdrops_;
  if (scope.show_borders) {
    shape(provider::ShapeKind::stroke, area, corners, 1.0F, 0.0F,
          pack(scope.colour(theme::ColourRole::separator), scope.opacity),
          scope.clip);
  }
}

void Painter::frost(composition::NodeId key, const Scope& scope,
                    theme::Material material, Box area, Corners outline,
                    std::optional<std::size_t> behind) {
  const Corners corners = plain_corners(outline, area);
  const theme::MaterialStyle& style = scope.theme.get().material(material);
  const theme::ColorScheme scheme = scope.resolved_scheme();
  const theme::Colour tint = style.tint.resolve(scheme);
  const float scale = target_.scale.value();
  const auto solid = [&] {
    const theme::Colour opaque = composite(
        tint.with_alpha(1.0F), std::max(tint.alpha, solid_glass_alpha), clear_);
    shape(provider::ShapeKind::fill, area, corners, 0.0F, 0.0F,
          pack(opaque, scope.opacity), scope.clip);
  };
  if (scope.reduce_transparency) {
    solid();
    return;
  }
  if (see_through_) {
    shape(provider::ShapeKind::fill, area, corners, 0.0F, 0.0F,
          pack(tint.with_alpha(std::max(tint.alpha, see_through_regular_alpha)),
               scope.opacity),
          scope.clip);
    return;
  }
  const float blur = style.blur * scale;
  const std::expected<Compositor::Surface, Compositor::BackdropRefusal>
      surface = backdrop_for(
          key.value(), backdrop_region(area, blur), backdrop_region(area, 0.0F),
          blur, static_cast<std::uint32_t>(behind.value_or(quads_.size())));
  if (!surface.has_value()) {
    solid();
    return;
  }
  quads_.push_back(
      Quad{.kind = provider::ShapeKind::material,
           .bounds = Box{.left = area.left - 1.0F,
                         .top = area.top - 1.0F,
                         .right = area.right + 1.0F,
                         .bottom = area.bottom + 1.0F},
           .uv = {},
           .centre = {(area.left + area.right) * 0.5F,
                      (area.top + area.bottom) * 0.5F},
           .half_size = {area.width() * 0.5F, area.height() * 0.5F},
           .shape = {corners.radius, corners.smoothing, style.saturation,
                     style.luminosity.resolve(scheme)},
           .colour = pack(tint, 1.0F),
           .texture = surface->texture,
           .clip = scope.clip,
           .transform = transform_,
           .mesh = 0,
           .effect = region_effect(surface->region),
           .detail = {style.noise, scope.opacity, 0.0F, 0.0F},
           .detail_texture = std::nullopt,
           .backdrop = true,
           .reach = blur});
  ++backdrops_;
}

void Painter::glass_stroke(const composition::Node& node, const Scope& scope,
                           const theme::Glass& glass, Box frame,
                           const ShapePainter::Geometry& geometry) {
  if (scope.reduce_transparency) {
    push_geometry(geometry, scope.clip);
    return;
  }
  const float scale = target_.scale.value();
  const std::array<float, 4>& drawn = geometry.extent;
  const bool measured = drawn[2] > drawn[0] && drawn[3] > drawn[1];
  const GlassLens lens = lens_of(scope, glass,
                                 measured ? Box{.left = drawn[0],
                                                .top = drawn[1],
                                                .right = drawn[2],
                                                .bottom = drawn[3]}
                                          : frame);
  const float blur = lens.blur();
  const float refraction =
      std::min(scope.theme.get().liquid_glass().refraction * scale,
               lens.amplitude() * 2.0F);
  const std::expected<Compositor::Surface, Compositor::BackdropRefusal>
      surface = backdrop_for(node.id().value(),
                             backdrop_region(frame, blur + refraction),
                             backdrop_region(frame, 0.0F), blur,
                             static_cast<std::uint32_t>(quads_.size()));
  if (!surface.has_value()) {
    flat_stroke(scope, geometry);
    return;
  }
  if (geometry.indices.empty() || geometry.vertices.empty() ||
      scope.clip.width == 0 || scope.clip.height == 0) {
    return;
  }
  const std::uint32_t tint = pack(lens.veil(), 1.0F);
  const std::array<float, 4> effect = region_effect(surface->region);
  const std::array<float, 4> detail{refraction, lens.rim(), scope.opacity,
                                    scale};
  const std::uint64_t revision = DrawListCache::hash_value(
      DrawListCache::hash_value(geometry.revision, effect),
      std::array<std::uint32_t, 6>{
          tint, std::bit_cast<std::uint32_t>(lens.response()),
          std::bit_cast<std::uint32_t>(detail[0]),
          std::bit_cast<std::uint32_t>(detail[1]),
          std::bit_cast<std::uint32_t>(detail[2]),
          std::bit_cast<std::uint32_t>(detail[3])});
  ShapePainter::Geometry& slot = stage_mesh(geometry, revision);
  if (slot.restaged) {
    for (provider::ShapeVertex& vertex : slot.vertices) {
      vertex.kind = provider::ShapeKind::glass_path;
      vertex.colour = tint;
      vertex.shape[1] = lens.response();
      vertex.effect = effect;
      vertex.detail = detail;
    }
  }
  slot.texture = surface->texture;
  push_mesh(slot, scope.clip, provider::ShapeKind::glass_path, true);
  quads_.back().reach = blur + refraction;
  probe_glass(node);
  ++backdrops_;
}

void Painter::flat_stroke(const Scope& scope,
                          const ShapePainter::Geometry& geometry) {
  if (geometry.indices.empty() || geometry.vertices.empty() ||
      scope.clip.width == 0 || scope.clip.height == 0) {
    return;
  }
  const std::uint32_t colour = pack(
      theme::Colour{1.0F, 1.0F, 1.0F, fallback_stroke_alpha}, scope.opacity);
  ShapePainter::Geometry& slot = stage_mesh(
      geometry, DrawListCache::hash_value(geometry.revision,
                                          std::array<std::uint32_t, 2>{
                                              colour, fallback_stroke_mark}));
  if (slot.restaged) {
    for (provider::ShapeVertex& vertex : slot.vertices) {
      vertex.kind = provider::ShapeKind::path;
      vertex.colour = colour;
    }
  }
  slot.texture = std::nullopt;
  push_mesh(slot, scope.clip, provider::ShapeKind::path, false);
}

void Painter::probe_glass(const composition::Node& node) {
  const std::uint64_t key = node.id().value();
  if (!glass_probe_ || glass_probed_.contains(key) ||
      std::ranges::contains(glass_pending_, key)) {
    return;
  }
  glass_pending_.push_back(key);
}

void Painter::report_glass(std::uint64_t node,
                           std::span<const provider::ShapeVertex> submitted,
                           const provider::ShapeGeometry& geometry,
                           const provider::DrawCommand& command) {
  std::erase(glass_pending_, node);
  glass_probed_.insert(node);
  if (!diagnostics_.has_value() || submitted.empty()) {
    return;
  }
  float lowest = std::numeric_limits<float>::max();
  float highest = std::numeric_limits<float>::lowest();
  std::size_t zero = 0;
  std::size_t foreign = 0;
  for (const provider::ShapeVertex& vertex : submitted) {
    lowest = std::min(lowest, vertex.detail[2]);
    highest = std::max(highest, vertex.detail[2]);
    zero += vertex.detail[2] == 0.0F ? 1U : 0U;
    foreign += vertex.kind != provider::ShapeKind::glass_path ? 1U : 0U;
  }
  const provider::ShapeVertex& first = submitted.front();
  const std::uint32_t fetched_index =
      command.index_offset < geometry.indices.size()
          ? geometry.indices[command.index_offset] + command.vertex_offset
          : 0;
  const provider::ShapeVertex fetched = fetched_index < geometry.vertices.size()
                                            ? geometry.vertices[fetched_index]
                                            : provider::ShapeVertex{};
  diagnostics_->get().report(provider::Diagnostic{
      .severity = provider::Severity::info,
      .category = "glass",
      .message = std::format(
          "glass coverage node {}: {} vertices, kind {} ({} not glass_path), "
          "detail ({:.3f}, {:.3f}, {:.3f}, {:.3f}), detail.z min {:.3f} max "
          "{:.3f} zero {}, shape ({:.3f}, {:.3f}, {:.1f}, {:.1f}), half "
          "({:.3f}, {:.3f}), effect ({:.1f}, {:.1f}, {:.1f}, {:.1f}), colour "
          "{:#010x}; command texture {} detail {} trim ({:.1f}, {:.1f}) "
          "indices {}+{} clip {}x{}+{}+{}; vertex {} fetched by the first "
          "index: kind {} detail ({:.3f}, {:.3f}, {:.3f}, {:.3f}) position "
          "({:.1f}, {:.1f})",
          node, submitted.size(), static_cast<std::uint32_t>(first.kind),
          foreign, first.detail[0], first.detail[1], first.detail[2],
          first.detail[3], lowest, highest, zero, first.shape[0],
          first.shape[1], first.shape[2], first.shape[3], first.half_size[0],
          first.half_size[1], first.effect[0], first.effect[1], first.effect[2],
          first.effect[3], first.colour, command.texture.value(),
          command.detail.value(), command.trim[0], command.trim[1],
          command.index_offset, command.index_count, command.clip.width,
          command.clip.height, command.clip.x, command.clip.y, fetched_index,
          static_cast<std::uint32_t>(fetched.kind), fetched.detail[0],
          fetched.detail[1], fetched.detail[2], fetched.detail[3],
          fetched.position[0], fetched.position[1])});
}

auto Painter::member_corners(const Scope& scope,
                             const UnionMember& member) const -> Corners {
  Corners corners =
      rounding(scope.theme, member.shape, member.area, target_.scale.value());
  if (member.radius.has_value()) {
    corners.radius =
        std::clamp(*member.radius, 0.0F,
                   std::min(member.area.width(), member.area.height()) * 0.5F);
  }
  return corners;
}

void Painter::unite(composition::Tree& tree, const composition::Node& container,
                    const Scope& scope) {
  using Member = UnionMember;
  std::vector<Member>& members = union_members_;
  members.clear();
  const float scale = target_.scale.value();
  const auto gather = [&](this const auto& self,
                          const composition::Node& node) -> void {
    for (const composition::Node& child : node.children()) {
      if (child.content_as<components::GlassEffectContainer::Content>()
              .has_value()) {
        continue;
      }
      const auto& effect = child.modifiers().glass;
      const std::optional<theme::Glass> glass =
          glass_of(child.modifiers(), scope.theme);
      const auto shaped = child.content_as<components::ShapeView::Content>();
      if (effect.has_value() && glass.has_value() &&
          !(shaped.has_value() && shaped->get().follows_glass())) {
        const composition::Rect& laid_at = child.frame();
        const motion::Presentation& shown = tree.presentation(child);
        const motion::AnimatableVector laid_vector{
            laid_at.x, laid_at.y, laid_at.width, laid_at.height};
        const bool moving =
            shown.in_flight(motion::Presentation::Property::frame) ||
            shown.morphing() || shown.morph_pending();
        const motion::AnimatableVector at =
            moving ? shown
                         .peek(motion::Presentation::Property::frame,
                               target_.time, laid_vector)
                         .value_or(laid_vector)
                   : laid_vector;
        const Box area =
            snap(Box{.left = static_cast<float>(at[0]) * scale,
                     .top = static_cast<float>(at[1]) * scale,
                     .right = static_cast<float>(at[0] + at[2]) * scale,
                     .bottom = static_cast<float>(at[1] + at[3]) * scale});
        if (shown.in_flight(motion::Presentation::Property::frame) ||
            shown.morphing() ||
            shown.in_flight(motion::Presentation::Property::corner_radius)) {
          unsettle();
        }
        std::optional<float> radius;
        if (shown.in_flight(motion::Presentation::Property::corner_radius)) {
          if (const auto corner =
                  shown.peek(motion::Presentation::Property::corner_radius,
                             target_.time, motion::AnimatableVector{})) {
            radius = static_cast<float>((*corner)[0]) * scale;
          }
        }
        const auto found =
            effect->unioned.has_value()
                ? std::ranges::find_if(members,
                                       [&](const Member& member) {
                                         return member.tag == effect->unioned;
                                       })
                : members.end();
        if (found == members.end()) {
          members.push_back(Member{.tag = effect->unioned,
                                   .id = child.id(),
                                   .glass = *glass,
                                   .shape = effect->shape,
                                   .area = area,
                                   .radius = radius});
        } else {
          found->area.left = std::min(found->area.left, area.left);
          found->area.top = std::min(found->area.top, area.top);
          found->area.right = std::max(found->area.right, area.right);
          found->area.bottom = std::max(found->area.bottom, area.bottom);
        }
        united_.insert(child.id());
      }
      self(child);
    }
  };
  gather(container);
  if (members.empty()) {
    return;
  }
  const auto content =
      container.content_as<components::GlassEffectContainer::Content>();
  if (scope.reduce_transparency || members.size() == 1 ||
      !content.has_value()) {
    for (const Member& member : members) {
      glaze(member.id, scope, member.glass, member.area,
            member_corners(scope, member), lit(member), Backing{});
    }
    return;
  }
  const float spacing =
      std::max(scope.theme.get().space(content->get().spacing()) * scale, 1.0F);
  Box covered = bounds(members.front().area, transform_);
  for (const Member& member : members) {
    const Box screen = bounds(member.area, transform_);
    covered.left = std::min(covered.left, screen.left);
    covered.top = std::min(covered.top, screen.top);
    covered.right = std::max(covered.right, screen.right);
    covered.bottom = std::max(covered.bottom, screen.bottom);
  }
  float smallest = std::numeric_limits<float>::max();
  for (const Member& member : members) {
    smallest =
        std::min(smallest, std::min(member.area.width(), member.area.height()));
  }
  const GlassLens lens = lens_of(
      scope, members.front().glass,
      Box{.left = 0.0F, .top = 0.0F, .right = smallest, .bottom = smallest});
  const float blur = lens.blur();
  const std::uint32_t outer = std::exchange(transform_, 0);
  const provider::PassRegion region =
      backdrop_region(covered, blur + (lens.amplitude() * 2.0F) + spacing);
  const provider::PassRegion core = backdrop_region(covered, spacing);
  transform_ = outer;
  const std::expected<Compositor::Surface, Compositor::BackdropRefusal>
      captured = backdrop_for(container.id().value(), region, core, blur,
                              static_cast<std::uint32_t>(quads_.size()));
  const std::optional<Compositor::Surface> backdrop =
      captured.has_value() ? std::optional{*captured} : std::nullopt;
  const std::span<const Member> all{members};
  for (std::size_t first = 0; first < all.size();
       first += Compositor::field_members) {
    const std::uint64_t key =
        first == 0 ? container.id().value()
                   : Compositor::mix(container.id().value(),
                                     std::as_bytes(std::span{&first, 1}));
    fuse(Fusion{.key = key,
                .owner = container.id().value(),
                .spacing = spacing,
                .backdrop = backdrop},
         all.subspan(first,
                     std::min(Compositor::field_members, all.size() - first)),
         scope);
  }
}

auto Painter::held(composition::NodeId node) const noexcept -> bool {
  return highlights_.focused == node || highlights_.pressed == node ||
         highlights_.touched == node;
}

auto Painter::lit(const UnionMember& member) const noexcept -> bool {
  return member.glass.is_interactive() && held(member.id);
}

void Painter::fuse(const Fusion& fusion, std::span<const UnionMember> members,
                   const Scope& scope) {
  const float scale = target_.scale.value();
  const auto individually = [&] {
    for (const UnionMember& member : members) {
      glaze(member.id, scope, member.glass, member.area,
            member_corners(scope, member), lit(member), Backing{});
    }
  };
  if (members.size() == 1 || !fusion.backdrop.has_value()) {
    individually();
    return;
  }
  const float blend = fusion.spacing;
  std::vector<Compositor::FieldShape>& shapes = field_shapes_;
  shapes.clear();
  Box covered = bounds(members.front().area, transform_);
  float reach = std::numeric_limits<float>::max();
  for (const UnionMember& member : members) {
    const Box screen = bounds(member.area, transform_);
    covered.left = std::min(covered.left, screen.left);
    covered.top = std::min(covered.top, screen.top);
    covered.right = std::max(covered.right, screen.right);
    covered.bottom = std::max(covered.bottom, screen.bottom);
    const Corners corners = member_corners(scope, member);
    shapes.push_back(Compositor::FieldShape{
        .centre = {(screen.left + screen.right) * 0.5F,
                   (screen.top + screen.bottom) * 0.5F},
        .half_size = {screen.width() * 0.5F, screen.height() * 0.5F},
        .radius = corners.radius,
        .smoothing = corners.smoothing});
    reach = std::min(reach, std::min(screen.width(), screen.height()) * 0.8F);
  }
  const theme::LiquidGlassStyle& style = scope.theme.get().liquid_glass();
  const theme::Glass& lead = members.front().glass;
  const GlassLens lens = lens_of(scope, lead,
                                 Box{.left = 0.0F,
                                     .top = 0.0F,
                                     .right = reach / 0.8F,
                                     .bottom = reach / 0.8F});
  const std::optional<provider::TextureId> field =
      compositor_.field(fusion.key, fusion.owner, shapes, blend);
  if (!field.has_value()) {
    individually();
    return;
  }
  place();
  field_calls_.push_back(FieldCall{.key = fusion.key, .texture = *field});
  const Compositor::Surface& backdrop = *fusion.backdrop;
  for (const UnionMember& member : members) {
    glass_shadow(scope, member.area, member_corners(scope, member),
                 lens.shadow());
  }
  const float margin = (blend * 0.25F) + union_margin;
  const Box drawn{.left = covered.left - margin,
                  .top = covered.top - margin,
                  .right = covered.right + margin,
                  .bottom = covered.bottom + margin};
  std::array<float, 2> spot{0.0F, 0.0F};
  float highlight = 0.0F;
  for (const UnionMember& member : members) {
    if (lit(member)) {
      const Box screen = bounds(member.area, transform_);
      const std::array<float, 2> local = highlight_spot(member.id, member.area);
      spot = {((screen.left + screen.right) * 0.5F) + local[0] -
                  ((drawn.left + drawn.right) * 0.5F),
              ((screen.top + screen.bottom) * 0.5F) + local[1] -
                  ((drawn.top + drawn.bottom) * 0.5F)};
      highlight = style.highlight;
      break;
    }
  }
  quads_.push_back(
      Quad{.kind = provider::ShapeKind::glass_union,
           .bounds = drawn,
           .uv = {scale, lens.response(), scale, lens.response()},
           .centre = {(drawn.left + drawn.right) * 0.5F,
                      (drawn.top + drawn.bottom) * 0.5F},
           .half_size = {drawn.width() * 0.5F, drawn.height() * 0.5F},
           .shape = {blend, reach, lens.amplitude(), scope.opacity},
           .colour = pack(lens.veil(), 1.0F),
           .texture = backdrop.texture,
           .clip = scope.clip,
           .transform = 0,
           .mesh = 0,
           .effect = region_effect(backdrop.region),
           .detail = {spot[0], spot[1], highlight, lens.rim()},
           .detail_texture = *field,
           .backdrop = true});
  ++backdrops_;
}

auto Painter::glassy(const composition::Node& node) noexcept -> bool {
  const composition::Modifiers& modifiers = node.modifiers();
  if (modifiers.glass.has_value() || modifiers.material.has_value() ||
      node.content_as<components::GlassEffectContainer::Content>()
          .has_value()) {
    return true;
  }
  return std::ranges::any_of(
      node.children(),
      [](const composition::Node& child) { return glassy(child); });
}

auto Painter::promotable(
    const composition::Node& node, const motion::Presentation& presentation,
    const std::optional<motion::Transition::Effect>& effect,
    const Scope& parent, bool carries) const -> bool {
  using Property = motion::Presentation::Property;
  if (node.children().empty() || carries || node.modifiers().clip) {
    return false;
  }
  const bool moving = presentation.in_flight(Property::opacity) ||
                      presentation.in_flight(Property::scale) ||
                      presentation.in_flight(Property::offset) ||
                      presentation.in_flight(Property::rotation) ||
                      effect.has_value();
  if (!moving || presentation.in_flight(Property::frame) ||
      !transforms_[parent.transform].translating() ||
      !compositor_.layerable(node.id().value())) {
    return false;
  }
  return !glassy(node);
}

void Painter::fade_range(std::size_t first, float factor) {
  for (std::size_t index = first; index < quads_.size(); ++index) {
    Quad& quad = quads_[index];
    quad.colour = faded(quad.colour, factor);
    if (quad.mesh != 0) {
      ShapePainter::Geometry& mesh = meshes_[quad.mesh - 1];
      for (provider::ShapeVertex& vertex : mesh.vertices) {
        vertex.colour = faded(vertex.colour, factor);
      }
      mesh.revision = DrawListCache::hash_value(mesh.revision, factor);
    }
  }
}

auto Painter::inverse(const Affine& matrix) const noexcept -> Affine {
  const float determinant = (matrix.a * matrix.d) - (matrix.b * matrix.c);
  if (std::abs(determinant) < 1.0e-12F) {
    return Affine{};
  }
  const float a = matrix.d / determinant;
  const float b = -matrix.b / determinant;
  const float c = -matrix.c / determinant;
  const float d = matrix.a / determinant;
  return Affine{.a = a,
                .b = b,
                .c = c,
                .d = d,
                .tx = -((a * matrix.tx) + (c * matrix.ty)),
                .ty = -((b * matrix.tx) + (d * matrix.ty))};
}

auto Painter::anchor_of(std::size_t first) const -> std::array<float, 2> {
  const Affine& matrix = transforms_[quads_[first].transform];
  return {matrix.tx, matrix.ty};
}

auto Painter::layer_hash(std::size_t first, std::size_t last,
                         std::array<float, 2> anchor,
                         const provider::ClipRect& parent, bool anchored) const
    -> std::uint64_t {
  std::uint64_t hash = Compositor::fnv_offset;
  const auto mixed = [&hash](const auto& value) {
    hash = Compositor::mix(hash, std::as_bytes(std::span{&value, 1}));
  };
  const auto quantised = [&mixed](float value) {
    mixed(std::lround(value * transform_quantum));
  };
  const auto quads = std::span{quads_}.subspan(first, last - first);
  if (anchored || target_.zoom != 1.0F ||
      std::ranges::any_of(quads, [](const Quad& quad) {
        return samples_backdrop(quad.kind) ||
               quad.kind == provider::ShapeKind::blur_down ||
               quad.kind == provider::ShapeKind::blur_up;
      })) {
    mixed(anchor);
  }
  for (const Quad& quad : quads) {
    mixed(quad.kind);
    mixed(quad.bounds.left);
    mixed(quad.bounds.top);
    mixed(quad.bounds.right);
    mixed(quad.bounds.bottom);
    mixed(quad.uv);
    mixed(quad.centre);
    mixed(quad.half_size);
    mixed(quad.shape);
    mixed(quad.colour);
    mixed(quad.texture.value_or(provider::TextureId{0}).value());
    if (quad.texture.has_value()) {
      mixed(shapes_.ramp_revision(*quad.texture));
    }
    if (quad.external && quad.texture.has_value()) {
      mixed(publication(*quad.texture)
                .value_or(provider::TexturePublications::Publication{})
                .generation);
    }
    if (quad.backdrop) {
      mixed(compositor_.generation_of(
          quad.texture.value_or(provider::TextureId{0})));
      mixed(compositor_.generation_of(
          quad.detail_texture.value_or(provider::TextureId{0})));
    }
    if (const std::optional<Box> shown =
            visible(quad, released(quad.clip, parent))) {
      quantised(shown->left - anchor[0]);
      quantised(shown->top - anchor[1]);
      quantised(shown->right - anchor[0]);
      quantised(shown->bottom - anchor[1]);
    }
    mixed(quad.effect);
    mixed(quad.detail);
    mixed(fades_[quad.fade].value);
    const Affine& matrix = transforms_[quad.transform];
    for (const float part : {matrix.a, matrix.b, matrix.c, matrix.d}) {
      quantised(part);
    }
    quantised(matrix.tx - anchor[0]);
    quantised(matrix.ty - anchor[1]);
    if (quad.mesh != 0) {
      const ShapePainter::Geometry& mesh = meshes_[quad.mesh - 1];
      mixed(mesh.revision);
      mixed(mesh.vertices.size());
      mixed(mesh.trim);
    }
  }
  return hash;
}

void Painter::promote(const composition::Node& node, const Scope& scope,
                      const Scope& parent, const Affine& local, float factor,
                      std::size_t first, std::size_t backdrops_before) {
  const std::size_t last = quads_.size();
  if (last == first) {
    return;
  }
  unsettle();
  forget_since(first);
  const std::uint64_t key = node.id().value();
  if (compositor_.pending_backdrops() != backdrops_before) {
    compositor_.spoil_layer(key);
    fade_range(first, factor);
    return;
  }
  const Affine outer = transforms_[parent.transform];
  std::vector<std::uint32_t>& original = promoted_transforms_;
  original.clear();
  for (std::size_t index = first; index < last; ++index) {
    original.push_back(quads_[index].transform);
  }
  if (!local.identity()) {
    const Affine shift = outer.then(inverse(local)).then(inverse(outer));
    std::vector<std::pair<std::uint32_t, std::uint32_t>>& remapped =
        remapped_transforms_;
    remapped.clear();
    for (std::size_t index = first; index < last; ++index) {
      Quad& quad = quads_[index];
      const auto found =
          std::ranges::find(remapped, quad.transform,
                            &std::pair<std::uint32_t, std::uint32_t>::first);
      if (found != remapped.end()) {
        quad.transform = found->second;
        continue;
      }
      transforms_.push_back(shift.then(transforms_[quad.transform]));
      adoptions_.push_back(Adoption{.parent = quad.transform});
      const auto made = static_cast<std::uint32_t>(transforms_.size() - 1);
      remapped.emplace_back(quad.transform, made);
      quad.transform = made;
    }
  }
  const auto restore = [&] {
    for (std::size_t index = first; index < last; ++index) {
      quads_[index].transform = original[index - first];
    }
    fade_range(first, factor);
  };
  const std::optional<LayerArea> area =
      layer_area(first, last, parent.clip, 1.0F);
  if (!area.has_value()) {
    restore();
    return;
  }
  const float left = area->region.left;
  const float top = area->region.top;
  const float right = area->region.right;
  const float bottom = area->region.bottom;
  const std::array<float, 2> anchor = anchor_of(first);
  const std::optional<Compositor::LayerPlan> plan = compositor_.layer(
      key,
      Compositor::LayerRequest{
          .wanted = provider::PassRegion{.x = left,
                                         .y = top,
                                         .width = right - left,
                                         .height = bottom - top},
          .hash = layer_hash(first, last, anchor, parent.clip, area->clamped),
          .anchor = anchor});
  if (!plan.has_value()) {
    restore();
    return;
  }
  if (plan->inexact) {
    animating_ = true;
  }
  note_used(std::span{quads_}.subspan(first));
  if (plan->render) {
    LayerPass& pass = add_layer_pass(*plan, key, key, "promoted layer pass");
    pass.quads.assign(std::make_move_iterator(
                          quads_.begin() + static_cast<std::ptrdiff_t>(first)),
                      std::make_move_iterator(quads_.end()));
    rebase(pass, parent.clip);
  }
  quads_.resize(first);
  const provider::PassRegion& region = plan->surface.region;
  const Box shown{.left = left - outer.tx,
                  .top = top - outer.ty,
                  .right = right - outer.tx,
                  .bottom = bottom - outer.ty};
  quads_.push_back(Quad{
      .kind = provider::ShapeKind::layer,
      .bounds = shown,
      .uv = {(left - region.x) / region.width, (top - region.y) / region.height,
             (right - region.x) / region.width,
             (bottom - region.y) / region.height},
      .centre = {(shown.left + shown.right) * 0.5F,
                 (shown.top + shown.bottom) * 0.5F},
      .half_size = {shown.width() * 0.5F, shown.height() * 0.5F},
      .shape = {},
      .colour = pack(theme::Colour{1.0F, 1.0F, 1.0F, 1.0F}, factor),
      .texture = plan->surface.texture,
      .clip = parent.clip,
      .transform = scope.transform,
      .mesh = 0,
      .effect = {},
      .detail = {},
      .detail_texture = std::nullopt,
      .backdrop = true,
      .owner = node.id().value(),
      .fresh = plan->render});
}

auto Painter::clip_box(const provider::ClipRect& clip) noexcept -> Box {
  return Box{
      .left = static_cast<float>(clip.x),
      .top = static_cast<float>(clip.y),
      .right = static_cast<float>(clip.x) + static_cast<float>(clip.width),
      .bottom = static_cast<float>(clip.y) + static_cast<float>(clip.height)};
}

auto Painter::released(const provider::ClipRect& clip,
                       const provider::ClipRect& parent) noexcept -> Box {
  Box box = clip_box(clip);
  const Box outer = clip_box(parent);
  if (box.left < outer.left || box.top < outer.top || box.right > outer.right ||
      box.bottom > outer.bottom) {
    return box;
  }
  constexpr float far = std::numeric_limits<float>::infinity();
  if (box.left == outer.left) {
    box.left = -far;
  }
  if (box.top == outer.top) {
    box.top = -far;
  }
  if (box.right == outer.right) {
    box.right = far;
  }
  if (box.bottom == outer.bottom) {
    box.bottom = far;
  }
  return box;
}

auto Painter::visible(const Quad& quad, Box clip) const -> std::optional<Box> {
  Box box = quad.bounds;
  if (quad.mesh != 0) {
    const ShapePainter::Geometry& mesh = meshes_[quad.mesh - 1];
    if (mesh.vertices.empty()) {
      return std::nullopt;
    }
    box = Box{.left = mesh.extent[0],
              .top = mesh.extent[1],
              .right = mesh.extent[2],
              .bottom = mesh.extent[3]};
  }
  Box placed = bounds(box, quad.transform);
  placed.left = std::max(placed.left, clip.left);
  placed.top = std::max(placed.top, clip.top);
  placed.right = std::min(placed.right, clip.right);
  placed.bottom = std::min(placed.bottom, clip.bottom);
  if (placed.right <= placed.left || placed.bottom <= placed.top) {
    return std::nullopt;
  }
  return placed;
}

auto Painter::covering(std::size_t first, std::size_t last,
                       std::optional<provider::ClipRect> parent) const
    -> std::optional<Box> {
  std::optional<Box> covered;
  for (const Quad& quad : std::span{quads_}.subspan(first, last - first)) {
    const std::optional<Box> placed =
        visible(quad, parent.has_value() ? released(quad.clip, *parent)
                                         : clip_box(quad.clip));
    if (!placed.has_value()) {
      continue;
    }
    if (!covered.has_value()) {
      covered = placed;
    } else {
      covered->left = std::min(covered->left, placed->left);
      covered->top = std::min(covered->top, placed->top);
      covered->right = std::max(covered->right, placed->right);
      covered->bottom = std::max(covered->bottom, placed->bottom);
    }
  }
  return covered;
}

auto Painter::layer_area(std::size_t first, std::size_t last,
                         const provider::ClipRect& parent, float pad) const
    -> std::optional<LayerArea> {
  if (!covering(first, last, std::nullopt).has_value()) {
    return std::nullopt;
  }
  const std::optional<Box> covered = covering(first, last, parent);
  if (!covered.has_value()) {
    return std::nullopt;
  }
  const auto width = static_cast<float>(target_.pixels.width.value());
  const auto height = static_cast<float>(target_.pixels.height.value());
  const Box whole{.left = std::floor(covered->left - pad),
                  .top = std::floor(covered->top - pad),
                  .right = std::ceil(covered->right + pad),
                  .bottom = std::ceil(covered->bottom + pad)};
  if (whole.width() <= width && whole.height() <= height) {
    return LayerArea{.region = whole, .clamped = false};
  }
  const Box clamped{.left = std::clamp(whole.left, 0.0F, width),
                    .top = std::clamp(whole.top, 0.0F, height),
                    .right = std::clamp(whole.right, 0.0F, width),
                    .bottom = std::clamp(whole.bottom, 0.0F, height)};
  if (clamped.width() < 1.0F || clamped.height() < 1.0F) {
    return std::nullopt;
  }
  return LayerArea{.region = clamped, .clamped = true};
}

void Painter::rebase(LayerPass& pass, const provider::ClipRect& parent) const {
  const float width = pass.region.width;
  const float height = pass.region.height;
  for (Quad& quad : pass.quads) {
    const Box clip = released(quad.clip, parent);
    const float left = std::clamp(clip.left - pass.origin[0], 0.0F, width);
    const float top = std::clamp(clip.top - pass.origin[1], 0.0F, height);
    const float right = std::clamp(clip.right - pass.origin[0], 0.0F, width);
    const float bottom = std::clamp(clip.bottom - pass.origin[1], 0.0F, height);
    quad.clip = provider::ClipRect{
        .x = static_cast<std::uint32_t>(left),
        .y = static_cast<std::uint32_t>(top),
        .width = static_cast<std::uint32_t>(std::max(right - left, 0.0F)),
        .height = static_cast<std::uint32_t>(std::max(bottom - top, 0.0F))};
  }
}

auto Painter::recoloured(const motion::Transition::Effect& effect) noexcept
    -> graphics::ColourMatrix {
  return graphics::ColourMatrix::brightness(effect.brightness)
      .then(graphics::ColourMatrix::contrast(effect.contrast))
      .then(graphics::ColourMatrix::saturation(effect.saturation))
      .then(graphics::ColourMatrix::grayscale(effect.grayscale))
      .then(graphics::ColourMatrix::hue_rotation(
          motion::Angle::radians(effect.hue_rotation)))
      .then(graphics::ColourMatrix::multiplied(effect.colour_multiply));
}

auto Painter::smart_invert() noexcept -> graphics::ColourMatrix {
  return graphics::ColourMatrix::inverted().then(
      graphics::ColourMatrix::hue_rotation(motion::Angle::degrees(180.0)));
}

auto Painter::effect_plan(
    const composition::Node& node, const Scope& scope,
    const composition::Rect& laid, const std::optional<Shading>& shaded,
    const std::optional<motion::Transition::Effect>& appearing,
    motion::Presentation& presentation,
    const motion::Presentation::Moment& moment) const -> EffectPlan {
  using Mix = motion::Presentation::Mix;
  const composition::GraphicEffects& declared = node.modifiers().graphics;
  EffectPlan plan;
  const auto decoration = node.content_as<composition::Decoration>();
  plan.masks = decoration.has_value() && decoration->get().masks() &&
               node.child_count() == 2;
  plan.inverse_mask = plan.masks && decoration->get().inverts();
  const bool varies =
      shaded.has_value() &&
      (!shaded->colour.is_identity() || shaded->blur > 0.0F ||
       !shaded->projection.is_identity() || shaded->shader.has_value() ||
       shaded->blend != graphics::BlendMode::normal);
  const float appearing_blur = appearing.has_value() ? appearing->blur : 0.0F;
  const bool appearing_colour = appearing.has_value() && appearing->recolours();
  const bool appearing_tilt = appearing.has_value() && appearing->tilt != 0.0F;
  const bool tracked = presentation.mixing(Mix::blur) ||
                       presentation.mixing(Mix::colour) ||
                       presentation.mixing(Mix::projection);
  if (declared.empty() && !varies && !declared.blurs &&
      appearing_blur <= 0.0F && !appearing_colour && !appearing_tilt &&
      !tracked && !scope.inverts_here) {
    return plan;
  }
  for (const composition::GraphicEffects::ColourAdjustment& adjustment :
       declared.colour) {
    const graphics::ColourMatrix step =
        std::holds_alternative<graphics::ColourMatrix>(adjustment)
            ? std::get<graphics::ColourMatrix>(adjustment)
            : graphics::ColourMatrix::multiplied(
                  scope.colour(std::get<theme::ColourValue>(adjustment)));
    plan.matrix = plan.matrix.then(step);
  }
  if (scope.inverts_here) {
    plan.matrix = plan.matrix.then(smart_invert());
  }
  plan.premultiplied = declared.premultiplied_colour;
  plan.linear = declared.linear_colour;
  if (!declared.colour.empty() || presentation.mixing(Mix::colour)) {
    const std::span<const float> mixed =
        presentation.mix(Mix::colour, plan.matrix.values(), moment);
    std::array<float, graphics::ColourMatrix::count> values{};
    std::ranges::copy(mixed, values.begin());
    plan.matrix = graphics::ColourMatrix::of(values);
  }
  plan.blur = declared.blur_radius;
  if (declared.blurs || presentation.mixing(Mix::blur)) {
    const std::array<float, 1> radius{plan.blur};
    plan.blur = presentation.mix(Mix::blur, radius, moment)[0];
  }
  plan.opaque = declared.blur_opaque;
  plan.blend = declared.blend;
  plan.group = declared.group;
  plan.extends = declared.extends;
  plan.shader = declared.shader;
  plan.sample_offset = declared.sample_offset;
  const composition::Size size{.width = laid.width, .height = laid.height};
  graphics::ProjectionTransform total =
      projected(declared.projections, size, presentation, moment);
  if (varies) {
    plan.matrix = plan.matrix.then(shaded->colour);
    plan.blur =
        std::sqrt((plan.blur * plan.blur) + (shaded->blur * shaded->blur));
    total = total.concatenating(shaded->projection);
    plan.opaque = plan.opaque || shaded->opaque;
    if (shaded->blend != graphics::BlendMode::normal) {
      plan.blend = shaded->blend;
    }
    if (!plan.shader.has_value() && shaded->shader.has_value()) {
      plan.shader = shaded->shader;
      plan.sample_offset = shaded->sample_offset;
    }
  }
  if (appearing_blur > 0.0F) {
    plan.blur =
        std::sqrt((plan.blur * plan.blur) + (appearing_blur * appearing_blur));
  }
  if (appearing_colour) {
    plan.matrix = plan.matrix.then(recoloured(*appearing));
  }
  if (appearing_tilt) {
    total = total.concatenating(graphics::Rotation3D{
        motion::Angle::radians(appearing->tilt),
        graphics::Rotation3D::Axis{.x = appearing->tilt_x,
                                   .y = appearing->tilt_y,
                                   .z = appearing->tilt_z},
        motion::UnitPoint::center, 0.0F, appearing->perspective}
                                    .projection(size));
  }
  if (!total.is_identity()) {
    if (total.is_affine()) {
      plan.affine = total.affine();
    } else {
      plan.projection = total;
    }
  }
  return plan;
}

auto Painter::projected(
    const std::vector<composition::GraphicEffects::Projection>& projections,
    composition::Size size, motion::Presentation& presentation,
    const motion::Presentation::Moment& moment)
    -> graphics::ProjectionTransform {
  using Mix = motion::Presentation::Mix;
  if (projections.empty() && !presentation.mixing(Mix::projection)) {
    return graphics::ProjectionTransform::identity();
  }
  constexpr std::size_t entries = 9;
  std::array<float, motion::Presentation::mix_capacity> parameters{};
  std::size_t used = 0;
  std::uint64_t shape = projections.size();
  for (const composition::GraphicEffects::Projection& projection :
       projections) {
    if (const auto* turn = std::get_if<graphics::Rotation3D>(&projection)) {
      if (used + 1 > parameters.size()) {
        break;
      }
      parameters[used++] = static_cast<float>(turn->angle().radians());
      const std::array<float, 7> traits{turn->axis().x,     turn->axis().y,
                                        turn->axis().z,     turn->anchor().x,
                                        turn->anchor().y,   turn->anchor_z(),
                                        turn->perspective()};
      shape = Compositor::mix(shape, std::as_bytes(std::span{traits}));
      continue;
    }
    const auto& matrix = std::get<graphics::ProjectionTransform>(projection);
    if (used + entries > parameters.size()) {
      break;
    }
    const std::array<float, entries> rows{
        matrix.m11(), matrix.m12(), matrix.m13(), matrix.m21(), matrix.m22(),
        matrix.m23(), matrix.m31(), matrix.m32(), matrix.m33()};
    std::ranges::copy(rows,
                      parameters.begin() + static_cast<std::ptrdiff_t>(used));
    used += entries;
    const std::uint64_t marker = entries;
    shape = Compositor::mix(shape, std::as_bytes(std::span{&marker, 1}));
  }
  const std::span<const float> mixed = presentation.mix(
      Mix::projection, std::span<const float>{parameters}.first(used), moment,
      shape);
  graphics::ProjectionTransform total;
  std::size_t read = 0;
  for (const composition::GraphicEffects::Projection& projection :
       projections) {
    if (const auto* turn = std::get_if<graphics::Rotation3D>(&projection)) {
      if (read + 1 > mixed.size()) {
        break;
      }
      const graphics::Rotation3D shown{motion::Angle::radians(mixed[read++]),
                                       turn->axis(), turn->anchor(),
                                       turn->anchor_z(), turn->perspective()};
      total = total.concatenating(shown.projection(size));
      continue;
    }
    if (read + entries > mixed.size()) {
      break;
    }
    std::array<float, entries> rows{};
    std::ranges::copy(mixed.subspan(read, entries), rows.begin());
    read += entries;
    total = total.concatenating(graphics::ProjectionTransform{rows});
  }
  return total;
}

auto Painter::effect_layer(std::uint64_t key, std::uint32_t stage, Box region,
                           std::size_t first, std::size_t last,
                           const LayerContent& content)
    -> std::optional<EffectSource> {
  const std::uint64_t salted =
      stage == 0 ? effect_key(key)
                 : Compositor::mix(key, std::as_bytes(std::span{&stage, 1}));
  const std::optional<Compositor::LayerPlan> plan = compositor_.effect_layer(
      salted, Compositor::LayerRequest{
                  .wanted = {.x = region.left,
                             .y = region.top,
                             .width = region.width(),
                             .height = region.height()},
                  .hash = content.hash,
                  .anchor = content.anchor,
                  .matching = stage == 0 ? std::nullopt
                                         : std::optional{effect_key(key)}});
  if (!plan.has_value()) {
    return std::nullopt;
  }
  note_used(std::span{quads_}.subspan(first, last - first));
  if (plan->render) {
    LayerPass& pass =
        add_layer_pass(*plan, key, effect_key(key),
                       stage == 0 ? "effect layer pass" : "mask layer pass");
    pass.quads.assign(std::make_move_iterator(
                          quads_.begin() + static_cast<std::ptrdiff_t>(first)),
                      std::make_move_iterator(
                          quads_.begin() + static_cast<std::ptrdiff_t>(last)));
    rebase(pass, content.clip);
  }
  if (plan->inexact) {
    animating_ = true;
  }
  const provider::PassRegion& surface = plan->surface.region;
  return EffectSource{
      .texture = plan->surface.texture,
      .width = static_cast<std::uint32_t>(std::ceil(plan->extent[0])),
      .height = static_cast<std::uint32_t>(std::ceil(plan->extent[1])),
      .extent = {plan->extent[0] / surface.width,
                 plan->extent[1] / surface.height},
      .origin = {surface.x, surface.y},
      .hash = Compositor::mix(content.hash,
                              std::as_bytes(std::span{&plan->generation, 1})),
      .rendered = plan->render};
}

auto Painter::effect_stage(std::uint64_t key, std::uint32_t stage,
                           const EffectSource& source, std::uint32_t width,
                           std::uint32_t height, std::uint64_t salt, Quad quad)
    -> std::optional<EffectSource> {
  const auto wide = static_cast<float>(width);
  const auto tall = static_cast<float>(height);
  const std::uint64_t salted =
      Compositor::mix(key, std::as_bytes(std::span{&stage, 1}));
  const std::uint64_t hash =
      Compositor::mix(source.hash, std::as_bytes(std::span{&salt, 1}));
  const std::optional<Compositor::LayerPlan> plan = compositor_.effect_layer(
      salted,
      Compositor::LayerRequest{
          .wanted = {.x = 0.0F, .y = 0.0F, .width = wide, .height = tall},
          .hash = hash,
          .anchor = {}});
  if (!plan.has_value()) {
    return std::nullopt;
  }
  if (plan->render) {
    LayerPass& pass =
        add_layer_pass(*plan, key, effect_key(key), effect_name(quad.kind));
    quad.bounds = Box{.left = 0.0F, .top = 0.0F, .right = wide, .bottom = tall};
    quad.uv = {0.0F, 0.0F, source.extent[0], source.extent[1]};
    quad.centre = {wide * 0.5F, tall * 0.5F};
    quad.half_size = {wide * 0.5F, tall * 0.5F};
    quad.texture = source.texture;
    quad.clip =
        provider::ClipRect{.x = 0, .y = 0, .width = width, .height = height};
    quad.transform = 0;
    quad.fade = 0;
    quad.colour = pack(theme::Colour{1.0F, 1.0F, 1.0F, 1.0F}, 1.0F);
    pass.quads.clear();
    pass.quads.push_back(quad);
  }
  const provider::PassRegion& surface = plan->surface.region;
  return EffectSource{.texture = plan->surface.texture,
                      .width = width,
                      .height = height,
                      .extent = {wide / surface.width, tall / surface.height},
                      .origin = source.origin,
                      .hash = Compositor::mix(
                          hash, std::as_bytes(std::span{&plan->generation, 1})),
                      .rendered = plan->render};
}

auto Painter::shaped_clip(const composition::Modifiers& modifiers,
                          const Scope& scope, Box area) const
    -> std::optional<Corners> {
  if (!modifiers.clip) {
    return std::nullopt;
  }
  if (modifiers.clip_path.has_value()) {
    return Corners{};
  }
  if (!modifiers.corners.has_value()) {
    return std::nullopt;
  }
  const Corners resolved = resolve(scope, *modifiers.corners, area);
  if (!resolved.ellipse && !uneven(resolved) && resolved.radius <= 0.0F) {
    return std::nullopt;
  }
  return resolved;
}

auto Painter::intrudes(std::size_t first, Box area, const Corners& corners,
                       std::uint32_t transform) const -> bool {
  const Affine& matrix = transforms_[transform];
  if (matrix.b != 0.0F || matrix.c != 0.0F) {
    return true;
  }
  const Box screen = bounds(area, transform);
  const float scale_x = std::abs(matrix.a);
  const float scale_y = std::abs(matrix.d);
  std::array<float, 4> reach{};
  if (corners.ellipse) {
    constexpr float outside = 1.0F - std::numbers::sqrt2_v<float> * 0.5F;
    const float across = screen.width() * 0.5F * outside;
    const float down = screen.height() * 0.5F * outside;
    reach = {std::max(across, down), std::max(across, down),
             std::max(across, down), std::max(across, down)};
  } else {
    for (std::size_t corner = 0; corner < reach.size(); ++corner) {
      reach[corner] =
          corner_of(corners, corner) * std::max(scale_x, scale_y) *
          (corners.smoothing > 0.0F ? graphics::Path::continuous_reach : 1.0F);
    }
  }
  constexpr float slack = 0.5F;
  const std::array<Box, 4> regions{Box{.left = screen.left,
                                       .top = screen.top,
                                       .right = screen.left + reach[0],
                                       .bottom = screen.top + reach[0]},
                                   Box{.left = screen.right - reach[1],
                                       .top = screen.top,
                                       .right = screen.right,
                                       .bottom = screen.top + reach[1]},
                                   Box{.left = screen.right - reach[2],
                                       .top = screen.bottom - reach[2],
                                       .right = screen.right,
                                       .bottom = screen.bottom},
                                   Box{.left = screen.left,
                                       .top = screen.bottom - reach[3],
                                       .right = screen.left + reach[3],
                                       .bottom = screen.bottom}};
  const std::array<float, 4> outline = detail_of(corners);
  for (std::size_t index = first; index < quads_.size(); ++index) {
    const Quad& quad = quads_[index];
    const bool sdf = quad.kind == provider::ShapeKind::fill ||
                     quad.kind == provider::ShapeKind::stroke;
    if (sdf && quad.transform == transform &&
        std::abs(quad.centre[0] - ((area.left + area.right) * 0.5F)) <= slack &&
        std::abs(quad.centre[1] - ((area.top + area.bottom) * 0.5F)) <= slack &&
        quad.half_size[0] <= (area.width() * 0.5F) + slack &&
        quad.half_size[1] <= (area.height() * 0.5F) + slack &&
        quad.shape[0] >= corners.radius && quad.detail == outline) {
      continue;
    }
    if (inscribed(quad, area, corners, transform)) {
      continue;
    }
    Box box = quad.bounds;
    if (quad.mesh != 0) {
      const ShapePainter::Geometry& mesh = meshes_[quad.mesh - 1];
      if (mesh.vertices.empty()) {
        continue;
      }
      box = Box{.left = mesh.extent[0],
                .top = mesh.extent[1],
                .right = mesh.extent[2],
                .bottom = mesh.extent[3]};
    }
    const Box placed = bounds(box, quad.transform);
    if (inside_clip(quad, placed, screen, reach, corners.ellipse)) {
      continue;
    }
    for (const Box& region : regions) {
      if (region.width() <= slack || region.height() <= slack) {
        continue;
      }
      if (placed.left < region.right - slack &&
          placed.right > region.left + slack &&
          placed.top < region.bottom - slack &&
          placed.bottom > region.top + slack) {
        return true;
      }
    }
  }
  return false;
}

auto Painter::encloses(std::array<float, 2> point, Box clip,
                       const std::array<float, 4>& reach, bool ellipse) noexcept
    -> bool {
  constexpr float slack = 0.5F;
  const auto [x, y] = point;
  if (x < clip.left - slack || x > clip.right + slack || y < clip.top - slack ||
      y > clip.bottom + slack) {
    return false;
  }
  if (ellipse) {
    const float half_x = (clip.width() * 0.5F) + slack;
    const float half_y = (clip.height() * 0.5F) + slack;
    const float dx = (x - ((clip.left + clip.right) * 0.5F)) / half_x;
    const float dy = (y - ((clip.top + clip.bottom) * 0.5F)) / half_y;
    return (dx * dx) + (dy * dy) <= 1.0F;
  }
  for (std::size_t corner = 0; corner < reach.size(); ++corner) {
    const float radius = reach[corner];
    const bool right = corner == 1 || corner == 2;
    const bool bottom = corner == 2 || corner == 3;
    const float centre_x = right ? clip.right - radius : clip.left + radius;
    const float centre_y = bottom ? clip.bottom - radius : clip.top + radius;
    if (radius > 0.0F && (right ? x > centre_x : x < centre_x) &&
        (bottom ? y > centre_y : y < centre_y) &&
        ((x - centre_x) * (x - centre_x)) + ((y - centre_y) * (y - centre_y)) >
            (radius + slack) * (radius + slack)) {
      return false;
    }
  }
  return true;
}

auto Painter::inside_clip(const Quad& quad, Box placed, Box clip,
                          const std::array<float, 4>& reach,
                          bool ellipse) const noexcept -> bool {
  const auto held = [&](std::array<float, 2> point) {
    return encloses(point, clip, reach, ellipse);
  };
  if (quad.mesh != 0) {
    const Affine& matrix = transforms_[quad.transform];
    return std::ranges::all_of(meshes_[quad.mesh - 1].vertices,
                               [&](const provider::ShapeVertex& vertex) {
                                 return held(matrix.apply(vertex.position));
                               });
  }
  Box box = placed;
  if (quad.kind == provider::ShapeKind::fill ||
      quad.kind == provider::ShapeKind::stroke) {
    box = bounds(Box{.left = quad.centre[0] - quad.half_size[0],
                     .top = quad.centre[1] - quad.half_size[1],
                     .right = quad.centre[0] + quad.half_size[0],
                     .bottom = quad.centre[1] + quad.half_size[1]},
                 quad.transform);
  }
  return held({box.left, box.top}) && held({box.right, box.top}) &&
         held({box.right, box.bottom}) && held({box.left, box.bottom});
}

auto Painter::inscribed(const Quad& quad, Box area, const Corners& corners,
                        std::uint32_t transform) noexcept -> bool {
  if (quad.kind != provider::ShapeKind::fill || quad.transform != transform ||
      corners.ellipse || quad.detail[0] == ellipse_marker) {
    return false;
  }
  constexpr float slack = 0.5F;
  const Box shape{.left = quad.centre[0] - quad.half_size[0],
                  .top = quad.centre[1] - quad.half_size[1],
                  .right = quad.centre[0] + quad.half_size[0],
                  .bottom = quad.centre[1] + quad.half_size[1]};
  const bool even = quad.detail == std::array<float, 4>{};
  for (std::size_t corner = 0; corner < 4; ++corner) {
    const float wanted = corner_of(corners, corner);
    const float reach =
        wanted *
        (corners.smoothing > 0.0F ? graphics::Path::continuous_reach : 1.0F);
    const bool right = corner == 1 || corner == 2;
    const bool bottom = corner == 2 || corner == 3;
    const float x = right ? area.right : area.left;
    const float y = bottom ? area.bottom : area.top;
    const Box region{.left = right ? x - reach : x,
                     .top = bottom ? y - reach : y,
                     .right = right ? x : x + reach,
                     .bottom = bottom ? y : y + reach};
    if (reach <= slack || shape.left >= region.right - slack ||
        shape.right <= region.left + slack ||
        shape.top >= region.bottom - slack ||
        shape.bottom <= region.top + slack) {
      continue;
    }
    const float held = even ? quad.shape[0] : quad.detail[corner];
    const bool shared =
        std::abs((right ? shape.right : shape.left) - x) <= slack &&
        std::abs((bottom ? shape.bottom : shape.top) - y) <= slack;
    const bool rounder = quad.shape[1] >= corners.smoothing
                             ? held + slack >= wanted
                             : held + slack >= reach;
    if (!shared || !rounder) {
      return false;
    }
  }
  return true;
}

void Painter::round_images(std::size_t first, Box area, const Corners& corners,
                           std::uint32_t transform) {
  for (Quad& quad : std::span{quads_}.subspan(first)) {
    if (quad.kind != provider::ShapeKind::image || quad.backdrop ||
        quad.transform != transform) {
      continue;
    }
    quad.centre = {(area.left + area.right) * 0.5F,
                   (area.top + area.bottom) * 0.5F};
    quad.half_size = {area.width() * 0.5F, area.height() * 0.5F};
    quad.shape = {corners.radius, corners.smoothing, 0.0F, 0.0F};
    quad.detail = detail_of(corners);
  }
}

void Painter::clip_mask(const composition::Node& node, const Scope& scope,
                        Box area, const Corners& corners) {
  const composition::Modifiers& modifiers = node.modifiers();
  const std::uint32_t white = pack(theme::Colour{1.0F, 1.0F, 1.0F, 1.0F}, 1.0F);
  if (!modifiers.clip_path.has_value()) {
    shape(provider::ShapeKind::fill, area, corners, 0.0F, 0.0F, white,
          scope.clip);
    return;
  }
  ClipSource& cached = clip_sources_[node.id()];
  const auto* held = cached.source == nullptr
                         ? nullptr
                         : std::get_if<graphics::AnyShape>(cached.source.get());
  if (held == nullptr || *held != *modifiers.clip_path) {
    cached.source = std::make_shared<const components::ShapeView::Source>(
        *modifiers.clip_path);
  }
  cached.used = frame_;
  const float scale = target_.scale.value();
  ShapePainter::Request& request = clip_request_;
  request.node = composition::NodeId{node.id().value() ^ clip_mask_salt};
  request.source = cached.source;
  request.area = ShapePainter::Area{.left = area.left,
                                    .top = area.top,
                                    .right = area.right,
                                    .bottom = area.bottom,
                                    .scale = scale};
  request.corner_radius = 0.0F;
  request.corners = theme::RectangleCornerRadii{};
  request.inset = 0.0F;
  request.stroke.reset();
  request.fill = modifiers.clip_fill;
  request.trim_from = 0.0;
  request.trim_to = 1.0;
  request.paint.kind = ShapePainter::PaintKind::solid;
  request.paint.colour = theme::Colour{1.0F, 1.0F, 1.0F, 1.0F};
  request.paint.stops.clear();
  request.opacity = 1.0F;
  request.guide.reset();
  request.reach = 0.0F;
  if (const ShapePainter::Geometry* geometry = shapes_.shape(request)) {
    push_geometry(*geometry, scope.clip);
  }
}

auto Painter::presented_clip(const composition::Tree& tree,
                             const Presence& presence,
                             const composition::PresentedFrame& presented) const
    -> composition::PresentedClip {
  const Corners& corners = *presence.shaped;
  const float scale = target_.scale.value();
  const Affine& matrix = transforms_[presence.transform];
  const float stretch =
      std::sqrt(std::abs((matrix.a * matrix.d) - (matrix.b * matrix.c)));
  composition::PresentedClip clip{.corners = presented.corners,
                                  .ellipse = corners.ellipse};
  for (std::size_t corner = 0; corner < clip.radii.size(); ++corner) {
    clip.radii[corner] = corner_of(corners, corner) / scale * stretch;
  }
  if (const auto node = tree.node(presence.node)) {
    const composition::Modifiers& modifiers = node->get().modifiers();
    clip.shape = modifiers.clip_path;
    clip.fill = modifiers.clip_fill;
  }
  return clip;
}

void Painter::composite(const composition::Node& node, const Scope& scope,
                        const Scope& parent, const EffectPlan& plan,
                        float factor, std::size_t first,
                        std::optional<std::size_t> split,
                        std::size_t backdrops_before, Box frame) {
  const std::size_t last = quads_.size();
  const std::size_t content_end = split.value_or(last);
  if (content_end == first) {
    quads_.resize(first);
    return;
  }
  if (compositor_.pending_backdrops() != backdrops_before) {
    quads_.resize(content_end);
    fade_range(first, factor);
    return;
  }
  forget_since(first);
  const std::uint64_t key = node.id().value();
  const float scale = target_.scale.value();
  const float sigma = plan.blur * scale;
  const bool spreads = sigma > 0.0F && !plan.opaque;
  const float sampling =
      plan.shader.has_value()
          ? std::max(plan.sample_offset.width, plan.sample_offset.height) *
                scale
          : 0.0F;
  const float pad = (spreads ? std::ceil(sigma * 3.0F) + 1.0F : 0.0F) +
                    std::ceil(std::max(sampling, 0.0F));
  const std::optional<LayerArea> area =
      layer_area(first, content_end, parent.clip, pad);
  if (!area.has_value()) {
    quads_.resize(first);
    return;
  }
  const Box& region = area->region;
  effect_content_.assign(
      quads_.begin() + static_cast<std::ptrdiff_t>(first),
      quads_.begin() + static_cast<std::ptrdiff_t>(content_end));
  const bool blends =
      plan.blend != graphics::BlendMode::normal && effect_depth_ == 0;
  const bool anchored = blends || area->clamped;
  LayerContent drawn{
      .hash = 0, .anchor = anchor_of(first), .clip = parent.clip};
  drawn.hash =
      layer_hash(first, content_end, drawn.anchor, parent.clip, anchored);
  if (split.has_value()) {
    const std::uint64_t masked =
        layer_hash(*split, last, drawn.anchor, parent.clip, anchored);
    drawn.hash =
        Compositor::mix(drawn.hash, std::as_bytes(std::span{&masked, 1}));
  }
  const std::optional<EffectSource> content =
      effect_layer(key, 0, region, first, content_end, drawn);
  if (!content.has_value()) {
    quads_.resize(content_end);
    fade_range(first, factor);
    return;
  }
  std::optional<EffectSource> mask;
  if (split.has_value()) {
    mask = effect_layer(key, 1, region, *split, last, drawn);
    if (mask.has_value() &&
        (mask->origin != content->origin || mask->extent != content->extent)) {
      compositor_.redraw_layer(effect_key(key));
      const std::uint32_t mask_stage = 1;
      compositor_.redraw_layer(
          Compositor::mix(key, std::as_bytes(std::span{&mask_stage, 1})));
      fall_back(first, factor);
      return;
    }
  }
  quads_.resize(first);
  const Box placed{
      .left = content->origin[0],
      .top = content->origin[1],
      .right = content->origin[0] + static_cast<float>(content->width),
      .bottom = content->origin[1] + static_cast<float>(content->height)};
  EffectSource current = *content;
  bool fresh = current.rendered || (mask.has_value() && mask->rendered);
  std::uint32_t stage = 2;
  const auto advance = [&](std::optional<EffectSource> next) {
    if (!next.has_value()) {
      return false;
    }
    fresh = fresh || next->rendered;
    current = *next;
    return true;
  };
  const auto staged = [](provider::ShapeKind kind) {
    Quad made{};
    made.kind = kind;
    return made;
  };
  if (mask.has_value()) {
    current.hash =
        Compositor::mix(current.hash, std::as_bytes(std::span{&mask->hash, 1}));
    Quad masking = staged(provider::ShapeKind::mask);
    masking.detail_texture = mask->texture;
    masking.shape = {plan.inverse_mask ? 1.0F : 0.0F, 0.0F, 0.0F, 0.0F};
    if (!advance(effect_stage(
            key, stage++, current, current.width, current.height,
            plan.inverse_mask ? effect_inverse_mask_salt : effect_mask_salt,
            masking))) {
      fall_back(first, factor);
      return;
    }
  }
  if (plan.shader.has_value()) {
    const std::array<float, 2> origin =
        transforms_[scope.transform].apply({frame.left, frame.top});
    std::array<float, Compositor::table_capacity> values{};
    values[0] = origin[0] - placed.left;
    values[1] = origin[1] - placed.top;
    values[2] = scale;
    values[5] = static_cast<float>(current.width) / current.extent[0];
    values[6] = static_cast<float>(current.height) / current.extent[1];
    values[7] = frame.width() / scale;
    values[8] = frame.height() / scale;
    const std::vector<float>& arguments = plan.shader->arguments();
    std::ranges::copy(
        std::span{arguments}.first(std::min<std::size_t>(
            arguments.size(), Compositor::table_capacity - shader_arguments)),
        values.begin() + shader_arguments);
    for (const std::size_t slot : plan.shader->bounds_slots()) {
      if (shader_arguments + slot + 3 < values.size()) {
        values[shader_arguments + slot] = 0.0F;
        values[shader_arguments + slot + 1] = 0.0F;
        values[shader_arguments + slot + 2] = frame.width() / scale;
        values[shader_arguments + slot + 3] = frame.height() / scale;
      }
    }
    const std::uint64_t table_key =
        Compositor::mix(key, std::as_bytes(std::span{&effect_shader_salt, 1}));
    const std::optional<provider::TextureId> shading =
        compositor_.table(table_key, values);
    if (shading.has_value()) {
      const std::uint32_t index = plan.shader->function().index();
      std::uint64_t salt = Compositor::mix(effect_shader_salt,
                                           std::as_bytes(std::span{&index, 1}));
      salt = Compositor::mix(salt, std::as_bytes(std::span{values}));
      Quad shaded = staged(provider::ShapeKind::shader);
      shaded.shape = {static_cast<float>(index),
                      plan.shader->dithers_color() ? 1.0F : 0.0F, 0.0F, 0.0F};
      shaded.detail_texture = shading;
      if (!advance(effect_stage(key, stage++, current, current.width,
                                current.height, salt, shaded))) {
        fall_back(first, factor);
        return;
      }
    }
  }
  const auto blurred = [&](float deviation) {
    float level = deviation;
    std::uint32_t reductions = 0;
    while (level > maximum_gaussian && reductions < maximum_reductions) {
      if (!advance(effect_stage(
              key, stage++, current, std::max((current.width + 1) / 2, 1U),
              std::max((current.height + 1) / 2, 1U), effect_reduce_salt,
              staged(provider::ShapeKind::layer)))) {
        return false;
      }
      level *= 0.5F;
      ++reductions;
    }
    const std::uint32_t bits = std::bit_cast<std::uint32_t>(level);
    for (const std::array<float, 2> direction :
         {std::array{1.0F, 0.0F}, std::array{0.0F, 1.0F}}) {
      const float wide = static_cast<float>(current.width) / current.extent[0];
      const float tall = static_cast<float>(current.height) / current.extent[1];
      const std::uint64_t salt =
          effect_gaussian_salt ^ bits ^ (direction[0] > 0.0F ? 1U : 2U);
      Quad blurring = staged(provider::ShapeKind::gaussian);
      blurring.shape = {level, direction[0], direction[1], 0.0F};
      blurring.effect = {0.5F / wide, 0.5F / tall,
                         current.extent[0] - (0.5F / wide),
                         current.extent[1] - (0.5F / tall)};
      if (!advance(effect_stage(key, stage++, current, current.width,
                                current.height, salt, blurring))) {
        return false;
      }
    }
    return true;
  };
  if (plan.extends) {
    const EffectSource sharp = current;
    if (blurred(extension_blur * scale)) {
      for (const int dy : {-1, 0, 1}) {
        for (const int dx : {-1, 0, 1}) {
          if (dx == 0 && dy == 0) {
            continue;
          }
          const float shift_x = static_cast<float>(dx) * placed.width();
          const float shift_y = static_cast<float>(dy) * placed.height();
          const Box tiled{.left = placed.left + shift_x,
                          .top = placed.top + shift_y,
                          .right = placed.right + shift_x,
                          .bottom = placed.bottom + shift_y};
          quads_.push_back(Quad{
              .kind = provider::ShapeKind::filter,
              .bounds = tiled,
              .uv = {dx != 0 ? current.extent[0] : 0.0F,
                     dy != 0 ? current.extent[1] : 0.0F,
                     dx != 0 ? 0.0F : current.extent[0],
                     dy != 0 ? 0.0F : current.extent[1]},
              .centre = {(tiled.left + tiled.right) * 0.5F,
                         (tiled.top + tiled.bottom) * 0.5F},
              .half_size = {tiled.width() * 0.5F, tiled.height() * 0.5F},
              .shape = {},
              .colour = pack(theme::Colour{1.0F, 1.0F, 1.0F, 1.0F}, factor),
              .texture = current.texture,
              .clip = parent.clip,
              .transform = 0,
              .backdrop = true,
              .owner = node.id().value(),
              .fresh = fresh,
              .fade = parent.fade});
        }
      }
    }
    current = sharp;
  }
  if (sigma > 0.0F && !blurred(sigma)) {
    fall_back(first, factor);
    return;
  }
  bool filters = !plan.matrix.is_identity();
  std::optional<provider::TextureId> table;
  if (filters) {
    const std::uint64_t table_key =
        Compositor::mix(key, std::as_bytes(std::span{&effect_table_salt, 1}));
    table = compositor_.table(table_key, plan.matrix.values());
    filters = table.has_value();
  }
  if (blends && filters) {
    std::uint64_t salt = effect_matrix_salt;
    for (const float value : plan.matrix.values()) {
      salt = Compositor::mix(salt, std::as_bytes(std::span{&value, 1}));
    }
    Quad filtering = staged(provider::ShapeKind::filter);
    filtering.shape = {1.0F, 0.0F, plan.premultiplied ? 1.0F : 0.0F,
                       plan.linear ? 1.0F : 0.0F};
    filtering.detail_texture = table;
    if (!advance(effect_stage(key, stage++, current, current.width,
                              current.height, salt, filtering))) {
      fall_back(first, factor);
      return;
    }
    filters = false;
  }
  Quad quad{
      .kind = provider::ShapeKind::filter,
      .bounds = placed,
      .uv = {0.0F, 0.0F, current.extent[0], current.extent[1]},
      .centre = {(placed.left + placed.right) * 0.5F,
                 (placed.top + placed.bottom) * 0.5F},
      .half_size = {placed.width() * 0.5F, placed.height() * 0.5F},
      .shape = {filters ? 1.0F : 0.0F, plan.opaque ? 1.0F : 0.0F,
                filters && plan.premultiplied ? 1.0F : 0.0F,
                filters && plan.linear ? 1.0F : 0.0F},
      .colour = pack(theme::Colour{1.0F, 1.0F, 1.0F, 1.0F}, factor),
      .texture = current.texture,
      .clip = parent.clip,
      .transform = 0,
      .detail_texture = filters ? table : std::optional<provider::TextureId>{},
      .backdrop = true,
      .owner = node.id().value(),
      .fresh = fresh,
      .fade = parent.fade};
  if (blends) {
    const std::uint64_t backdrop_key =
        Compositor::mix(key, std::as_bytes(std::span{&effect_blend_salt, 1}));
    const provider::PassRegion wanted{.x = region.left,
                                      .y = region.top,
                                      .width = region.width(),
                                      .height = region.height()};
    const auto surface =
        backdrop_for(backdrop_key, wanted, wanted, 0.0F,
                     static_cast<std::uint32_t>(quads_.size()));
    if (surface.has_value()) {
      quad.kind = provider::ShapeKind::blend;
      quad.shape = {static_cast<float>(std::to_underlying(plan.blend)), 0.0F,
                    0.0F, 0.0F};
      quad.detail_texture = surface->texture;
      quad.effect = region_effect(surface->region);
    }
  }
  if (plan.projection.has_value()) {
    project(*plan.projection, transforms_[scope.transform], frame, placed,
            current, quad, pad);
    return;
  }
  quads_.push_back(quad);
}

void Painter::project(const graphics::ProjectionTransform& projection,
                      const Affine& world, Box frame, Box region,
                      const EffectSource& source, Quad quad, float pad) {
  const float scale = target_.scale.value();
  ShapePainter::Geometry& mesh = projected_;
  mesh.vertices.clear();
  mesh.indices.clear();
  const float left = frame.left - pad;
  const float top = frame.top - pad;
  const float wide = frame.width() + (2.0F * pad);
  const float tall = frame.height() + (2.0F * pad);
  std::array<float, 4> extent{std::numeric_limits<float>::max(),
                              std::numeric_limits<float>::max(),
                              std::numeric_limits<float>::lowest(),
                              std::numeric_limits<float>::lowest()};
  for (std::uint32_t row = 0; row <= projection_cells; ++row) {
    for (std::uint32_t column = 0; column <= projection_cells; ++column) {
      const float x = left + (wide * static_cast<float>(column) /
                              static_cast<float>(projection_cells));
      const float y = top + (tall * static_cast<float>(row) /
                             static_cast<float>(projection_cells));
      const std::optional<composition::Point> moved =
          projection.apply(composition::Point{.x = (x - frame.left) / scale,
                                              .y = (y - frame.top) / scale});
      if (!moved.has_value()) {
        return;
      }
      const std::array<float, 2> placed = world.apply(
          {frame.left + (moved->x * scale), frame.top + (moved->y * scale)});
      const std::array<float, 2> sampled = world.apply({x, y});
      extent = {std::min(extent[0], placed[0]), std::min(extent[1], placed[1]),
                std::max(extent[2], placed[0]), std::max(extent[3], placed[1])};
      mesh.vertices.push_back(provider::ShapeVertex{
          .position = placed,
          .uv = {(sampled[0] - region.left) / region.width() * source.extent[0],
                 (sampled[1] - region.top) / region.height() *
                     source.extent[1]},
          .local = {},
          .half_size = {},
          .shape = quad.shape,
          .colour = quad.colour,
          .kind = quad.kind,
          .effect = quad.effect,
          .detail = {}});
    }
  }
  const std::uint32_t stride = projection_cells + 1;
  for (std::uint32_t row = 0; row < projection_cells; ++row) {
    for (std::uint32_t column = 0; column < projection_cells; ++column) {
      const provider::Index at = (row * stride) + column;
      const provider::Index below = at + stride;
      for (const provider::Index index :
           {at, at + 1, below, below, at + 1, below + 1}) {
        mesh.indices.push_back(index);
      }
    }
  }
  mesh.texture = quad.texture;
  mesh.extent = extent;
  stage_mesh(mesh, 0);
  quad.mesh = static_cast<std::uint32_t>(mesh_count_);
  quad.bounds = Box{.left = extent[0],
                    .top = extent[1],
                    .right = extent[2],
                    .bottom = extent[3]};
  quads_.push_back(quad);
}

}
