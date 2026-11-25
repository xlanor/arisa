#include <arisa/components/canvas.hpp>
#include <arisa/components/graphics_context.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/mesh_gradient.hpp>
#include <arisa/components/shape_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/background_environment.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/list_environment.hpp>
#include <arisa/composition/presented_frame.hpp>
#include <arisa/composition/visual_effect.hpp>
#include <arisa/controls/text_input_border_shape.hpp>
#include <arisa/graphics/any_shape.hpp>
#include <arisa/graphics/gradient.hpp>
#include <arisa/graphics/image_paint.hpp>
#include <arisa/graphics/path.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/graphics/stroke_style.hpp>
#include <arisa/provider/assets/asset_path.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/rgb_color_space.hpp>
#include <arisa/theme/shape.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::runtime {

class CanvasRasterTest : public ::testing::Test {
 protected:
  static constexpr int tolerance = 4;
  static constexpr std::uint32_t origin = 60;

  using Builder = std::function<composition::Element()>;
  using Renderer = components::Canvas::Renderer;

  class Scene final : public composition::Component {
   public:
    explicit Scene(Builder builder) : builder_(std::move(builder)) {}

    auto operator==(const Scene&) const -> bool { return false; }

    auto body(composition::Context&) const -> composition::Element override {
      return builder_();
    }

   private:
    Builder builder_;
  };

  static auto placed(composition::Element content) -> composition::Element {
    return components::ZStack(std::move(content))
        .frame({.width = 160.0F, .height = 160.0F})
        .background(theme::Colour::hex(0xFFFFFF), theme::Shape::rectangle());
  }

  static void fonts(provider::headless::HeadlessProviderFactory& providers) {
    const std::string_view file{"arisa/fonts/InterVariable.ttf"};
    std::ifstream stream{std::filesystem::path{ARISA_SOURCE_ASSETS} / file,
                         std::ios::binary};
    const std::vector<char> raw{std::istreambuf_iterator<char>{stream},
                                std::istreambuf_iterator<char>{}};
    std::vector<std::byte> bytes(raw.size());
    std::ranges::transform(raw, bytes.begin(),
                           [](char value) { return std::byte(value); });
    providers.assets().add(*provider::AssetPath::create(file), bytes);
  }

  static auto render(Builder builder, int frames = 4, bool typeset = false)
      -> std::optional<provider::Image> {
    provider::headless::HeadlessProviderFactory providers;
    if (typeset) {
      fonts(providers);
    }
    const provider::SurfaceSize size{.width = provider::SurfaceWidth{160},
                                     .height = provider::SurfaceHeight{160}};
    providers.surface().resize(size);
    providers.gpu().rasterize(size);
    auto created = Application::create(providers, Scene{std::move(builder)});
    EXPECT_TRUE(created.has_value());
    if (!created.has_value()) {
      return std::nullopt;
    }
    Application application = std::move(*created);
    for (int step = 0; step < frames; ++step) {
      EXPECT_TRUE(
          providers.clock().advance(std::chrono::milliseconds{16}).has_value());
      const auto framed = application.frame();
      EXPECT_TRUE(framed.has_value())
          << (framed.has_value() ? std::string{}
                                 : framed.error().explanation());
    }
    auto image = providers.gpu().capture();
    EXPECT_TRUE(image.has_value());
    if (!image.has_value()) {
      return std::nullopt;
    }
    return std::move(*image);
  }

  static auto canvas(Renderer renderer) -> std::optional<provider::Image> {
    return render([renderer] {
      return placed(components::Canvas(renderer).frame(
          {.width = 40.0F, .height = 40.0F}));
    });
  }

  static auto rgb(const provider::Image& image, std::uint32_t x,
                  std::uint32_t y) -> std::array<int, 3> {
    const auto seen = image.pixel(x, y);
    if (!seen.has_value()) {
      return {-1, -1, -1};
    }
    return {(*seen)[0], (*seen)[1], (*seen)[2]};
  }

  static void near(const std::array<int, 3>& seen, std::uint32_t expected,
                   int slack = tolerance) {
    const std::array<int, 3> wanted{static_cast<int>((expected >> 16U) & 0xffU),
                                    static_cast<int>((expected >> 8U) & 0xffU),
                                    static_cast<int>(expected & 0xffU)};
    for (std::size_t channel = 0; channel < 3; ++channel) {
      EXPECT_LE(std::abs(seen[channel] - wanted[channel]), slack)
          << "channel " << channel << " saw " << seen[channel] << " wanted "
          << wanted[channel];
    }
  }

  static auto rect(float x, float y, float width, float height)
      -> graphics::Path {
    return graphics::Path{
        composition::Rect{.x = x, .y = y, .width = width, .height = height}};
  }

  static auto blended(components::GraphicsContext::BlendMode mode)
      -> std::optional<provider::Image> {
    return canvas([mode](components::GraphicsContext& context,
                         composition::Size) {
      context.fill(rect(0.0F, 0.0F, 20.0F, 40.0F),
                   graphics::ShapeStyle{theme::Colour::hex(0x0000FF)});
      context.set_blend_mode(mode);
      context.fill(rect(10.0F, 10.0F, 20.0F, 20.0F),
                   graphics::ShapeStyle{theme::Colour{1.0F, 0.0F, 0.0F, 0.5F}});
    });
  }

