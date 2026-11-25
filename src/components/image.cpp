#include <arisa/components/environment.hpp>
#include <arisa/components/geometry_reader.hpp>
#include <arisa/components/glyph.hpp>
#include <arisa/components/graphics_context.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/shape_view.hpp>
#include <arisa/components/system_symbols.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/geometry_proxy.hpp>
#include <arisa/composition/image_environment.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/graphics/colour_matrix.hpp>
#include <arisa/graphics/gradient.hpp>
#include <arisa/graphics/stroke_style.hpp>
#include <arisa/graphics/style_hierarchy.hpp>
#include <arisa/motion/angle.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/support/base64.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/stroke.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <format>
#include <memory>
#include <numbers>
#include <utility>
#include <vector>

namespace arisa::components {

Image::Image(std::string name)
    : Element(std::make_unique<Content>(std::move(name), false)) {}

Image::Image(SystemName name)
    : Element(std::make_unique<Content>(name.value(), true)) {}

Image::Image(std::shared_ptr<const provider::Image> pixels, float scale)
    : Element(std::make_unique<Content>(std::move(pixels),
                                        scale > 0.0F ? scale : 1.0F)) {}

Image::Image(Resource name)
    : Element(std::make_unique<Content>(name.value(), false)) {}

Image::Image(std::string name, ImageLibrary bundle) : Image(std::move(name)) {
  edit<Content>().set_bundle(std::move(bundle));
}

Image::Image(std::string name, ImageLibrary bundle, std::string label)
    : Image(std::move(name), std::move(bundle)) {
  edit<Content>().set_label(std::move(label));
}

Image::Image(std::shared_ptr<const provider::Image> pixels, float scale,
             Orientation orientation, std::string label)
    : Image(std::move(pixels), scale) {
  Content& content = edit<Content>();
  content.set_orientation(orientation);
  content.set_label(std::move(label));
}

Image::Image(SystemName name, double variable_value) : Image(std::move(name)) {
  edit<Content>().set_variable_value(variable_value);
}

Image::Image(std::string name, double variable_value,
             std::optional<ImageLibrary> bundle)
    : Image(std::move(name)) {
  Content& content = edit<Content>();
  content.set_variable_value(variable_value);
  if (bundle.has_value()) {
    content.set_bundle(std::move(*bundle));
  }
}

Image::Image(std::string name, double variable_value,
             std::optional<ImageLibrary> bundle, std::string label)
    : Image(std::move(name), variable_value, std::move(bundle)) {
  edit<Content>().set_label(std::move(label));
}

Image::Image(composition::Size size, std::optional<std::string> label,
             bool opaque, graphics::ColorRenderingMode color_mode,
             Renderer renderer)
    : Element(std::make_unique<Content>(std::string{}, false)) {
  Content& content = edit<Content>();
  content.set_drawing(std::make_shared<const Content::Drawing>(
      Content::Drawing{.size = size,
                       .opaque = opaque,
                       .color_mode = color_mode,
                       .renderer = std::move(renderer)}));
  if (label.has_value()) {
    content.set_label(std::move(*label));
  } else {
    content.set_decorative();
  }
}

Image::Image(std::unique_ptr<Content> content) : Element(std::move(content)) {}

auto Image::duplicate() const -> Image {
  Image made{std::make_unique<Content>(static_cast<const Content&>(content()))};
  made.edit_modifiers() = modifiers();
  return made;
}

auto Image::system_name(std::string name) -> Image {
  return Image{SystemName{std::move(name)}};
}

auto Image::decorative(std::string name, std::optional<ImageLibrary> bundle)
    -> Image {
  Image made{std::move(name)};
  Content& content = made.edit<Content>();
  content.set_decorative();
  if (bundle.has_value()) {
    content.set_bundle(std::move(*bundle));
  }
  return made;
}

auto Image::decorative(std::shared_ptr<const provider::Image> pixels,
                       float scale, Orientation orientation) -> Image {
  Image made{std::move(pixels), scale};
  Content& content = made.edit<Content>();
  content.set_orientation(orientation);
  content.set_decorative();
  return made;
}

auto Image::decorative(std::string name, double variable_value,
                       std::optional<ImageLibrary> bundle) -> Image {
  Image made{std::move(name), variable_value, std::move(bundle)};
  made.edit<Content>().set_decorative();
  return made;
}

auto Image::resizable(theme::EdgeInsets cap_insets,
                      ResizingMode mode) && -> Image {
  Content& content = edit<Content>();
  content.set_resizable();
  content.set_slicing(cap_insets, mode);
  return std::move(*this);
}

auto Image::antialiased(bool is_antialiased) && -> Image {
  Content& content = edit<Content>();
  Rendering rendering = content.rendering();
  rendering.antialiased = is_antialiased;
  content.set_rendering(rendering);
  return std::move(*this);
}

auto Image::allowed_dynamic_range(
    std::optional<DynamicRange> range) && -> Image {
  edit<Content>().set_dynamic_range(range);
  return std::move(*this);
}

auto Image::content_type() -> std::string { return std::string{transfer_type}; }

auto Image::encoded() const -> std::string {
  const auto& content = static_cast<const Content&>(this->content());
  if (content.system()) {
    return std::format("symbol:{}", content.name());
  }
  if (const std::shared_ptr<const provider::Image>& pixels = content.pixels();
      pixels != nullptr) {
    return std::format("rgba:{}:{}:{}:{}", pixels->width(), pixels->height(),
                       content.scale(),
                       support::Base64::encode(pixels->pixels()));
  }
  return std::format("asset:{}", content.name());
}

auto Image::decoded(std::string_view text) -> std::optional<Image> {
  const std::size_t colon = text.find(':');
  if (colon == std::string_view::npos) {
    return std::nullopt;
  }
  const std::string_view kind = text.substr(0, colon);
  const std::string_view rest = text.substr(colon + 1);
  if (kind == "symbol") {
    return Image::system_name(std::string{rest});
  }
  if (kind == "asset") {
    return Image{std::string{rest}};
  }
  if (kind != "rgba") {
    return std::nullopt;
  }
  std::vector<std::string_view> fields;
  std::size_t from = 0;
  for (int count = 0; count < 3; ++count) {
    const std::size_t next = rest.find(':', from);
    if (next == std::string_view::npos) {
      return std::nullopt;
    }
    fields.push_back(rest.substr(from, next - from));
    from = next + 1;
  }
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  float scale = 1.0F;
  const auto number = [](std::string_view field, auto& value) {
    const auto parsed =
        std::from_chars(field.data(), field.data() + field.size(), value);
    return parsed.ec == std::errc{} &&
           parsed.ptr == field.data() + field.size();
  };
  if (!number(fields[0], width) || !number(fields[1], height) ||
      !number(fields[2], scale)) {
    return std::nullopt;
  }
  std::optional<std::vector<std::byte>> bytes =
      support::Base64::decode(rest.substr(from));
  if (!bytes.has_value()) {
    return std::nullopt;
  }
  auto made = provider::Image::create(width, height, std::move(*bytes));
  if (!made.has_value()) {
    return std::nullopt;
  }
  return Image{std::make_shared<const provider::Image>(std::move(*made)),
               scale};
}

auto operator==(const Image& left, const Image& right) -> bool {
  return left.content().same_as(right.content());
}

auto Image::resizable() && -> Image {
  edit<Content>().set_resizable();
  return std::move(*this);
}

auto Image::shadow(theme::ColourValue colour, float radius, float x,
                   float y) && -> composition::Element {
  composition::Element cast =
      duplicate()
          .colour_matrix(graphics::ColourMatrix::of(
              {0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F,
               0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F}))
          .color_multiply(colour)
          .blur(radius)
          .offset(x, y)
          .allows_hit_testing(false)
          .accessibility_hidden(true);
  return ZStack(std::move(cast), composition::Element{std::move(*this)});
}

auto Image::font(theme::Font value) && -> Image {
  edit<Content>().set_font(value);
  return std::move(*this);
}

auto Image::aspect_ratio(std::optional<float> ratio,
                         composition::ContentMode mode) && -> Image {
  if (ratio.has_value() && !(*ratio > 0.0F)) {
    ratio.reset();
  }
  edit<Content>().set_aspect(
      composition::AspectRatio{.ratio = ratio, .mode = mode});
  return std::move(*this);
}

auto Image::scaled_to_fit() && -> Image {
  return std::move(*this).aspect_ratio(std::nullopt,
                                       composition::ContentMode::fit);
}

auto Image::scaled_to_fill() && -> Image {
  return std::move(*this).aspect_ratio(std::nullopt,
                                       composition::ContentMode::fill);
}

auto Image::rendering_mode(TemplateRenderingMode mode) && -> Image {
  Content& content = edit<Content>();
  Rendering rendering = content.rendering();
  rendering.mode = mode;
  content.set_rendering(rendering);
  return std::move(*this);
}

auto Image::interpolation(Interpolation value) && -> Image {
  Content& content = edit<Content>();
  Rendering rendering = content.rendering();
  rendering.interpolation = value;
  content.set_rendering(rendering);
  return std::move(*this);
}

auto Image::layer(std::string glyph, theme::Font font,
                  std::optional<graphics::ShapeStyle> style, float opacity,
                  bool gradient) -> composition::Element {
  composition::Element shown = Text(glyph).font(font);
  if (style.has_value()) {
    shown = std::move(shown).foreground_style(std::move(*style));
  }
  if (gradient) {
    shown = std::move(shown).overlay(
        ZStack()
            .frame({.max_width = composition::infinity,
                    .max_height = composition::infinity})
            .background(graphics::LinearGradient{
                .gradient = graphics::Gradient{std::vector<theme::ColourValue>{
                    theme::Colour{1.0F, 1.0F, 1.0F, 0.4F},
                    theme::Colour{1.0F, 1.0F, 1.0F, 0.0F}}},
                .start_point = motion::UnitPoint::top,
                .end_point = motion::UnitPoint::bottom})
            .mask(composition::Alignment::center,
                  Text(std::move(glyph)).font(font)));
  }
  if (opacity < 1.0F) {
    shown = std::move(shown).opacity(std::max(opacity, 0.0F));
  }
  return shown;
}

auto Image::symbol(const Content& content, composition::Context& context)
    -> composition::Element {
  const composition::ImageScale scale =
      context.environment(composition::ImageEnvironment::image_scale);
  const composition::SymbolVariants variants =
      context.environment(composition::ImageEnvironment::symbol_variants);
  const theme::Font font =
      content.font()
          .value_or(theme::Font::body)
          .scaled(composition::ImageEnvironment::factor(scale));
  std::optional<SystemSymbols::Symbol> resolved =
      SystemSymbols::resolve(content.name(), variants);
  if (!resolved.has_value()) {
    return composition::Element{
        ShapeView(theme::RoundedRectangle{3.0F})
            .stroke(theme::ColourRole::text_secondary,
                    graphics::StrokeStyle{.line_width = 1.0F,
                                          .dash = {2.0F, 2.0F}})}
        .frame({.width = missing_extent, .height = missing_extent})
        .allows_hit_testing(false);
  }
  const composition::SymbolRenderingMode mode =
      context.environment(composition::ImageEnvironment::symbol_rendering_mode);
  const bool gradient =
      context.environment(
          composition::ImageEnvironment::symbol_color_rendering_mode) ==
      composition::SymbolColorRenderingMode::gradient;
  const composition::SymbolVariableValueMode value_mode =
      context
          .environment(
              composition::ImageEnvironment::symbol_variable_value_mode)
          .value_or(composition::SymbolVariableValueMode::color);
  const std::optional<double> phase =
      context.environment(composition::ImageEnvironment::symbol_layer_phase);
  std::vector<composition::Element> layers;
  const std::vector<std::string>& series = resolved->series;
  const auto rung = [&series](double share) {
    const auto last = static_cast<double>(series.size() - 1);
    return static_cast<std::size_t>(
        std::clamp(std::round(std::clamp(share, 0.0, 1.0) * last), 0.0, last));
  };
  if (!series.empty() &&
      (content.variable_value().has_value() || phase.has_value())) {
    const std::size_t chosen =
        content.variable_value().has_value()
            ? rung(*content.variable_value())
            : std::min(static_cast<std::size_t>(
                           *phase * static_cast<double>(series.size())),
                       series.size() - 1);
    if (value_mode == composition::SymbolVariableValueMode::color ||
        phase.has_value()) {
      layers.push_back(
          layer(series.back(), font, std::nullopt, dimmed_layer, gradient));
    }
    layers.push_back(layer(series[chosen], font, std::nullopt, 1.0F, gradient));
  } else if (mode != composition::SymbolRenderingMode::monochrome &&
             !resolved->layers.empty()) {
    const std::size_t count = resolved->layers.size();
    for (std::size_t index = 0; index < count; ++index) {
      const SystemSymbols::Glyph& glyph = resolved->layers[index];
      std::optional<graphics::ShapeStyle> style;
      float opacity = 1.0F;
      if (mode == composition::SymbolRenderingMode::multicolor) {
        if (glyph.multicolour.has_value()) {
          style = graphics::ShapeStyle{*glyph.multicolour};
        }
      } else if (mode == composition::SymbolRenderingMode::palette) {
        style = graphics::ShapeStyle{glyph.tier};
      } else {
        opacity = graphics::StyleHierarchy::derived_opacity.at(
            static_cast<std::size_t>(glyph.tier));
      }
      if (phase.has_value() && count > 1) {
        const auto lit = std::min(
            static_cast<std::size_t>(*phase * static_cast<double>(count)),
            count - 1);
        opacity *= lit == index ? 1.0F : dimmed_layer;
      }
      composition::Element drawn =
          layer(glyph.text, font, std::move(style), opacity, gradient);
      if (glyph.scale != 1.0F) {
        drawn = std::move(drawn).scale_effect(
            glyph.scale, motion::UnitPoint{.x = 0.5F, .y = glyph_centre});
      }
      layers.push_back(std::move(drawn));
    }
  } else {
    float opacity = 1.0F;
    if (phase.has_value()) {
      const double dip = std::sin(std::numbers::pi * *phase);
      opacity = static_cast<float>(1.0 - (0.6 * dip * dip));
    }
    layers.push_back(layer(std::move(resolved->base), font, std::nullopt,
                           opacity, gradient));
  }
  if (layers.size() == 1) {
    return std::move(layers.front()).allows_hit_testing(false);
  }
  return ZStack(ZStackOptions{}, std::move(layers)).allows_hit_testing(false);
}

auto Image::placeholder(bool resizable) -> composition::Element {
  composition::FrameSpec frame{.width = placeholder_extent,
                               .height = placeholder_extent};
  if (resizable) {
    frame = composition::FrameSpec{.min_width = placeholder_extent * 0.5F,
                                   .max_width = composition::infinity,
                                   .min_height = placeholder_extent * 0.5F,
                                   .max_height = composition::infinity};
  }
  composition::Element shape = ZStack(Glyph::cross(
      theme::ColourRole::text_secondary, placeholder_extent * 0.4F));
  if (resizable) {
    shape = std::move(shape).flex_grow(1.0F);
  }
  return std::move(shape)
      .frame(frame)
      .background(theme::ColourRole::surface,
                  theme::Shape::rounded(theme::radius::control, true))
      .border(theme::ColourRole::separator, theme::Stroke::hairline());
}

auto Image::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto Image::Content::same_as(const composition::ElementContent& other) const
    -> bool {
  if (other.type() != type()) {
    return false;
  }
  const auto& theirs = static_cast<const Content&>(other);
  return name_ == theirs.name_ && system_ == theirs.system_ &&
         resizable_ == theirs.resizable_ && font_ == theirs.font_ &&
         aspect_ == theirs.aspect_ && pixels_ == theirs.pixels_ &&
         scale_ == theirs.scale_ && rendering_ == theirs.rendering_ &&
         bundle_ == theirs.bundle_ && label_ == theirs.label_ &&
         decorative_ == theirs.decorative_ &&
         orientation_ == theirs.orientation_ &&
         variable_value_ == theirs.variable_value_ &&
         cap_insets_ == theirs.cap_insets_ &&
         resizing_mode_ == theirs.resizing_mode_ &&
         dynamic_range_ == theirs.dynamic_range_ && drawing_ == theirs.drawing_;
}

auto Image::Content::accessibility() const
    -> composition::AccessibilityDefaults {
  if (decorative_) {
    return composition::AccessibilityDefaults{};
  }
  std::optional<std::string> spoken = label_;
  if (!spoken.has_value() && !name_.empty()) {
    spoken = name_;
  }
  return composition::AccessibilityDefaults{
      .label = std::move(spoken),
      .value = std::nullopt,
      .traits = composition::AccessibilityTraits{
          composition::AccessibilityTrait::is_image}};
}

auto Image::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  std::vector<composition::Element> produced;
  if (drawing_ != nullptr) {
    produced.push_back(drawn(*this));
    return produced;
  }
  if (system_) {
    produced.push_back(symbol(*this, context));
    return produced;
  }
  if (pixels_ != nullptr) {
    produced.push_back(raster(*this, pixels_, context));
    return produced;
  }
  const std::optional<ImageLibrary>& library =
      bundle_.has_value() ? bundle_
                          : context.environment(Environment::image_library);
  ImageLibrary::Loaded loaded = std::unexpected(
      provider::ProviderError{provider::ProviderErrorCode::not_found});
  if (library.has_value()) {
    loaded = library->image(name_);
  }
  if (!loaded.has_value()) {
    produced.push_back(placeholder(resizable_));
    return produced;
  }
  produced.push_back(raster(*this, *loaded, context));
  return produced;
}

