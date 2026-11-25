#pragma once

#include <arisa/composition/point.hpp>
#include <arisa/graphics/affine_transform.hpp>

#include <array>
#include <cmath>
#include <optional>

namespace arisa::graphics {

class ProjectionTransform {
 public:
  static constexpr float minimum_depth = 1.0e-3F;

  constexpr ProjectionTransform() noexcept = default;

  constexpr explicit ProjectionTransform(std::array<float, 9> rows) noexcept
      : m_(rows) {}

  constexpr explicit ProjectionTransform(const AffineTransform& affine) noexcept
      : m_{affine.a(), affine.b(),  0.0F,        affine.c(), affine.d(),
           0.0F,       affine.tx(), affine.ty(), 1.0F} {}

  static constexpr auto identity() noexcept -> ProjectionTransform {
    return ProjectionTransform{};
  }

  constexpr auto concatenating(const ProjectionTransform& after) const noexcept
      -> ProjectionTransform {
    std::array<float, 9> made{};
    for (std::size_t row = 0; row < 3; ++row) {
      for (std::size_t column = 0; column < 3; ++column) {
        float sum = 0.0F;
        for (std::size_t inner = 0; inner < 3; ++inner) {
          sum += m_[(row * 3) + inner] * after.m_[(inner * 3) + column];
        }
        made[(row * 3) + column] = sum;
      }
    }
    return ProjectionTransform{made};
  }

  constexpr auto invert() noexcept -> bool {
    const float c00 = (m_[4] * m_[8]) - (m_[5] * m_[7]);
    const float c01 = (m_[5] * m_[6]) - (m_[3] * m_[8]);
    const float c02 = (m_[3] * m_[7]) - (m_[4] * m_[6]);
    const float determinant = (m_[0] * c00) + (m_[1] * c01) + (m_[2] * c02);
    if (determinant == 0.0F || !std::isfinite(determinant)) {
      return false;
    }
    const float r = 1.0F / determinant;
    m_ = std::array<float, 9>{c00 * r,
                              ((m_[2] * m_[7]) - (m_[1] * m_[8])) * r,
                              ((m_[1] * m_[5]) - (m_[2] * m_[4])) * r,
                              c01 * r,
                              ((m_[0] * m_[8]) - (m_[2] * m_[6])) * r,
                              ((m_[2] * m_[3]) - (m_[0] * m_[5])) * r,
                              c02 * r,
                              ((m_[1] * m_[6]) - (m_[0] * m_[7])) * r,
                              ((m_[0] * m_[4]) - (m_[1] * m_[3])) * r};
    return true;
  }

  constexpr auto inverted() const noexcept -> ProjectionTransform {
    ProjectionTransform made = *this;
    if (!made.invert()) {
      return *this;
    }
    return made;
  }

  constexpr auto is_affine() const noexcept -> bool {
    return m_[2] == 0.0F && m_[5] == 0.0F && m_[8] == 1.0F;
  }

  constexpr auto is_identity() const noexcept -> bool {
    return *this == ProjectionTransform{};
  }

  constexpr auto affine() const noexcept -> AffineTransform {
    return AffineTransform{m_[0], m_[1], m_[3], m_[4], m_[6], m_[7]};
  }

  constexpr auto apply(composition::Point point) const noexcept
      -> std::optional<composition::Point> {
    const float w = (m_[2] * point.x) + (m_[5] * point.y) + m_[8];
    if (w < minimum_depth) {
      return std::nullopt;
    }
    return composition::Point{
        .x = ((m_[0] * point.x) + (m_[3] * point.y) + m_[6]) / w,
        .y = ((m_[1] * point.x) + (m_[4] * point.y) + m_[7]) / w};
  }

  constexpr auto m11() const noexcept -> float { return m_[0]; }
  constexpr auto m12() const noexcept -> float { return m_[1]; }
  constexpr auto m13() const noexcept -> float { return m_[2]; }
  constexpr auto m21() const noexcept -> float { return m_[3]; }
  constexpr auto m22() const noexcept -> float { return m_[4]; }
  constexpr auto m23() const noexcept -> float { return m_[5]; }
  constexpr auto m31() const noexcept -> float { return m_[6]; }
  constexpr auto m32() const noexcept -> float { return m_[7]; }
  constexpr auto m33() const noexcept -> float { return m_[8]; }

  friend constexpr auto operator==(const ProjectionTransform&,
                                   const ProjectionTransform&)
      -> bool = default;

 private:
  std::array<float, 9> m_{1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F};
};

}
