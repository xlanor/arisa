#include <arisa/components/graphics_context.hpp>

#include <arisa/components/shape_view.hpp>
#include <arisa/components/text_glyphs.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/image_environment.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/colour_mix.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace arisa::components {

auto GraphicsContext::Shading::backdrop() -> Shading {
  return Shading{Value{Backdrop{}}};
}

auto GraphicsContext::Shading::color(theme::RGBColorSpace space, float red,
                                     float green, float blue, float opacity)
    -> Shading {
  return Shading{graphics::ShapeStyle{
      theme::ColourMix::rgb(space, red, green, blue, opacity)}};
}

auto GraphicsContext::Shading::color(theme::RGBColorSpace space, float white,
                                     float opacity) -> Shading {
  return Shading{
      graphics::ShapeStyle{theme::ColourMix::grey(space, white, opacity)}};
}

auto GraphicsContext::Shading::palette(std::vector<graphics::ShapeStyle> layers)
    -> Shading {
  return Shading{Value{Palette{.layers = std::move(layers)}}};
}

auto GraphicsContext::Shading::mesh_gradient(const MeshGradient& mesh)
    -> Shading {
  return Shading{Value{
      Mesh{.content = std::make_shared<const MeshGradient::Content>(
               static_cast<const MeshGradient::Content&>(mesh.content()))}}};
}

auto GraphicsContext::Shading::shader(graphics::Shader shader,
                                      composition::Rect bounds) -> Shading {
  return Shading{
      Value{ShaderFill{.shader = std::move(shader), .bounds = bounds}}};
}

auto GraphicsContext::Shading::linear_gradient(graphics::Gradient gradient,
                                               composition::Point start_point,
                                               composition::Point end_point,
                                               GradientOptions options)
    -> Shading {
  return Shading{Value{Ramp{.kind = Ramp::Kind::linear,
                            .gradient = std::move(gradient),
                            .start = start_point,
                            .end = end_point,
                            .start_radius = 0.0F,
                            .end_radius = 0.0F,
                            .angle = {},
                            .options = options}}};
}

auto GraphicsContext::Shading::radial_gradient(
    graphics::Gradient gradient, composition::Point center, float start_radius,
    float end_radius, GradientOptions options) -> Shading {
  return Shading{Value{Ramp{.kind = Ramp::Kind::radial,
                            .gradient = std::move(gradient),
                            .start = center,
                            .end = center,
                            .start_radius = start_radius,
                            .end_radius = end_radius,
                            .angle = {},
                            .options = options}}};
}

auto GraphicsContext::Shading::conic_gradient(graphics::Gradient gradient,
                                              composition::Point center,
                                              motion::Angle angle,
                                              GradientOptions options)
    -> Shading {
  return Shading{Value{Ramp{.kind = Ramp::Kind::conic,
                            .gradient = std::move(gradient),
                            .start = center,
                            .end = center,
                            .start_radius = 0.0F,
                            .end_radius = 0.0F,
                            .angle = angle,
                            .options = options}}};
}

auto GraphicsContext::Shading::style() const noexcept
    -> const graphics::ShapeStyle* {
  if (const auto* plain = std::get_if<graphics::ShapeStyle>(&value_)) {
    return plain;
  }
  if (const auto* layered = std::get_if<Palette>(&value_);
      layered != nullptr && !layered->layers.empty()) {
    return &layered->layers.front();
  }
  return nullptr;
}

auto GraphicsContext::Filter::blur(float radius, BlurOptions options)
    -> Filter {
  return Filter{
      Value{Blur{.radius = std::max(radius, 0.0F), .options = options}}};
}

auto GraphicsContext::Filter::colour_matrix(graphics::ColourMatrix matrix,
                                            bool is_premultiplied) -> Filter {
  return Filter{
      Value{Matrix{.matrix = matrix, .premultiplied = is_premultiplied}}};
}

auto GraphicsContext::Filter::grayscale(double amount) -> Filter {
  return colour_matrix(graphics::ColourMatrix::grayscale(amount));
}

auto GraphicsContext::Filter::brightness(double amount) -> Filter {
  return colour_matrix(graphics::ColourMatrix::brightness(amount));
}

auto GraphicsContext::Filter::saturation(double amount) -> Filter {
  return colour_matrix(graphics::ColourMatrix::saturation(amount));
}

auto GraphicsContext::Filter::hue_rotation(motion::Angle angle) -> Filter {
  return colour_matrix(graphics::ColourMatrix::hue_rotation(angle));
}

auto GraphicsContext::Filter::color_multiply(theme::Colour colour) -> Filter {
  return colour_matrix(graphics::ColourMatrix::multiplied(colour));
}

auto GraphicsContext::Filter::alpha_multiply(theme::Colour colour) -> Filter {
  return colour_matrix(graphics::ColourMatrix::of(
      {0.0F, 0.0F,         0.0F, colour.red, 0.0F,         0.0F, 0.0F,
       0.0F, colour.green, 0.0F, 0.0F,       0.0F,         0.0F, colour.blue,
       0.0F, 0.0F,         0.0F, 0.0F,       colour.alpha, 0.0F}));
}

auto GraphicsContext::Filter::alpha_threshold(float min, float max,
                                              theme::Colour colour) -> Filter {
  return Filter{Value{Threshold{.min = min, .max = max, .colour = colour}}};
}

