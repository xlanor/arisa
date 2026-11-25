#pragma once

#include <arisa/components/image_library.hpp>
#include <arisa/composition/aspect_ratio.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/image_attributes.hpp>
#include <arisa/composition/size.hpp>
#include <arisa/graphics/color_rendering_mode.hpp>
#include <arisa/provider/gpu/image.hpp>
#include <arisa/provider/gpu/texture.hpp>
#include <arisa/support/strong_value.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::components {

class GraphicsContext;

class Image : public composition::Element {
 public:
  class Content;
  class Raster;
  class External;

  enum class ContentMode : std::uint8_t { stretch, fill, fit };

  enum class TemplateRenderingMode : std::uint8_t { original, template_ };

  enum class Interpolation : std::uint8_t { none, low, medium, high };

  using Scale = composition::ImageScale;

  using ResizingMode = composition::ImageResizingMode;

  using Orientation = composition::ImageOrientation;

  using DynamicRange = composition::ImageDynamicRange;

  using Renderer = std::function<void(GraphicsContext&)>;

  struct Rendering {
    TemplateRenderingMode mode{TemplateRenderingMode::original};
    Interpolation interpolation{Interpolation::high};
    bool antialiased{true};

    friend constexpr auto operator==(const Rendering&, const Rendering&)
        -> bool = default;
  };

  struct Slicing {
    std::array<float, 4> insets{};
    ResizingMode mode{ResizingMode::stretch};
    float scale{1.0F};

    friend constexpr auto operator==(const Slicing&, const Slicing&)
        -> bool = default;
  };

  struct SystemNameTag;
  using SystemName = support::StrongValue<std::string, SystemNameTag>;

  struct ResourceTag;
  using Resource = support::StrongValue<std::string, ResourceTag>;

  static constexpr std::string_view transfer_type = "public.image";

  static constexpr float placeholder_extent = 64.0F;
  static constexpr float missing_extent = 16.0F;
  static constexpr float dimmed_layer = 0.3F;
  static constexpr float glyph_centre = 0.4F;

  explicit Image(std::string name);

  explicit Image(SystemName name);

  explicit Image(std::shared_ptr<const provider::Image> pixels,
                 float scale = 1.0F);

  explicit Image(Resource name);

  Image(std::string name, ImageLibrary bundle);

  Image(std::string name, ImageLibrary bundle, std::string label);

  Image(std::shared_ptr<const provider::Image> pixels, float scale,
        Orientation orientation, std::string label);

  Image(SystemName name, double variable_value);

  Image(std::string name, double variable_value,
        std::optional<ImageLibrary> bundle = std::nullopt);

  Image(std::string name, double variable_value,
        std::optional<ImageLibrary> bundle, std::string label);

  Image(composition::Size size, std::optional<std::string> label, bool opaque,
        graphics::ColorRenderingMode color_mode, Renderer renderer);

  static auto system_name(std::string name) -> Image;

  static auto decorative(std::string name,
                         std::optional<ImageLibrary> bundle = std::nullopt)
      -> Image;

  static auto decorative(std::shared_ptr<const provider::Image> pixels,
                         float scale, Orientation orientation = Orientation::up)
      -> Image;

  static auto decorative(std::string name, double variable_value,
                         std::optional<ImageLibrary> bundle = std::nullopt)
      -> Image;

  auto resizable() && -> Image;

  auto resizable(theme::EdgeInsets cap_insets,
                 ResizingMode mode = ResizingMode::stretch) && -> Image;

  auto antialiased(bool is_antialiased) && -> Image;

  auto allowed_dynamic_range(std::optional<DynamicRange> range) && -> Image;

  auto encoded() const -> std::string;

  static auto decoded(std::string_view text) -> std::optional<Image>;

  static auto content_type() -> std::string;

  friend auto operator==(const Image& left, const Image& right) -> bool;

  auto font(theme::Font value) && -> Image;

  auto aspect_ratio(std::optional<float> ratio = std::nullopt,
                    composition::ContentMode mode =
                        composition::ContentMode::fit) && -> Image;

  auto scaled_to_fit() && -> Image;

  auto scaled_to_fill() && -> Image;

  auto rendering_mode(TemplateRenderingMode mode) && -> Image;

  auto interpolation(Interpolation value) && -> Image;

  auto duplicate() const -> Image;

  using Element::shadow;

  auto shadow(theme::ColourValue colour, float radius, float x = 0.0F,
              float y = 0.0F) && -> composition::Element;

 private:
  explicit Image(std::unique_ptr<Content> content);

  static auto placeholder(bool resizable) -> composition::Element;

  static auto symbol(const Content& content, composition::Context& context)
      -> composition::Element;

  static auto raster(const Content& content,
                     const std::shared_ptr<const provider::Image>& pixels,
                     composition::Context& context) -> composition::Element;

  static auto oriented(composition::Element shown, Orientation orientation,
                       composition::FrameSpec frame) -> composition::Element;

  static auto drawn(const Content& content) -> composition::Element;

  static auto layer(std::string glyph, theme::Font font,
                    std::optional<graphics::ShapeStyle> style, float opacity,
                    bool gradient) -> composition::Element;
};

class Image::Content final : public composition::ElementContent {
 public:
  Content(std::string name, bool system) noexcept
      : name_(std::move(name)), system_(system) {}

  Content(std::shared_ptr<const provider::Image> pixels, float scale) noexcept
      : pixels_(std::move(pixels)), scale_(scale) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto framed() const noexcept -> bool override { return true; }

  auto same_as(const composition::ElementContent& other) const -> bool override;

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

  auto name() const noexcept -> std::string_view { return name_; }

