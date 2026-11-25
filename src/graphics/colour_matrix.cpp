#include <arisa/graphics/colour_matrix.hpp>

#include <algorithm>
#include <cmath>

namespace arisa::graphics {

auto ColourMatrix::of(std::array<float, count> values) noexcept
    -> ColourMatrix {
  ColourMatrix made;
  made.values_ = values;
  return made;
}

auto ColourMatrix::brightness(double amount) noexcept -> ColourMatrix {
  const auto shift = static_cast<float>(amount);
  return of({1.0F, 0.0F, 0.0F, 0.0F, shift, 0.0F, 1.0F, 0.0F, 0.0F, shift,
             0.0F, 0.0F, 1.0F, 0.0F, shift, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F});
}

auto ColourMatrix::contrast(double amount) noexcept -> ColourMatrix {
  const auto gain = static_cast<float>(amount);
  const float shift = 0.5F * (1.0F - gain);
  return of({gain, 0.0F, 0.0F, 0.0F, shift, 0.0F, gain, 0.0F, 0.0F, shift,
             0.0F, 0.0F, gain, 0.0F, shift, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F});
}

auto ColourMatrix::saturation(double amount) noexcept -> ColourMatrix {
  const auto s = static_cast<float>(amount);
  const float r = luminance[0] * (1.0F - s);
  const float g = luminance[1] * (1.0F - s);
  const float b = luminance[2] * (1.0F - s);
  return of({r + s, g, b,     0.0F, 0.0F, r,    g + s, b,    0.0F, 0.0F,
             r,     g, b + s, 0.0F, 0.0F, 0.0F, 0.0F,  0.0F, 1.0F, 0.0F});
}

auto ColourMatrix::grayscale(double amount) noexcept -> ColourMatrix {
  return saturation(1.0 - std::clamp(amount, 0.0, 1.0));
}

auto ColourMatrix::hue_rotation(motion::Angle angle) noexcept -> ColourMatrix {
  const auto c = static_cast<float>(std::cos(angle.radians()));
  const auto s = static_cast<float>(std::sin(angle.radians()));
  const auto [wr, wg, wb] = hue_weights;
  return of({wr + (c * (1.0F - wr)) - (s * wr),
             wg - (c * wg) - (s * wg),
             wb - (c * wb) + (s * (1.0F - wb)),
             0.0F,
             0.0F,
             wr - (c * wr) + (s * 0.143F),
             wg + (c * (1.0F - wg)) + (s * 0.140F),
             wb - (c * wb) - (s * 0.283F),
             0.0F,
             0.0F,
             wr - (c * wr) - (s * (1.0F - wr)),
             wg - (c * wg) + (s * wg),
             wb + (c * (1.0F - wb)) + (s * wb),
             0.0F,
             0.0F,
             0.0F,
             0.0F,
             0.0F,
             1.0F,
             0.0F});
}

auto ColourMatrix::inverted() noexcept -> ColourMatrix {
  return of({-1.0F, 0.0F, 0.0F,  0.0F, 1.0F, 0.0F, -1.0F, 0.0F, 0.0F, 1.0F,
             0.0F,  0.0F, -1.0F, 0.0F, 1.0F, 0.0F, 0.0F,  0.0F, 1.0F, 0.0F});
}

auto ColourMatrix::multiplied(theme::Colour colour) noexcept -> ColourMatrix {
  return of({colour.red, 0.0F,         0.0F,        0.0F,         0.0F,
             0.0F,       colour.green, 0.0F,        0.0F,         0.0F,
             0.0F,       0.0F,         colour.blue, 0.0F,         0.0F,
             0.0F,       0.0F,         0.0F,        colour.alpha, 0.0F});
}

auto ColourMatrix::luminance_to_alpha() noexcept -> ColourMatrix {
  return of({0.0F, 0.0F,         0.0F,         0.0F,         0.0F, 0.0F, 0.0F,
             0.0F, 0.0F,         0.0F,         0.0F,         0.0F, 0.0F, 0.0F,
             0.0F, luminance[0], luminance[1], luminance[2], 0.0F, 0.0F});
}

auto ColourMatrix::then(const ColourMatrix& next) const noexcept
    -> ColourMatrix {
  ColourMatrix made;
  for (std::size_t row = 0; row < rows; ++row) {
    for (std::size_t column = 0; column < columns; ++column) {
      float sum = column == columns - 1 ? next.at(row, column) : 0.0F;
      for (std::size_t inner = 0; inner < rows; ++inner) {
        sum += next.at(row, inner) * at(inner, column);
      }
      made.values_[(row * columns) + column] = sum;
    }
  }
  return made;
}

auto ColourMatrix::apply(theme::Colour straight) const noexcept
    -> theme::Colour {
  const std::array<float, 4> in{straight.red, straight.green, straight.blue,
                                straight.alpha};
  std::array<float, 4> out{};
  for (std::size_t row = 0; row < rows; ++row) {
    float sum = at(row, columns - 1);
    for (std::size_t column = 0; column < rows; ++column) {
      sum += at(row, column) * in[column];
    }
    out[row] = std::clamp(sum, 0.0F, 1.0F);
  }
  return theme::Colour{
      .red = out[0], .green = out[1], .blue = out[2], .alpha = out[3]};
}

auto ColourMatrix::is_identity() const noexcept -> bool {
  return *this == identity();
}

auto ColourMatrix::touches_alpha() const noexcept -> bool {
  return at(3, 0) != 0.0F || at(3, 1) != 0.0F || at(3, 2) != 0.0F ||
         at(3, 3) != 1.0F || at(3, 4) != 0.0F;
}

}
