#include "focus_fixture.hpp"

#include <arisa/components/qr_code.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/provider/gpu/image.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/theme/colour.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace arisa::runtime {

class QrCodeTest : public FocusFixture {
 protected:
  using Qr = components::QrCode;

  static constexpr std::size_t side = 21;
  static constexpr provider::SurfaceSize surface{
      .width = provider::SurfaceWidth{300},
      .height = provider::SurfaceHeight{300}};

  static auto dark(std::size_t column, std::size_t row) -> bool {
    return (column + row) % 2 == 0;
  }

  static auto matrix() -> Qr::Matrix {
    std::vector<bool> modules(side * side);
    for (std::size_t row = 0; row < side; ++row) {
      for (std::size_t column = 0; column < side; ++column) {
        modules[(row * side) + column] = dark(column, row);
      }
    }
    std::optional<Qr::Matrix> made = Qr::Matrix::create(side, std::move(modules));
    EXPECT_TRUE(made.has_value());
    return std::move(*made);
  }

  static auto at(const provider::Image& image, float x, float y)
      -> std::array<std::uint8_t, 4> {
    return image
        .pixel(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y))
        .value_or(std::array<std::uint8_t, 4>{});
  }

  static auto black(std::array<std::uint8_t, 4> seen) -> bool {
    return seen[0] == 0 && seen[1] == 0 && seen[2] == 0;
  }

  static auto white(std::array<std::uint8_t, 4> seen) -> bool {
    return seen[0] == 255 && seen[1] == 255 && seen[2] == 255;
  }

  provider::headless::HeadlessProviderFactory providers_;
};

TEST_F(QrCodeTest, AMatrixNeedsASquareModuleCount) {
  EXPECT_FALSE(Qr::Matrix::create(0, {}).has_value());
  EXPECT_FALSE(Qr::Matrix::create(3, std::vector<bool>(8)).has_value());
  const std::optional<Qr::Matrix> made =
      Qr::Matrix::create(2, std::vector<bool>{true, false, false, true});
  ASSERT_TRUE(made.has_value());
  EXPECT_EQ(made->side(), 2U);
  EXPECT_TRUE(made->dark(0, 0));
  EXPECT_FALSE(made->dark(1, 0));
  EXPECT_TRUE(made->dark(1, 1));
  EXPECT_FALSE(made->dark(2, 0));
}

TEST_F(QrCodeTest, ModulesSnapToWholeDevicePixelsAndCentre) {
  const auto shared = std::make_shared<const Qr::Matrix>(matrix());
  const Qr::Modules unit{shared, Qr::Modules::Layer::dark, 4, 12.0F, 1.0F};
  const composition::Rect box{.x = 10.0F, .y = 20.0F, .width = 200.0F,
                              .height = 180.0F};
  EXPECT_FLOAT_EQ(unit.module_extent(box), 6.0F);
  const composition::Rect placed = unit.placement(box);
  EXPECT_FLOAT_EQ(placed.width, 174.0F);
  EXPECT_FLOAT_EQ(placed.height, 174.0F);
  EXPECT_FLOAT_EQ(placed.x, 23.0F);
  EXPECT_FLOAT_EQ(placed.y, 23.0F);
  const Qr::Modules doubled{shared, Qr::Modules::Layer::dark, 4, 12.0F, 2.0F};
  EXPECT_FLOAT_EQ(doubled.module_extent(box), 6.0F);
  const composition::Rect fine{.x = 0.0F, .y = 0.0F, .width = 203.0F,
                               .height = 203.0F};
  EXPECT_FLOAT_EQ(doubled.module_extent(fine), 7.0F);
  const Qr::Modules tight{shared, Qr::Modules::Layer::dark, 4, 12.0F, 1.0F};
  const composition::Rect tiny{.x = 0.0F, .y = 0.0F, .width = 20.0F,
                               .height = 20.0F};
  EXPECT_FLOAT_EQ(tight.module_extent(tiny), 20.0F / 29.0F);
}

TEST_F(QrCodeTest, PaintsCrispModulesOnAPlateWithAQuietZone) {
  providers_.surface().resize(surface);
  providers_.surface().set_scale(provider::SurfaceScale::unit());
  providers_.gpu().rasterize(surface);
  Application application = start(
      providers_,
      components::ZStack(Qr(matrix()).frame({.width = 204.0F, .height = 204.0F})
                             .id("qr"))
          .frame({.width = 300.0F, .height = 300.0F})
          .background(theme::Colour::hex(0x3366CC)));
  frames(application, 3);
  const auto image = providers_.gpu().capture();
  ASSERT_TRUE(image.has_value());
  const auto node = node_named(application, "qr");
  ASSERT_TRUE(node.has_value());
  const composition::Rect frame = node->get().frame();
  const Qr::Modules modules{std::make_shared<const Qr::Matrix>(matrix()),
                           Qr::Modules::Layer::dark, 4, 12.0F, 1.0F};
  const composition::Rect local{
      .x = 0.0F, .y = 0.0F, .width = frame.width, .height = frame.height};
  const float module = modules.module_extent(local);
  ASSERT_FLOAT_EQ(module, 7.0F);
  const float left = frame.x + modules.placement(local).x;
  const float top = frame.y + modules.placement(local).y;
  const float origin_x = left + (4.0F * module);
  const float origin_y = top + (4.0F * module);
  EXPECT_TRUE(white(at(*image, left + module, top + module)));
  for (std::size_t row = 0; row < 3; ++row) {
    for (std::size_t column = 0; column < 3; ++column) {
      const float x = origin_x + (static_cast<float>(column) * module);
      const float y = origin_y + (static_cast<float>(row) * module);
      const bool expected = dark(column, row);
      for (const auto& [dx, dy] :
           std::array<std::array<float, 2>, 3>{{{0.5F, 0.5F},
                                                {3.5F, 3.5F},
                                                {6.5F, 6.5F}}}) {
        const auto seen = at(*image, x + dx, y + dy);
        EXPECT_TRUE(expected ? black(seen) : white(seen))
            << column << "," << row << " +" << dx;
      }
    }
  }
  const auto outside = at(*image, frame.x - 4.0F, frame.y + 100.0F);
  EXPECT_FALSE(white(outside));
}

}
