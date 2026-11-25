#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/provider/gpu/colour.hpp>
#include <arisa/provider/gpu/draw_data.hpp>
#include <arisa/provider/gpu/image.hpp>
#include <arisa/provider/gpu/pixel_rows.hpp>
#include <arisa/provider/gpu/render_pass.hpp>
#include <arisa/provider/gpu/shape_vertex.hpp>
#include <arisa/provider/gpu/texture.hpp>
#include <arisa/provider/gpu/yuv_conversion.hpp>

#include <array>
#include <cstdint>
#include <expected>
#include <functional>
#include <span>
#include <vector>

namespace arisa::provider::headless {

class SoftwareRaster {
 public:
  using Rgba = std::array<float, 4>;

  class Canvas {
   public:
    Canvas(std::uint32_t width, std::uint32_t height);

    auto width() const noexcept -> std::uint32_t { return width_; }

    auto height() const noexcept -> std::uint32_t { return height_; }

    void fill(Colour colour);

    void fill(ClipRect area, Colour colour);

    void upload(TextureFormat format, TextureRegion region, PixelRows pixels);

    auto texel(std::int64_t x, std::int64_t y) const noexcept -> Rgba;

    auto sample(float u, float v) const noexcept -> Rgba;

    auto sample(float u, float v, SamplerFilter filter) const noexcept -> Rgba;

    void blend(std::uint32_t x, std::uint32_t y, const Rgba& source) noexcept;

    [[nodiscard]] auto image() const -> std::expected<Image, ProviderError>;

   private:
    void store(std::size_t at, TextureFormat format,
               std::span<const std::byte> pixel) noexcept;

    std::uint32_t width_;
    std::uint32_t height_;
    std::vector<std::uint8_t> bytes_;
  };

  struct Source {
    const Canvas* canvas{nullptr};
    SamplerFilter filter{SamplerFilter::linear};
  };

  using Lookup = std::function<Source(TextureId)>;

  static void draw(Canvas& target, const PassRegion& region,
                   std::span<const ShapeVertex> vertices,
                   std::span<const Index> indices,
                   std::span<const DrawCommand> commands, const Lookup& lookup);

 private:
  struct Varying {
    std::array<float, 2> uv{};
    std::array<float, 2> local{};
    Rgba colour{};
    std::array<float, 2> screen{};
  };

  struct Fragment {
    Varying varying;
    std::array<float, 2> uv_dx{};
    std::array<float, 2> uv_dy{};
    std::array<float, 2> coordinate{};
  };

  struct Samplers {
    const Canvas* atlas{nullptr};
    const Canvas* detail{nullptr};
    SamplerFilter filter{SamplerFilter::linear};
    SamplerFilter detail_filter{SamplerFilter::linear};
    std::array<float, 2> trim{DrawCommand::untrimmed};
    std::array<float, 2> translation{};
    std::array<float, 2> glyph_translation{};
    float opacity{1.0F};

    auto shifted(const ShapeVertex& vertex) const noexcept
        -> std::array<float, 2> {
      const std::array<float, 2>& shift =
          vertex.kind == ShapeKind::glyph ? glyph_translation : translation;
      return {vertex.position[0] + shift[0], vertex.position[1] + shift[1]};
    }
  };

  static void triangle(Canvas& target, const PassRegion& region,
                       const std::array<const ShapeVertex*, 3>& corners,
                       const ClipRect& scissor, const Samplers& samplers);

  static auto unpack(std::uint32_t colour) noexcept -> Rgba;

  static auto shade(const Fragment& fragment, const ShapeVertex& flat,
                    const Samplers& samplers) noexcept -> Rgba;

  static auto sample(const Samplers& samplers, float u, float v) noexcept
      -> Rgba;

  static auto yuv(std::array<float, 2> uv, const ShapeVertex& flat,
                  const Samplers& samplers) noexcept -> YuvConversion::Rgb;

  static auto size_of(const Canvas* canvas) noexcept -> std::array<float, 2>;

  static auto corner_length(float qx, float qy, float exponent) noexcept
      -> float;

  static auto rounded_box(std::array<float, 2> position,
                          std::array<float, 2> half_size, float radius,
                          float smoothing) noexcept -> float;

  static auto box_normal(std::array<float, 2> position,
                         std::array<float, 2> half_size, float radius,
                         float smoothing) noexcept -> std::array<float, 2>;

  static auto ellipse_distance(std::array<float, 2> position,
                               std::array<float, 2> half_size) noexcept
      -> float;

  static auto outline_distance(std::array<float, 2> position,
                               std::array<float, 2> half_size, float radius,
                               float smoothing,
                               const std::array<float, 4>& corners) noexcept
      -> float;