auto GraphicsContext::Filter::color_shader(graphics::Shader shader) -> Filter {
  return Filter{Value{Effect{.shader = std::move(shader),
                             .offset = {},
                             .kind = Effect::Kind::colour}}};
}

auto GraphicsContext::Filter::distortion_shader(
    graphics::Shader shader, composition::Size max_sample_offset) -> Filter {
  return Filter{Value{Effect{.shader = std::move(shader),
                             .offset = max_sample_offset,
                             .kind = Effect::Kind::distortion}}};
}

auto GraphicsContext::Filter::layer_shader(graphics::Shader shader,
                                           composition::Size max_sample_offset)
    -> Filter {
  return Filter{Value{Effect{.shader = std::move(shader),
                             .offset = max_sample_offset,
                             .kind = Effect::Kind::layer}}};
}

auto GraphicsContext::Filter::projection_transform(
    graphics::ProjectionTransform matrix) -> Filter {
  return Filter{Value{matrix}};
}

auto GraphicsContext::Filter::shadow(theme::Colour colour, float radius,
                                     float x, float y, BlendMode blend_mode,
                                     ShadowOptions options) -> Filter {
  return Filter{Value{Shadow{.colour = colour,
                             .radius = std::max(radius, 0.0F),
                             .x = x,
                             .y = y,
                             .blend = blend_mode,
                             .options = options}}};
}

auto GraphicsContext::Filter::silhouette(composition::Element element,
                                         theme::Colour colour)
    -> composition::Element {
  return std::move(element).colour_matrix(graphics::ColourMatrix::of(
      {0.0F,        0.0F, 0.0F,         0.0F, colour.red,   0.0F, 0.0F,
       0.0F,        0.0F, colour.green, 0.0F, 0.0F,         0.0F, 0.0F,
       colour.blue, 0.0F, 0.0F,         0.0F, colour.alpha, 0.0F}));
}

auto GraphicsContext::Filter::shadowed(
    const Shadow& shadow, composition::Element element,
    const std::function<composition::Element()>& again)
    -> composition::Element {
  const bool inner = shadow.options.contains(ShadowOptions::inverts_alpha);
  composition::Element cast =
      inner ? silhouette(again(), shadow.colour)
                  .reverse_mask(
                      composition::Alignment::top_leading,
                      again().blur(shadow.radius).offset(shadow.x, shadow.y))
            : silhouette(again(), shadow.colour)
                  .blur(shadow.radius)
                  .offset(shadow.x, shadow.y);
  if (const std::optional<graphics::BlendMode> mode =
          graphics_mode(shadow.blend);
      mode.has_value() && *mode != graphics::BlendMode::normal) {
    cast = std::move(cast).blend_mode(*mode);
  }
  const ZStackOptions corner{.alignment = composition::Alignment::top_leading};
  if (inner) {
    if (shadow.options.contains(ShadowOptions::shadow_only)) {
      return cast;
    }
    return ZStack(corner, std::move(element), std::move(cast));
  }
  if (shadow.options.contains(ShadowOptions::shadow_only)) {
    return cast;
  }
  if (shadow.options.contains(ShadowOptions::shadow_above)) {
    return ZStack(corner, std::move(element), std::move(cast));
  }
  return ZStack(corner, std::move(cast), std::move(element));
}

auto GraphicsContext::Filter::apply(
    composition::Element element,
    const std::function<composition::Element()>& again, bool linear) const
    -> composition::Element {
  return std::visit(
      [&](const auto& each) -> composition::Element {
        using T = std::remove_cvref_t<decltype(each)>;
        if constexpr (std::same_as<T, Blur>) {
          return std::move(element).blur(
              each.radius, each.options.contains(BlurOptions::opaque));
        } else if constexpr (std::same_as<T, Matrix>) {
          if (!each.premultiplied && !linear) {
            return std::move(element).colour_matrix(each.matrix);
          }
          return std::move(element).colour_matrix(each.matrix,
                                                  each.premultiplied, linear);
        } else if constexpr (std::same_as<T, Threshold>) {
          constexpr float steep = 4096.0F;
          const graphics::ColourMatrix gate = graphics::ColourMatrix::of(
              {0.0F, 0.0F, 0.0F, steep,  1.0F - (steep * each.min),
               0.0F, 0.0F, 0.0F, -steep, 1.0F + (steep * each.max),
               0.0F, 0.0F, 0.0F, 0.0F,   0.0F,
               0.0F, 0.0F, 0.0F, 0.0F,   1.0F});
          const graphics::ColourMatrix paint =
              graphics::ColourMatrix::of({0.0F,
                                          0.0F,
                                          0.0F,
                                          0.0F,
                                          each.colour.red,
                                          0.0F,
                                          0.0F,
                                          0.0F,
                                          0.0F,
                                          each.colour.green,
                                          0.0F,
                                          0.0F,
                                          0.0F,
                                          0.0F,
                                          each.colour.blue,
                                          each.colour.alpha,
                                          each.colour.alpha,
                                          0.0F,
                                          0.0F,
                                          -each.colour.alpha});
          return ZStack(std::move(element).colour_matrix(gate))
              .colour_matrix(paint);
        } else if constexpr (std::same_as<T, Effect>) {
          switch (each.kind) {
            case Effect::Kind::colour:
              return std::move(element).color_effect(each.shader);
            case Effect::Kind::distortion:
              return std::move(element).distortion_effect(each.shader,
                                                          each.offset);
            case Effect::Kind::layer:
              return std::move(element).layer_effect(each.shader, each.offset);
          }
          return std::move(element);
        } else if constexpr (std::same_as<T, graphics::ProjectionTransform>) {
          return std::move(element).projection_effect(each);
        } else {
          return shadowed(each, std::move(element), again);
        }
      },
      value_);
}