  static void expect_blend(components::GraphicsContext::BlendMode mode,
                           std::uint32_t destination, std::uint32_t overlap,
                           std::uint32_t source) {
    SCOPED_TRACE(static_cast<int>(mode));
    const auto image = blended(mode);
    ASSERT_TRUE(image.has_value());
    near(rgb(*image, origin + 5, origin + 5), destination);
    near(rgb(*image, origin + 15, origin + 20), overlap);
    near(rgb(*image, origin + 25, origin + 20), source);
    near(rgb(*image, origin + 35, origin + 35), 0xFFFFFF);
  }
};

TEST_F(CanvasRasterTest, PorterDuffModesMatchTheSwiftUIReference) {
  using Mode = components::GraphicsContext::BlendMode;
  expect_blend(Mode::clear, 0x0000FF, 0xFFFFFF, 0xFFFFFF);
  expect_blend(Mode::copy, 0x0000FF, 0xFF8080, 0xFF8080);
  expect_blend(Mode::source_in, 0x0000FF, 0xFF8080, 0xFFFFFF);
  expect_blend(Mode::source_out, 0x0000FF, 0xFFFFFF, 0xFF8080);
  expect_blend(Mode::destination_in, 0x0000FF, 0x8080FF, 0xFFFFFF);
  expect_blend(Mode::destination_out, 0x0000FF, 0x8080FF, 0xFFFFFF);
  expect_blend(Mode::destination_atop, 0x0000FF, 0x8080FF, 0xFF8080);
  expect_blend(Mode::exclusive_or, 0x0000FF, 0x8080FF, 0xFF8080);
  expect_blend(Mode::source_atop, 0x0000FF, 0x800080, 0xFFFFFF);
  expect_blend(Mode::destination_over, 0x0000FF, 0x0000FF, 0xFF8080);
}

TEST_F(CanvasRasterTest, InverseEvenOddAndLayerClips) {
  const auto inverse =
      canvas([](components::GraphicsContext& context, composition::Size) {
        context.clip(rect(10.0F, 10.0F, 20.0F, 20.0F), graphics::FillStyle{},
                     components::GraphicsContext::ClipOptions::inverse);
        context.fill(rect(0.0F, 0.0F, 40.0F, 40.0F),
                     graphics::ShapeStyle{theme::Colour::hex(0xFF0000)});
      });
  ASSERT_TRUE(inverse.has_value());
  near(rgb(*inverse, origin + 20, origin + 20), 0xFFFFFF);
  near(rgb(*inverse, origin + 5, origin + 5), 0xFF0000);
  const auto even =
      canvas([](components::GraphicsContext& context, composition::Size) {
        graphics::Path ring = rect(0.0F, 0.0F, 40.0F, 40.0F);
        ring.add_rect(composition::Rect{
            .x = 10.0F, .y = 10.0F, .width = 20.0F, .height = 20.0F});
        context.clip(ring, graphics::FillStyle{.eo_fill = true});
        context.fill(rect(0.0F, 0.0F, 40.0F, 40.0F),
                     graphics::ShapeStyle{theme::Colour::hex(0xFF0000)});
      });
  ASSERT_TRUE(even.has_value());
  near(rgb(*even, origin + 20, origin + 20), 0xFFFFFF);
  near(rgb(*even, origin + 5, origin + 5), 0xFF0000);
  const auto layer =
      canvas([](components::GraphicsContext& context, composition::Size) {
        context.clip_to_layer(
            [](components::GraphicsContext& inner) {
              inner.fill(rect(0.0F, 0.0F, 20.0F, 40.0F),
                         graphics::ShapeStyle{theme::Colour::hex(0x000000)});
            },
            0.5F);
        context.fill(rect(0.0F, 0.0F, 40.0F, 40.0F),
                     graphics::ShapeStyle{theme::Colour::hex(0xFF0000)});
      });
  ASSERT_TRUE(layer.has_value());
  near(rgb(*layer, origin + 5, origin + 20), 0xFF8080);
  near(rgb(*layer, origin + 30, origin + 20), 0xFFFFFF);
}

TEST_F(CanvasRasterTest, ClipBoundingRectFollowsTheCurrentTransform) {
  components::GraphicsContext context{{.width = 100.0F, .height = 100.0F}};
  context.clip(rect(10.0F, 20.0F, 30.0F, 40.0F));
  context.translate_by(10.0F, 10.0F);
  EXPECT_EQ(context.clip_bounding_rect(),
            (composition::Rect{
                .x = 0.0F, .y = 10.0F, .width = 30.0F, .height = 40.0F}));
  context.clip(rect(0.0F, 0.0F, 10.0F, 10.0F), graphics::FillStyle{},
               components::GraphicsContext::ClipOptions::inverse);
  EXPECT_EQ(context.clip_bounding_rect().width, 30.0F);
}