auto Image::drawn(const Content& content) -> composition::Element {
  const std::shared_ptr<const Content::Drawing> drawing = content.drawing();
  const auto paint = [drawing](composition::Size extent) {
    GraphicsContext canvas{extent};
    if (drawing->size.width > 0.0F && drawing->size.height > 0.0F &&
        extent != drawing->size) {
      canvas.scale_by(extent.width / drawing->size.width,
                      extent.height / drawing->size.height);
    }
    if (drawing->renderer) {
      drawing->renderer(canvas);
    }
    composition::Element shown = std::move(canvas).finish();
    if (drawing->opaque) {
      shown = std::move(shown).drawing_group(true);
    }
    return shown;
  };
  if (content.resizable()) {
    return composition::Element{
        GeometryReader([paint](const composition::GeometryProxy& proxy) {
          return paint(proxy.size());
        })};
  }
  return paint(drawing->size)
      .frame({.width = drawing->size.width, .height = drawing->size.height});
}

auto Image::oriented(composition::Element shown, Orientation orientation,
                     composition::FrameSpec frame) -> composition::Element {
  const bool mirrored = orientation == Orientation::up_mirrored ||
                        orientation == Orientation::down_mirrored ||
                        orientation == Orientation::left_mirrored ||
                        orientation == Orientation::right_mirrored;
  double degrees = 0.0;
  switch (orientation) {
    case Orientation::up:
    case Orientation::up_mirrored:
      break;
    case Orientation::down:
    case Orientation::down_mirrored:
      degrees = 180.0;
      break;
    case Orientation::left:
    case Orientation::left_mirrored:
      degrees = -90.0;
      break;
    case Orientation::right:
    case Orientation::right_mirrored:
      degrees = 90.0;
      break;
  }
  const bool turned = degrees == 90.0 || degrees == -90.0;
  if (mirrored) {
    shown = std::move(shown).scale_effect(-1.0F, 1.0F);
  }
  if (degrees != 0.0) {
    shown = std::move(shown).rotation_effect(motion::Angle::degrees(degrees));
  }
  if (!turned) {
    return shown;
  }
  composition::FrameSpec swapped = frame;
  swapped.width = frame.height;
  swapped.height = frame.width;
  swapped.ideal_width = frame.ideal_height;
  swapped.ideal_height = frame.ideal_width;
  return std::move(shown).frame(swapped);
}

