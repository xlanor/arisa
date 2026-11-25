#pragma once

#include <arisa/components/image.hpp>
#include <arisa/components/image_library.hpp>
#include <arisa/components/mesh_gradient.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/text_layout.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/environment_values.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/composition/size.hpp>
#include <arisa/composition/tag_value.hpp>
#include <arisa/graphics/affine_transform.hpp>
#include <arisa/graphics/blend_mode.hpp>
#include <arisa/graphics/colour_matrix.hpp>
#include <arisa/graphics/gradient.hpp>
#include <arisa/graphics/image_paint.hpp>
#include <arisa/graphics/path.hpp>
#include <arisa/graphics/projection_transform.hpp>
#include <arisa/graphics/shader.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/graphics/stroke_style.hpp>
#include <arisa/motion/angle.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/support/option_set.hpp>
#include <arisa/text/locale.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/rgb_color_space.hpp>
#include <arisa/theme/theme.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

namespace arisa::components {

class GraphicsContext {
 public:
  enum class BlendMode : std::uint8_t {
    normal,
    multiply,
    screen,
    overlay,
    darken,
    lighten,
    color_dodge,
    color_burn,
    soft_light,
    hard_light,
    difference,
    exclusion,
    hue,
    saturation,
    color,
    luminosity,
    source_atop,
    destination_over,
    destination_out,
    plus_darker,
    plus_lighter,
    clear,
    copy,
    source_in,
    source_out,
    destination_in,
    destination_atop,
    exclusive_or,
  };

  class BlurOptions : public support::OptionSet<BlurOptions> {
   public:
    static const BlurOptions opaque;
    static const BlurOptions dithers_result;

    constexpr BlurOptions() noexcept = default;

    constexpr explicit BlurOptions(std::uint8_t bits) noexcept
        : OptionSet(bits) {}
  };

  class ClipOptions : public support::OptionSet<ClipOptions> {
   public:
    static const ClipOptions inverse;

    constexpr ClipOptions() noexcept = default;

    constexpr explicit ClipOptions(std::uint8_t bits) noexcept
        : OptionSet(bits) {}
  };

  class FilterOptions : public support::OptionSet<FilterOptions> {
   public:
    static const FilterOptions linear_color;

    constexpr FilterOptions() noexcept = default;

    constexpr explicit FilterOptions(std::uint8_t bits) noexcept
        : OptionSet(bits) {}
  };

  class GradientOptions : public support::OptionSet<GradientOptions> {
   public:
    static const GradientOptions repeat;
    static const GradientOptions mirror;
    static const GradientOptions linear_color;

    constexpr GradientOptions() noexcept = default;

    constexpr explicit GradientOptions(std::uint8_t bits) noexcept
        : OptionSet(bits) {}
  };

  class ShadowOptions : public support::OptionSet<ShadowOptions> {
   public:
    static const ShadowOptions shadow_above;
    static const ShadowOptions shadow_only;
    static const ShadowOptions inverts_alpha;
    static const ShadowOptions disables_group;

    constexpr ShadowOptions() noexcept = default;

    constexpr explicit ShadowOptions(std::uint8_t bits) noexcept
        : OptionSet(bits) {}
  };

  class Shading {
   public:
    struct Backdrop {
      friend constexpr auto operator==(const Backdrop&, const Backdrop&)
          -> bool = default;
    };

    struct Mesh {
      std::shared_ptr<const MeshGradient::Content> content;

      friend auto operator==(const Mesh& left, const Mesh& right) -> bool {
        return left.content == right.content ||
               (left.content != nullptr && right.content != nullptr &&
                *left.content == *right.content);
      }
    };

    struct ShaderFill {
      graphics::Shader shader;
      composition::Rect bounds;

      friend auto operator==(const ShaderFill&, const ShaderFill&)
          -> bool = default;
    };

    struct Palette {
      std::vector<graphics::ShapeStyle> layers;

      friend auto operator==(const Palette&, const Palette&) -> bool = default;
    };

    struct Ramp {
      enum class Kind : std::uint8_t { linear, radial, conic };

