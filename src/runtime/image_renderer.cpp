#include <arisa/runtime/image_renderer.hpp>

#include <arisa/components/zstack.hpp>
#include <arisa/layout/layout_context.hpp>
#include <arisa/layout/viewport.hpp>
#include <arisa/provider/headless/null_gpu.hpp>
#include <arisa/provider/surface/surface_scale.hpp>
#include <arisa/provider/surface/surface_size.hpp>
#include <arisa/rendering/painter.hpp>
#include <arisa/theme/colour.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

namespace arisa::runtime {

ImageRenderer::ImageRenderer(composition::Element content)
    : tree_(std::make_unique<composition::Tree>(
          components::ZStack(
              components::ZStackOptions{
                  .alignment = composition::Alignment::top_leading},
              std::move(content)),
          [] { return true; })) {
  tree_->reconcile();
}

void ImageRenderer::set_scale(float scale) noexcept {
  scale_ = std::max(scale, 0.01F);
}

auto ImageRenderer::measured(TypesetterRef typesetter)
    -> std::optional<composition::Size> {
  if (proposed_.has_value()) {
    return proposed_;
  }
  auto context = layout::LayoutContext::create();
  if (!context.has_value()) {
    return std::nullopt;
  }
  const auto extent = static_cast<std::uint32_t>(measuring_extent);
  const layout::Viewport viewport{
      .pixels =
          provider::SurfaceSize{.width = provider::SurfaceWidth{extent},
                                .height = provider::SurfaceHeight{extent}},
      .scale = provider::SurfaceScale::unit()};
  if (!context->update(*tree_, viewport, typesetter).has_value()) {
    return std::nullopt;
  }
  const composition::Node& root = tree_->root();
  if (root.child_count() == 0) {
    return std::nullopt;
  }
  const composition::Rect frame = (*root.children().begin()).frame();
  return composition::Size{.width = frame.width, .height = frame.height};
}

auto ImageRenderer::image(TypesetterRef typesetter)
    -> std::expected<provider::Image, Failure> {
  const std::optional<composition::Size> size = measured(typesetter);
  if (!size.has_value()) {
    return std::unexpected(Failure::layout);
  }
  const auto width =
      static_cast<std::uint32_t>(std::ceil(size->width * scale_));
  const auto height =
      static_cast<std::uint32_t>(std::ceil(size->height * scale_));
  if (width == 0 || height == 0) {
    return std::unexpected(Failure::empty);
  }
  const std::optional<provider::SurfaceScale> surface_scale =
      provider::SurfaceScale::create(scale_);
  if (!surface_scale.has_value()) {
    return std::unexpected(Failure::layout);
  }
  const provider::SurfaceSize pixels{.width = provider::SurfaceWidth{width},
                                     .height = provider::SurfaceHeight{height}};
  auto context = layout::LayoutContext::create();
  if (!context.has_value() ||
      !context
           ->update(*tree_,
                    layout::Viewport{.pixels = pixels, .scale = *surface_scale},
                    typesetter)
           .has_value()) {
    return std::unexpected(Failure::layout);
  }
  provider::headless::NullGpu gpu;
  gpu.rasterize(pixels);
  auto painter = rendering::Painter::create(gpu);
  if (!painter.has_value()) {
    return std::unexpected(Failure::paint);
  }
  auto frame = gpu.begin_frame();
  if (!frame.has_value()) {
    return std::unexpected(Failure::paint);
  }
  const theme::Colour clear = opaque_ ? theme::Colour{0.0F, 0.0F, 0.0F, 1.0F}
                                      : theme::Colour{0.0F, 0.0F, 0.0F, 0.0F};
  if (!painter
           ->paint(
               *tree_, *frame,
               rendering::Painter::Target{.pixels = pixels,
                                          .scale = *surface_scale,
                                          .time = std::chrono::nanoseconds{0},
                                          .clear = clear},
               typesetter)
           .has_value() ||
      !frame->present().has_value()) {
    return std::unexpected(Failure::paint);
  }
  auto captured = gpu.capture();
  if (!captured.has_value()) {
    return std::unexpected(Failure::capture);
  }
  std::vector<std::byte> straight(captured->pixels().begin(),
                                  captured->pixels().end());
  for (std::size_t at = 0; at + 3 < straight.size(); at += 4) {
    const auto alpha = std::to_integer<std::uint32_t>(straight[at + 3]);
    if (alpha == 0 || alpha == 255) {
      continue;
    }
    for (std::size_t channel = 0; channel < 3; ++channel) {
      const auto value = std::to_integer<std::uint32_t>(straight[at + channel]);
      straight[at + channel] = static_cast<std::byte>(
          std::min(255U, ((value * 255U) + (alpha / 2U)) / alpha));
    }
  }
  auto made = provider::Image::create(width, height, std::move(straight));
  if (!made.has_value()) {
    return std::unexpected(Failure::capture);
  }
  return std::move(*made);
}

}