auto GraphicsContext::TextLedger::metrics(std::size_t text,
                                          composition::Size proposal,
                                          const std::function<Text()>& make)
    -> std::optional<TextMetrics> {
  const auto known = std::ranges::find_if(known_, [&](const TextMeasure& each) {
    return each.text == text && each.proposal == proposal;
  });
  if (known != known_.end()) {
    return known->metrics;
  }
  const bool asked = std::ranges::any_of(requests_, [&](const Request& each) {
    return each.text == text && each.proposal == proposal;
  });
  if (!asked) {
    requests_.push_back(
        Request{.text = text, .proposal = proposal, .make = make});
  }
  return std::nullopt;
}

auto GraphicsContext::ResolvedText::metrics(composition::Size proposal) const
    -> std::optional<TextMetrics> {
  if (ledger_ == nullptr) {
    return std::nullopt;
  }
  return ledger_->metrics(index_, proposal, make_);
}

auto GraphicsContext::ResolvedText::measure(composition::Size proposal) const
    -> composition::Size {
  return metrics(proposal).value_or(TextMetrics{}).size;
}

auto GraphicsContext::ResolvedText::first_baseline(
    composition::Size proposal) const -> float {
  return metrics(proposal).value_or(TextMetrics{}).first_baseline;
}

auto GraphicsContext::ResolvedText::last_baseline(
    composition::Size proposal) const -> float {
  return metrics(proposal).value_or(TextMetrics{}).last_baseline;
}

GraphicsContext::GraphicsContext(composition::Size size) : size_(size) {}

GraphicsContext::GraphicsContext(composition::Size size, Resources resources)
    : size_(size), resources_(std::move(resources)) {}

auto GraphicsContext::tiled_image(std::shared_ptr<const provider::Image> image,
                                  composition::Point origin,
                                  graphics::ImagePaint::SourceRect source_rect,
                                  float scale) -> graphics::ShapeStyle {
  return graphics::ShapeStyle{graphics::ImagePaint{
      std::move(image), source_rect, scale,
      graphics::ImagePaint::Origin{.x = origin.x, .y = origin.y}}};
}

auto GraphicsContext::measured(const Image::Content& content) const
    -> composition::Size {
  if (const auto& pixels = content.pixels(); pixels != nullptr) {
    return composition::Size{
        .width = static_cast<float>(pixels->width()) / content.scale(),
        .height = static_cast<float>(pixels->height()) / content.scale()};
  }
  if (content.drawing() != nullptr) {
    return content.drawing()->size;
  }
  if (content.system()) {
    if (resources_.theme == nullptr) {
      return composition::Size{};
    }
    const float extent =
        Text::text_style(*resources_.theme,
                         content.font().value_or(theme::Font::body),
                         resources_.locale)
            .size.value();
    return composition::Size{.width = extent, .height = extent};
  }
  const std::optional<ImageLibrary>& library =
      content.bundle().has_value() ? content.bundle() : resources_.library;
  if (!library.has_value()) {
    return composition::Size{};
  }
  const ImageLibrary::Loaded loaded = library->image(content.name());
  if (!loaded.has_value() || *loaded == nullptr) {
    return composition::Size{};
  }
  return composition::Size{
      .width = static_cast<float>((*loaded)->width()) / content.scale(),
      .height = static_cast<float>((*loaded)->height()) / content.scale()};
}

auto GraphicsContext::resolve(Image image) const -> ResolvedImage {
  const auto& content = static_cast<const Image::Content&>(image.content());
  const composition::Size extent = measured(content);
  auto shared = std::make_shared<const Image>(std::move(image));
  return ResolvedImage{[shared] { return shared->duplicate(); }, extent,
                       extent.height};
}

auto GraphicsContext::resolve(Text text) const -> ResolvedText {
  auto shared = std::make_shared<const Text>(std::move(text));
  const std::size_t index =
      resources_.texts != nullptr ? resources_.texts->next() : 0;
  return ResolvedText{[shared] { return shared->duplicate(); }, index,
                      resources_.texts};
}

auto GraphicsContext::resolve(const Shading& shading) const -> Shading {
  if (resources_.theme == nullptr) {
    return shading;
  }
  const theme::Theme& theme = *resources_.theme;
  const theme::ColorScheme scheme = theme.scheme();
  if (const auto* plain = std::get_if<graphics::ShapeStyle>(&shading.value())) {
    return Shading{plain->resolve(theme, scheme)};
  }
  if (const auto* layered = std::get_if<Shading::Palette>(&shading.value())) {
    std::vector<graphics::ShapeStyle> resolved;
    for (const graphics::ShapeStyle& layer : layered->layers) {
      resolved.push_back(layer.resolve(theme, scheme));
    }
    return Shading::palette(std::move(resolved));
  }
  return shading;
}