      Kind kind{Kind::linear};
      graphics::Gradient gradient;
      composition::Point start{};
      composition::Point end{};
      float start_radius{0.0F};
      float end_radius{0.0F};
      motion::Angle angle{};
      GradientOptions options{};

      friend auto operator==(const Ramp&, const Ramp&) -> bool = default;
    };

    using Value = std::variant<graphics::ShapeStyle, Backdrop, Mesh, ShaderFill,
                               Palette, Ramp>;

    Shading(graphics::ShapeStyle style) : value_(std::move(style)) {}

    static auto backdrop() -> Shading;

    static auto color(theme::RGBColorSpace space, float red, float green,
                      float blue, float opacity = 1.0F) -> Shading;

    static auto color(theme::RGBColorSpace space, float white,
                      float opacity = 1.0F) -> Shading;

    static auto palette(std::vector<graphics::ShapeStyle> layers) -> Shading;

    static auto mesh_gradient(const MeshGradient& mesh) -> Shading;

    static auto shader(graphics::Shader shader, composition::Rect bounds)
        -> Shading;

    static auto linear_gradient(graphics::Gradient gradient,
                                composition::Point start_point,
                                composition::Point end_point,
                                GradientOptions options = {}) -> Shading;

    static auto radial_gradient(graphics::Gradient gradient,
                                composition::Point center, float start_radius,
                                float end_radius, GradientOptions options = {})
        -> Shading;

    static auto conic_gradient(graphics::Gradient gradient,
                               composition::Point center,
                               motion::Angle angle = {},
                               GradientOptions options = {}) -> Shading;

    auto value() const noexcept -> const Value& { return value_; }

    auto style() const noexcept -> const graphics::ShapeStyle*;

    friend auto operator==(const Shading&, const Shading&) -> bool = default;

   private:
    explicit Shading(Value value) : value_(std::move(value)) {}

    Value value_;
  };

  class Filter {
   public:
    static auto blur(float radius, BlurOptions options = {}) -> Filter;

    static auto colour_matrix(graphics::ColourMatrix matrix,
                              bool is_premultiplied = false) -> Filter;

    static auto grayscale(double amount) -> Filter;

    static auto brightness(double amount) -> Filter;

    static auto saturation(double amount) -> Filter;

    static auto hue_rotation(motion::Angle angle) -> Filter;

    static auto color_multiply(theme::Colour colour) -> Filter;

    static auto alpha_multiply(theme::Colour colour) -> Filter;

    static auto alpha_threshold(float min, float max = 1.0F,
                                theme::Colour colour = theme::Colour{
                                    0.0F, 0.0F, 0.0F, 1.0F}) -> Filter;

    static auto color_shader(graphics::Shader shader) -> Filter;

    static auto distortion_shader(graphics::Shader shader,
                                  composition::Size max_sample_offset)
        -> Filter;

    static auto layer_shader(graphics::Shader shader,
                             composition::Size max_sample_offset) -> Filter;

    static auto projection_transform(graphics::ProjectionTransform matrix)
        -> Filter;

    static auto shadow(theme::Colour colour, float radius, float x = 0.0F,
                       float y = 0.0F, BlendMode blend_mode = BlendMode::normal,
                       ShadowOptions options = {}) -> Filter;

    auto apply(composition::Element element,
               const std::function<composition::Element()>& again,
               bool linear) const -> composition::Element;

    friend auto operator==(const Filter&, const Filter&) -> bool = default;

   private:
    struct Blur {
      float radius{0.0F};
      BlurOptions options{};

      friend constexpr auto operator==(const Blur&, const Blur&)
          -> bool = default;
    };

    struct Matrix {
      graphics::ColourMatrix matrix;
      bool premultiplied{false};

      friend constexpr auto operator==(const Matrix&, const Matrix&)
          -> bool = default;
    };

    struct Threshold {
      float min{0.0F};
      float max{1.0F};
      theme::Colour colour{};

      friend constexpr auto operator==(const Threshold&, const Threshold&)
          -> bool = default;
    };