  static auto inset_corners(const std::array<float, 4>& corners,
                            float amount) noexcept -> std::array<float, 4>;

  static auto gradient_position(std::array<float, 2> position, float mode,
                                const std::array<float, 4>& points,
                                std::array<float, 2> extent) noexcept -> float;

  static auto ramped(const Canvas* canvas, float position) noexcept -> Rgba;

  static auto fringe(std::array<float, 2> uv, float multiplier) noexcept
      -> float;

  static auto ramp_position(const Varying& varying,
                            const ShapeVertex& flat) noexcept -> float;

  static auto trimmed(const Varying& varying, const ShapeVertex& flat,
                      std::array<float, 2> trim) noexcept -> float;

  static auto grain(std::array<float, 2> coordinate) noexcept -> float;

  static auto smoothstep(float edge0, float edge1, float value) noexcept
      -> float;

  static auto saturated(Rgba colour, float amount) noexcept -> Rgba;

  static auto backdrop_uv(const Fragment& fragment, const ShapeVertex& flat,
                          const Samplers& samplers,
                          std::array<float, 2> offset) noexcept
      -> std::array<float, 2>;

  struct Lens {
    float inside{0.0F};
    std::array<float, 2> normal{};
    float zone{0.0F};
    float amplitude{0.0F};
    std::array<float, 2> body{};
    float unit{1.0F};
    float rim{0.0F};
    float response{0.0F};
  };

  static constexpr float lens_zone = 18.0F;
  static constexpr float lens_share = 0.7F;
  static constexpr float lens_exponent = 2.2F;
  static constexpr float lens_body = 0.04F;
  static constexpr float lens_body_cap = 1.0F;
  static constexpr float lens_dispersion = 0.05F;
  static constexpr float rim_sharp = 0.45F;
  static constexpr float rim_soft = 2.5F;
  static constexpr float rim_sharp_weight = 0.66F;
  static constexpr float rim_soft_weight = 0.14F;
  static constexpr float rim_lower = 0.25F;
  static constexpr float chroma_span = 4.0F;
  static constexpr float chroma_bias = 0.5F;
  static constexpr float curve_steps = 1000.0F;

  static auto responded(Rgba colour, const Rgba& veil, float response) noexcept
      -> Rgba;

  static auto lens_zone_of(float limit, float unit) noexcept -> float;

  static auto lensed(const Fragment& fragment, const ShapeVertex& flat,
                     const Samplers& samplers, const Lens& lens) noexcept
      -> Rgba;

  static auto highlighted(Rgba colour, std::array<float, 2> position,
                          std::array<float, 2> centre, float strength,
                          float reach) noexcept -> Rgba;

  static auto table_value(const Canvas* canvas, std::int64_t index) noexcept
      -> float;

  static auto box_field(std::array<float, 2> position,
                        std::array<float, 2> half_size, float radius,
                        float smoothing) noexcept -> std::array<float, 3>;

  static auto neck_blend(float gap, float thickness, float rounding,
                         float spacing) noexcept -> float;

  static auto fusion(const Canvas* table, std::int64_t member,
                     float spacing) noexcept -> float;

  static auto union_field(const Canvas* table,
                          std::array<float, 2> position) noexcept
      -> std::array<float, 3>;

  using Rgb = std::array<float, 3>;

  static constexpr std::int64_t gaussian_reach = 12;
  static constexpr Rgb blend_luma{0.3F, 0.59F, 0.11F};

  static auto unpremultiplied(const Rgba& texel) noexcept -> Rgba;

  static auto srgb_decoded(float value) noexcept -> float;

  static auto srgb_encoded(float value) noexcept -> float;

  static auto matrix_filtered(const Canvas* table, const Rgba& colour) noexcept
      -> Rgba;

  static auto gaussian(const Varying& varying, const ShapeVertex& flat,
                       const Samplers& samplers) noexcept -> Rgba;

  static auto dodge(float backdrop, float source) noexcept -> float;

  static auto burn(float backdrop, float source) noexcept -> float;

  static auto hard(float backdrop, float source) noexcept -> float;

  static auto lum(const Rgb& colour) noexcept -> float;

  static auto clip_colour(Rgb colour) noexcept -> Rgb;

  static auto set_lum(const Rgb& colour, float level) noexcept -> Rgb;

  static auto sat(const Rgb& colour) noexcept -> float;

  static auto set_sat(const Rgb& colour, float level) noexcept -> Rgb;

  static auto separable(const Rgb& backdrop, const Rgb& source,
                        int mode) noexcept -> Rgb;

  static auto blended(const Rgba& source, const Rgba& backdrop,
                      int mode) noexcept -> Rgba;
};

}