  auto accessibility() const -> composition::AccessibilityDefaults override;

  auto resizable() const noexcept -> bool { return resizable_; }

  auto system() const noexcept -> bool { return system_; }

  auto font() const noexcept -> const std::optional<theme::Font>& {
    return font_;
  }

  void set_resizable() noexcept { resizable_ = true; }

  void set_font(theme::Font value) noexcept { font_ = value; }

  auto aspect() const noexcept
      -> const std::optional<composition::AspectRatio>& {
    return aspect_;
  }

  void set_aspect(composition::AspectRatio aspect) noexcept {
    aspect_ = aspect;
  }

  auto pixels() const noexcept
      -> const std::shared_ptr<const provider::Image>& {
    return pixels_;
  }

  auto scale() const noexcept -> float { return scale_; }

  auto rendering() const noexcept -> const Rendering& { return rendering_; }

  void set_rendering(Rendering rendering) noexcept { rendering_ = rendering; }

  auto bundle() const noexcept -> const std::optional<ImageLibrary>& {
    return bundle_;
  }

  void set_bundle(ImageLibrary bundle) { bundle_ = std::move(bundle); }

  auto label() const noexcept -> const std::optional<std::string>& {
    return label_;
  }

  void set_label(std::string label) { label_ = std::move(label); }

  auto decorative() const noexcept -> bool { return decorative_; }

  void set_decorative() noexcept { decorative_ = true; }

  auto orientation() const noexcept -> Orientation { return orientation_; }

  void set_orientation(Orientation orientation) noexcept {
    orientation_ = orientation;
  }

  auto variable_value() const noexcept -> std::optional<double> {
    return variable_value_;
  }

  void set_variable_value(double value) noexcept { variable_value_ = value; }

  auto cap_insets() const noexcept -> const std::optional<theme::EdgeInsets>& {
    return cap_insets_;
  }

  auto resizing_mode() const noexcept -> ResizingMode { return resizing_mode_; }

  void set_slicing(theme::EdgeInsets insets, ResizingMode mode) noexcept {
    cap_insets_ = insets;
    resizing_mode_ = mode;
  }

  auto dynamic_range() const noexcept -> std::optional<DynamicRange> {
    return dynamic_range_;
  }

  void set_dynamic_range(std::optional<DynamicRange> range) noexcept {
    dynamic_range_ = range;
  }

  struct Drawing {
    composition::Size size;
    bool opaque{false};
    graphics::ColorRenderingMode color_mode{
        graphics::ColorRenderingMode::non_linear};
    Renderer renderer;
  };

  auto drawing() const noexcept -> const std::shared_ptr<const Drawing>& {
    return drawing_;
  }

  void set_drawing(std::shared_ptr<const Drawing> drawing) noexcept {
    drawing_ = std::move(drawing);
  }

 private:
  std::string name_;
  bool system_{false};
  bool resizable_{false};
  std::optional<theme::Font> font_;
  std::optional<composition::AspectRatio> aspect_;
  std::shared_ptr<const provider::Image> pixels_;
  float scale_{1.0F};
  Rendering rendering_{};
  std::optional<ImageLibrary> bundle_;
  std::optional<std::string> label_;
  bool decorative_{false};
  Orientation orientation_{Orientation::up};
  std::optional<double> variable_value_;
  std::optional<theme::EdgeInsets> cap_insets_;
  ResizingMode resizing_mode_{ResizingMode::stretch};
  std::optional<DynamicRange> dynamic_range_;
  std::shared_ptr<const Drawing> drawing_;
};

class Image::Raster : public composition::Element {
 public:
  class Content;

  explicit Raster(std::shared_ptr<const provider::Image> pixels,
                  ContentMode mode = ContentMode::stretch);

  Raster(std::shared_ptr<const provider::Image> pixels, ContentMode mode,
         Rendering rendering);

  Raster(std::shared_ptr<const provider::Image> pixels, Rendering rendering,
         Slicing slicing);
};

class Image::Raster::Content final : public composition::ElementContent {
 public:
  Content(std::shared_ptr<const provider::Image> pixels, ContentMode mode,
          Rendering rendering = {},
          std::optional<Slicing> slicing = std::nullopt) noexcept
      : pixels_(std::move(pixels)),
        mode_(mode),
        rendering_(rendering),
        slicing_(slicing) {}

  auto type() const noexcept -> composition::ElementType override;

  auto same_as(const composition::ElementContent& other) const -> bool override;

  auto paints_like(const composition::ElementContent& other) const
      -> bool override;

  auto pixels() const noexcept
      -> const std::shared_ptr<const provider::Image>& {
    return pixels_;
  }

  auto mode() const noexcept -> ContentMode { return mode_; }

  auto rendering() const noexcept -> const Rendering& { return rendering_; }

  auto slicing() const noexcept -> const std::optional<Slicing>& {
    return slicing_;
  }

 private:
  std::shared_ptr<const provider::Image> pixels_;
  ContentMode mode_{ContentMode::stretch};
  Rendering rendering_{};
  std::optional<Slicing> slicing_;
};

class Image::External : public composition::Element {
 public:
  class Content;

  explicit External(provider::TextureId texture);
};

class Image::External::Content final : public composition::ElementContent {
 public:
  explicit Content(provider::TextureId texture) noexcept : texture_(texture) {}

  auto type() const noexcept -> composition::ElementType override;

  auto same_as(const composition::ElementContent& other) const -> bool override;

  auto paints_like(const composition::ElementContent& other) const
      -> bool override;

  auto texture() const noexcept -> provider::TextureId { return texture_; }

 private:
  provider::TextureId texture_;
};

}