    struct Effect {
      enum class Kind : std::uint8_t { colour, distortion, layer };

      graphics::Shader shader;
      composition::Size offset{};
      Kind kind{Kind::colour};

      friend auto operator==(const Effect&, const Effect&) -> bool = default;
    };

    struct Shadow {
      theme::Colour colour{0.0F, 0.0F, 0.0F, 0.33F};
      float radius{0.0F};
      float x{0.0F};
      float y{0.0F};
      BlendMode blend{BlendMode::normal};
      ShadowOptions options{};

      friend constexpr auto operator==(const Shadow&, const Shadow&)
          -> bool = default;
    };

    using Value = std::variant<Blur, Matrix, Threshold, Effect,
                               graphics::ProjectionTransform, Shadow>;

    explicit Filter(Value value) : value_(std::move(value)) {}

    static auto silhouette(composition::Element element, theme::Colour colour)
        -> composition::Element;

    static auto shadowed(const Shadow& shadow, composition::Element element,
                         const std::function<composition::Element()>& again)
        -> composition::Element;

    Value value_;
  };

  class ResolvedImage {
   public:
    auto size() const noexcept -> composition::Size { return size_; }

    auto baseline() const noexcept -> float { return baseline_; }

    std::optional<graphics::ShapeStyle> shading;

   private:
    friend class GraphicsContext;

    using Make = std::function<Image()>;

    ResolvedImage(Make make, composition::Size size, float baseline)
        : make_(std::move(make)), size_(size), baseline_(baseline) {}

    Make make_;
    composition::Size size_;
    float baseline_{0.0F};
  };

  class ResolvedSymbol {
   public:
    auto size() const noexcept -> composition::Size { return size_; }

   private:
    friend class GraphicsContext;

    using Make = std::function<composition::Element()>;

    ResolvedSymbol(Make make, composition::Size size)
        : make_(std::move(make)), size_(size) {}

    Make make_;
    composition::Size size_;
  };

  struct TextMetrics {
    composition::Size size{};
    float first_baseline{0.0F};
    float last_baseline{0.0F};

    friend constexpr auto operator==(const TextMetrics&, const TextMetrics&)
        -> bool = default;
  };

  struct TextMeasure {
    std::size_t text{0};
    composition::Size proposal{};
    TextMetrics metrics{};

    friend constexpr auto operator==(const TextMeasure&, const TextMeasure&)
        -> bool = default;
  };

  class TextLedger {
   public:
    struct Request {
      std::size_t text{0};
      composition::Size proposal{};
      std::function<Text()> make;
    };

    explicit TextLedger(std::vector<TextMeasure> known)
        : known_(std::move(known)) {}

    auto metrics(std::size_t text, composition::Size proposal,
                 const std::function<Text()>& make)
        -> std::optional<TextMetrics>;

    auto next() noexcept -> std::size_t { return resolved_++; }

    auto requests() const noexcept -> const std::vector<Request>& {
      return requests_;
    }

    auto known() const noexcept -> const std::vector<TextMeasure>& {
      return known_;
    }

   private:
    std::vector<TextMeasure> known_;
    std::vector<Request> requests_;
    std::size_t resolved_{0};
  };

  class ResolvedText {
   public:
    auto measure(composition::Size proposal) const -> composition::Size;

    auto first_baseline(composition::Size proposal) const -> float;

    auto last_baseline(composition::Size proposal) const -> float;

    std::optional<graphics::ShapeStyle> shading;

   private:
    friend class GraphicsContext;

    using Make = std::function<Text()>;

    ResolvedText(Make make, std::size_t index,
                 std::shared_ptr<TextLedger> ledger)
        : make_(std::move(make)), index_(index), ledger_(std::move(ledger)) {}

    auto metrics(composition::Size proposal) const
        -> std::optional<TextMetrics>;

    Make make_;
    std::size_t index_{0};
    std::shared_ptr<TextLedger> ledger_;
  };

  struct Symbol {
    std::shared_ptr<const composition::TagValue> tag;
    std::function<composition::Element()> make;
    composition::Size size;
  };

