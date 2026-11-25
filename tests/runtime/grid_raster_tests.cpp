#include <arisa/components/grid.hpp>
#include <arisa/components/grid_item.hpp>
#include <arisa/components/grid_row.hpp>
#include <arisa/components/lazy_vgrid.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/shape.hpp>

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>

namespace arisa::runtime {

class GridRasterTest : public ::testing::Test {
 protected:
  static constexpr std::string_view lazy_key = "raster-lazy";
  static constexpr std::string_view table_key = "raster-table";

  static auto swatch(theme::ColourRole role, float width, float height)
      -> composition::Element {
    return components::VStack()
        .frame({.width = width, .height = height})
        .background(role, theme::Shape::rectangle());
  }

  class Grids final : public composition::Component {
   public:
    auto operator==(const Grids&) const -> bool = default;

    auto body(composition::Context&) const -> composition::Element override {
      return components::VStack(
          components::VStackOptions{.spacing = 24.0F},
          components::LazyVGrid(
              components::LazyVGridOptions{
                  .columns = {components::GridItem{},
                              components::GridItem{
                                  components::GridItem::Size::flexible(),
                                  std::nullopt,
                                  composition::Alignment::leading},
                              components::GridItem{
                                  components::GridItem::Size::flexible(),
                                  std::nullopt,
                                  composition::Alignment::trailing}}},
              swatch(theme::ColourRole::destructive, 20.0F, 20.0F),
              swatch(theme::ColourRole::destructive, 20.0F, 20.0F),
              swatch(theme::ColourRole::destructive, 20.0F, 20.0F))
              .frame({.width = 300.0F})
              .id(std::string{lazy_key}),
          components::Grid(
              components::GridRow(
                  swatch(theme::ColourRole::separator, 80.0F, 60.0F),
                  swatch(theme::ColourRole::separator, 80.0F, 60.0F)),
              components::GridRow(
                  swatch(theme::ColourRole::destructive, 20.0F, 20.0F)
                      .grid_cell_anchor(motion::UnitPoint::top_leading),
                  swatch(theme::ColourRole::destructive, 20.0F, 20.0F)
                      .grid_cell_anchor(
                          motion::UnitPoint{.x = 0.75F, .y = 1.0F})))
              .id(std::string{table_key}));
    }
  };

  static auto rect(Application& application, std::string_view key)
      -> composition::Rect {
    const auto id = application.root().tree().find(
        composition::ElementKey{std::string{key}});
    EXPECT_TRUE(id.has_value());
    if (!id.has_value()) {
      return {};
    }
    return application.root().tree().node(*id)->get().frame();
  }

  class Decorated final : public composition::Component {
   public:
    auto operator==(const Decorated&) const -> bool = default;

    auto body(composition::Context&) const -> composition::Element override {
      const auto clear = [](float width, float height) {
        return composition::Element{
            components::VStack().frame({.width = width, .height = height})};
      };
      return components::Grid(
                 components::GridOptions{
                     .alignment = composition::Alignment::top_leading,
                     .horizontal_spacing = 10.0F,
                     .vertical_spacing = 10.0F},
                 components::GridRow(clear(30.0F, 20.0F), clear(60.0F, 40.0F))
                     .background(theme::Colour::hex(0xFF0000),
                                 theme::Shape::rectangle()),
                 components::GridRow(clear(50.0F, 20.0F), clear(20.0F, 20.0F))
                     .border(theme::Colour::hex(0x0000FF), 2.0F))
          .id(std::string{table_key});
    }
  };

  static auto blue(const provider::Image& image, float x, float y) -> bool {
    const auto seen = image.pixel(static_cast<std::uint32_t>(x),
                                  static_cast<std::uint32_t>(y));
    return seen.has_value() && (*seen)[0] < 120 && (*seen)[1] < 120 &&
           (*seen)[2] > 180;
  }