auto Image::raster(const Content& content,
                   const std::shared_ptr<const provider::Image>& pixels,
                   composition::Context& context) -> composition::Element {
  const float scale = content.scale();
  composition::FrameSpec frame{
      .width = static_cast<float>(pixels->width()) / scale,
      .height = static_cast<float>(pixels->height()) / scale};
  if (content.resizable()) {
    frame = composition::FrameSpec{.ideal_width = frame.width,
                                   .max_width = composition::infinity,
                                   .ideal_height = frame.height,
                                   .max_height = composition::infinity};
  }
  const bool sliced =
      content.resizable() && (content.cap_insets().has_value() ||
                              content.resizing_mode() == ResizingMode::tile);
  composition::Element shown = composition::Element{components::Group()};
  if (sliced) {
    theme::Edges edges{};
    if (content.cap_insets().has_value()) {
      edges = context.environment(Environment::theme)
                  .edges(*content.cap_insets())
                  .positive();
    }
    shown = Raster(pixels, content.rendering(),
                   Slicing{.insets = {edges.top, edges.leading, edges.bottom,
                                      edges.trailing},
                           .mode = content.resizing_mode(),
                           .scale = scale});
  } else {
    shown = Raster(pixels, ContentMode::stretch, content.rendering());
  }
  shown = std::move(shown).frame(frame);
  if (content.resizable()) {
    shown = std::move(shown).flex_grow(1.0F);
  }
  if (content.aspect().has_value() && pixels->height() > 0) {
    shown = std::move(shown).aspect_ratio(
        content.aspect()->ratio.value_or(static_cast<float>(pixels->width()) /
                                         static_cast<float>(pixels->height())),
        content.aspect()->mode);
  }
  if (content.orientation() != Orientation::up) {
    shown = oriented(std::move(shown), content.orientation(), frame);
  }
  return shown;
}