  struct Resources {
    std::optional<ImageLibrary> library{};
    std::shared_ptr<const theme::Theme> theme{};
    text::Locale locale{text::Locale::undetermined()};
    std::vector<Symbol> symbols{};
    std::shared_ptr<TextLedger> texts{};
    composition::EnvironmentValues environment{};
  };

  explicit GraphicsContext(composition::Size size);

  GraphicsContext(composition::Size size, Resources resources);

  auto environment() const noexcept -> const composition::EnvironmentValues& {
    return resources_.environment;
  }

  static auto tiled_image(std::shared_ptr<const provider::Image> image,
                          composition::Point origin = {},
                          graphics::ImagePaint::SourceRect source_rect = {},
                          float scale = 1.0F) -> graphics::ShapeStyle;

  auto resolve(Image image) const -> ResolvedImage;

  auto resolve(Text text) const -> ResolvedText;

  auto resolve(const Shading& shading) const -> Shading;

  template <std::equality_comparable T>
  auto resolve_symbol(const T& id) const -> std::optional<ResolvedSymbol> {
    for (const Symbol& symbol : resources_.symbols) {
      if (symbol.tag == nullptr) {
        continue;
      }
      if (const T* value = symbol.tag->get<T>();
          value != nullptr && *value == id) {
        return ResolvedSymbol{symbol.make, symbol.size};
      }
    }
    return std::nullopt;
  }

  void draw(const ResolvedImage& image, composition::Rect rect);

  void draw(const ResolvedImage& image, composition::Point at,
            motion::UnitPoint anchor = motion::UnitPoint::center);

  void draw(const ResolvedSymbol& symbol, composition::Point at,
            motion::UnitPoint anchor = motion::UnitPoint::center);

  void draw(const ResolvedSymbol& symbol, composition::Rect rect);

  void draw(const ResolvedText& text, composition::Point at,
            motion::UnitPoint anchor = motion::UnitPoint::center);

  void draw(const ResolvedText& text, composition::Rect rect);

  auto size() const noexcept -> composition::Size { return size_; }

  void fill(const graphics::Path& path, graphics::ShapeStyle style,
            graphics::FillStyle fill = {});

  void fill(const graphics::Path& path, const Shading& shading,
            graphics::FillStyle fill = {});

  void stroke(const graphics::Path& path, graphics::ShapeStyle style,
              float line_width = 1.0F);

  void stroke(const graphics::Path& path, graphics::ShapeStyle style,
              graphics::StrokeStyle stroke);

  void stroke(const graphics::Path& path, const Shading& shading,
              graphics::StrokeStyle stroke);

  void draw(Text text, composition::Point at,
            motion::UnitPoint anchor = motion::UnitPoint::center);

  void draw(Text text, composition::Point at, text::DrawingOptions options,
            motion::UnitPoint anchor = motion::UnitPoint::center);

  void draw(Text text, composition::Rect rect);

  void draw(Image image, composition::Rect rect);

  void draw(Image image, composition::Rect rect, graphics::FillStyle style);

  void draw(Image image, composition::Point at,
            motion::UnitPoint anchor = motion::UnitPoint::center);

  void draw(const Text::Layout::Line& line, text::DrawingOptions options = {});

  void draw(const Text::Layout::Run& run, text::DrawingOptions options = {});

  void draw(const Text::Layout::RunSlice& slice,
            text::DrawingOptions options = {});

  void draw_layer(const std::function<void(GraphicsContext&)>& content);

  void clip_to_layer(const std::function<void(GraphicsContext&)>& content,
                     float opacity = 1.0F, ClipOptions options = {});

  void translate_by(float x, float y);

  void scale_by(float x, float y);

  void rotate(motion::Angle angle);

  void concatenate(const graphics::AffineTransform& transform);

  auto transform() const noexcept -> const graphics::AffineTransform& {
    return transform_;
  }

  void set_transform(graphics::AffineTransform transform) noexcept {
    transform_ = transform;
  }

  auto opacity() const noexcept -> float { return opacity_; }

