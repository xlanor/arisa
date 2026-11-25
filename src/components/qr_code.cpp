#include <arisa/components/environment.hpp>
#include <arisa/components/qr_code.hpp>
#include <arisa/components/shape_view.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/graphics/stroke_style.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace arisa::components {

QrCode::QrCode(Matrix matrix, QrCodeOptions options)
    : Element(std::make_unique<Content>(std::move(matrix), options)) {}

auto QrCode::Matrix::create(std::size_t side, std::vector<bool> dark)
    -> std::optional<Matrix> {
  if (side == 0 || dark.size() != side * side) {
    return std::nullopt;
  }
  return Matrix{side, std::move(dark)};
}

auto QrCode::Matrix::dark(std::size_t column, std::size_t row) const noexcept
    -> bool {
  if (column >= side_ || row >= side_) {
    return false;
  }
  return dark_[(row * side_) + column];
}

auto QrCode::Modules::count() const noexcept -> std::size_t {
  return matrix_->side() + (2 * quiet_zone_);
}

auto QrCode::Modules::module_extent(composition::Rect rect) const -> float {
  const float shortest = std::max(std::min(rect.width, rect.height), 0.0F);
  const auto modules = static_cast<float>(count());
  const float pixels = std::floor(shortest * scale_ / modules);
  if (pixels >= 1.0F) {
    return pixels / scale_;
  }
  return shortest / modules;
}

auto QrCode::Modules::placement(composition::Rect rect) const
    -> composition::Rect {
  const float extent = module_extent(rect) * static_cast<float>(count());
  const float left =
      std::round((rect.x + ((rect.width - extent) * 0.5F)) * scale_) / scale_;
  const float top =
      std::round((rect.y + ((rect.height - extent) * 0.5F)) * scale_) / scale_;
  return composition::Rect{
      .x = left, .y = top, .width = extent, .height = extent};
}

auto QrCode::Modules::path(composition::Rect rect) const -> graphics::Path {
  graphics::Path drawn;
  const composition::Rect plate = placement(rect);
  if (plate.width <= 0.0F) {
    return drawn;
  }
  if (layer_ == Layer::plate) {
    const float radius = std::min(corner_radius_, plate.width * 0.5F);
    drawn.add_rounded_rect(plate,
                           composition::Size{.width = radius, .height = radius});
    return drawn;
  }
  const float module = module_extent(rect);
  const float inset = module * static_cast<float>(quiet_zone_);
  const std::size_t side = matrix_->side();
  std::vector<composition::Rect> runs;
  for (std::size_t row = 0; row < side; ++row) {
    std::size_t column = 0;
    while (column < side) {
      if (!matrix_->dark(column, row)) {
        ++column;
        continue;
      }
      const std::size_t start = column;
      while (column < side && matrix_->dark(column, row)) {
        ++column;
      }
      runs.push_back(composition::Rect{
          .x = plate.x + inset + (static_cast<float>(start) * module),
          .y = plate.y + inset + (static_cast<float>(row) * module),
          .width = static_cast<float>(column - start) * module,
          .height = module});
    }
  }
  drawn.add_rects(runs);
  return drawn;
}

auto QrCode::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto QrCode::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  const float declared = context.environment(Environment::display_scale);
  const float scale = declared > 0.0F ? declared : 1.0F;
  const float ideal =
      static_cast<float>(matrix_->side() + (2 * options_.quiet_zone)) *
      options_.module_extent;
  std::vector<composition::Element> produced;
  produced.push_back(
      ZStack(ShapeView(Modules{matrix_, Modules::Layer::plate,
                               options_.quiet_zone, options_.corner_radius,
                               scale})
                 .fill(options_.plate),
             ShapeView(Modules{matrix_, Modules::Layer::dark,
                               options_.quiet_zone, options_.corner_radius,
                               scale})
                 .fill(options_.modules,
                       graphics::FillStyle{.eo_fill = false,
                                           .antialiased = false}))
          .frame({.ideal_width = ideal,
                  .max_width = composition::infinity,
                  .ideal_height = ideal,
                  .max_height = composition::infinity}));
  return produced;
}

}