TEST_F(CanvasRasterTest, CirclesCentreInWideFrames) {
  const auto image = render([] {
    return placed(components::ShapeView(theme::Circle())
                      .fill(theme::Colour::hex(0x000000))
                      .frame({.width = 100.0F, .height = 40.0F}));
  });
  ASSERT_TRUE(image.has_value());
  near(rgb(*image, 80, 80), 0x000000);
  near(rgb(*image, 80 - 19, 80), 0x000000, 60);
  near(rgb(*image, 80 - 22, 80), 0xFFFFFF);
  near(rgb(*image, 80 + 22, 80), 0xFFFFFF);
  const auto background = render([] {
    return placed(
        components::VStack()
            .frame({.width = 100.0F, .height = 40.0F})
            .background(theme::Colour::hex(0x000000), theme::Circle()));
  });
  ASSERT_TRUE(background.has_value());
  near(rgb(*background, 80, 80), 0x000000);
  near(rgb(*background, 80 - 25, 80), 0xFFFFFF);
  near(rgb(*background, 80 + 25, 80), 0xFFFFFF);
}

TEST_F(CanvasRasterTest, ShapeViewsFillThenStroke) {
  const auto image = render([] {
    return placed(components::ShapeView(theme::Circle())
                      .fill(theme::Colour::hex(0xFF0000))
                      .stroke(theme::Colour::hex(0x0000FF), 8.0F)
                      .frame({.width = 40.0F, .height = 40.0F}));
  });
  ASSERT_TRUE(image.has_value());
  near(rgb(*image, 80, 80), 0xFF0000);
  near(rgb(*image, 80, 61), 0x0000FF);
  const components::ShapeView plain =
      components::ShapeView(theme::Circle()).fill(theme::Colour::hex(0xFF0000));
  EXPECT_FALSE(
      static_cast<const components::ShapeView::Content&>(plain.content())
          .has_background());
  const components::ShapeView layered =
      components::ShapeView(theme::Circle())
          .fill(theme::Colour::hex(0xFF0000))
          .stroke_border(theme::Colour::hex(0x0000FF), 2.0F, false);
  const auto& content =
      static_cast<const components::ShapeView::Content&>(layered.content());
  EXPECT_TRUE(content.has_background());
  EXPECT_FALSE(content.is_antialiased());
}

TEST_F(CanvasRasterTest, AliasedStrokesHaveHardEdges) {
  const auto image = render([] {
    graphics::Path line;
    line.move({.x = 0.0F, .y = 20.3F});
    line.add_line({.x = 40.0F, .y = 20.3F});
    return placed(components::ShapeView(line)
                      .stroke(theme::Colour::hex(0x000000), 3.0F, false)
                      .frame({.width = 40.0F, .height = 40.0F}));
  });
  ASSERT_TRUE(image.has_value());
  int partial = 0;
  for (std::uint32_t y = origin + 15; y < origin + 26; ++y) {
    const int level = rgb(*image, origin + 20, y)[0];
    if (level > 16 && level < 239) {
      ++partial;
    }
  }
  EXPECT_EQ(partial, 0);
}

TEST_F(CanvasRasterTest, ReverseMasksKeepWhatTheMaskDoesNotCover) {
  const auto image = render([] {
    return placed(
        components::VStack()
            .frame({.width = 40.0F, .height = 40.0F})
            .background(theme::Colour::hex(0x0000FF), theme::Shape::rectangle())
            .reverse_mask(composition::Alignment::top_leading,
                          components::VStack()
                              .frame({.width = 20.0F, .height = 40.0F})
                              .background(theme::Colour::hex(0x000000),
                                          theme::Shape::rectangle())));
  });
  ASSERT_TRUE(image.has_value());
  near(rgb(*image, origin + 5, origin + 20), 0xFFFFFF);
  near(rgb(*image, origin + 30, origin + 20), 0x0000FF);
}