  void set_opacity(float opacity) noexcept;

  auto blend_mode() const noexcept -> BlendMode { return blend_; }

  void set_blend_mode(graphics::BlendMode mode) noexcept;

  void set_blend_mode(BlendMode mode) noexcept { blend_ = mode; }

  void clip(const graphics::Path& path);

  void clip(const graphics::Path& path, graphics::FillStyle style,
            ClipOptions options = {});

  auto clip_bounding_rect() const -> composition::Rect;

  void add_filter(Filter filter);

  void add_filter(Filter filter, FilterOptions options);

  auto drawn() const noexcept -> std::size_t { return drawn_; }

  auto finish() && -> composition::Element;

 private:
  using Make = std::function<composition::Element()>;

  struct Clipping {
    graphics::Path path;
    graphics::FillStyle style{};
    bool inverse{false};
    std::shared_ptr<const std::vector<Make>> layer{};
    float opacity{1.0F};
  };

  struct Filtering {
    Filter filter;
    bool linear{false};
  };

  void record(const Make& raw, const Make& coverage);

  auto placed(Make raw, bool decorated) const -> Make;

  static auto filtered(const Make& raw, const std::vector<Filtering>& filters,
                       std::size_t count) -> composition::Element;

  auto filled(const graphics::Path& path, const Shading& shading,
              std::optional<graphics::StrokeStyle> stroke,
              graphics::FillStyle fill) const -> Make;

  auto ramp(const Shading::Ramp& ramp) const -> graphics::ShapeStyle;

  auto pinned(composition::Element element, composition::Point at) const
      -> composition::Element;

  static auto pinned_by(composition::Element element, composition::Point at,
                        const graphics::AffineTransform& transform,
                        composition::Size size) -> composition::Element;

  static auto aligned(motion::UnitPoint anchor) noexcept
      -> composition::Alignment;

  static auto stack(const std::vector<Make>& layers, composition::Size size)
      -> composition::Element;

  static auto composed(BlendMode mode, const std::vector<Make>& below,
                       const Make& source, const Make& coverage,
                       composition::Size size) -> composition::Element;

  static auto graphics_mode(BlendMode mode) noexcept
      -> std::optional<graphics::BlendMode>;

  auto measured(const Image::Content& content) const -> composition::Size;

  auto unit(composition::Point point) const noexcept -> motion::UnitPoint;

  void glyphs(Make glyphs);

  composition::Size size_;
  Resources resources_;
  graphics::AffineTransform transform_;
  float opacity_{1.0F};
  BlendMode blend_{BlendMode::normal};
  std::vector<Clipping> clips_;
  std::vector<Filtering> filters_;
  std::vector<Make> layers_;
  std::size_t drawn_{0};
};

inline constexpr GraphicsContext::BlurOptions
    GraphicsContext::BlurOptions::opaque{1U};
inline constexpr GraphicsContext::BlurOptions
    GraphicsContext::BlurOptions::dithers_result{2U};
inline constexpr GraphicsContext::ClipOptions
    GraphicsContext::ClipOptions::inverse{1U};
inline constexpr GraphicsContext::FilterOptions
    GraphicsContext::FilterOptions::linear_color{1U};
inline constexpr GraphicsContext::GradientOptions
    GraphicsContext::GradientOptions::repeat{1U};
inline constexpr GraphicsContext::GradientOptions
    GraphicsContext::GradientOptions::mirror{2U};
inline constexpr GraphicsContext::GradientOptions
    GraphicsContext::GradientOptions::linear_color{4U};
inline constexpr GraphicsContext::ShadowOptions
    GraphicsContext::ShadowOptions::shadow_above{1U};
inline constexpr GraphicsContext::ShadowOptions
    GraphicsContext::ShadowOptions::shadow_only{2U};
inline constexpr GraphicsContext::ShadowOptions
    GraphicsContext::ShadowOptions::inverts_alpha{4U};
inline constexpr GraphicsContext::ShadowOptions
    GraphicsContext::ShadowOptions::disables_group{8U};

}