auto GraphicsContext::unit(composition::Point point) const noexcept
    -> motion::UnitPoint {
  return motion::UnitPoint{size_.width > 0.0F ? point.x / size_.width : 0.0F,
                           size_.height > 0.0F ? point.y / size_.height : 0.0F};
}

auto GraphicsContext::ramp(const Shading::Ramp& ramp) const
    -> graphics::ShapeStyle {
  graphics::Gradient gradient = ramp.gradient;
  if (ramp.options.contains(GradientOptions::linear_color)) {
    gradient = gradient.color_space(theme::ColourInterpolation::linear);
  }
  const bool repeats = ramp.options.contains(GradientOptions::repeat);
  const bool mirrors = ramp.options.contains(GradientOptions::mirror);
  const float reach = std::hypot(size_.width, size_.height);
  const auto spread = [&gradient, mirrors](int before, int after) {
    const int periods = before + 1 + after;
    std::vector<graphics::Gradient::Stop> stops;
    for (int period = 0; period < periods; ++period) {
      const bool flipped = mirrors && ((period - before) % 2 != 0);
      for (const graphics::Gradient::Stop& stop : gradient.stops()) {
        const float local = flipped ? 1.0F - stop.location : stop.location;
        stops.push_back(graphics::Gradient::Stop{
            .color = stop.color,
            .location = (static_cast<float>(period) + local) /
                        static_cast<float>(periods)});
      }
    }
    return graphics::Gradient{std::move(stops)}.color_space(
        gradient.interpolation());
  };
  constexpr int most = 32;
  const float scale = std::sqrt(std::abs((transform_.a() * transform_.d()) -
                                         (transform_.b() * transform_.c())));
  switch (ramp.kind) {
    case Shading::Ramp::Kind::linear: {
      composition::Point start = transform_.apply(ramp.start);
      composition::Point end = transform_.apply(ramp.end);
      const float length = std::hypot(end.x - start.x, end.y - start.y);
      if ((repeats || mirrors) && length > 1.0e-3F) {
        const int count =
            std::min(static_cast<int>(std::ceil(reach / length)) + 1, most);
        const float dx = end.x - start.x;
        const float dy = end.y - start.y;
        const auto times = static_cast<float>(count);
        gradient = spread(count, count);
        start = composition::Point{.x = start.x - (dx * times),
                                   .y = start.y - (dy * times)};
        end = composition::Point{.x = end.x + (dx * times),
                                 .y = end.y + (dy * times)};
      }
      return graphics::ShapeStyle{
          graphics::LinearGradient{.gradient = std::move(gradient),
                                   .start_point = unit(start),
                                   .end_point = unit(end)}};
    }
    case Shading::Ramp::Kind::radial: {
      const composition::Point centre = transform_.apply(ramp.start);
      float first = ramp.start_radius * scale;
      float last = ramp.end_radius * scale;
      const float length = last - first;
      if ((repeats || mirrors) && length > 1.0e-3F) {
        const int inward =
            std::min(static_cast<int>(std::floor(first / length)), most);
        const int outward =
            std::min(static_cast<int>(std::ceil(reach / length)) + 1, most);
        gradient = spread(inward, outward);
        first -= length * static_cast<float>(inward);
        last += length * static_cast<float>(outward);
      }
      return graphics::ShapeStyle{
          graphics::RadialGradient{.gradient = std::move(gradient),
                                   .center = unit(centre),
                                   .start_radius = first,
                                   .end_radius = last}};
    }
    case Shading::Ramp::Kind::conic:
      break;
  }
  const double turn = std::atan2(static_cast<double>(transform_.b()),
                                 static_cast<double>(transform_.a()));
  const motion::Angle start = ramp.angle + motion::Angle::radians(turn);
  return graphics::ShapeStyle{graphics::AngularGradient{
      .gradient = std::move(gradient),
      .center = unit(transform_.apply(ramp.start)),
      .start_angle = start,
      .end_angle = start + motion::Angle::degrees(360.0)}};
}

auto GraphicsContext::filled(const graphics::Path& path, const Shading& shading,
                             std::optional<graphics::StrokeStyle> stroke,
                             graphics::FillStyle fill) const -> Make {
  const composition::Size size = size_;
  const graphics::Path placed = path.applying(transform_);
  const auto shape = [placed, stroke, fill, size](graphics::ShapeStyle style) {
    ShapeView view{placed};
    if (stroke.has_value()) {
      return std::move(view)
          .stroke(std::move(style), *stroke)
          .frame({.width = size.width, .height = size.height});
    }
    return std::move(view)
        .fill(std::move(style), fill)
        .frame({.width = size.width, .height = size.height});
  };
  const theme::Colour ink{0.0F, 0.0F, 0.0F, 1.0F};
  return std::visit(
      [&](const auto& each) -> Make {
        using T = std::remove_cvref_t<decltype(each)>;
        if constexpr (std::same_as<T, graphics::ShapeStyle>) {
          return [shape, each] { return shape(each); };
        } else if constexpr (std::same_as<T, Shading::Palette>) {
          const graphics::ShapeStyle first =
              each.layers.empty() ? graphics::ShapeStyle::foreground()
                                  : each.layers.front();
          return [shape, first] { return shape(first); };
        } else if constexpr (std::same_as<T, Shading::Ramp>) {
          return [shape, style = ramp(each)] { return shape(style); };
        } else if constexpr (std::same_as<T, Shading::Mesh>) {
          return [shape, content = each.content, size, ink] {
            composition::Element mesh =
                MeshGradient(content->width(), content->height(),
                             content->points(), content->colors(),
                             content->background(), content->smooths_colors(),
                             content->color_space())
                    .frame({.width = size.width, .height = size.height});
            return std::move(mesh).mask(composition::Alignment::top_leading,
                                        shape(graphics::ShapeStyle{ink}));
          };
        } else if constexpr (std::same_as<T, Shading::ShaderFill>) {
          return [shape, shader = each.shader,
                  bounds = graphics::Path{each.bounds}.applying(transform_),
                  size, ink] {
            return ShapeView{bounds}
                .fill(theme::Colour{1.0F, 1.0F, 1.0F, 1.0F})
                .frame({.width = size.width, .height = size.height})
                .color_effect(shader)
                .mask(composition::Alignment::top_leading,
                      shape(graphics::ShapeStyle{ink}));
          };
        } else {
          return [shape, ink] { return shape(graphics::ShapeStyle{ink}); };
        }
      },
      shading.value());
}

