#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/graphics/path.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/colour_value.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace arisa::components {

struct QrCodeOptions {
  std::size_t quiet_zone{4};
  theme::ColourValue modules{theme::Colour::hex(0x000000)};
  theme::ColourValue plate{theme::Colour::hex(0xFFFFFF)};
  float corner_radius{12.0F};
  float module_extent{4.0F};

  friend auto operator==(const QrCodeOptions&, const QrCodeOptions&)
      -> bool = default;
};

class QrCode : public composition::Element {
 public:
  class Matrix;
  class Modules;
  class Content;

  explicit QrCode(Matrix matrix, QrCodeOptions options = {});
};

class QrCode::Matrix {
 public:
  static auto create(std::size_t side, std::vector<bool> dark)
      -> std::optional<Matrix>;

  auto side() const noexcept -> std::size_t { return side_; }

  auto dark(std::size_t column, std::size_t row) const noexcept -> bool;

  friend auto operator==(const Matrix&, const Matrix&) -> bool = default;

 private:
  Matrix(std::size_t side, std::vector<bool> dark) noexcept
      : side_(side), dark_(std::move(dark)) {}

  std::size_t side_;
  std::vector<bool> dark_;
};

class QrCode::Modules {
 public:
  enum class Layer : std::uint8_t { plate, dark };

  Modules(std::shared_ptr<const Matrix> matrix, Layer layer,
          std::size_t quiet_zone, float corner_radius, float scale) noexcept
      : matrix_(std::move(matrix)),
        layer_(layer),
        quiet_zone_(quiet_zone),
        corner_radius_(corner_radius),
        scale_(scale) {}

  auto path(composition::Rect rect) const -> graphics::Path;

  auto placement(composition::Rect rect) const -> composition::Rect;

  auto module_extent(composition::Rect rect) const -> float;

  friend auto operator==(const Modules& left, const Modules& right) -> bool {
    return *left.matrix_ == *right.matrix_ && left.layer_ == right.layer_ &&
           left.quiet_zone_ == right.quiet_zone_ &&
           left.corner_radius_ == right.corner_radius_ &&
           left.scale_ == right.scale_;
  }

 private:
  auto count() const noexcept -> std::size_t;

  std::shared_ptr<const Matrix> matrix_;
  Layer layer_;
  std::size_t quiet_zone_;
  float corner_radius_;
  float scale_;
};

class QrCode::Content final : public composition::ElementContent {
 public:
  Content(Matrix matrix, QrCodeOptions options)
      : matrix_(std::make_shared<const Matrix>(std::move(matrix))),
        options_(options) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

  auto matrix() const noexcept -> const Matrix& { return *matrix_; }

  auto options() const noexcept -> const QrCodeOptions& { return options_; }

 private:
  std::shared_ptr<const Matrix> matrix_;
  QrCodeOptions options_;
};

}