  static auto red(const provider::Image& image, float x, float y) -> bool {
    const auto seen = image.pixel(static_cast<std::uint32_t>(x),
                                  static_cast<std::uint32_t>(y));
    return seen.has_value() && (*seen)[0] > 180 && (*seen)[1] < 120 &&
           (*seen)[2] < 120;
  }
};

TEST_F(GridRasterTest, GridCellsPaintWhereTheirTracksAndAnchorsPlaceThem) {
  provider::headless::HeadlessProviderFactory providers;
  const provider::SurfaceSize size{.width = provider::SurfaceWidth{400},
                                   .height = provider::SurfaceHeight{300}};
  providers.surface().resize(size);
  providers.gpu().rasterize(size);
  auto created = Application::create(providers, Grids{});
  ASSERT_TRUE(created.has_value());
  Application application = std::move(*created);
  for (int step = 0; step < 6; ++step) {
    ASSERT_TRUE(
        providers.clock().advance(std::chrono::milliseconds{16}).has_value());
    ASSERT_TRUE(application.frame().has_value());
  }
  const auto image = providers.gpu().capture();
  ASSERT_TRUE(image.has_value());
  const composition::Rect lazy = rect(application, lazy_key);
  const float middle = lazy.y + 10.0F;
  EXPECT_TRUE(red(*image, lazy.x + 37.333F + 10.0F, middle));
  EXPECT_FALSE(red(*image, lazy.x + 2.0F, middle));
  EXPECT_TRUE(red(*image, lazy.x + 102.667F + 3.0F, middle));
  EXPECT_FALSE(red(*image, lazy.x + 102.667F + 47.0F, middle));
  EXPECT_TRUE(red(*image, lazy.x + 300.0F - 3.0F, middle));
  EXPECT_FALSE(red(*image, lazy.x + 205.333F + 37.0F, middle));
  const composition::Rect table = rect(application, table_key);
  const float row = table.y + 68.0F;
  EXPECT_TRUE(red(*image, table.x + 3.0F, row + 3.0F));
  EXPECT_FALSE(red(*image, table.x + 40.0F, row + 10.0F));
  EXPECT_TRUE(red(*image, table.x + 133.0F + 3.0F, row + 17.0F));
  EXPECT_TRUE(red(*image, table.x + 153.0F - 3.0F, row + 3.0F));
  EXPECT_FALSE(red(*image, table.x + 128.0F - 3.0F, row + 10.0F));
}

TEST_F(GridRasterTest, RowBackgroundsAndBordersPaintEachCellLikeSwiftUI) {
  provider::headless::HeadlessProviderFactory providers;
  const provider::SurfaceSize size{.width = provider::SurfaceWidth{400},
                                   .height = provider::SurfaceHeight{300}};
  providers.surface().resize(size);
  providers.gpu().rasterize(size);
  auto created = Application::create(providers, Decorated{});
  ASSERT_TRUE(created.has_value());
  Application application = std::move(*created);
  for (int step = 0; step < 6; ++step) {
    ASSERT_TRUE(
        providers.clock().advance(std::chrono::milliseconds{16}).has_value());
    ASSERT_TRUE(application.frame().has_value());
  }
  const auto image = providers.gpu().capture();
  ASSERT_TRUE(image.has_value());
  const composition::Rect grid = rect(application, table_key);
  const auto at = [&grid](float x, float y) {
    return std::array{grid.x + x, grid.y + y};
  };
  const auto red_at = [&](float x, float y) {
    const auto [px, py] = at(x, y);
    return red(*image, px, py);
  };
  const auto blue_at = [&](float x, float y) {
    const auto [px, py] = at(x, y);
    return blue(*image, px, py);
  };
  EXPECT_TRUE(red_at(5.0F, 5.0F));
  EXPECT_FALSE(red_at(5.0F, 30.0F));
  EXPECT_FALSE(red_at(40.0F, 5.0F));
  EXPECT_FALSE(red_at(55.0F, 5.0F));
  EXPECT_TRUE(red_at(80.0F, 30.0F));
  EXPECT_TRUE(blue_at(1.0F, 55.0F));
  EXPECT_FALSE(blue_at(20.0F, 60.0F));
  EXPECT_FALSE(blue_at(55.0F, 55.0F));
  EXPECT_TRUE(blue_at(61.0F, 55.0F));
  EXPECT_FALSE(blue_at(95.0F, 55.0F));
}

}