void GraphicsContext::fill(const graphics::Path& path,
                           graphics::ShapeStyle style,
                           graphics::FillStyle fill) {
  this->fill(path, Shading{std::move(style)}, fill);
}

void GraphicsContext::fill(const graphics::Path& path, const Shading& shading,
                           graphics::FillStyle fill) {
  const Make coverage = filled(
      path,
      Shading{graphics::ShapeStyle{theme::Colour{0.0F, 0.0F, 0.0F, 1.0F}}},
      std::nullopt, fill);
  if (std::holds_alternative<Shading::Backdrop>(shading.value())) {
    const BlendMode held = blend_;
    blend_ = BlendMode::clear;
    record(coverage, coverage);
    blend_ = held;
    return;
  }
  record(filled(path, shading, std::nullopt, fill), coverage);
}

void GraphicsContext::stroke(const graphics::Path& path,
                             graphics::ShapeStyle style, float line_width) {
  stroke(path, Shading{std::move(style)},
         graphics::StrokeStyle{.line_width = line_width});
}

void GraphicsContext::stroke(const graphics::Path& path,
                             graphics::ShapeStyle style,
                             graphics::StrokeStyle stroke) {
  this->stroke(path, Shading{std::move(style)}, std::move(stroke));
}

void GraphicsContext::stroke(const graphics::Path& path, const Shading& shading,
                             graphics::StrokeStyle stroke) {
  const Make coverage = filled(
      path,
      Shading{graphics::ShapeStyle{theme::Colour{0.0F, 0.0F, 0.0F, 1.0F}}},
      stroke, {});
  if (std::holds_alternative<Shading::Backdrop>(shading.value())) {
    const BlendMode held = blend_;
    blend_ = BlendMode::clear;
    record(coverage, coverage);
    blend_ = held;
    return;
  }
  record(filled(path, shading, std::move(stroke), {}), coverage);
}

auto GraphicsContext::pinned_by(composition::Element element,
                                composition::Point at,
                                const graphics::AffineTransform& transform,
                                composition::Size size)
    -> composition::Element {
  const composition::Point point = transform.apply(at);
  const graphics::AffineTransform linear{
      transform.a(), transform.b(), transform.c(), transform.d(), 0.0F, 0.0F};
  if (!linear.is_identity()) {
    element = std::move(element).transform_effect(linear);
  }
  return std::move(element)
      .offset(point.x, point.y)
      .frame({.width = size.width,
              .height = size.height,
              .alignment = composition::Alignment::top_leading});
}

auto GraphicsContext::pinned(composition::Element element,
                             composition::Point at) const
    -> composition::Element {
  return pinned_by(std::move(element), at, transform_, size_);
}

void GraphicsContext::draw(const ResolvedImage& image, composition::Rect rect) {
  const Make made = [make = image.make_, shading = image.shading, rect,
                     transform = transform_, size = size_] {
    composition::Element shown =
        make().resizable().frame({.width = rect.width, .height = rect.height});
    if (shading.has_value()) {
      shown = std::move(shown).foreground_style(*shading);
    }
    return pinned_by(std::move(shown),
                     composition::Point{.x = rect.x, .y = rect.y}, transform,
                     size);
  };
  record(made, made);
}

void GraphicsContext::draw(const ResolvedImage& image, composition::Point at,
                           motion::UnitPoint anchor) {
  const composition::Size extent = image.size();
  draw(image, composition::Rect{.x = at.x - (extent.width * anchor.x),
                                .y = at.y - (extent.height * anchor.y),
                                .width = extent.width,
                                .height = extent.height});
}

void GraphicsContext::draw(const ResolvedSymbol& symbol, composition::Point at,
                           motion::UnitPoint anchor) {
  const composition::Size extent = symbol.size();
  draw(symbol, composition::Rect{.x = at.x - (extent.width * anchor.x),
                                 .y = at.y - (extent.height * anchor.y),
                                 .width = extent.width,
                                 .height = extent.height});
}