TEST_F(CanvasRasterTest, ShadingsFillWithBackdropMeshAndRepeatingRamps) {
  const auto backdrop =
      canvas([](components::GraphicsContext& context, composition::Size) {
        context.fill(rect(0.0F, 0.0F, 40.0F, 40.0F),
                     graphics::ShapeStyle{theme::Colour::hex(0xFF0000)});
        context.fill(rect(10.0F, 10.0F, 20.0F, 20.0F),
                     components::GraphicsContext::Shading::backdrop());
      });
  ASSERT_TRUE(backdrop.has_value());
  near(rgb(*backdrop, origin + 20, origin + 20), 0xFFFFFF);
  near(rgb(*backdrop, origin + 5, origin + 5), 0xFF0000);
  const auto repeated =
      canvas([](components::GraphicsContext& context, composition::Size) {
        const graphics::Gradient ramp{std::vector<theme::ColourValue>{
            theme::Colour::hex(0x000000), theme::Colour::hex(0xFFFFFF)}};
        context.fill(rect(0.0F, 0.0F, 40.0F, 40.0F),
                     components::GraphicsContext::Shading::linear_gradient(
                         ramp, {.x = 0.0F, .y = 0.0F}, {.x = 10.0F, .y = 0.0F},
                         components::GraphicsContext::GradientOptions::repeat));
      });
  ASSERT_TRUE(repeated.has_value());
  EXPECT_LT(rgb(*repeated, origin + 11, origin + 20)[0], 60);
  EXPECT_GT(rgb(*repeated, origin + 19, origin + 20)[0], 200);
  EXPECT_LT(rgb(*repeated, origin + 21, origin + 20)[0], 60);
  const auto mirrored =
      canvas([](components::GraphicsContext& context, composition::Size) {
        const graphics::Gradient ramp{std::vector<theme::ColourValue>{
            theme::Colour::hex(0x000000), theme::Colour::hex(0xFFFFFF)}};
        context.fill(rect(0.0F, 0.0F, 40.0F, 40.0F),
                     components::GraphicsContext::Shading::linear_gradient(
                         ramp, {.x = 0.0F, .y = 0.0F}, {.x = 10.0F, .y = 0.0F},
                         components::GraphicsContext::GradientOptions::mirror));
      });
  ASSERT_TRUE(mirrored.has_value());
  EXPECT_GT(rgb(*mirrored, origin + 11, origin + 20)[0], 200);
  EXPECT_LT(rgb(*mirrored, origin + 19, origin + 20)[0], 60);
  const auto mesh = canvas([](components::GraphicsContext& context,
                              composition::Size) {
    const components::MeshGradient gradient{
        2, 2,
        std::vector<composition::Point>{{.x = 0.0F, .y = 0.0F},
                                        {.x = 1.0F, .y = 0.0F},
                                        {.x = 0.0F, .y = 1.0F},
                                        {.x = 1.0F, .y = 1.0F}},
        std::vector<theme::ColourValue>{
            theme::Colour::hex(0xFF0000), theme::Colour::hex(0xFF0000),
            theme::Colour::hex(0xFF0000), theme::Colour::hex(0xFF0000)}};
    context.fill(rect(10.0F, 10.0F, 20.0F, 20.0F),
                 components::GraphicsContext::Shading::mesh_gradient(gradient));
  });
  ASSERT_TRUE(mesh.has_value());
  near(rgb(*mesh, origin + 20, origin + 20), 0xFF0000);
  near(rgb(*mesh, origin + 5, origin + 5), 0xFFFFFF);
}

TEST_F(CanvasRasterTest, FiltersShadowThresholdAndMultiplyAlpha) {
  const auto shadowed =
      canvas([](components::GraphicsContext& context, composition::Size) {
        context.add_filter(components::GraphicsContext::Filter::shadow(
            theme::Colour::hex(0x000000), 0.0F, 10.0F, 0.0F));
        context.fill(rect(0.0F, 10.0F, 20.0F, 20.0F),
                     graphics::ShapeStyle{theme::Colour::hex(0xFF0000)});
      });
  ASSERT_TRUE(shadowed.has_value());
  near(rgb(*shadowed, origin + 5, origin + 20), 0xFF0000);
  near(rgb(*shadowed, origin + 25, origin + 20), 0x000000);
  near(rgb(*shadowed, origin + 35, origin + 20), 0xFFFFFF);
  const auto only =
      canvas([](components::GraphicsContext& context, composition::Size) {
        context.add_filter(components::GraphicsContext::Filter::shadow(
            theme::Colour::hex(0x000000), 0.0F, 10.0F, 0.0F,
            components::GraphicsContext::BlendMode::normal,
            components::GraphicsContext::ShadowOptions::shadow_only));
        context.fill(rect(0.0F, 10.0F, 20.0F, 20.0F),
                     graphics::ShapeStyle{theme::Colour::hex(0xFF0000)});
      });
  ASSERT_TRUE(only.has_value());
  near(rgb(*only, origin + 5, origin + 20), 0xFFFFFF);
  near(rgb(*only, origin + 25, origin + 20), 0x000000);
  const auto threshold = canvas([](components::GraphicsContext& context,
                                   composition::Size) {
    context.add_filter(components::GraphicsContext::Filter::alpha_threshold(
        0.4F, 1.0F, theme::Colour::hex(0x00FF00)));
    context.fill(rect(0.0F, 0.0F, 20.0F, 40.0F),
                 graphics::ShapeStyle{theme::Colour{1.0F, 0.0F, 0.0F, 0.6F}});
    context.fill(rect(20.0F, 0.0F, 20.0F, 40.0F),
                 graphics::ShapeStyle{theme::Colour{1.0F, 0.0F, 0.0F, 0.2F}});
  });
  ASSERT_TRUE(threshold.has_value());
  near(rgb(*threshold, origin + 5, origin + 20), 0x00FF00);
  near(rgb(*threshold, origin + 30, origin + 20), 0xFFFFFF);
  const auto multiplied =
      canvas([](components::GraphicsContext& context, composition::Size) {
        context.add_filter(components::GraphicsContext::Filter::alpha_multiply(
            theme::Colour::hex(0x00FF00)));
        context.fill(rect(0.0F, 0.0F, 40.0F, 40.0F),
                     graphics::ShapeStyle{theme::Colour::hex(0xFF0000)});
      });
  ASSERT_TRUE(multiplied.has_value());
  near(rgb(*multiplied, origin + 20, origin + 20), 0x00FF00);
}