Image::Raster::Raster(std::shared_ptr<const provider::Image> pixels,
                      ContentMode mode)
    : Element(std::make_unique<Content>(std::move(pixels), mode)) {}

Image::Raster::Raster(std::shared_ptr<const provider::Image> pixels,
                      ContentMode mode, Rendering rendering)
    : Element(std::make_unique<Content>(std::move(pixels), mode, rendering)) {}

Image::Raster::Raster(std::shared_ptr<const provider::Image> pixels,
                      Rendering rendering, Slicing slicing)
    : Element(std::make_unique<Content>(std::move(pixels), ContentMode::stretch,
                                        rendering, slicing)) {}

auto Image::Raster::Content::same_as(
    const composition::ElementContent& other) const -> bool {
  return paints_like(other);
}

auto Image::Raster::Content::paints_like(
    const composition::ElementContent& other) const -> bool {
  if (other.type() != type()) {
    return false;
  }
  const auto& theirs = static_cast<const Content&>(other);
  return pixels_ == theirs.pixels_ && mode_ == theirs.mode_ &&
         rendering_ == theirs.rendering_ && slicing_ == theirs.slicing_;
}

auto Image::External::Content::same_as(
    const composition::ElementContent& other) const -> bool {
  return paints_like(other);
}

auto Image::External::Content::paints_like(
    const composition::ElementContent& other) const -> bool {
  return other.type() == type() &&
         static_cast<const Content&>(other).texture_ == texture_;
}

auto Image::Raster::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

Image::External::External(provider::TextureId texture)
    : Element(std::make_unique<Content>(texture)) {}

auto Image::External::Content::type() const noexcept
    -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

}