void GraphicsContext::draw(const ResolvedSymbol& symbol,
                           composition::Rect rect) {
  if (!symbol.make_) {
    return;
  }
  const Make made = [make = symbol.make_, rect, transform = transform_,
                     size = size_] {
    return pinned_by(make().frame({.width = rect.width, .height = rect.height}),
                     composition::Point{.x = rect.x, .y = rect.y}, transform,
                     size);
  };
  record(made, made);
}

void GraphicsContext::draw(const ResolvedText& text, composition::Point at,
                           motion::UnitPoint anchor) {
  constexpr float unbounded = std::numeric_limits<float>::infinity();
  const std::optional<TextMetrics> known =
      text.metrics(composition::Size{.width = unbounded, .height = unbounded});
  const Make made = [make = text.make_, shading = text.shading, at, anchor,
                     known, transform = transform_, size = size_] {
    composition::Element shown = make();
    if (shading.has_value()) {
      shown = std::move(shown).foreground_style(*shading);
    }
    if (!known.has_value()) {
      return pinned_by(
          std::move(shown).fixed_size().frame(
              {.width = 0.0F, .height = 0.0F, .alignment = aligned(anchor)}),
          at, transform, size);
    }
    const composition::Size extent = known->size;
    return pinned_by(std::move(shown).fixed_size().frame(
                         {.width = extent.width,
                          .height = extent.height,
                          .alignment = composition::Alignment::top_leading}),
                     composition::Point{.x = at.x - (extent.width * anchor.x),
                                        .y = at.y - (extent.height * anchor.y)},
                     transform, size);
  };
  record(made, made);
}

void GraphicsContext::draw(const ResolvedText& text, composition::Rect rect) {
  const Make made = [make = text.make_, shading = text.shading, rect,
                     transform = transform_, size = size_] {
    composition::Element shown = make();
    if (shading.has_value()) {
      shown = std::move(shown).foreground_style(*shading);
    }
    return pinned_by(std::move(shown).frame(
                         {.width = rect.width,
                          .height = rect.height,
                          .alignment = composition::Alignment::top_leading}),
                     composition::Point{.x = rect.x, .y = rect.y}, transform,
                     size);
  };
  record(made, made);
}

void GraphicsContext::draw(Text text, composition::Point at,
                           motion::UnitPoint anchor) {
  draw(resolve(std::move(text)), at, anchor);
}

void GraphicsContext::draw(Text text, composition::Point at,
                           text::DrawingOptions options,
                           motion::UnitPoint anchor) {
  draw(std::move(text).drawing_options(options), at, anchor);
}

void GraphicsContext::draw(Text text, composition::Rect rect) {
  draw(resolve(std::move(text)), rect);
}

void GraphicsContext::draw(Image image, composition::Rect rect) {
  draw(resolve(std::move(image)), rect);
}

void GraphicsContext::draw(Image image, composition::Rect rect,
                           graphics::FillStyle style) {
  draw(resolve(std::move(image).antialiased(style.antialiased)), rect);
}

void GraphicsContext::draw(Image image, composition::Point at,
                           motion::UnitPoint anchor) {
  draw(resolve(std::move(image)), at, anchor);
}

void GraphicsContext::glyphs(Make glyphs) { record(glyphs, glyphs); }

void GraphicsContext::draw(const Text::Layout::Line& line,
                           text::DrawingOptions options) {
  if (line.size() == 0) {
    return;
  }
  glyphs([line, options, size = size_] {
    const composition::Size extent = Text::Layout{line[0].source()}.extent();
    return pinned_by(
        Text::Glyphs(line, options)
            .allows_hit_testing(false)
            .frame({.width = extent.width,
                    .height = extent.height,
                    .alignment = composition::Alignment::top_leading}),
        composition::Point{}, graphics::AffineTransform{}, size);
  });
}

void GraphicsContext::draw(const Text::Layout::Run& run,
                           text::DrawingOptions options) {
  glyphs([run, options, size = size_] {
    const composition::Size extent = Text::Layout{run.source()}.extent();
    return pinned_by(
        Text::Glyphs(run, options)
            .allows_hit_testing(false)
            .frame({.width = extent.width,
                    .height = extent.height,
                    .alignment = composition::Alignment::top_leading}),
        composition::Point{}, graphics::AffineTransform{}, size);
  });
}

void GraphicsContext::draw(const Text::Layout::RunSlice& slice,
                           text::DrawingOptions options) {
  glyphs([slice, options, size = size_] {
    const composition::Size extent =
        Text::Layout{slice.run().source()}.extent();
    return pinned_by(
        Text::Glyphs(slice, options)
            .allows_hit_testing(false)
            .frame({.width = extent.width,
                    .height = extent.height,
                    .alignment = composition::Alignment::top_leading}),
        composition::Point{}, graphics::AffineTransform{}, size);
  });
}

void GraphicsContext::draw_layer(
    const std::function<void(GraphicsContext&)>& content) {
  GraphicsContext nested{size_, resources_};
  nested.transform_ = transform_;
  content(nested);
  const Make made = [layers = std::move(nested.layers_), size = size_] {
    return stack(layers, size).compositing_group();
  };
  record(made, made);
}