TEST_F(CanvasRasterTest, PremultipliedAndLinearMatricesDifferFromPlain) {
  const graphics::ColourMatrix halve = graphics::ColourMatrix::of(
      {0.5F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.5F, 0.0F, 0.0F, 0.0F,
       0.0F, 0.0F, 0.5F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F});
  const auto level = [&halve](bool linear) {
    const auto image =
        canvas([&halve, linear](components::GraphicsContext& context,
                                composition::Size) {
          context.add_filter(
              components::GraphicsContext::Filter::colour_matrix(halve),
              linear ? components::GraphicsContext::FilterOptions::linear_color
                     : components::GraphicsContext::FilterOptions{});
          context.fill(rect(0.0F, 0.0F, 40.0F, 40.0F),
                       graphics::ShapeStyle{theme::Colour::hex(0xFFFFFF)});
        });
    EXPECT_TRUE(image.has_value());
    return image.has_value() ? rgb(*image, origin + 20, origin + 20)[0] : -1;
  };
  EXPECT_NEAR(level(false), 128, tolerance);
  EXPECT_NEAR(level(true), 188, tolerance);
}

TEST_F(CanvasRasterTest, ResizableDrawnImagesRedrawAtTheirFrame) {
  const auto image = render([] {
    return placed(
        components::Image(
            composition::Size{.width = 10.0F, .height = 10.0F}, std::nullopt,
            false, graphics::ColorRenderingMode::non_linear,
            [](components::GraphicsContext& context) {
              context.fill(
                  graphics::Path{composition::Rect{
                      .x = 0.0F, .y = 0.0F, .width = 5.0F, .height = 10.0F}},
                  graphics::ShapeStyle{theme::Colour::hex(0x000000)});
            })
            .resizable()
            .frame({.width = 40.0F, .height = 40.0F}));
  });
  ASSERT_TRUE(image.has_value());
  near(rgb(*image, origin + 10, origin + 20), 0x000000);
  near(rgb(*image, origin + 30, origin + 20), 0xFFFFFF);
}

TEST_F(CanvasRasterTest, BezierMeshesCurveTheirPatches) {
  const auto flat = [](float bend) {
    std::vector<components::MeshGradient::BezierPoint> points;
    for (int row = 0; row < 2; ++row) {
      for (int column = 0; column < 2; ++column) {
        const composition::Point at{.x = static_cast<float>(column),
                                    .y = static_cast<float>(row)};
        points.push_back(components::MeshGradient::BezierPoint{
            .position = at,
            .leading_control_point = {.x = at.x - 0.3F, .y = at.y},
            .top_control_point = {.x = at.x, .y = at.y - 0.3F},
            .trailing_control_point = {.x = at.x + 0.3F,
                                       .y = at.y + (row == 0 ? bend : 0.0F)},
            .bottom_control_point = {.x = at.x, .y = at.y + 0.3F}});
      }
    }
    return points;
  };
  const auto image =
      [](std::vector<components::MeshGradient::BezierPoint> points) {
        return render([points] {
          return placed(
              components::MeshGradient(
                  2, 2, points,
                  std::vector<theme::ColourValue>{theme::Colour::hex(0xFF0000),
                                                  theme::Colour::hex(0xFF0000),
                                                  theme::Colour::hex(0x0000FF),
                                                  theme::Colour::hex(0x0000FF)},
                  theme::Colour{1.0F, 1.0F, 1.0F, 1.0F})
                  .frame({.width = 40.0F, .height = 40.0F}));
        });
      };
  const auto straight = image(flat(0.0F));
  const auto bent = image(flat(0.6F));
  ASSERT_TRUE(straight.has_value());
  ASSERT_TRUE(bent.has_value());
  EXPECT_NE(rgb(*straight, origin + 13, origin + 2),
            rgb(*bent, origin + 13, origin + 2));
  const components::MeshGradient::Locations locations =
      components::MeshGradient::Locations::bezier_points(flat(0.0F));
  EXPECT_EQ(locations,
            components::MeshGradient::Locations::bezier_points(flat(0.0F)));
  EXPECT_NE(locations, components::MeshGradient::Locations::points({}));
  const components::MeshGradient::Colors colours =
      components::MeshGradient::Colors::resolved_colors(
          {theme::Colour::hex(0xFF0000)});
  EXPECT_EQ(colours.values().size(), 1U);
  EXPECT_NE(colours, components::MeshGradient::Colors::colors({}));
}

