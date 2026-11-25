#include <arisa/provider/headless/software_raster.hpp>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <numbers>
#include <utility>

namespace arisa::provider::headless {

SoftwareRaster::Canvas::Canvas(std::uint32_t width, std::uint32_t height)
    : width_(width),
      height_(height),
      bytes_(static_cast<std::size_t>(width) * height * 4, 0) {}

void SoftwareRaster::Canvas::fill(Colour colour) {
  const auto byte = [](float value) {
    return static_cast<std::uint8_t>(
        std::lround(std::clamp(value, 0.0F, 1.0F) * 255.0F));
  };
  const std::array<std::uint8_t, 4> packed{byte(colour.red), byte(colour.green),
                                           byte(colour.blue),
                                           byte(colour.alpha)};
  for (std::size_t index = 0; index < bytes_.size(); index += 4) {
    std::ranges::copy(packed,
                      bytes_.begin() + static_cast<std::ptrdiff_t>(index));
  }
}

void SoftwareRaster::Canvas::fill(ClipRect area, Colour colour) {
  const auto byte = [](float value) {
    return static_cast<std::uint8_t>(
        std::lround(std::clamp(value, 0.0F, 1.0F) * 255.0F));
  };
  const std::array<std::uint8_t, 4> packed{byte(colour.red), byte(colour.green),
                                           byte(colour.blue),
                                           byte(colour.alpha)};
  const std::uint32_t left = std::min(area.x, width_);
  const std::uint32_t top = std::min(area.y, height_);
  const std::uint32_t right = std::min(area.x + area.width, width_);
  const std::uint32_t bottom = std::min(area.y + area.height, height_);
  for (std::uint32_t y = top; y < bottom; ++y) {
    for (std::uint32_t x = left; x < right; ++x) {
      const std::size_t index =
          ((static_cast<std::size_t>(y) * width_) + x) * 4;
      std::ranges::copy(packed,
                        bytes_.begin() + static_cast<std::ptrdiff_t>(index));
    }
  }
}

void SoftwareRaster::Canvas::upload(TextureFormat format, TextureRegion region,
                                    PixelRows pixels) {
  const std::size_t channels = Texture::bytes_per_pixel(format);
  const std::size_t row_bytes =
      static_cast<std::size_t>(region.width) * channels;
  for (std::uint32_t line = 0; line < region.height; ++line) {
    const std::span<const std::byte> source = pixels.row(line, row_bytes);
    for (std::uint32_t column = 0; column < region.width; ++column) {
      const std::size_t at =
          ((static_cast<std::size_t>(region.y + line) * width_) + region.x +
           column) *
          4;
      store(at, format, source.subspan(column * channels, channels));
    }
  }
}

void SoftwareRaster::Canvas::store(std::size_t at, TextureFormat format,
                                   std::span<const std::byte> pixel) noexcept {
  const auto byte = [&pixel](std::size_t index) {
    return std::to_integer<std::uint8_t>(pixel[index]);
  };
  switch (format) {
    case TextureFormat::alpha8:
      bytes_[at] = 255;
      bytes_[at + 1] = 255;
      bytes_[at + 2] = 255;
      bytes_[at + 3] = byte(0);
      return;
    case TextureFormat::rgba8:
      for (std::size_t channel = 0; channel < 4; ++channel) {
        bytes_[at + channel] = byte(channel);
      }
      return;
    case TextureFormat::bgra8:
      bytes_[at] = byte(2);
      bytes_[at + 1] = byte(1);
      bytes_[at + 2] = byte(0);
      bytes_[at + 3] = byte(3);
      return;
    case TextureFormat::rgb565: {
      const std::uint32_t packed = static_cast<std::uint32_t>(byte(0)) |
                                   (static_cast<std::uint32_t>(byte(1)) << 8U);
      const std::uint32_t red = (packed >> 11U) & 0x1FU;
      const std::uint32_t green = (packed >> 5U) & 0x3FU;
      const std::uint32_t blue = packed & 0x1FU;
      bytes_[at] = static_cast<std::uint8_t>((red << 3U) | (red >> 2U));
      bytes_[at + 1] = static_cast<std::uint8_t>((green << 2U) | (green >> 4U));
      bytes_[at + 2] = static_cast<std::uint8_t>((blue << 3U) | (blue >> 2U));
      bytes_[at + 3] = 255;
      return;
    }
    case TextureFormat::r8:
      bytes_[at] = byte(0);
      bytes_[at + 1] = 0;
      bytes_[at + 2] = 0;
      bytes_[at + 3] = 255;
      return;
    case TextureFormat::rg8:
      bytes_[at] = byte(0);
      bytes_[at + 1] = byte(1);
      bytes_[at + 2] = 0;
      bytes_[at + 3] = 255;
      return;
    case TextureFormat::nv12:
      return;
  }
}

auto SoftwareRaster::Canvas::texel(std::int64_t x,
                                   std::int64_t y) const noexcept -> Rgba {
  const std::int64_t cx =
      std::clamp<std::int64_t>(x, 0, static_cast<std::int64_t>(width_) - 1);
  const std::int64_t cy =
      std::clamp<std::int64_t>(y, 0, static_cast<std::int64_t>(height_) - 1);
  const std::size_t at =
      ((static_cast<std::size_t>(cy) * width_) + static_cast<std::size_t>(cx)) *
      4;
  return Rgba{static_cast<float>(bytes_[at]) / 255.0F,
              static_cast<float>(bytes_[at + 1]) / 255.0F,
              static_cast<float>(bytes_[at + 2]) / 255.0F,
              static_cast<float>(bytes_[at + 3]) / 255.0F};
}

auto SoftwareRaster::Canvas::sample(float u, float v) const noexcept -> Rgba {
  const float x = (u * static_cast<float>(width_)) - 0.5F;
  const float y = (v * static_cast<float>(height_)) - 0.5F;
  const float fx = std::floor(x);
  const float fy = std::floor(y);
  const float ax = x - fx;
  const float ay = y - fy;
  const auto ix = static_cast<std::int64_t>(fx);
  const auto iy = static_cast<std::int64_t>(fy);
  const Rgba a = texel(ix, iy);
  const Rgba b = texel(ix + 1, iy);
  const Rgba c = texel(ix, iy + 1);
  const Rgba d = texel(ix + 1, iy + 1);
  Rgba mixed{};
  for (std::size_t channel = 0; channel < 4; ++channel) {
    const float top = a[channel] + ((b[channel] - a[channel]) * ax);
    const float bottom = c[channel] + ((d[channel] - c[channel]) * ax);
    mixed[channel] = top + ((bottom - top) * ay);
  }
  return mixed;
}

auto SoftwareRaster::Canvas::sample(float u, float v,
                                    SamplerFilter filter) const noexcept
    -> Rgba {
  if (filter == SamplerFilter::linear) {
    return sample(u, v);
  }
  return texel(
      static_cast<std::int64_t>(std::floor(u * static_cast<float>(width_))),
      static_cast<std::int64_t>(std::floor(v * static_cast<float>(height_))));
}

void SoftwareRaster::Canvas::blend(std::uint32_t x, std::uint32_t y,
                                   const Rgba& source) noexcept {
  const std::size_t at = ((static_cast<std::size_t>(y) * width_) + x) * 4;
  const float alpha = std::clamp(source[3], 0.0F, 1.0F);
  if (alpha <= 0.0F) {
    return;
  }
  for (std::size_t channel = 0; channel < 3; ++channel) {
    const float existing = static_cast<float>(bytes_[at + channel]) / 255.0F;
    const float value = (std::clamp(source[channel], 0.0F, 1.0F) * alpha) +
                        (existing * (1.0F - alpha));
    bytes_[at + channel] = static_cast<std::uint8_t>(
        std::lround(std::clamp(value, 0.0F, 1.0F) * 255.0F));
  }
  const float existing = static_cast<float>(bytes_[at + 3]) / 255.0F;
  const float value = alpha + (existing * (1.0F - alpha));
  bytes_[at + 3] = static_cast<std::uint8_t>(
      std::lround(std::clamp(value, 0.0F, 1.0F) * 255.0F));
}

auto SoftwareRaster::Canvas::image() const
    -> std::expected<Image, ProviderError> {
  std::vector<std::byte> pixels(bytes_.size());
  std::ranges::transform(bytes_, pixels.begin(),
                         [](std::uint8_t value) { return std::byte{value}; });
  return Image::create(width_, height_, std::move(pixels));
}

void SoftwareRaster::draw(Canvas& target, const PassRegion& region,
                          std::span<const ShapeVertex> vertices,
                          std::span<const Index> indices,
                          std::span<const DrawCommand> commands,
                          const Lookup& lookup) {
  if (!(region.width > 0.0F) || !(region.height > 0.0F)) {
    return;
  }
  for (const DrawCommand& command : commands) {
    const Source atlas = lookup(command.texture);
    const Source detail =
        lookup(command.detail.value() != 0 ? command.detail : command.texture);
    const Samplers samplers{
        .atlas = atlas.canvas,
        .detail = detail.canvas,
        .filter = atlas.filter,
        .detail_filter = detail.filter,
        .trim = command.trim,
        .translation = command.translation_for(ShapeKind::fill),
        .glyph_translation = command.translation_for(ShapeKind::glyph),
        .opacity = command.opacity};
    const std::size_t end = std::min<std::size_t>(
        indices.size(),
        static_cast<std::size_t>(command.index_offset) + command.index_count);
    for (std::size_t index = command.index_offset; index + 2 < end;
         index += 3) {
      const std::array<std::size_t, 3> picked{
          static_cast<std::size_t>(indices[index]) + command.vertex_offset,
          static_cast<std::size_t>(indices[index + 1]) + command.vertex_offset,
          static_cast<std::size_t>(indices[index + 2]) + command.vertex_offset};
      if (std::ranges::any_of(picked, [&vertices](std::size_t at) {
            return at >= vertices.size();
          })) {
        continue;
      }
      triangle(
          target, region,
          {&vertices[picked[0]], &vertices[picked[1]], &vertices[picked[2]]},
          command.clip, samplers);
    }
  }
}

void SoftwareRaster::triangle(Canvas& target, const PassRegion& region,
                              const std::array<const ShapeVertex*, 3>& corners,
                              const ClipRect& scissor,
                              const Samplers& samplers) {
  const float sx = static_cast<float>(target.width()) / region.width;
  const float sy = static_cast<float>(target.height()) / region.height;
  std::array<std::array<float, 2>, 3> placed{};
  std::array<std::array<float, 2>, 3> points{};
  for (std::size_t corner = 0; corner < 3; ++corner) {
    placed[corner] = samplers.shifted(*corners[corner]);
    points[corner] = {(placed[corner][0] - region.x) * sx,
                      (placed[corner][1] - region.y) * sy};
  }
  const auto edge = [](std::array<float, 2> a, std::array<float, 2> b, float px,
                       float py) {
    return ((b[0] - a[0]) * (py - a[1])) - ((b[1] - a[1]) * (px - a[0]));
  };
  std::array<std::size_t, 3> order{0, 1, 2};
  float area = edge(points[0], points[1], points[2][0], points[2][1]);
  if (std::abs(area) < 1.0e-8F) {
    return;
  }
  if (area < 0.0F) {
    std::swap(order[1], order[2]);
    area = -area;
  }
  const std::array<std::array<float, 2>, 3> p{
      points[order[0]], points[order[1]], points[order[2]]};
  const std::array<const ShapeVertex*, 3> v{
      corners[order[0]], corners[order[1]], corners[order[2]]};
  const std::array<std::array<float, 2>, 3> screen{
      placed[order[0]], placed[order[1]], placed[order[2]]};
  const float clip_left = (static_cast<float>(scissor.x) - region.x) * sx;
  const float clip_top = (static_cast<float>(scissor.y) - region.y) * sy;
  const float clip_right =
      (static_cast<float>(scissor.x + scissor.width) - region.x) * sx;
  const float clip_bottom =
      (static_cast<float>(scissor.y + scissor.height) - region.y) * sy;
  const float min_x = std::max(
      {std::min({p[0][0], p[1][0], p[2][0]}), std::floor(clip_left), 0.0F});
  const float min_y = std::max(
      {std::min({p[0][1], p[1][1], p[2][1]}), std::floor(clip_top), 0.0F});
  const float max_x =
      std::min({std::max({p[0][0], p[1][0], p[2][0]}), std::ceil(clip_right),
                static_cast<float>(target.width())});
  const float max_y =
      std::min({std::max({p[0][1], p[1][1], p[2][1]}), std::ceil(clip_bottom),
                static_cast<float>(target.height())});
  if (max_x <= min_x || max_y <= min_y) {
    return;
  }
  const std::array<std::array<std::size_t, 2>, 3> edges{
      {{1, 2}, {2, 0}, {0, 1}}};
  std::array<std::array<float, 2>, 3> gradient{};
  std::array<bool, 3> owns{};
  for (std::size_t which = 0; which < 3; ++which) {
    const auto& a = p[edges[which][0]];
    const auto& b = p[edges[which][1]];
    gradient[which] = {-(b[1] - a[1]) / area, (b[0] - a[0]) / area};
    const float dy = b[1] - a[1];
    const float dx = b[0] - a[0];
    owns[which] = dy > 0.0F || (dy == 0.0F && dx < 0.0F);
  }
  const auto weighted = [&gradient](const std::array<float, 3>& values) {
    std::array<float, 2> slope{};
    for (std::size_t which = 0; which < 3; ++which) {
      slope[0] += values[which] * gradient[which][0];
      slope[1] += values[which] * gradient[which][1];
    }
    return slope;
  };
  const std::array<float, 2> du =
      weighted({v[0]->uv[0], v[1]->uv[0], v[2]->uv[0]});
  const std::array<float, 2> dv =
      weighted({v[0]->uv[1], v[1]->uv[1], v[2]->uv[1]});
  const std::array<Rgba, 3> colours{unpack(v[0]->colour), unpack(v[1]->colour),
                                    unpack(v[2]->colour)};
  const auto first_x =
      static_cast<std::uint32_t>(std::max(std::floor(min_x), 0.0F));
  const auto first_y =
      static_cast<std::uint32_t>(std::max(std::floor(min_y), 0.0F));
  const auto last_x = static_cast<std::uint32_t>(std::ceil(max_x));
  const auto last_y = static_cast<std::uint32_t>(std::ceil(max_y));
  for (std::uint32_t y = first_y; y < last_y && y < target.height(); ++y) {
    const float py = static_cast<float>(y) + 0.5F;
    if (py < clip_top || py >= clip_bottom) {
      continue;
    }
    for (std::uint32_t x = first_x; x < last_x && x < target.width(); ++x) {
      const float px = static_cast<float>(x) + 0.5F;
      if (px < clip_left || px >= clip_right) {
        continue;
      }
      std::array<float, 3> weights{};
      bool inside = true;
      for (std::size_t which = 0; which < 3; ++which) {
        const float w =
            edge(p[edges[which][0]], p[edges[which][1]], px, py) / area;
        if (w < 0.0F || (w == 0.0F && !owns[which])) {
          inside = false;
          break;
        }
        weights[which] = w;
      }
      if (!inside) {
        continue;
      }
      Fragment fragment;
      for (std::size_t which = 0; which < 3; ++which) {
        const float w = weights[which];
        for (std::size_t axis = 0; axis < 2; ++axis) {
          fragment.varying.uv[axis] += v[which]->uv[axis] * w;
          fragment.varying.local[axis] += v[which]->local[axis] * w;
          fragment.varying.screen[axis] += screen[which][axis] * w;
        }
        for (std::size_t channel = 0; channel < 4; ++channel) {
          fragment.varying.colour[channel] += colours[which][channel] * w;
        }
      }
      fragment.uv_dx = {du[0], dv[0]};
      fragment.uv_dy = {du[1], dv[1]};
      fragment.coordinate = {px, py};
      target.blend(x, y, shade(fragment, *corners[0], samplers));
    }
  }
}

auto SoftwareRaster::unpack(std::uint32_t colour) noexcept -> Rgba {
  return Rgba{static_cast<float>(colour & 0xffU) / 255.0F,
              static_cast<float>((colour >> 8U) & 0xffU) / 255.0F,
              static_cast<float>((colour >> 16U) & 0xffU) / 255.0F,
              static_cast<float>((colour >> 24U) & 0xffU) / 255.0F};
}

auto SoftwareRaster::sample(const Samplers& samplers, float u, float v) noexcept
    -> Rgba {
  if (samplers.atlas == nullptr) {
    return Rgba{1.0F, 1.0F, 1.0F, 1.0F};
  }
  return samplers.atlas->sample(u, v, samplers.filter);
}

auto SoftwareRaster::yuv(std::array<float, 2> uv, const ShapeVertex& flat,
                         const Samplers& samplers) noexcept
    -> YuvConversion::Rgb {
  if (samplers.atlas == nullptr || samplers.detail == nullptr) {
    return {1.0F, 1.0F, 1.0F};
  }
  const std::array<float, 2> luma_size = size_of(samplers.atlas);
  const std::array<float, 2> chroma_size = size_of(samplers.detail);
  const float chroma_u =
      ((uv[0] * luma_size[0] * 0.5F) + 0.25F) / chroma_size[0];
  const float chroma_v = (uv[1] * luma_size[1] * 0.5F) / chroma_size[1];
  const float luma = samplers.atlas->sample(uv[0], uv[1], samplers.filter)[0];
  const Rgba chroma =
      samplers.detail->sample(chroma_u, chroma_v, samplers.detail_filter);
  return YuvConversion::rgb(flat.effect, flat.detail, luma, chroma[0],
                            chroma[1]);
}

auto SoftwareRaster::size_of(const Canvas* canvas) noexcept
    -> std::array<float, 2> {
  if (canvas == nullptr) {
    return {1.0F, 1.0F};
  }
  return {static_cast<float>(canvas->width()),
          static_cast<float>(canvas->height())};
}

auto SoftwareRaster::corner_length(float qx, float qy, float exponent) noexcept
    -> float {
  const float ox = std::max(qx, 0.0F);
  const float oy = std::max(qy, 0.0F);
  if (exponent <= 2.0F) {
    return std::hypot(ox, oy);
  }
  const float largest = std::max(ox, oy);
  if (largest <= 0.0F) {
    return 0.0F;
  }
  const float nx = ox / largest;
  const float ny = oy / largest;
  return largest * std::pow(std::pow(nx, exponent) + std::pow(ny, exponent),
                            1.0F / exponent);
}

auto SoftwareRaster::rounded_box(std::array<float, 2> position,
                                 std::array<float, 2> half_size, float radius,
                                 float smoothing) noexcept -> float {
  const float limit = std::min(half_size[0], half_size[1]);
  const float extent = std::min(radius * (1.0F + (0.6F * smoothing)), limit);
  const float exponent = 2.0F + (3.0F * smoothing);
  const float qx = std::abs(position[0]) - half_size[0] + extent;
  const float qy = std::abs(position[1]) - half_size[1] + extent;
  return std::min(std::max(qx, qy), 0.0F) + corner_length(qx, qy, exponent) -
         extent;
}

auto SoftwareRaster::box_normal(std::array<float, 2> position,
                                std::array<float, 2> half_size, float radius,
                                float smoothing) noexcept
    -> std::array<float, 2> {
  constexpr float e = 0.5F;
  const float nx =
      rounded_box({position[0] + e, position[1]}, half_size, radius,
                  smoothing) -
      rounded_box({position[0] - e, position[1]}, half_size, radius, smoothing);
  const float ny =
      rounded_box({position[0], position[1] + e}, half_size, radius,
                  smoothing) -
      rounded_box({position[0], position[1] - e}, half_size, radius, smoothing);
  const float span = std::hypot(nx, ny);
  if (span <= 1.0e-5F) {
    return {0.0F, 0.0F};
  }
  return {nx / span, ny / span};
}

auto SoftwareRaster::ellipse_distance(std::array<float, 2> position,
                                      std::array<float, 2> half_size) noexcept
    -> float {
  const float rx = std::max(half_size[0], 1.0e-3F);
  const float ry = std::max(half_size[1], 1.0e-3F);
  const float stretched = std::hypot(position[0] / rx, position[1] / ry);
  const float slope =
      std::hypot(position[0] / (rx * rx), position[1] / (ry * ry));
  return slope > 1.0e-6F ? stretched * (stretched - 1.0F) / slope
                         : -std::min(rx, ry);
}

auto SoftwareRaster::outline_distance(
    std::array<float, 2> position, std::array<float, 2> half_size, float radius,
    float smoothing, const std::array<float, 4>& corners) noexcept -> float {
  if (corners[0] < 0.0F) {
    return ellipse_distance(position,
                            {std::max(half_size[0] - radius, 1.0e-3F),
                             std::max(half_size[1] - radius, 1.0e-3F)}) -
           radius;
  }
  if (corners != std::array<float, 4>{}) {
    const float chosen = position[0] < 0.0F
                             ? (position[1] < 0.0F ? corners[0] : corners[3])
                             : (position[1] < 0.0F ? corners[1] : corners[2]);
    return rounded_box(position, half_size, chosen, smoothing);
  }
  return rounded_box(position, half_size, radius, smoothing);
}

auto SoftwareRaster::inset_corners(const std::array<float, 4>& corners,
                                   float amount) noexcept
    -> std::array<float, 4> {
  if (corners[0] < 0.0F) {
    return corners;
  }
  return {
      std::max(corners[0] - amount, 0.0F), std::max(corners[1] - amount, 0.0F),
      std::max(corners[2] - amount, 0.0F), std::max(corners[3] - amount, 0.0F)};
}

auto SoftwareRaster::gradient_position(std::array<float, 2> position,
                                       float mode,
                                       const std::array<float, 4>& points,
                                       std::array<float, 2> extent) noexcept
    -> float {
  if (mode < 0.5F) {
    const float ax = points[2] - points[0];
    const float ay = points[3] - points[1];
    const float span = (ax * ax) + (ay * ay);
    return span > 1.0e-6F ? (((position[0] - points[0]) * ax) +
                             ((position[1] - points[1]) * ay)) /
                                span
                          : 0.0F;
  }
  const float dx = position[0] - points[0];
  const float dy = position[1] - points[1];
  if (mode < 1.5F) {
    return (std::hypot(dx, dy) - points[2]) /
           std::max(points[3] - points[2], 1.0e-4F);
  }
  if (mode < 2.5F) {
    constexpr float tau = 2.0F * std::numbers::pi_v<float>;
    const float span = points[3] - points[2];
    const float raw = std::atan2(dy, dx) - points[2];
    const float angle = raw - (tau * std::floor(raw / tau));
    return std::abs(span) < 1.0e-4F ? 0.0F : angle / span;
  }
  return (std::hypot(dx / std::max(extent[0], 1.0e-4F),
                     dy / std::max(extent[1], 1.0e-4F)) -
          points[2]) /
         std::max(points[3] - points[2], 1.0e-4F);
}

auto SoftwareRaster::ramped(const Canvas* canvas, float position) noexcept
    -> Rgba {
  if (canvas == nullptr) {
    return Rgba{1.0F, 1.0F, 1.0F, 1.0F};
  }
  const float u = ((std::clamp(position, 0.0F, 1.0F) * 255.0F) + 0.5F) / 256.0F;
  return canvas->sample(u, 0.5F);
}

auto SoftwareRaster::fringe(std::array<float, 2> uv, float multiplier) noexcept
    -> float {
  return std::min(1.0F, (1.0F - std::abs((uv[0] * 2.0F) - 1.0F)) * multiplier) *
         std::min(1.0F, uv[1]);
}

auto SoftwareRaster::ramp_position(const Varying& varying,
                                   const ShapeVertex& flat) noexcept -> float {
  if (flat.shape[1] < 0.5F) {
    return varying.local[0];
  }
  if (flat.shape[1] < 1.5F) {
    const float span = std::max(flat.half_size[1] - flat.half_size[0], 1.0e-4F);
    return (std::hypot(varying.local[0], varying.local[1]) -
            flat.half_size[0]) /
           span;
  }
  constexpr float tau = 2.0F * std::numbers::pi_v<float>;
  const float span = flat.half_size[1] - flat.half_size[0];
  const float raw =
      std::atan2(varying.local[1], varying.local[0]) - flat.half_size[0];
  const float angle = raw - (tau * std::floor(raw / tau));
  return std::abs(span) < 1.0e-4F ? 0.0F : angle / span;
}

auto SoftwareRaster::trimmed(const Varying& varying, const ShapeVertex& flat,
                             std::array<float, 2> trim) noexcept -> float {
  const float along = varying.local[1];
  const float first = std::max(flat.shape[2], trim[0]);
  const float last = std::min(flat.shape[3], trim[1]);
  if (along >= first && along <= last) {
    return 1.0F;
  }
  const float beyond = along < first ? first - along : along - last;
  if (flat.half_size[1] > 0.5F) {
    return 1.0F - smoothstep(0.0F, flat.half_size[0], beyond);
  }
  const float across = (varying.uv[0] - 0.5F) * flat.half_size[0];
  return std::clamp(
      (flat.half_size[0] * 0.5F) - std::hypot(beyond, across) + 0.5F, 0.0F,
      1.0F);
}

auto SoftwareRaster::grain(std::array<float, 2> coordinate) noexcept -> float {
  const float cx = std::floor(coordinate[0]);
  const float cy = std::floor(coordinate[1]);
  const float value = std::sin((cx * 12.9898F) + (cy * 78.233F)) * 43758.5453F;
  return value - std::floor(value) - 0.5F;
}

auto SoftwareRaster::smoothstep(float edge0, float edge1, float value) noexcept
    -> float {
  const float span = edge1 - edge0;
  const float t = span == 0.0F ? (value < edge0 ? 0.0F : 1.0F)
                               : std::clamp((value - edge0) / span, 0.0F, 1.0F);
  return t * t * (3.0F - (2.0F * t));
}

auto SoftwareRaster::saturated(Rgba colour, float amount) noexcept -> Rgba {
  const float grey =
      (colour[0] * 0.2126F) + (colour[1] * 0.7152F) + (colour[2] * 0.0722F);
  for (std::size_t channel = 0; channel < 3; ++channel) {
    colour[channel] =
        std::clamp(grey + ((colour[channel] - grey) * amount), 0.0F, 1.0F);
  }
  return colour;
}

auto SoftwareRaster::backdrop_uv(const Fragment& fragment,
                                 const ShapeVertex& flat,
                                 const Samplers& samplers,
                                 std::array<float, 2> offset) noexcept
    -> std::array<float, 2> {
  const std::array<float, 2> size = size_of(samplers.atlas);
  const float width = std::max(flat.effect[2], 1.0F);
  const float height = std::max(flat.effect[3], 1.0F);
  const float tx = 0.5F / size[0];
  const float ty = 0.5F / size[1];
  return {std::clamp(
              (fragment.varying.screen[0] + offset[0] - flat.effect[0]) / width,
              tx, 1.0F - tx),
          std::clamp((fragment.varying.screen[1] + offset[1] - flat.effect[1]) /
                         height,
                     ty, 1.0F - ty)};
}

auto SoftwareRaster::lens_zone_of(float limit, float unit) noexcept -> float {
  return std::min(lens_zone * unit, lens_share * limit);
}

auto SoftwareRaster::lensed(const Fragment& fragment, const ShapeVertex& flat,
                            const Samplers& samplers, const Lens& lens) noexcept
    -> Rgba {
  const float t =
      std::clamp(1.0F - (lens.inside / std::max(lens.zone, 1.0F)), 0.0F, 1.0F);
  const float push = lens.amplitude * std::pow(t, lens_exponent);
  const float reach =
      std::hypot(lens.body[0] * lens_body, lens.body[1] * lens_body);
  const float kept = lens_body * std::min(1.0F, lens_body_cap * lens.unit /
                                                    std::max(reach, 1.0e-4F));
  const std::array<float, 2> offset{
      (-lens.normal[0] * push) - (lens.body[0] * kept),
      (-lens.normal[1] * push) - (lens.body[1] * kept)};
  const std::array<float, 2> spread{offset[0] * lens_dispersion,
                                    offset[1] * lens_dispersion};
  const std::array<float, 2> red_uv = backdrop_uv(
      fragment, flat, samplers, {offset[0] + spread[0], offset[1] + spread[1]});
  const std::array<float, 2> green_uv =
      backdrop_uv(fragment, flat, samplers, offset);
  const std::array<float, 2> blue_uv = backdrop_uv(
      fragment, flat, samplers, {offset[0] - spread[0], offset[1] - spread[1]});
  Rgba colour{sample(samplers, red_uv[0], red_uv[1])[0],
              sample(samplers, green_uv[0], green_uv[1])[1],
              sample(samplers, blue_uv[0], blue_uv[1])[2], 1.0F};
  colour = responded(colour, fragment.varying.colour, lens.response);
  const float depth =
      std::max(lens.inside, 0.0F) / std::max(lens.unit, 1.0e-3F);
  const float glow =
      lens.rim * ((rim_sharp_weight * std::exp(-depth / rim_sharp)) +
                  (rim_soft_weight * std::exp(-depth / rim_soft)));
  const float facing = std::abs(lens.normal[1]) *
                       (1.0F + (rim_lower * std::max(lens.normal[1], 0.0F)));
  for (std::size_t channel = 0; channel < 3; ++channel) {
    colour[channel] =
        std::clamp(colour[channel] + (glow * facing * facing), 0.0F, 1.0F);
  }
  return colour;
}

auto SoftwareRaster::responded(Rgba colour, const Rgba& veil,
                               float response) noexcept -> Rgba {
  const float curve = std::floor(response / chroma_span) / curve_steps;
  const float chroma =
      (response - (chroma_span * std::floor(response / chroma_span))) -
      chroma_bias;
  const float level =
      (colour[0] * 0.2126F) + (colour[1] * 0.7152F) + (colour[2] * 0.0722F);
  const float tone = (veil[3] * level) - (curve * level * level);
  for (std::size_t channel = 0; channel < 3; ++channel) {
    colour[channel] =
        tone + (chroma * (colour[channel] - level)) + veil[channel];
  }
  return colour;
}

auto SoftwareRaster::highlighted(Rgba colour, std::array<float, 2> position,
                                 std::array<float, 2> centre, float strength,
                                 float reach) noexcept -> Rgba {
  if (strength <= 0.0F) {
    return colour;
  }
  const float span = std::max(reach, 1.0F);
  const float dx = (position[0] - centre[0]) / span;
  const float dy = (position[1] - centre[1]) / span;
  const float glow = std::exp(-((dx * dx) + (dy * dy)) * 2.0F) * strength;
  for (std::size_t channel = 0; channel < 3; ++channel) {
    colour[channel] = std::clamp(colour[channel] + (0.22F * glow), 0.0F, 1.0F);
  }
  return colour;
}

auto SoftwareRaster::table_value(const Canvas* canvas,
                                 std::int64_t index) noexcept -> float {
  if (canvas == nullptr) {
    return 0.0F;
  }
  const auto width = std::max<std::int64_t>(canvas->width(), 1);
  const Rgba texel = canvas->texel(index % width, index / width);
  std::uint32_t bits = 0;
  for (std::size_t channel = 0; channel < 4; ++channel) {
    const auto byte = static_cast<std::uint32_t>(
        std::lround(std::clamp(texel[channel], 0.0F, 1.0F) * 255.0F));
    bits |= byte << (8U * channel);
  }
  return std::bit_cast<float>(bits);
}

auto SoftwareRaster::box_field(std::array<float, 2> position,
                               std::array<float, 2> half_size, float radius,
                               float smoothing) noexcept
    -> std::array<float, 3> {
  const float limit = std::min(half_size[0], half_size[1]);
  const float extent = std::min(radius * (1.0F + (0.6F * smoothing)), limit);
  const float exponent = 2.0F + (3.0F * smoothing);
  const float qx = std::abs(position[0]) - half_size[0] + extent;
  const float qy = std::abs(position[1]) - half_size[1] + extent;
  const float ox = std::max(qx, 0.0F);
  const float oy = std::max(qy, 0.0F);
  const float sx = position[0] < 0.0F ? -1.0F : 1.0F;
  const float sy = position[1] < 0.0F ? -1.0F : 1.0F;
  const float largest = std::max(ox, oy);
  if (largest <= 0.0F) {
    return qx > qy ? std::array<float, 3>{qx - extent, sx, 0.0F}
                   : std::array<float, 3>{qy - extent, 0.0F, sy};
  }
  const float span = corner_length(qx, qy, exponent);
  return {span - extent, std::pow(ox / span, exponent - 1.0F) * sx,
          std::pow(oy / span, exponent - 1.0F) * sy};
}

auto SoftwareRaster::neck_blend(float gap, float thickness, float rounding,
                                float spacing) noexcept -> float {
  const float apart = std::max(gap, 0.0F);
  if (apart >= spacing) {
    return 0.0F;
  }
  const float waist = thickness * (1.0F - (apart / spacing));
  const float rise = std::max(waist - (thickness - rounding), 0.0F);
  const float reach = (0.5F * apart) + rounding;
  return 4.0F * (std::sqrt((reach * reach) + (rise * rise)) - rounding);
}

auto SoftwareRaster::fusion(const Canvas* table, std::int64_t member,
                            float spacing) noexcept -> float {
  const std::int64_t base = 2 + (member * 6);
  const std::array<float, 2> centre{table_value(table, base),
                                    table_value(table, base + 1)};
  const std::array<float, 2> half{table_value(table, base + 2),
                                  table_value(table, base + 3)};
  const float radius = table_value(table, base + 4);
  float nearest = 1.0e6F;
  float chosen = 0.0F;
  for (std::int64_t other = 0; other < member; ++other) {
    const std::int64_t at = 2 + (other * 6);
    const std::array<float, 2> middle{table_value(table, at),
                                      table_value(table, at + 1)};
    const std::array<float, 2> extent{table_value(table, at + 2),
                                      table_value(table, at + 3)};
    const float gap_x = std::abs(centre[0] - middle[0]) - (half[0] + extent[0]);
    const float gap_y = std::abs(centre[1] - middle[1]) - (half[1] + extent[1]);
    const float gap = std::max(gap_x, gap_y);
    if (gap >= nearest) {
      continue;
    }
    nearest = gap;
    const std::size_t cross = gap_x >= gap_y ? 1 : 0;
    const float thickness = std::min(half[cross], extent[cross]);
    const float rounding =
        std::min(std::min(radius, table_value(table, at + 4)), thickness);
    chosen = neck_blend(gap, thickness, rounding, spacing);
  }
  return chosen;
}

auto SoftwareRaster::union_field(const Canvas* table,
                                 std::array<float, 2> position) noexcept
    -> std::array<float, 3> {
  const auto count = static_cast<std::int64_t>(
      std::clamp(std::lround(table_value(table, 0)), 0L, 8L));
  const float spacing = std::max(table_value(table, 1), 1.0e-3F);
  std::array<float, 3> united{1.0e6F, 0.0F, 0.0F};
  for (std::int64_t member = 0; member < count; ++member) {
    const std::int64_t base = 2 + (member * 6);
    const std::array<float, 3> field =
        box_field({position[0] - table_value(table, base),
                   position[1] - table_value(table, base + 1)},
                  {table_value(table, base + 2), table_value(table, base + 3)},
                  table_value(table, base + 4), table_value(table, base + 5));
    if (member == 0) {
      united = field;
      continue;
    }
    const float blend = std::max(fusion(table, member, spacing), 1.0e-3F);
    const float h =
        std::clamp(0.5F + (0.5F * (field[0] - united[0]) / blend), 0.0F, 1.0F);
    united = {
        field[0] + ((united[0] - field[0]) * h) - (blend * h * (1.0F - h)),
        field[1] + ((united[1] - field[1]) * h),
        field[2] + ((united[2] - field[2]) * h)};
  }
  return united;
}

auto SoftwareRaster::shade(const Fragment& fragment, const ShapeVertex& flat,
                           const Samplers& samplers) noexcept -> Rgba {
  const Varying& varying = fragment.varying;
  const float radius = flat.shape[0];
  const float smoothing = flat.shape[1];
  const float stroke = flat.shape[2];
  const float softness = flat.shape[3];
  float coverage = 1.0F;
  Rgba colour = varying.colour;
  switch (flat.kind) {
    case ShapeKind::glyph:
      coverage = sample(samplers, varying.uv[0], varying.uv[1])[3];
      break;
    case ShapeKind::image: {
      const Rgba texel = sample(samplers, varying.uv[0], varying.uv[1]);
      for (std::size_t channel = 0; channel < 4; ++channel) {
        colour[channel] *= texel[channel];
      }
      if (radius > 0.0F || flat.detail != std::array<float, 4>{}) {
        coverage =
            std::clamp(0.5F - outline_distance(varying.local, flat.half_size,
                                               radius, smoothing, flat.detail),
                       0.0F, 1.0F);
      }
      break;
    }
    case ShapeKind::fill:
      coverage =
          std::clamp(0.5F - outline_distance(varying.local, flat.half_size,
                                             radius, smoothing, flat.detail),
                     0.0F, 1.0F);
      break;
    case ShapeKind::stroke: {
      const float distance = outline_distance(varying.local, flat.half_size,
                                              radius, smoothing, flat.detail);
      float inner = std::clamp(0.5F + distance + stroke, 0.0F, 1.0F);
      if (softness > 0.0F) {
        const float within =
            outline_distance(varying.local,
                             {std::max(flat.half_size[0] - stroke, 0.0F),
                              std::max(flat.half_size[1] - stroke, 0.0F)},
                             std::max(radius - stroke, 0.0F), smoothing,
                             inset_corners(flat.detail, stroke));
        inner = std::clamp(1.0F + (2.0F * within), 0.0F, 1.0F);
      }
      coverage = std::clamp(0.5F - distance, 0.0F, 1.0F) * inner;
      break;
    }
    case ShapeKind::shadow: {
      const float distance = outline_distance(varying.local, flat.half_size,
                                              radius, smoothing, flat.detail);
      coverage = 1.0F - smoothstep(-softness, softness, distance);
      break;
    }
    case ShapeKind::inner_shadow: {
      const float distance = outline_distance(varying.local, flat.half_size,
                                              radius, smoothing, flat.detail);
      const float shifted =
          outline_distance({varying.local[0] - flat.effect[0],
                            varying.local[1] - flat.effect[1]},
                           flat.half_size, radius, smoothing, flat.detail);
      coverage = std::clamp(0.5F - distance, 0.0F, 1.0F) *
                 smoothstep(-softness, softness, shifted);
      break;
    }
    case ShapeKind::gradient_fill: {
      const float distance = outline_distance(varying.local, flat.half_size,
                                              radius, smoothing, flat.detail);
      coverage = std::clamp(0.5F - distance, 0.0F, 1.0F);
      const Rgba texel = ramped(
          samplers.atlas,
          gradient_position(varying.local, stroke, flat.effect, varying.uv));
      for (std::size_t channel = 0; channel < 4; ++channel) {
        colour[channel] *= texel[channel];
      }
      break;
    }
    case ShapeKind::glyph_gradient: {
      coverage = sample(samplers, varying.uv[0], varying.uv[1])[3];
      const Rgba texel = ramped(
          samplers.detail, gradient_position(varying.local, stroke, flat.effect,
                                             flat.half_size));
      for (std::size_t channel = 0; channel < 4; ++channel) {
        colour[channel] *= texel[channel];
      }
      break;
    }
    case ShapeKind::path:
      coverage = fringe(varying.uv, flat.shape[0]) *
                 trimmed(varying, flat, samplers.trim);
      break;
    case ShapeKind::gradient: {
      coverage = fringe(varying.uv, flat.shape[0]) *
                 trimmed(varying, flat, samplers.trim);
      const float position =
          std::clamp(ramp_position(varying, flat), 0.0F, 1.0F);
      const Rgba texel =
          sample(samplers, ((position * 255.0F) + 0.5F) / 256.0F, 0.5F);
      for (std::size_t channel = 0; channel < 4; ++channel) {
        colour[channel] *= texel[channel];
      }
      break;
    }
    case ShapeKind::mesh: {
      const float noise = grain(fragment.coordinate) * flat.shape[0];
      for (std::size_t channel = 0; channel < 3; ++channel) {
        colour[channel] = std::clamp(colour[channel] + noise, 0.0F, 1.0F);
      }
      break;
    }
    case ShapeKind::blur_down: {
      const std::array<float, 2> size = size_of(samplers.atlas);
      const float hx = 1.0F / size[0];
      const float hy = 1.0F / size[1];
      const float u = varying.uv[0];
      const float v = varying.uv[1];
      Rgba sum{};
      const std::array<std::array<float, 3>, 5> taps{{{0.0F, 0.0F, 4.0F},
                                                      {-hx, -hy, 1.0F},
                                                      {hx, hy, 1.0F},
                                                      {hx, -hy, 1.0F},
                                                      {-hx, hy, 1.0F}}};
      for (const auto& tap : taps) {
        const Rgba texel = sample(samplers, u + tap[0], v + tap[1]);
        for (std::size_t channel = 0; channel < 4; ++channel) {
          sum[channel] += texel[channel] * tap[2];
        }
      }
      colour = {sum[0] / 8.0F, sum[1] / 8.0F, sum[2] / 8.0F, 1.0F};
      break;
    }
    case ShapeKind::blur_up: {
      const std::array<float, 2> size = size_of(samplers.atlas);
      const float hx = 0.5F / size[0];
      const float hy = 0.5F / size[1];
      const float u = varying.uv[0];
      const float v = varying.uv[1];
      Rgba sum{};
      const std::array<std::array<float, 3>, 8> taps{{{-hx * 2.0F, 0.0F, 1.0F},
                                                      {-hx, hy, 2.0F},
                                                      {0.0F, hy * 2.0F, 1.0F},
                                                      {hx, hy, 2.0F},
                                                      {hx * 2.0F, 0.0F, 1.0F},
                                                      {hx, -hy, 2.0F},
                                                      {0.0F, -hy * 2.0F, 1.0F},
                                                      {-hx, -hy, 2.0F}}};
      for (const auto& tap : taps) {
        const Rgba texel = sample(samplers, u + tap[0], v + tap[1]);
        for (std::size_t channel = 0; channel < 4; ++channel) {
          sum[channel] += texel[channel] * tap[2];
        }
      }
      colour = {sum[0] / 12.0F, sum[1] / 12.0F, sum[2] / 12.0F, 1.0F};
      break;
    }
    case ShapeKind::material: {
      const float distance =
          rounded_box(varying.local, flat.half_size, radius, smoothing);
      coverage = std::clamp(0.5F - distance, 0.0F, 1.0F) * flat.detail[1];
      const std::array<float, 2> uv =
          backdrop_uv(fragment, flat, samplers, {0.0F, 0.0F});
      Rgba seen = saturated(sample(samplers, uv[0], uv[1]), flat.shape[2]);
      const float noise = grain(fragment.coordinate) * flat.detail[0];
      for (std::size_t channel = 0; channel < 3; ++channel) {
        float value = std::clamp(seen[channel] + flat.shape[3], 0.0F, 1.0F);
        value += (varying.colour[channel] - value) * varying.colour[3];
        seen[channel] = std::clamp(value + noise, 0.0F, 1.0F);
      }
      colour = {seen[0], seen[1], seen[2], 1.0F};
      break;
    }
    case ShapeKind::glass: {
      const float distance =
          rounded_box(varying.local, flat.half_size, radius, smoothing);
      const std::array<float, 2> normal =
          box_normal(varying.local, flat.half_size, radius, smoothing);
      coverage = std::clamp(0.5F - distance, 0.0F, 1.0F) * flat.shape[3];
      const float limit = std::min(flat.half_size[0], flat.half_size[1]);
      Rgba seen = lensed(fragment, flat, samplers,
                         Lens{.inside = -distance,
                              .normal = normal,
                              .zone = lens_zone_of(limit, varying.uv[0]),
                              .amplitude = flat.shape[2] * flat.shape[3],
                              .body = varying.local,
                              .unit = varying.uv[0],
                              .rim = flat.detail[3] * flat.shape[3],
                              .response = varying.uv[1]});
      seen = highlighted(seen, varying.local, {flat.detail[0], flat.detail[1]},
                         flat.detail[2], limit * 1.6F);
      colour = {seen[0], seen[1], seen[2], 1.0F};
      break;
    }
    case ShapeKind::glass_path: {
      coverage = fringe(varying.uv, flat.shape[0]) *
                 trimmed(varying, flat, samplers.trim) * flat.detail[2];
      const float across = (varying.uv[0] * 2.0F) - 1.0F;
      const float gx = fragment.uv_dx[0];
      const float gy = fragment.uv_dy[0];
      const float span = std::hypot(gx, gy);
      const float sign = across > 0.0F ? 1.0F : (across < 0.0F ? -1.0F : 0.0F);
      const std::array<float, 2> normal =
          span > 1.0e-6F
              ? std::array<float, 2>{gx / span * sign, gy / span * sign}
              : std::array<float, 2>{0.0F, 0.0F};
      const float half_width = span > 1.0e-6F ? 0.5F / span : 1.0F;
      const float inside = (1.0F - std::abs(across)) * half_width;
      const float unit = flat.detail[3];
      Rgba seen = lensed(fragment, flat, samplers,
                         Lens{.inside = inside,
                              .normal = normal,
                              .zone = lens_zone * unit,
                              .amplitude = flat.detail[0] * flat.detail[2],
                              .body = {normal[0] * (half_width - inside),
                                       normal[1] * (half_width - inside)},
                              .unit = unit,
                              .rim = flat.detail[1] * flat.detail[2],
                              .response = flat.shape[1]});
      colour = {seen[0], seen[1], seen[2], 1.0F};
      break;
    }
    case ShapeKind::glass_union: {
      const std::array<float, 3> field =
          union_field(samplers.detail, varying.screen);
      const float slope = std::hypot(field[1], field[2]);
      const std::array<float, 2> normal =
          slope > 1.0e-5F
              ? std::array<float, 2>{field[1] / slope, field[2] / slope}
              : std::array<float, 2>{0.0F, 0.0F};
      const float edge = field[0] / std::max(slope, 0.25F);
      coverage = std::clamp(0.5F - edge, 0.0F, 1.0F) * flat.shape[3];
      const float limit = std::max(flat.shape[1], 8.0F) * 0.625F;
      Rgba seen = lensed(fragment, flat, samplers,
                         Lens{.inside = -field[0],
                              .normal = normal,
                              .zone = lens_zone_of(limit, varying.uv[0]),
                              .amplitude = flat.shape[2] * flat.shape[3],
                              .body = {0.0F, 0.0F},
                              .unit = varying.uv[0],
                              .rim = flat.detail[3] * flat.shape[3],
                              .response = varying.uv[1]});
      seen = highlighted(seen, varying.local, {flat.detail[0], flat.detail[1]},
                         flat.detail[2], limit * 1.6F);
      colour = {seen[0], seen[1], seen[2], 1.0F};
      break;
    }
    case ShapeKind::yuv: {
      const YuvConversion::Rgb rgb = yuv(varying.uv, flat, samplers);
      for (std::size_t channel = 0; channel < 3; ++channel) {
        colour[channel] *= rgb[channel];
      }
      break;
    }
    case ShapeKind::layer: {
      const Rgba texel = sample(samplers, varying.uv[0], varying.uv[1]);
      if (texel[3] > 1.0e-4F) {
        colour = {texel[0] / texel[3], texel[1] / texel[3], texel[2] / texel[3],
                  texel[3] * varying.colour[3]};
      } else {
        colour = {0.0F, 0.0F, 0.0F, 0.0F};
      }
      break;
    }
    case ShapeKind::filter: {
      Rgba seen =
          unpremultiplied(sample(samplers, varying.uv[0], varying.uv[1]));
      if (flat.shape[0] > 0.5F) {
        if (flat.shape[3] > 0.5F) {
          for (std::size_t channel = 0; channel < 3; ++channel) {
            seen[channel] = srgb_decoded(seen[channel]);
          }
        }
        if (flat.shape[2] > 0.5F) {
          for (std::size_t channel = 0; channel < 3; ++channel) {
            seen[channel] *= seen[3];
          }
        }
        seen = matrix_filtered(samplers.detail, seen);
        if (flat.shape[2] > 0.5F) {
          if (seen[3] > 1.0e-4F) {
            for (std::size_t channel = 0; channel < 3; ++channel) {
              seen[channel] /= seen[3];
            }
          } else {
            seen = Rgba{0.0F, 0.0F, 0.0F, 0.0F};
          }
        }
        if (flat.shape[3] > 0.5F) {
          for (std::size_t channel = 0; channel < 3; ++channel) {
            seen[channel] = srgb_encoded(std::max(seen[channel], 0.0F));
          }
        }
      }
      if (flat.shape[1] > 0.5F) {
        seen[3] = 1.0F;
      }
      colour = {seen[0], seen[1], seen[2], seen[3] * varying.colour[3]};
      break;
    }
    case ShapeKind::blend: {
      const Rgba seen =
          unpremultiplied(sample(samplers, varying.uv[0], varying.uv[1]));
      Rgba under{0.0F, 0.0F, 0.0F, 1.0F};
      if (samplers.detail != nullptr) {
        const std::array<float, 2> size = size_of(samplers.detail);
        const float width = std::max(flat.effect[2], 1.0F);
        const float height = std::max(flat.effect[3], 1.0F);
        const float u = std::clamp((varying.screen[0] - flat.effect[0]) / width,
                                   0.5F / size[0], 1.0F - (0.5F / size[0]));
        const float v =
            std::clamp((varying.screen[1] - flat.effect[1]) / height,
                       0.5F / size[1], 1.0F - (0.5F / size[1]));
        under = samplers.detail->sample(u, v, samplers.detail_filter);
      }
      const Rgba result =
          blended(seen, under, static_cast<int>(std::lround(flat.shape[0])));
      colour = {result[0], result[1], result[2], result[3] * varying.colour[3]};
      break;
    }
    case ShapeKind::mask: {
      const Rgba seen =
          unpremultiplied(sample(samplers, varying.uv[0], varying.uv[1]));
      const float sampled =
          samplers.detail != nullptr
              ? samplers.detail->sample(varying.uv[0], varying.uv[1],
                                        samplers.detail_filter)[3]
              : 1.0F;
      const float covered = flat.shape[0] > 0.5F ? 1.0F - sampled : sampled;
      colour = {seen[0], seen[1], seen[2],
                seen[3] * covered * varying.colour[3]};
      break;
    }
    case ShapeKind::gaussian:
      colour = gaussian(varying, flat, samplers);
      break;
    case ShapeKind::shader:
      colour = unpremultiplied(sample(samplers, varying.uv[0], varying.uv[1]));
      break;
  }
  return Rgba{colour[0], colour[1], colour[2],
              colour[3] * coverage * samplers.opacity};
}

auto SoftwareRaster::unpremultiplied(const Rgba& texel) noexcept -> Rgba {
  if (texel[3] <= 1.0e-4F) {
    return Rgba{0.0F, 0.0F, 0.0F, 0.0F};
  }
  return Rgba{texel[0] / texel[3], texel[1] / texel[3], texel[2] / texel[3],
              texel[3]};
}

auto SoftwareRaster::srgb_decoded(float value) noexcept -> float {
  return value <= 0.04045F ? value / 12.92F
                           : std::pow((value + 0.055F) / 1.055F, 2.4F);
}

auto SoftwareRaster::srgb_encoded(float value) noexcept -> float {
  return value <= 0.0031308F ? value * 12.92F
                             : (1.055F * std::pow(value, 1.0F / 2.4F)) - 0.055F;
}

auto SoftwareRaster::matrix_filtered(const Canvas* table,
                                     const Rgba& colour) noexcept -> Rgba {
  Rgba out{};
  for (std::int64_t row = 0; row < 4; ++row) {
    const std::int64_t at = row * 5;
    float sum = table_value(table, at + 4);
    for (std::int64_t column = 0; column < 4; ++column) {
      sum += table_value(table, at + column) *
             colour[static_cast<std::size_t>(column)];
    }
    out[static_cast<std::size_t>(row)] = std::clamp(sum, 0.0F, 1.0F);
  }
  return out;
}

auto SoftwareRaster::gaussian(const Varying& varying, const ShapeVertex& flat,
                              const Samplers& samplers) noexcept -> Rgba {
  const std::array<float, 2> size = size_of(samplers.atlas);
  const float sigma = std::max(flat.shape[0], 1.0e-3F);
  const auto reach = static_cast<std::int64_t>(
      std::min(std::ceil(sigma * 3.0F), static_cast<float>(gaussian_reach)));
  Rgba sum{};
  float total = 0.0F;
  for (std::int64_t index = -reach; index <= reach; ++index) {
    const auto offset = static_cast<float>(index);
    const float weight = std::exp(-0.5F * offset * offset / (sigma * sigma));
    const float u =
        std::clamp(varying.uv[0] + (flat.shape[1] * offset / size[0]),
                   flat.effect[0], flat.effect[2]);
    const float v =
        std::clamp(varying.uv[1] + (flat.shape[2] * offset / size[1]),
                   flat.effect[1], flat.effect[3]);
    const Rgba texel = sample(samplers, u, v);
    for (std::size_t channel = 0; channel < 4; ++channel) {
      sum[channel] += texel[channel] * weight;
    }
    total += weight;
  }
  for (float& channel : sum) {
    channel /= std::max(total, 1.0e-6F);
  }
  return unpremultiplied(sum);
}

auto SoftwareRaster::dodge(float backdrop, float source) noexcept -> float {
  if (backdrop <= 0.0F) {
    return 0.0F;
  }
  if (source >= 1.0F) {
    return 1.0F;
  }
  return std::min(1.0F, backdrop / (1.0F - source));
}

auto SoftwareRaster::burn(float backdrop, float source) noexcept -> float {
  if (backdrop >= 1.0F) {
    return 1.0F;
  }
  if (source <= 0.0F) {
    return 0.0F;
  }
  return 1.0F - std::min(1.0F, (1.0F - backdrop) / source);
}

auto SoftwareRaster::hard(float backdrop, float source) noexcept -> float {
  if (source <= 0.5F) {
    return backdrop * 2.0F * source;
  }
  const float doubled = (2.0F * source) - 1.0F;
  return backdrop + doubled - (backdrop * doubled);
}

auto SoftwareRaster::lum(const Rgb& colour) noexcept -> float {
  return (colour[0] * blend_luma[0]) + (colour[1] * blend_luma[1]) +
         (colour[2] * blend_luma[2]);
}

auto SoftwareRaster::clip_colour(Rgb colour) noexcept -> Rgb {
  const float level = lum(colour);
  const float lowest = std::min({colour[0], colour[1], colour[2]});
  const float highest = std::max({colour[0], colour[1], colour[2]});
  if (lowest < 0.0F) {
    for (float& channel : colour) {
      channel = level +
                ((channel - level) * level / std::max(level - lowest, 1.0e-6F));
    }
  }
  if (highest > 1.0F) {
    for (float& channel : colour) {
      channel = level + ((channel - level) * (1.0F - level) /
                         std::max(highest - level, 1.0e-6F));
    }
  }
  return colour;
}

auto SoftwareRaster::set_lum(const Rgb& colour, float level) noexcept -> Rgb {
  const float shift = level - lum(colour);
  return clip_colour(
      Rgb{colour[0] + shift, colour[1] + shift, colour[2] + shift});
}

auto SoftwareRaster::sat(const Rgb& colour) noexcept -> float {
  return std::max({colour[0], colour[1], colour[2]}) -
         std::min({colour[0], colour[1], colour[2]});
}

auto SoftwareRaster::set_sat(const Rgb& colour, float level) noexcept -> Rgb {
  const float lowest = std::min({colour[0], colour[1], colour[2]});
  const float highest = std::max({colour[0], colour[1], colour[2]});
  if (highest <= lowest) {
    return Rgb{0.0F, 0.0F, 0.0F};
  }
  Rgb made{};
  for (std::size_t channel = 0; channel < 3; ++channel) {
    made[channel] = (colour[channel] - lowest) * level / (highest - lowest);
  }
  return made;
}

auto SoftwareRaster::separable(const Rgb& backdrop, const Rgb& source,
                               int mode) noexcept -> Rgb {
  Rgb made = source;
  for (std::size_t channel = 0; channel < 3; ++channel) {
    const float cb = backdrop[channel];
    const float cs = source[channel];
    switch (mode) {
      case 1:
        made[channel] = cb * cs;
        break;
      case 2:
        made[channel] = cb + cs - (cb * cs);
        break;
      case 3:
        made[channel] = hard(cs, cb);
        break;
      case 4:
        made[channel] = std::min(cb, cs);
        break;
      case 5:
        made[channel] = std::max(cb, cs);
        break;
      case 6:
        made[channel] = dodge(cb, cs);
        break;
      case 7:
        made[channel] = burn(cb, cs);
        break;
      case 8:
        made[channel] = ((1.0F - (2.0F * cs)) * cb * cb) + (2.0F * cs * cb);
        break;
      case 9:
        made[channel] = hard(cb, cs);
        break;
      case 10:
        made[channel] = std::abs(cb - cs);
        break;
      case 11:
        made[channel] = cb + cs - (2.0F * cb * cs);
        break;
      default:
        break;
    }
  }
  return made;
}

auto SoftwareRaster::blended(const Rgba& source, const Rgba& backdrop,
                             int mode) noexcept -> Rgba {
  const Rgb s{source[0], source[1], source[2]};
  const Rgb b{backdrop[0], backdrop[1], backdrop[2]};
  switch (mode) {
    case 16:
      return Rgba{s[0], s[1], s[2], source[3] * backdrop[3]};
    case 17:
      return Rgba{s[0], s[1], s[2], source[3] * (1.0F - backdrop[3])};
    case 18:
      return Rgba{0.0F, 0.0F, 0.0F, source[3]};
    case 19:
    case 20: {
      if (source[3] <= 1.0e-4F) {
        return Rgba{0.0F, 0.0F, 0.0F, 0.0F};
      }
      Rgba made{0.0F, 0.0F, 0.0F, 1.0F};
      for (std::size_t channel = 0; channel < 3; ++channel) {
        made[channel] =
            mode == 19
                ? std::max(0.0F, b[channel] - ((1.0F - s[channel]) * source[3]))
                : std::min(1.0F, b[channel] + (s[channel] * source[3]));
      }
      return made;
    }
    default:
      break;
  }
  Rgb mixed = s;
  if (mode == 12) {
    mixed = set_lum(set_sat(s, sat(b)), lum(b));
  } else if (mode == 13) {
    mixed = set_lum(set_sat(b, sat(s)), lum(b));
  } else if (mode == 14) {
    mixed = set_lum(s, lum(b));
  } else if (mode == 15) {
    mixed = set_lum(b, lum(s));
  } else if (mode > 0) {
    mixed = separable(b, s, mode);
  }
  Rgba made{0.0F, 0.0F, 0.0F, source[3]};
  for (std::size_t channel = 0; channel < 3; ++channel) {
    made[channel] = std::clamp(
        s[channel] + ((mixed[channel] - s[channel]) * backdrop[3]), 0.0F, 1.0F);
  }
  return made;
}

}