void GraphicsContext::clip_to_layer(
    const std::function<void(GraphicsContext&)>& content, float opacity,
    ClipOptions options) {
  GraphicsContext nested{size_, resources_};
  nested.transform_ = transform_;
  content(nested);
  clips_.push_back(Clipping{.path = {},
                            .style = {},
                            .inverse = options.contains(ClipOptions::inverse),
                            .layer = std::make_shared<const std::vector<Make>>(
                                std::move(nested.layers_)),
                            .opacity = std::clamp(opacity, 0.0F, 1.0F)});
}

void GraphicsContext::translate_by(float x, float y) {
  transform_ = transform_.translated_by(x, y);
}

void GraphicsContext::scale_by(float x, float y) {
  transform_ = transform_.scaled_by(x, y);
}

void GraphicsContext::rotate(motion::Angle angle) {
  transform_ = transform_.rotated_by(angle);
}

void GraphicsContext::concatenate(const graphics::AffineTransform& transform) {
  transform_ = transform.concatenating(transform_);
}

void GraphicsContext::set_opacity(float opacity) noexcept {
  opacity_ = std::clamp(opacity, 0.0F, 1.0F);
}

void GraphicsContext::set_blend_mode(graphics::BlendMode mode) noexcept {
  blend_ = static_cast<BlendMode>(std::to_underlying(mode));
}

void GraphicsContext::clip(const graphics::Path& path) {
  clip(path, graphics::FillStyle{}, ClipOptions{});
}

void GraphicsContext::clip(const graphics::Path& path,
                           graphics::FillStyle style, ClipOptions options) {
  clips_.push_back(Clipping{.path = path.applying(transform_),
                            .style = style,
                            .inverse = options.contains(ClipOptions::inverse),
                            .layer = nullptr,
                            .opacity = 1.0F});
}

auto GraphicsContext::clip_bounding_rect() const -> composition::Rect {
  float left = 0.0F;
  float top = 0.0F;
  float right = size_.width;
  float bottom = size_.height;
  for (const Clipping& clipping : clips_) {
    if (clipping.inverse || clipping.layer != nullptr) {
      continue;
    }
    const composition::Rect bounds = clipping.path.bounding_rect();
    left = std::max(left, bounds.x);
    top = std::max(top, bounds.y);
    right = std::min(right, bounds.x + bounds.width);
    bottom = std::min(bottom, bounds.y + bounds.height);
  }
  if (right <= left || bottom <= top) {
    return composition::Rect{};
  }
  const float a = transform_.a();
  const float b = transform_.b();
  const float c = transform_.c();
  const float d = transform_.d();
  const float determinant = (a * d) - (b * c);
  if (determinant == 0.0F) {
    return composition::Rect{};
  }
  const auto back = [&](float x, float y) {
    const float dx = x - transform_.tx();
    const float dy = y - transform_.ty();
    return composition::Point{.x = ((d * dx) - (c * dy)) / determinant,
                              .y = ((a * dy) - (b * dx)) / determinant};
  };
  const std::array<composition::Point, 4> corners{
      back(left, top), back(right, top), back(right, bottom),
      back(left, bottom)};
  float low_x = std::numeric_limits<float>::max();
  float low_y = std::numeric_limits<float>::max();
  float high_x = std::numeric_limits<float>::lowest();
  float high_y = std::numeric_limits<float>::lowest();
  for (const composition::Point& corner : corners) {
    low_x = std::min(low_x, corner.x);
    low_y = std::min(low_y, corner.y);
    high_x = std::max(high_x, corner.x);
    high_y = std::max(high_y, corner.y);
  }
  return composition::Rect{.x = low_x,
                           .y = low_y,
                           .width = high_x - low_x,
                           .height = high_y - low_y};
}

void GraphicsContext::add_filter(Filter filter) {
  filters_.push_back(Filtering{.filter = std::move(filter), .linear = false});
}

void GraphicsContext::add_filter(Filter filter, FilterOptions options) {
  filters_.push_back(
      Filtering{.filter = std::move(filter),
                .linear = options.contains(FilterOptions::linear_color)});
}

auto GraphicsContext::stack(const std::vector<Make>& layers,
                            composition::Size size) -> composition::Element {
  std::vector<composition::Element> built;
  built.reserve(layers.size());
  for (const Make& layer : layers) {
    built.push_back(layer());
  }
  return ZStack(ZStackOptions{.alignment = composition::Alignment::top_leading},
                std::move(built))
      .frame({.width = size.width,
              .height = size.height,
              .alignment = composition::Alignment::top_leading});
}

auto GraphicsContext::filtered(const Make& raw,
                               const std::vector<Filtering>& filters,
                               std::size_t count) -> composition::Element {
  composition::Element element = raw();
  for (std::size_t index = 0; index < count && index < filters.size();
       ++index) {
    const std::function<composition::Element()> again =
        [&raw, &filters, index] { return filtered(raw, filters, index); };
    element = filters[index].filter.apply(std::move(element), again,
                                          filters[index].linear);
  }
  return element;
}