TEST_F(CanvasRasterTest, PathContentShapesHitTestTheirOutline) {
  composition::PresentedFrame frame;
  frame.corners = {composition::Point{.x = 0.0F, .y = 0.0F},
                   composition::Point{.x = 100.0F, .y = 0.0F},
                   composition::Point{.x = 100.0F, .y = 40.0F},
                   composition::Point{.x = 0.0F, .y = 40.0F}};
  frame.bounds =
      composition::Rect{.x = 0.0F, .y = 0.0F, .width = 100.0F, .height = 40.0F};
  frame.hit_shape =
      composition::PresentedClip{.corners = frame.corners,
                                 .radii = {},
                                 .ellipse = false,
                                 .shape = graphics::AnyShape{theme::Circle()},
                                 .fill = {}};
  EXPECT_TRUE(frame.hits({.x = 50.0F, .y = 20.0F}));
  EXPECT_FALSE(frame.hits({.x = 10.0F, .y = 20.0F}));
  const composition::Element shaped =
      components::VStack().content_shape(theme::Circle());
  EXPECT_TRUE(shaped.interactions().hit_path.has_value());
  graphics::Path ring{
      composition::Rect{.x = 0.0F, .y = 0.0F, .width = 40.0F, .height = 40.0F}};
  ring.add_rect(composition::Rect{
      .x = 10.0F, .y = 10.0F, .width = 20.0F, .height = 20.0F});
  const composition::Element even =
      components::VStack().content_shape(graphics::AnyShape{ring}, true);
  ASSERT_TRUE(even.interactions().hit_path.has_value());
  EXPECT_TRUE(even.interactions().hit_fill.eo_fill);
  const composition::Element kinds = components::VStack().content_shape(
      composition::ContentShapeKinds::accessibility |
          composition::ContentShapeKinds::context_menu_preview,
      graphics::AnyShape{ring});
  EXPECT_TRUE(kinds.interactions().accessibility_shape.has_value());
  EXPECT_TRUE(kinds.interactions().context_menu_preview_shape.has_value());
}

TEST_F(CanvasRasterTest, ChainedShadersLayerInOrder) {
  const graphics::Shader first{
      graphics::ShaderFunction{1, graphics::ShaderFunction::Kind::colour},
      {4.0F}};
  const graphics::Shader second{
      graphics::ShaderFunction{1, graphics::ShaderFunction::Kind::colour},
      {8.0F}};
  const composition::Element chained =
      components::VStack().color_effect(first).color_effect(second);
  ASSERT_TRUE(chained.modifiers().graphics.shader.has_value());
  EXPECT_EQ(*chained.modifiers().graphics.shader, second);
  ASSERT_EQ(chained.children().size(), 1U);
  ASSERT_TRUE(chained.children()[0].modifiers().graphics.shader.has_value());
  EXPECT_EQ(*chained.children()[0].modifiers().graphics.shader, first);
}

TEST_F(CanvasRasterTest, BackgroundEnvironmentFollowsTheModifiers) {
  const composition::Element styled =
      components::VStack().background_style(graphics::ShapeStyle::black());
  const auto style =
      styled.declared_environment(composition::BackgroundEnvironment::style);
  ASSERT_TRUE(style.has_value());
  EXPECT_EQ(style->get(), std::optional{graphics::ShapeStyle::black()});
  const composition::Element material =
      components::VStack().background(theme::Material::bar);
  const auto declared = material.declared_environment(
      composition::BackgroundEnvironment::material);
  ASSERT_TRUE(declared.has_value());
  EXPECT_EQ(declared->get(), std::optional{theme::Material::bar});
  const composition::Element prominent =
      components::VStack().background_prominence(
          composition::BackgroundProminence::increased);
  const auto prominence = prominent.declared_environment(
      composition::BackgroundEnvironment::prominence);
  ASSERT_TRUE(prominence.has_value());
  EXPECT_EQ(prominence->get(), composition::BackgroundProminence::increased);
  EXPECT_FALSE(
      composition::BackgroundEnvironment::content_transition_adds_drawing_group
          .fallback());
}

TEST_F(CanvasRasterTest, AlternatingRowBehaviourOverridesTheListDefault) {
  using Behaviour = composition::AlternatingRowBackgroundBehavior;
  EXPECT_TRUE(Behaviour::enabled.alternates(false));
  EXPECT_FALSE(Behaviour::disabled.alternates(true));
  EXPECT_TRUE(Behaviour::automatic.alternates(true));
  EXPECT_FALSE(Behaviour::automatic.alternates(false));
  const composition::Element rows =
      components::VStack().alternating_row_backgrounds(Behaviour::disabled);
  const auto declared = rows.declared_environment(
      composition::ListEnvironment::alternating_row_background_behavior);
  ASSERT_TRUE(declared.has_value());
  EXPECT_EQ(declared->get(), Behaviour::disabled);
}

TEST_F(CanvasRasterTest, VisualEffectsBlendAndBlurOpaquely) {
  const composition::VisualEffect effect =
      composition::VisualEffect{}
          .blend_mode(graphics::BlendMode::multiply)
          .blur(4.0F, true);
  EXPECT_EQ(effect.blend(), graphics::BlendMode::multiply);
  EXPECT_TRUE(effect.blur_opaque());
  EXPECT_EQ(effect.blur_radius(), 4.0F);
  EXPECT_EQ(controls::TextInputBorderShape::text_input_border(),
            controls::TextInputBorderShape::automatic());
}

