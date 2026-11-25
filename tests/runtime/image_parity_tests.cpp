#include <arisa/components/canvas.hpp>
#include <arisa/components/graphics_context.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/image_library.hpp>
#include <arisa/components/shape_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/accessibility.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/image_attributes.hpp>
#include <arisa/composition/image_environment.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/size.hpp>
#include <arisa/composition/transfer_item.hpp>
#include <arisa/composition/transferable.hpp>
#include <arisa/graphics/color_rendering_mode.hpp>
#include <arisa/graphics/image_paint.hpp>
#include <arisa/graphics/path.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/provider/gpu/image.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/surface/surface_scale.hpp>
#include <arisa/provider/surface/surface_size.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>

#include <focus_fixture.hpp>

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace arisa::runtime {

using composition::Element;

class ImageParityTest : public FocusFixture {
 protected:
  static constexpr std::uint32_t side = 120;

  using Builder = std::function<Element()>;

  class Scene final : public composition::Component {
   public:
    explicit Scene(const Builder* builder) noexcept : builder_(builder) {}

    auto operator==(const Scene&) const -> bool = default;

    auto body(composition::Context&) const -> Element override {
      return (*builder_)();
    }

   private:
    const Builder* builder_;
  };

  static auto solid(std::uint32_t width, std::uint32_t height,
                    std::array<std::uint8_t, 4> colour)
      -> std::shared_ptr<const provider::Image> {
    std::vector<std::byte> bytes;
    bytes.reserve(std::size_t{width} * height * 4);
    for (std::uint32_t at = 0; at < width * height; ++at) {
      for (const std::uint8_t channel : colour) {
        bytes.push_back(static_cast<std::byte>(channel));
      }
    }
    auto made = provider::Image::create(width, height, std::move(bytes));
    EXPECT_TRUE(made.has_value());
    return std::make_shared<const provider::Image>(std::move(*made));
  }

  static auto framed(std::uint32_t extent)
      -> std::shared_ptr<const provider::Image> {
    std::vector<std::byte> bytes;
    for (std::uint32_t y = 0; y < extent; ++y) {
      for (std::uint32_t x = 0; x < extent; ++x) {
        const bool edge = x < 2 || y < 2 || x >= extent - 2 || y >= extent - 2;
        const std::array<std::uint8_t, 4> colour =
            edge ? std::array<std::uint8_t, 4>{255, 0, 0, 255}
                 : std::array<std::uint8_t, 4>{0, 0, 255, 255};
        for (const std::uint8_t channel : colour) {
          bytes.push_back(static_cast<std::byte>(channel));
        }
      }
    }
    auto made = provider::Image::create(extent, extent, std::move(bytes));
    EXPECT_TRUE(made.has_value());
    return std::make_shared<const provider::Image>(std::move(*made));
  }

  static auto checker() -> std::shared_ptr<const provider::Image> {
    std::vector<std::byte> bytes;
    for (std::uint32_t y = 0; y < 4; ++y) {
      for (std::uint32_t x = 0; x < 4; ++x) {
        const bool dark = (x < 2) == (y < 2);
        const std::uint8_t level = dark ? 0 : 255;
        for (const std::uint8_t channel :
             std::array<std::uint8_t, 4>{level, level, level, 255}) {
          bytes.push_back(static_cast<std::byte>(channel));
        }
      }
    }
    auto made = provider::Image::create(4, 4, std::move(bytes));
    EXPECT_TRUE(made.has_value());
    return std::make_shared<const provider::Image>(std::move(*made));
  }

  static auto render(const Builder& builder) -> std::optional<provider::Image> {
    provider::headless::HeadlessProviderFactory providers;
    const provider::SurfaceSize size{.width = provider::SurfaceWidth{side},
                                     .height = provider::SurfaceHeight{side}};
    providers.surface().resize(size);
    providers.surface().set_scale(provider::SurfaceScale::unit());
    providers.gpu().rasterize(size);
    auto created = Application::create(providers, Scene{&builder});
    EXPECT_TRUE(created.has_value());
    if (!created.has_value()) {
      return std::nullopt;
    }
    Application application = std::move(*created);
    for (int step = 0; step < 4; ++step) {
      EXPECT_TRUE(
          providers.clock().advance(std::chrono::milliseconds{16}).has_value());
      EXPECT_TRUE(application.frame().has_value());
    }
    auto image = providers.gpu().capture();
    EXPECT_TRUE(image.has_value());
    if (!image.has_value()) {
      return std::nullopt;
    }
    return std::move(*image);
  }

  static auto rgb(const provider::Image& image, int x, int y)
      -> std::array<int, 3> {
    const auto seen = image.pixel(static_cast<std::uint32_t>(x),
                                  static_cast<std::uint32_t>(y));
    if (!seen.has_value()) {
      return {-1, -1, -1};
    }
    return {(*seen)[0], (*seen)[1], (*seen)[2]};
  }

  static auto placed(Element content) -> Element {
    return components::ZStack(
               components::ZStackOptions{
                   .alignment = composition::Alignment::top_leading},
               std::move(content))
        .frame({.width = static_cast<float>(side),
                .height = static_cast<float>(side),
                .alignment = composition::Alignment::top_leading})
        .background(theme::Colour::hex(0xFFFFFF), theme::Shape::rectangle());
  }
};

TEST_F(ImageParityTest, NineSliceKeepsCapsAndStretchesTheMiddle) {
  const std::shared_ptr<const provider::Image> frame = framed(8);
  const Builder builder = [frame] {
    return placed(components::Image{frame}
                      .resizable(theme::EdgeInsets::all(theme::Space{2.0F}))
                      .interpolation(components::Image::Interpolation::none)
                      .frame({.width = 80.0F, .height = 60.0F}));
  };
  const std::optional<provider::Image> drawn = render(builder);
  ASSERT_TRUE(drawn.has_value());
  EXPECT_EQ(rgb(*drawn, 0, 30), (std::array<int, 3>{255, 0, 0}));
  EXPECT_EQ(rgb(*drawn, 1, 30), (std::array<int, 3>{255, 0, 0}));
  EXPECT_EQ(rgb(*drawn, 3, 30), (std::array<int, 3>{0, 0, 255}));
  EXPECT_EQ(rgb(*drawn, 40, 30), (std::array<int, 3>{0, 0, 255}));
  EXPECT_EQ(rgb(*drawn, 78, 30), (std::array<int, 3>{255, 0, 0}));
  EXPECT_EQ(rgb(*drawn, 40, 1), (std::array<int, 3>{255, 0, 0}));
  EXPECT_EQ(rgb(*drawn, 40, 58), (std::array<int, 3>{255, 0, 0}));
}

TEST_F(ImageParityTest, TileModeRepeatsTheImageAtItsSize) {
  const std::shared_ptr<const provider::Image> tile = checker();
  const Builder builder = [tile] {
    return placed(components::Image{tile}
                      .resizable(theme::EdgeInsets{},
                                 components::Image::ResizingMode::tile)
                      .interpolation(components::Image::Interpolation::none)
                      .frame({.width = 16.0F, .height = 16.0F}));
  };
  const std::optional<provider::Image> drawn = render(builder);
  ASSERT_TRUE(drawn.has_value());
  EXPECT_EQ(rgb(*drawn, 0, 0)[0], 0);
  EXPECT_EQ(rgb(*drawn, 2, 0)[0], 255);
  EXPECT_EQ(rgb(*drawn, 4, 0)[0], 0);
  EXPECT_EQ(rgb(*drawn, 6, 0)[0], 255);
  EXPECT_EQ(rgb(*drawn, 12, 12)[0], 0);
}

TEST_F(ImageParityTest, ImagePaintTilesBackgroundsAndShapeFills) {
  const std::shared_ptr<const provider::Image> tile = checker();
  const graphics::ImagePaint paint{tile};
  EXPECT_EQ(paint.image(), tile);
  EXPECT_FLOAT_EQ(paint.scale(), 1.0F);
  EXPECT_EQ(paint.source_rect(), graphics::ImagePaint::SourceRect{});
  const graphics::ImagePaint doubled{
      tile,
      graphics::ImagePaint::SourceRect{
          .x = 0.0F, .y = 0.0F, .width = 0.5F, .height = 0.5F},
      2.0F};
  EXPECT_FLOAT_EQ(doubled.source_rect().width, 0.5F);
  EXPECT_FALSE(paint == doubled);
  const Builder builder = [paint] {
    return placed(components::VStack(
        components::VStackOptions{.spacing = 0.0F},
        components::ZStack()
            .frame({.width = 16.0F, .height = 16.0F})
            .background(graphics::ShapeStyle{paint}, theme::Shape::rectangle()),
        components::ShapeView(theme::Shape::rectangle())
            .fill(graphics::ShapeStyle{paint})
            .frame({.width = 16.0F, .height = 16.0F})));
  };
  const std::optional<provider::Image> drawn = render(builder);
  ASSERT_TRUE(drawn.has_value());
  EXPECT_LT(rgb(*drawn, 0, 0)[0], 64);
  EXPECT_GT(rgb(*drawn, 2, 0)[0], 192);
  EXPECT_LT(rgb(*drawn, 4, 0)[0], 64);
  EXPECT_LT(rgb(*drawn, 0, 16)[0], 64);
  EXPECT_GT(rgb(*drawn, 2, 16)[0], 192);
}

TEST_F(ImageParityTest, OrientedRastersTurnAndSwapTheirFrame) {
  EXPECT_EQ(composition::all_image_orientations.size(), 8U);
  provider::headless::HeadlessProviderFactory providers;
  const std::shared_ptr<const provider::Image> wide =
      solid(4, 2, {255, 0, 0, 255});
  const Builder builder = [wide] {
    return components::VStack(
        components::Image{wide, 1.0F, components::Image::Orientation::left,
                          "Turned"}
            .id("turned"),
        components::Image{wide, 1.0F,
                          components::Image::Orientation::down_mirrored,
                          "Flipped"}
            .id("flipped"),
        components::Image::decorative(wide, 1.0F).id("plain"));
  };
  Application application = start(providers, Element{Scene{&builder}});
  const auto turned = node_named(application, "turned");
  ASSERT_TRUE(turned.has_value());
  EXPECT_FLOAT_EQ(turned->get().frame().width, 2.0F);
  EXPECT_FLOAT_EQ(turned->get().frame().height, 4.0F);
  const auto flipped = node_named(application, "flipped");
  ASSERT_TRUE(flipped.has_value());
  EXPECT_FLOAT_EQ(flipped->get().frame().width, 4.0F);
}

TEST_F(ImageParityTest, NamedImagesTakeABundleLabelVariableAndDecorativeForms) {
  provider::headless::HeadlessProviderFactory providers;
  const components::ImageLibrary library{providers.assets()};
  const components::Image labelled{"badge", library, "Badge"};
  const auto& content =
      static_cast<const components::Image::Content&>(labelled.content());
  EXPECT_TRUE(content.bundle().has_value());
  EXPECT_EQ(content.accessibility().label, std::optional<std::string>{"Badge"});
  const components::Image hidden = components::Image::decorative("badge");
  EXPECT_FALSE(static_cast<const components::Image::Content&>(hidden.content())
                   .accessibility()
                   .label.has_value());
  const components::Image graded{"badge", 0.5, library, "Level"};
  EXPECT_EQ(static_cast<const components::Image::Content&>(graded.content())
                .variable_value(),
            std::optional<double>{0.5});
  const components::Image quiet =
      components::Image::decorative("badge", 0.25, library);
  EXPECT_TRUE(static_cast<const components::Image::Content&>(quiet.content())
                  .decorative());
  const components::Image resource{components::Image::Resource{"badge"}};
  EXPECT_TRUE(resource == components::Image{"badge"});
  EXPECT_FALSE(resource == components::Image::system_name("badge"));
  const components::Image ranged =
      components::Image{"badge"}
          .allowed_dynamic_range(components::Image::DynamicRange::high)
          .antialiased(false);
  const auto& ranged_content =
      static_cast<const components::Image::Content&>(ranged.content());
  EXPECT_EQ(ranged_content.dynamic_range(),
            std::optional{components::Image::DynamicRange::high});
  EXPECT_FALSE(ranged_content.rendering().antialiased);
  const Builder builder = [library] {
    return components::VStack(
        components::Image{"missing", library}.id("missing"),
        components::Text("probe").allowed_dynamic_range(
            composition::ImageDynamicRange::constrained_high));
  };
  Application application = start(providers, Element{Scene{&builder}});
  EXPECT_TRUE(node_of(application, "missing").has_value());
}

TEST_F(ImageParityTest, ImagesCompareAndTransferAsTheirSource) {
  static_assert(composition::Transferable<components::Image>);
  const std::shared_ptr<const provider::Image> pixels =
      solid(2, 1, {10, 20, 30, 255});
  const components::Image raster{pixels, 2.0F};
  const auto decoded = components::Image::decoded(raster.encoded());
  ASSERT_TRUE(decoded.has_value());
  const auto& again =
      static_cast<const components::Image::Content&>(decoded->content());
  ASSERT_NE(again.pixels(), nullptr);
  EXPECT_EQ(again.pixels()->width(), 2U);
  EXPECT_FLOAT_EQ(again.scale(), 2.0F);
  EXPECT_EQ(again.pixels()->pixel(1, 0), pixels->pixel(1, 0));
  const components::Image symbol = components::Image::system_name("star");
  const auto symbol_again = components::Image::decoded(symbol.encoded());
  ASSERT_TRUE(symbol_again.has_value());
  EXPECT_TRUE(*symbol_again == symbol);
  const auto asset_again =
      components::Image::decoded(components::Image{"badge"}.encoded());
  ASSERT_TRUE(asset_again.has_value());
  EXPECT_TRUE(*asset_again == components::Image{"badge"});
  EXPECT_FALSE(components::Image::decoded("rgba:2:1:1:!!").has_value());
  const composition::TransferItem item = composition::TransferItem::of(symbol);
  EXPECT_EQ(item.type().identifier(), "public.image");
  EXPECT_TRUE(item.as<components::Image>().has_value());
}

TEST_F(ImageParityTest, DrawnImagesRunTheirRendererAtTheirSize) {
  provider::headless::HeadlessProviderFactory providers;
  const Builder builder = [] {
    return components::VStack(components::Image{
        composition::Size{.width = 30.0F, .height = 20.0F},
        std::string{"Drawn"}, true, graphics::ColorRenderingMode::linear,
        [](components::GraphicsContext& context) {
          context.fill(
              graphics::Path::ellipse_in(composition::Rect{
                  .x = 0.0F, .y = 0.0F, .width = 30.0F, .height = 20.0F}),
              graphics::ShapeStyle{theme::Colour::hex(0x00FF00)});
        }}.id("drawn"));
  };
  Application application = start(providers, Element{Scene{&builder}});
  const auto drawn = node_named(application, "drawn");
  ASSERT_TRUE(drawn.has_value());
  EXPECT_FLOAT_EQ(drawn->get().frame().width, 30.0F);
  EXPECT_FLOAT_EQ(drawn->get().frame().height, 20.0F);
}

TEST_F(ImageParityTest, CanvasResolvesTaggedSymbolsImagesAndTiledShading) {
  const std::shared_ptr<const provider::Image> pixels =
      solid(6, 3, {0, 0, 0, 255});
  const components::GraphicsContext plain{
      composition::Size{.width = 10.0F, .height = 10.0F}};
  const components::GraphicsContext::ResolvedImage resolved =
      plain.resolve(components::Image{pixels, 3.0F});
  EXPECT_FLOAT_EQ(resolved.size().width, 2.0F);
  EXPECT_FLOAT_EQ(resolved.size().height, 1.0F);
  EXPECT_FLOAT_EQ(resolved.baseline(), resolved.size().height);
  EXPECT_FALSE(plain.resolve_symbol(1).has_value());
  const graphics::ShapeStyle tiled = components::GraphicsContext::tiled_image(
      pixels, composition::Point{.x = 3.0F, .y = 1.0F});
  ASSERT_NE(tiled.image_paint(), nullptr);
  EXPECT_FLOAT_EQ(tiled.image_paint()->origin().x, 3.0F);
  auto sizes = std::make_shared<std::vector<composition::Size>>();
  provider::headless::HeadlessProviderFactory providers;
  const Builder builder = [sizes, pixels] {
    return components::VStack(
        components::Canvas(
            [sizes, pixels](components::GraphicsContext& context,
                            composition::Size) {
              if (const auto symbol = context.resolve_symbol(7)) {
                sizes->push_back(symbol->size());
                context.draw(*symbol,
                             composition::Point{.x = 40.0F, .y = 20.0F});
              }
              components::GraphicsContext::ResolvedImage shown =
                  context.resolve(components::Image{pixels, 1.0F});
              shown.shading =
                  graphics::ShapeStyle{theme::Colour::hex(0xFF0000)};
              context.draw(shown, composition::Point{.x = 10.0F, .y = 10.0F});
              graphics::Path square;
              square.add_rect(composition::Rect{
                  .x = 0.0F, .y = 30.0F, .width = 12.0F, .height = 12.0F});
              context.fill(square,
                           components::GraphicsContext::tiled_image(pixels));
            },
            false,
            [] {
              return Element{components::Text("Tagged")
                                 .frame({.width = 24.0F, .height = 12.0F})
                                 .tag(7)};
            })
            .frame({.width = 100.0F, .height = 60.0F}));
  };
  Application application = start(providers, Element{Scene{&builder}});
  frames(application, 4);
  ASSERT_FALSE(sizes->empty());
  EXPECT_FLOAT_EQ(sizes->back().width, 24.0F);
  EXPECT_FLOAT_EQ(sizes->back().height, 12.0F);
}

}