auto GraphicsContext::placed(Make raw, bool decorated) const -> Make {
  return [raw = std::move(raw),
          filters = decorated ? filters_ : std::vector<Filtering>{},
          opacity = decorated ? opacity_ : 1.0F, clips = clips_, size = size_] {
    composition::Element element = filtered(raw, filters, filters.size());
    if (opacity < 1.0F) {
      element = std::move(element).opacity(opacity);
    }
    for (const Clipping& clipping : clips) {
      composition::Element shape =
          clipping.layer != nullptr
              ? stack(*clipping.layer, size).opacity(clipping.opacity)
              : ShapeView(clipping.path)
                    .fill(theme::Colour{0.0F, 0.0F, 0.0F, 1.0F}, clipping.style)
                    .frame({.width = size.width, .height = size.height});
      element =
          clipping.inverse
              ? std::move(element).reverse_mask(
                    composition::Alignment::top_leading, std::move(shape))
              : std::move(element).mask(composition::Alignment::top_leading,
                                        std::move(shape));
    }
    return element;
  };
}

auto GraphicsContext::graphics_mode(BlendMode mode) noexcept
    -> std::optional<graphics::BlendMode> {
  if (std::to_underlying(mode) > std::to_underlying(BlendMode::plus_lighter)) {
    return std::nullopt;
  }
  return static_cast<graphics::BlendMode>(std::to_underlying(mode));
}

auto GraphicsContext::composed(BlendMode mode, const std::vector<Make>& below,
                               const Make& source, const Make& coverage,
                               composition::Size size) -> composition::Element {
  const auto layered = [size](std::vector<composition::Element> parts) {
    return ZStack(
               ZStackOptions{.alignment = composition::Alignment::top_leading},
               std::move(parts))
        .frame({.width = size.width,
                .height = size.height,
                .alignment = composition::Alignment::top_leading});
  };
  const auto under = [&below, size] { return stack(below, size); };
  const auto keep = [&under, &coverage] {
    return under().reverse_mask(composition::Alignment::top_leading,
                                coverage());
  };
  const auto inside = [&under](composition::Element element) {
    return std::move(element).mask(composition::Alignment::top_leading,
                                   under());
  };
  const auto outside = [&under](composition::Element element) {
    return std::move(element).reverse_mask(composition::Alignment::top_leading,
                                           under());
  };
  std::vector<composition::Element> parts;
  if (below.empty()) {
    switch (mode) {
      case BlendMode::copy:
      case BlendMode::source_out:
      case BlendMode::destination_atop:
      case BlendMode::exclusive_or:
      case BlendMode::destination_over:
        parts.push_back(source());
        break;
      default:
        break;
    }
    return layered(std::move(parts));
  }
  switch (mode) {
    case BlendMode::clear:
      parts.push_back(keep());
      break;
    case BlendMode::copy:
      parts.push_back(keep());
      parts.push_back(source());
      break;
    case BlendMode::source_in:
      parts.push_back(keep());
      parts.push_back(inside(source()));
      break;
    case BlendMode::source_out:
      parts.push_back(keep());
      parts.push_back(outside(source()));
      break;
    case BlendMode::destination_in:
      parts.push_back(keep());
      parts.push_back(
          under().mask(composition::Alignment::top_leading, source()));
      break;
    case BlendMode::destination_atop:
      parts.push_back(keep());
      parts.push_back(
          under().mask(composition::Alignment::top_leading, source()));
      parts.push_back(outside(source()));
      break;
    case BlendMode::exclusive_or:
      parts.push_back(
          under().reverse_mask(composition::Alignment::top_leading, source()));
      parts.push_back(outside(source()));
      break;
    case BlendMode::source_atop:
      parts.push_back(under());
      parts.push_back(inside(source()));
      break;
    case BlendMode::destination_over:
      parts.push_back(source());
      parts.push_back(under());
      break;
    case BlendMode::destination_out:
      parts.push_back(
          under().reverse_mask(composition::Alignment::top_leading, source()));
      break;
    default:
      parts.push_back(under());
      parts.push_back(source());
      break;
  }
  return layered(std::move(parts));
}

void GraphicsContext::record(const Make& raw, const Make& coverage) {
  ++drawn_;
  const Make source = placed(raw, true);
  const std::optional<graphics::BlendMode> mode = graphics_mode(blend_);
  const bool composes = !mode.has_value() ||
                        *mode == graphics::BlendMode::source_atop ||
                        *mode == graphics::BlendMode::destination_over ||
                        *mode == graphics::BlendMode::destination_out;
  if (composes) {
    const Make cover = placed(coverage, false);
    std::vector<Make> below = std::move(layers_);
    layers_ = std::vector<Make>{};
    layers_.push_back(
        [below = std::move(below), source, cover, blend = blend_,
         size = size_] { return composed(blend, below, source, cover, size); });
    return;
  }
  if (*mode == graphics::BlendMode::normal) {
    layers_.push_back(source);
    return;
  }
  layers_.push_back(
      [source, blend = *mode] { return source().blend_mode(blend); });
}

auto GraphicsContext::finish() && -> composition::Element {
  return stack(layers_, size_);
}

auto GraphicsContext::aligned(motion::UnitPoint anchor) noexcept
    -> composition::Alignment {
  const auto horizontal =
      anchor.x < 0.25F   ? composition::HorizontalAlignment::leading
      : anchor.x > 0.75F ? composition::HorizontalAlignment::trailing
                         : composition::HorizontalAlignment::center;
  const auto vertical = anchor.y < 0.25F ? composition::VerticalAlignment::top
                        : anchor.y > 0.75F
                            ? composition::VerticalAlignment::bottom
                            : composition::VerticalAlignment::center;
  return composition::Alignment{horizontal, vertical};
}

}