TEST_F(CanvasRasterTest, ShadingConstructorsPaint) {
  using Shading = components::GraphicsContext::Shading;
  const graphics::Gradient ramp{std::vector<theme::ColourValue>{
      theme::Colour::hex(0xFF0000), theme::Colour::hex(0x0000FF)}};
  const std::vector<Shading> shadings{
      Shading::color(theme::RGBColorSpace::display_p3, 0.5F, 0.5F, 0.2F),
      Shading::color(theme::RGBColorSpace::srgb_linear, 0.5F),
      Shading::palette({graphics::ShapeStyle{theme::Colour::hex(0x00FF00)}}),
      Shading::radial_gradient(ramp, {.x = 20.0F, .y = 20.0F}, 0.0F, 20.0F),
      Shading::conic_gradient(ramp, {.x = 20.0F, .y = 20.0F}),
      Shading::shader(
          graphics::Shader{graphics::ShaderFunction{
              0, graphics::ShaderFunction::Kind::colour}},
          composition::Rect{
              .x = 0.0F, .y = 0.0F, .width = 40.0F, .height = 40.0F}),
  };
  ASSERT_NE(shadings[0].style(), nullptr);
  const auto first = canvas([&shadings](components::GraphicsContext& context,
                                        composition::Size) {
    context.fill(rect(0.0F, 0.0F, 40.0F, 40.0F), context.resolve(shadings[0]));
  });
  ASSERT_TRUE(first.has_value());
  near(rgb(*first, origin + 20, origin + 20), 0x808021);
  for (std::size_t index = 1; index < 5; ++index) {
    SCOPED_TRACE(index);
    const auto image =
        canvas([&shadings, index](components::GraphicsContext& context,
                                  composition::Size) {
          context.fill(rect(0.0F, 0.0F, 40.0F, 40.0F), shadings[index]);
        });
    ASSERT_TRUE(image.has_value());
    EXPECT_NE(rgb(*image, origin + 30, origin + 25),
              (std::array<int, 3>{255, 255, 255}));
  }
}

TEST_F(CanvasRasterTest, ResolvedTextMeasuresAfterTheFirstFrame) {
  auto measured = std::make_shared<composition::Size>();
  auto baseline = std::make_shared<float>(0.0F);
  const auto image = render(
      [measured, baseline] {
        return placed(
            components::Canvas(
                false, true,
                [measured, baseline](components::GraphicsContext& context,
                                     composition::Size) {
                  const components::GraphicsContext::ResolvedText text =
                      context.resolve(components::Text("Wide label"));
                  *measured = text.measure({.width = 160.0F, .height = 160.0F});
                  *baseline =
                      text.first_baseline({.width = 160.0F, .height = 160.0F});
                  context.draw(text,
                               composition::Point{.x = 20.0F, .y = 20.0F});
                })
                .frame({.width = 40.0F, .height = 40.0F}));
      },
      6, true);
  ASSERT_TRUE(image.has_value());
  EXPECT_GT(measured->width, 20.0F);
  EXPECT_GT(measured->height, 5.0F);
  EXPECT_GT(*baseline, 0.0F);
  const components::Canvas asynchronous{
      false, true, [](components::GraphicsContext&, composition::Size) {}};
  EXPECT_TRUE(
      static_cast<const components::Canvas::Content&>(asynchronous.content())
          .renders_asynchronously());
}

TEST_F(CanvasRasterTest, ShadowOptionsPlaceTheShadow) {
  using Context = components::GraphicsContext;
  const auto shadowed = [](Context::ShadowOptions options) {
    return canvas([options](Context& context, composition::Size) {
      context.add_filter(
          Context::Filter::shadow(theme::Colour::hex(0x000000), 0.0F, 10.0F,
                                  0.0F, Context::BlendMode::normal, options));
      context.fill(rect(0.0F, 10.0F, 20.0F, 20.0F),
                   graphics::ShapeStyle{theme::Colour::hex(0xFF0000)});
    });
  };
  const auto above = shadowed(Context::ShadowOptions::shadow_above);
  ASSERT_TRUE(above.has_value());
  near(rgb(*above, origin + 15, origin + 20), 0x000000);
  near(rgb(*above, origin + 5, origin + 20), 0xFF0000);
  const auto inner = shadowed(Context::ShadowOptions::inverts_alpha);
  ASSERT_TRUE(inner.has_value());
  near(rgb(*inner, origin + 5, origin + 20), 0x000000);
  near(rgb(*inner, origin + 15, origin + 20), 0xFF0000);
  near(rgb(*inner, origin + 25, origin + 20), 0xFFFFFF);
  const auto grouped = shadowed(Context::ShadowOptions::disables_group);
  ASSERT_TRUE(grouped.has_value());
  near(rgb(*grouped, origin + 25, origin + 20), 0x000000);
  EXPECT_TRUE((Context::ShadowOptions::shadow_only |
               Context::ShadowOptions::inverts_alpha)
                  .contains(Context::ShadowOptions::shadow_only));
  EXPECT_TRUE(Context::BlurOptions{}.empty());
  EXPECT_FALSE(
      (Context::BlurOptions::opaque | Context::BlurOptions::dithers_result)
          .empty());
  EXPECT_TRUE(Context::GradientOptions::linear_color.contains(
      Context::GradientOptions::linear_color));
}

TEST_F(CanvasRasterTest, FilterKindsAndImagesDraw) {
  using Context = components::GraphicsContext;
  const graphics::Shader shader{
      graphics::ShaderFunction{0, graphics::ShaderFunction::Kind::colour}};
  const std::vector<Context::Filter> filters{
      Context::Filter::blur(2.0F, Context::BlurOptions::opaque),
      Context::Filter::grayscale(1.0),
      Context::Filter::color_shader(shader),
      Context::Filter::distortion_shader(
          graphics::Shader{graphics::ShaderFunction{
              0, graphics::ShaderFunction::Kind::distortion}},
          {.width = 4.0F, .height = 4.0F}),
      Context::Filter::layer_shader(
          graphics::Shader{graphics::ShaderFunction{
              0, graphics::ShaderFunction::Kind::layer}},
          {.width = 4.0F, .height = 4.0F}),
      Context::Filter::projection_transform(
          graphics::ProjectionTransform::identity()),
  };
  for (std::size_t index = 0; index < filters.size(); ++index) {
    SCOPED_TRACE(index);
    const auto image =
        canvas([&filters, index](Context& context, composition::Size) {
          context.add_filter(filters[index]);
          context.fill(rect(0.0F, 0.0F, 40.0F, 40.0F),
                       graphics::ShapeStyle{theme::Colour::hex(0x000000)});
        });
    ASSERT_TRUE(image.has_value());
    EXPECT_LT(rgb(*image, origin + 20, origin + 20)[0], 128);
  }
  const auto drawn = canvas([](Context& context, composition::Size) {
    const components::Image picture{
        composition::Size{.width = 10.0F, .height = 10.0F}, std::nullopt, false,
        graphics::ColorRenderingMode::non_linear, [](Context& inner) {
          inner.fill(rect(0.0F, 0.0F, 10.0F, 10.0F),
                     graphics::ShapeStyle{theme::Colour::hex(0x0000FF)});
        }};
    context.draw(picture.duplicate(),
                 composition::Rect{
                     .x = 0.0F, .y = 0.0F, .width = 20.0F, .height = 20.0F},
                 graphics::FillStyle{.eo_fill = false, .antialiased = false});
    context.draw(picture.duplicate(),
                 composition::Point{.x = 30.0F, .y = 30.0F});
    const Context::ResolvedImage resolved =
        context.resolve(picture.duplicate());
    EXPECT_GT(resolved.size().width, 0.0F);
  });
  ASSERT_TRUE(drawn.has_value());
  near(rgb(*drawn, origin + 10, origin + 10), 0x0000FF);
  near(rgb(*drawn, origin + 30, origin + 30), 0x0000FF);
}

TEST_F(CanvasRasterTest, ShapeBackgroundsAndOverlaysTakeAnyShape) {
  const auto image = render([] {
    graphics::Path ring{composition::Rect{
        .x = 0.0F, .y = 0.0F, .width = 40.0F, .height = 40.0F}};
    ring.add_rect(composition::Rect{
        .x = 10.0F, .y = 10.0F, .width = 20.0F, .height = 20.0F});
    return placed(
        components::VStack()
            .frame({.width = 40.0F, .height = 40.0F})
            .background(graphics::ShapeStyle{theme::Colour::hex(0xFF0000)},
                        graphics::AnyShape{ring},
                        graphics::FillStyle{.eo_fill = true})
            .overlay(graphics::ShapeStyle{theme::Colour::hex(0x0000FF)},
                     graphics::AnyShape{theme::Circle()}.size(8.0F, 8.0F)));
  });
  ASSERT_TRUE(image.has_value());
  near(rgb(*image, origin + 20, origin + 20), 0xFFFFFF);
  near(rgb(*image, origin + 35, origin + 35), 0xFF0000);
  near(rgb(*image, origin + 4, origin + 4), 0x0000FF);
  const auto edges = render([] {
    return placed(
        components::VStack()
            .frame({.width = 40.0F, .height = 40.0F})
            .background(graphics::ShapeStyle{theme::Colour::hex(0x00FF00)},
                        theme::Edge::all)
            .background_style(
                graphics::ShapeStyle{theme::Colour::hex(0x00FF00)}));
  });
  ASSERT_TRUE(edges.has_value());
  near(rgb(*edges, origin + 20, origin + 20), 0x00FF00);
  const auto plain = render([] {
    return placed(components::VStack()
                      .frame({.width = 40.0F, .height = 40.0F})
                      .background(theme::Edge::all)
                      .background(graphics::AnyShape{theme::Circle()}));
  });
  ASSERT_TRUE(plain.has_value());
}

}
