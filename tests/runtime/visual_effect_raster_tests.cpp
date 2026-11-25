#include <arisa/components/canvas.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/glass_effect_container.hpp>
#include <arisa/components/graphics_context.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/shape_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/content_margin_placement.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/namespace.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/visual_effect.hpp>
#include <arisa/graphics/affine_transform.hpp>
#include <arisa/graphics/blend_mode.hpp>
#include <arisa/graphics/colour_matrix.hpp>
#include <arisa/graphics/gradient.hpp>
#include <arisa/graphics/path.hpp>
#include <arisa/graphics/projection_transform.hpp>
#include <arisa/graphics/rotation_3d.hpp>
#include <arisa/graphics/shader.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/motion/angle.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/provider/diagnostics/provider.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/headless/null_gpu.hpp>
#include <arisa/rendering/paint_error.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/frame_timings.hpp>
#include <arisa/runtime/image_renderer.hpp>
#include <arisa/theme/color.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/shape.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::runtime {

class VisualEffectRasterTest : public ::testing::Test {
 protected:
  static constexpr std::uint32_t source_rgb = 0xCC6633;
  static constexpr std::uint32_t backdrop_rgb = 0x3399E6;
  static constexpr int tolerance = 3;

  using Builder = std::function<composition::Element()>;

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

  static auto swatch(std::uint32_t rgb, float width = 40.0F,
                     float height = 40.0F) -> composition::Element {
    return components::VStack()
        .frame({.width = width, .height = height})
        .background(theme::Colour::hex(rgb), theme::Shape::rectangle());
  }

  static auto placed(composition::Element content) -> composition::Element {
    return components::ZStack(std::move(content))
        .frame({.width = 160.0F, .height = 160.0F})
        .background(theme::Colour::hex(0xFFFFFF), theme::Shape::rectangle());
  }

  using Configure = std::function<void(provider::headless::NullGpu&)>;

  struct Rendered {
    std::optional<provider::Image> image;
    std::vector<provider::Diagnostic> reports;
  };

  static auto rendered(Builder builder, const Configure& configure)
      -> Rendered {
    provider::headless::HeadlessProviderFactory providers;
    const provider::SurfaceSize size{.width = provider::SurfaceWidth{160},
                                     .height = provider::SurfaceHeight{160}};
    providers.surface().resize(size);
    providers.gpu().rasterize(size);
    configure(providers.gpu());
    auto created = Application::create(providers, Scene{std::move(builder)});
    EXPECT_TRUE(created.has_value());
    if (!created.has_value()) {
      return {};
    }
    Application application = std::move(*created);
    for (int step = 0; step < 4; ++step) {
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
      return {};
    }
    return Rendered{.image = std::move(*image),
                    .reports = providers.diagnostics().snapshot()};
  }

  struct Shift {
    std::optional<composition::StateHandle<float>> handle;
    std::optional<composition::StateHandle<int>> phase;
  };

  class Shifted final : public composition::Component {
   public:
    static constexpr composition::StateKey<float> offset{"shift", 0.0F};

    Shifted(float start, Shift* shift) noexcept
        : start_(start), shift_(shift) {}

    auto operator==(const Shifted&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      const auto moved = context.state(offset);
      shift_->handle = moved;
      const float at = start_ + moved.get();
      return placed(components::ZStack(swatch(source_rgb, 80.0F, 80.0F),
                                       components::Text("Ag").foreground_style(
                                           theme::Colour::hex(0x000000)))
                        .frame({.width = 60.0F, .height = 60.0F})
                        .clip_shape(theme::RoundedRectangle(12.0F))
                        .offset(at, at));
    }

   private:
    float start_;
    Shift* shift_;
  };

  static auto shifted(provider::headless::HeadlessProviderFactory& providers,
                      Shifted scene) -> Application {
    const provider::SurfaceSize size{.width = provider::SurfaceWidth{160},
                                     .height = provider::SurfaceHeight{160}};
    providers.surface().resize(size);
    providers.gpu().rasterize(size);
    auto created = Application::create(providers, std::move(scene));
    EXPECT_TRUE(created.has_value());
    Application application = std::move(*created);
    for (int step = 0; step < 4; ++step) {
      EXPECT_TRUE(
          providers.clock().advance(std::chrono::milliseconds{16}).has_value());
      EXPECT_TRUE(application.frame().has_value());
    }
    return application;
  }

  class Handover final : public composition::Component {
   public:
    static constexpr composition::StateKey<int> phase{"phase", 0};

    explicit Handover(Shift* shift) noexcept : shift_(shift) {}

    auto operator==(const Handover&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      const auto shown = context.state(phase);
      shift_->phase = shown;
      if (shown.get() == 0) {
        return placed(swatch(source_rgb, 150.0F, 150.0F)
                          .compositing_group()
                          .opacity(0.5F));
      }
      if (shown.get() == 1) {
        return placed(swatch(source_rgb, 10.0F, 10.0F));
      }
      return placed(components::ZStack(swatch(0x000000, 80.0F, 80.0F))
                        .frame({.width = 60.0F, .height = 60.0F})
                        .clip_shape(theme::RoundedRectangle(12.0F)));
    }

   private:
    Shift* shift_;
  };

  struct Wedge {
    auto path(composition::Rect rect) const -> graphics::Path {
      graphics::Path made;
      made.move({.x = rect.x + (rect.width * 0.5F), .y = rect.y});
      made.add_line({.x = rect.x + rect.width, .y = rect.y + rect.height});
      made.add_line({.x = rect.x, .y = rect.y + rect.height});
      made.close_subpath();
      return made;
    }

    friend auto operator==(const Wedge&, const Wedge&) -> bool = default;
  };

  static auto pixels(std::uint32_t rgb, std::uint32_t width,
                     std::uint32_t height)
      -> std::shared_ptr<const provider::Image> {
    std::vector<std::byte> bytes(static_cast<std::size_t>(width) * height *
                                 provider::Image::channels);
    for (std::size_t index = 0; index < bytes.size();
         index += provider::Image::channels) {
      bytes[index] = static_cast<std::byte>((rgb >> 16U) & 0xffU);
      bytes[index + 1] = static_cast<std::byte>((rgb >> 8U) & 0xffU);
      bytes[index + 2] = static_cast<std::byte>(rgb & 0xffU);
      bytes[index + 3] = std::byte{0xFF};
    }
    return std::make_shared<const provider::Image>(
        provider::Image::create(width, height, std::move(bytes)).value());
  }

  static auto halves(std::uint32_t left, std::uint32_t right,
                     std::uint32_t width, std::uint32_t height)
      -> std::shared_ptr<const provider::Image> {
    std::vector<std::byte> bytes(static_cast<std::size_t>(width) * height *
                                 provider::Image::channels);
    for (std::uint32_t y = 0; y < height; ++y) {
      for (std::uint32_t x = 0; x < width; ++x) {
        const std::uint32_t rgb = x < width / 2 ? left : right;
        const std::size_t index = ((static_cast<std::size_t>(y) * width) + x) *
                                  provider::Image::channels;
        bytes[index] = static_cast<std::byte>((rgb >> 16U) & 0xffU);
        bytes[index + 1] = static_cast<std::byte>((rgb >> 8U) & 0xffU);
        bytes[index + 2] = static_cast<std::byte>(rgb & 0xffU);
        bytes[index + 3] = std::byte{0xFF};
      }
    }
    return std::make_shared<const provider::Image>(
        provider::Image::create(width, height, std::move(bytes)).value());
  }

  static auto render(Builder builder) -> std::optional<provider::Image> {
    return rendered(std::move(builder), [](provider::headless::NullGpu&) {})
        .image;
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

  static void adjusted(
      std::function<composition::Element(composition::Element)> apply,
      std::uint32_t expected) {
    const auto image =
        render([apply] { return placed(apply(swatch(source_rgb))); });
    ASSERT_TRUE(image.has_value());
    near(rgb(*image, 80, 80), expected);
  }

  static void blended(graphics::BlendMode mode, std::uint32_t expected,
                      int slack = tolerance) {
    const auto image = render([mode] {
      return placed(components::ZStack(swatch(backdrop_rgb, 80.0F, 80.0F),
                                       swatch(source_rgb).blend_mode(mode)));
    });
    ASSERT_TRUE(image.has_value());
    near(rgb(*image, 80, 80), expected, slack);
  }
};

TEST_F(VisualEffectRasterTest, ColourAdjustmentsMatchTheSwiftUIReference) {
  adjusted([](composition::Element e) { return std::move(e).brightness(0.2); },
           0xFF9966);
  adjusted([](composition::Element e) { return std::move(e).contrast(1.5); },
           0xF2590D);
  adjusted([](composition::Element e) { return std::move(e).saturation(0.5); },
           0xA26F55);
  adjusted([](composition::Element e) { return std::move(e).grayscale(1.0); },
           0x787878);
  adjusted(
      [](composition::Element e) {
        return std::move(e).hue_rotation(motion::Angle::degrees(90.0));
      },
      0x339524);
  adjusted([](composition::Element e) { return std::move(e).color_invert(); },
           0x3399CC);
  adjusted(
      [](composition::Element e) {
        return std::move(e).color_multiply(
            theme::Colour{0.5F, 1.0F, 0.5F, 1.0F});
      },
      0x666619);
  adjusted(
      [](composition::Element e) { return std::move(e).luminance_to_alpha(); },
      0x878787);
}

TEST_F(VisualEffectRasterTest, InvertColoursSparesViewsThatIgnoreIt) {
  const auto image = render([] {
    return placed(components::HStack(
                      components::HStackOptions{.spacing = 0.0F},
                      swatch(source_rgb),
                      swatch(source_rgb).accessibility_ignores_invert_colors()))
        .environment(components::Environment::accessibility_invert_colors,
                     true);
  });
  ASSERT_TRUE(image.has_value());
  near(rgb(*image, 4, 4), 0x000000);
  near(rgb(*image, 60, 80), 0xDB7542, 4);
  near(rgb(*image, 100, 80), source_rgb, 4);
}

TEST_F(VisualEffectRasterTest, BlendModesMatchTheSwiftUIReference) {
  blended(graphics::BlendMode::multiply, 0x293D2E);
  blended(graphics::BlendMode::screen, 0xD6C2EA);
  blended(graphics::BlendMode::overlay, 0x5285D5);
  blended(graphics::BlendMode::darken, 0x336633);
  blended(graphics::BlendMode::lighten, 0xCC99E5);
  blended(graphics::BlendMode::difference, 0x9933B2);
  blended(graphics::BlendMode::exclusion, 0xAD85BC);
  blended(graphics::BlendMode::color_dodge, 0xFFFFFF);
  blended(graphics::BlendMode::color_burn, 0x00007D, 6);
  blended(graphics::BlendMode::soft_light, 0x4B8DD7);
  blended(graphics::BlendMode::hard_light, 0xAD7A5C);
  blended(graphics::BlendMode::hue, 0xDC662A);
  blended(graphics::BlendMode::saturation, 0x3E96D7);
  blended(graphics::BlendMode::color, 0xD06A37);
  blended(graphics::BlendMode::luminosity, 0x2F95E1);
  blended(graphics::BlendMode::plus_lighter, 0xFFFFFF);
  blended(graphics::BlendMode::plus_darker, 0x000019);
}

TEST_F(VisualEffectRasterTest, BlurSpreadsAGaussianEdgeLikeSwiftUI) {
  const auto image = render([] {
    return components::ZStack(swatch(0x000000, 60.0F, 60.0F).blur(10.0F))
        .frame({.width = 160.0F, .height = 160.0F})
        .background(theme::Colour::hex(0xFFFFFF), theme::Shape::rectangle());
  });
  ASSERT_TRUE(image.has_value());
  const std::array<std::pair<int, float>, 5> profile{
      {{-10, 0.847F}, {-5, 0.694F}, {0, 0.486F}, {5, 0.286F}, {10, 0.137F}}};
  for (const auto& [distance, level] : profile) {
    const auto x = static_cast<std::uint32_t>(50 - distance);
    const int expected = static_cast<int>(std::lround((1.0F - level) * 255.0F));
    EXPECT_LE(std::abs(rgb(*image, x, 80)[0] - expected), 26)
        << "distance " << distance;
  }
  EXPECT_GT(rgb(*image, 44, 80)[0], 20);
}

TEST_F(VisualEffectRasterTest, OpaqueBlurKeepsItsEdges) {
  const auto image = render([] {
    return components::ZStack(swatch(0x000000, 60.0F, 60.0F).blur(10.0F, true))
        .frame({.width = 160.0F, .height = 160.0F})
        .background(theme::Colour::hex(0xFFFFFF), theme::Shape::rectangle());
  });
  ASSERT_TRUE(image.has_value());
  near(rgb(*image, 45, 80), 0xFFFFFF);
  near(rgb(*image, 55, 80), 0x000000);
}

TEST_F(VisualEffectRasterTest, MaskUsesTheMaskAlpha) {
  const auto image = render([] {
    return placed(swatch(0x000000, 100.0F, 20.0F)
                      .mask(composition::Alignment::leading,
                            swatch(0x000000, 50.0F, 20.0F).opacity(0.5F)));
  });
  ASSERT_TRUE(image.has_value());
  near(rgb(*image, 40, 80), 0x808080, 4);
  near(rgb(*image, 120, 80), 0xFFFFFF);
}

TEST_F(VisualEffectRasterTest, Rotation3DProjectsLikeSwiftUI) {
  const auto image = render([] {
    return placed(swatch(0x000000, 80.0F, 60.0F)
                      .rotation_3d_effect(motion::Angle::degrees(45.0),
                                          {.x = 0.0F, .y = 1.0F, .z = 0.0F}));
  });
  ASSERT_TRUE(image.has_value());
  std::uint32_t left = 160;
  std::uint32_t right = 0;
  for (std::uint32_t x = 0; x < 160; ++x) {
    if (rgb(*image, x, 80)[0] < 128) {
      left = std::min(left, x);
      right = std::max(right, x + 1);
    }
  }
  EXPECT_NEAR(static_cast<double>(left), 36.0, 1.5);
  EXPECT_NEAR(static_cast<double>(right), 101.0, 1.5);
  std::uint32_t top_near = 0;
  for (std::uint32_t y = 0; y < 160; ++y) {
    if (rgb(*image, left + 1, y)[0] < 128) {
      top_near = y;
      break;
    }
  }
  EXPECT_NEAR(static_cast<double>(top_near), 34.0, 1.5);
}

TEST_F(VisualEffectRasterTest, CompositingGroupFlattensOpacity) {
  const auto image = render([] {
    return placed(components::ZStack(swatch(0x000000), swatch(0x000000))
                      .compositing_group()
                      .opacity(0.5F));
  });
  ASSERT_TRUE(image.has_value());
  near(rgb(*image, 80, 80), 0x808080, 4);
}

TEST_F(VisualEffectRasterTest, DrawingGroupOpaqueDrawsLikeSwiftUI) {
  for (const bool opaque : {false, true}) {
    const auto image = render([opaque] {
      return placed(components::ZStack(swatch(source_rgb, 20.0F, 20.0F))
                        .clip_shape(theme::Circle())
                        .drawing_group(opaque));
    });
    ASSERT_TRUE(image.has_value());
    near(rgb(*image, 80, 80), source_rgb);
    near(rgb(*image, 71, 71), 0xFFFFFF);
    near(rgb(*image, 60, 60), 0xFFFFFF);
  }
}

TEST_F(VisualEffectRasterTest, ClipShapeMasksAnImageToACircle) {
  const auto image = render([] {
    return placed(components::Image(pixels(0x000000, 64, 64))
                      .resizable()
                      .frame({.width = 40.0F, .height = 40.0F})
                      .clip_shape(theme::Circle()));
  });
  ASSERT_TRUE(image.has_value());
  near(rgb(*image, 60, 60), 0xFFFFFF);
  near(rgb(*image, 61, 61), 0xFFFFFF);
  near(rgb(*image, 99, 61), 0xFFFFFF);
  near(rgb(*image, 61, 99), 0xFFFFFF);
  near(rgb(*image, 99, 99), 0xFFFFFF);
  EXPECT_GT(rgb(*image, 65, 65)[0], 200);
  near(rgb(*image, 67, 67), 0x000000, 24);
  near(rgb(*image, 80, 80), 0x000000);
  near(rgb(*image, 60, 80), 0x000000, 16);
  near(rgb(*image, 80, 60), 0x000000, 16);
}

TEST_F(VisualEffectRasterTest, ClipShapeMasksAnImageToAContinuousCorner) {
  const auto image = render([] {
    return placed(components::Image(pixels(0x000000, 64, 64))
                      .resizable()
                      .frame({.width = 60.0F, .height = 40.0F})
                      .clip_shape(theme::RoundedRectangle(
                          14.0F, theme::RoundedCornerStyle::continuous)));
  });
  ASSERT_TRUE(image.has_value());
  for (const auto& [x, y] :
       {std::pair{50U, 60U}, std::pair{51U, 61U}, std::pair{52U, 62U},
        std::pair{109U, 60U}, std::pair{50U, 99U}, std::pair{109U, 99U}}) {
    EXPECT_GT(rgb(*image, x, y)[0], 230) << x << "," << y;
  }
  near(rgb(*image, 56, 66), 0x000000, 24);
  near(rgb(*image, 80, 60), 0x000000, 12);
  near(rgb(*image, 50, 80), 0x000000, 12);
  near(rgb(*image, 80, 80), 0x000000);
}

TEST_F(VisualEffectRasterTest, ClipShapeClipsGradientsTextAndChildren) {
  const auto image = render([] {
    return placed(
        components::ZStack(
            components::VStack()
                .frame({.width = 80.0F, .height = 80.0F})
                .background(graphics::LinearGradient{
                    .gradient =
                        graphics::Gradient{std::vector<theme::ColourValue>{
                            theme::Colour::hex(0x000000),
                            theme::Colour::hex(0x000000)}},
                    .start_point = motion::UnitPoint::top,
                    .end_point = motion::UnitPoint::bottom}))
            .frame({.width = 40.0F, .height = 40.0F})
            .clip_shape(theme::Capsule()));
  });
  ASSERT_TRUE(image.has_value());
  near(rgb(*image, 61, 61), 0xFFFFFF);
  near(rgb(*image, 80, 80), 0x000000);
}

TEST_F(VisualEffectRasterTest, ClipShapeFollowsAnArbitraryPath) {
  const auto image = render([] {
    return placed(components::ZStack(swatch(0x000000, 80.0F, 80.0F))
                      .frame({.width = 40.0F, .height = 40.0F})
                      .clip_shape(Wedge{}));
  });
  ASSERT_TRUE(image.has_value());
  near(rgb(*image, 62, 62), 0xFFFFFF);
  near(rgb(*image, 97, 62), 0xFFFFFF);
  near(rgb(*image, 80, 65), 0x000000, 24);
  near(rgb(*image, 80, 97), 0x000000);
  near(rgb(*image, 62, 97), 0x000000, 24);
}

TEST_F(VisualEffectRasterTest, ClippedKeepsTheRectangle) {
  const auto image = render([] {
    return placed(components::ZStack(swatch(0x000000, 60.0F, 60.0F))
                      .frame({.width = 20.0F, .height = 20.0F})
                      .clipped());
  });
  ASSERT_TRUE(image.has_value());
  near(rgb(*image, 70, 70), 0x000000);
  near(rgb(*image, 89, 89), 0x000000);
  near(rgb(*image, 69, 69), 0xFFFFFF);
  near(rgb(*image, 90, 90), 0xFFFFFF);
}

TEST_F(VisualEffectRasterTest, MovedLayerCompositesLikeOneDrawnAtItsNewOffset) {
  constexpr float moved = 7.0F;
  Shift shift{};
  provider::headless::HeadlessProviderFactory reused;
  Application sliding = shifted(reused, Shifted{0.0F, &shift});
  ASSERT_TRUE(shift.handle.has_value());
  shift.handle->set(moved);
  ASSERT_TRUE(
      reused.clock().advance(std::chrono::milliseconds{16}).has_value());
  ASSERT_TRUE(sliding.frame().has_value());
  EXPECT_EQ(sliding.last_frame().layer_renders, 0U);
  EXPECT_GT(sliding.last_frame().layer_hits, 0U);
  const auto slid = reused.gpu().capture();
  ASSERT_TRUE(slid.has_value());

  Shift unused{};
  provider::headless::HeadlessProviderFactory fresh;
  Application drawn = shifted(fresh, Shifted{moved, &unused});
  const auto placed_fresh = fresh.gpu().capture();
  ASSERT_TRUE(placed_fresh.has_value());

  ASSERT_EQ(slid->width(), placed_fresh->width());
  ASSERT_EQ(slid->height(), placed_fresh->height());
  EXPECT_TRUE(std::ranges::equal(slid->pixels(), placed_fresh->pixels()));
  EXPECT_NE(rgb(*slid, 80, 80), rgb(*slid, 2, 2));
}

TEST_F(VisualEffectRasterTest, AMaskLayerSharesItsContentLayerTargetSize) {
  Shift shift{};
  provider::headless::HeadlessProviderFactory providers;
  const provider::SurfaceSize size{.width = provider::SurfaceWidth{160},
                                   .height = provider::SurfaceHeight{160}};
  providers.surface().resize(size);
  providers.gpu().rasterize(size);
  auto created = Application::create(providers, Handover{&shift});
  ASSERT_TRUE(created.has_value());
  Application application = std::move(*created);
  const auto step = [&] {
    ASSERT_TRUE(
        providers.clock().advance(std::chrono::milliseconds{16}).has_value());
    ASSERT_TRUE(application.frame().has_value());
  };
  for (int frame = 0; frame < 4; ++frame) {
    step();
  }
  ASSERT_TRUE(shift.phase.has_value());
  for (const int next : {1, 2}) {
    shift.phase->set(next);
    for (int frame = 0; frame < 4; ++frame) {
      step();
    }
  }
  const auto image = providers.gpu().capture();
  ASSERT_TRUE(image.has_value());
  for (const auto& [x, y] : {std::pair{51U, 51U}, std::pair{108U, 51U},
                             std::pair{51U, 108U}, std::pair{108U, 108U}}) {
    SCOPED_TRACE(testing::Message() << x << "," << y);
    near(rgb(*image, x, y), 0xFFFFFF);
  }
  for (const auto& [x, y] :
       {std::pair{80U, 51U}, std::pair{80U, 108U}, std::pair{51U, 80U},
        std::pair{108U, 80U}, std::pair{80U, 80U}}) {
    SCOPED_TRACE(testing::Message() << x << "," << y);
    near(rgb(*image, x, y), 0x000000);
  }
}

TEST_F(VisualEffectRasterTest, RoundedClipsMaskOnlyWhenContentReachesACorner) {
  const auto passes = [](Builder builder) {
    provider::headless::HeadlessProviderFactory providers;
    const provider::SurfaceSize size{.width = provider::SurfaceWidth{160},
                                     .height = provider::SurfaceHeight{160}};
    providers.surface().resize(size);
    providers.gpu().rasterize(size);
    auto created = Application::create(providers, Scene{std::move(builder)});
    EXPECT_TRUE(created.has_value());
    std::uint32_t total = 0;
    if (!created.has_value()) {
      return total;
    }
    for (int step = 0; step < 4; ++step) {
      EXPECT_TRUE(
          providers.clock().advance(std::chrono::milliseconds{16}).has_value());
      EXPECT_TRUE(created->frame().has_value());
      total += created->last_frame().effect_passes +
               created->last_frame().layer_renders;
    }
    return total;
  };
  EXPECT_EQ(passes([] {
              return placed(components::ZStack(swatch(0x000000, 20.0F, 20.0F))
                                .frame({.width = 60.0F, .height = 60.0F})
                                .background(theme::Colour::hex(0x808080),
                                            theme::RoundedRectangle(12.0F))
                                .clip_shape(theme::RoundedRectangle(12.0F)));
            }),
            0U);
  EXPECT_GT(passes([] {
              return placed(components::ZStack(swatch(0x000000, 80.0F, 80.0F))
                                .frame({.width = 60.0F, .height = 60.0F})
                                .clip_shape(theme::RoundedRectangle(12.0F)));
            }),
            0U);
  EXPECT_EQ(passes([] {
              return placed(components::ZStack(swatch(0x000000, 80.0F, 80.0F))
                                .frame({.width = 60.0F, .height = 60.0F})
                                .clip_shape(theme::Rectangle()));
            }),
            0U);
}

TEST_F(VisualEffectRasterTest, BlackAndWhiteWorkEverywhereAStyleDoes) {
  const auto image = render([] {
    return placed(
        components::HStack(components::VStack()
                               .frame({.width = 30.0F, .height = 30.0F})
                               .background(theme::Color::black),
                           components::ShapeView(theme::Rectangle())
                               .fill(theme::Color::white)
                               .frame({.width = 30.0F, .height = 30.0F})
                               .background(theme::Color::black),
                           components::Text("A")
                               .foreground_style(theme::Color::white)
                               .frame({.width = 30.0F, .height = 30.0F})
                               .background(theme::Color::black)
                               .tint(theme::Color::white))
            .background(theme::Color::clear));
  });
  ASSERT_TRUE(image.has_value());
  near(rgb(*image, 50, 80), 0x000000);
  near(rgb(*image, 80, 80), 0xFFFFFF);
  EXPECT_EQ(theme::Color::black,
            theme::ColourValue{theme::Colour::hex(0x000000)});
  EXPECT_EQ(theme::Color::white,
            theme::ColourValue{theme::Colour::hex(0xFFFFFF)});
}

TEST_F(VisualEffectRasterTest, VisualEffectAppliesColourAndBlur) {
  const auto image = render([] {
    return placed(swatch(source_rgb)
                      .visual_effect([](composition::VisualEffect effect,
                                        const composition::GeometryProxy&) {
                        return effect.grayscale(1.0);
                      }));
  });
  ASSERT_TRUE(image.has_value());
  near(rgb(*image, 80, 80), 0x787878);
}

TEST_F(VisualEffectRasterTest, TransformEffectMovesWithoutALayer) {
  const auto image = render([] {
    return placed(swatch(0x000000, 20.0F, 20.0F)
                      .transform_effect(
                          graphics::AffineTransform::translation(30.0F, 0.0F)));
  });
  ASSERT_TRUE(image.has_value());
  near(rgb(*image, 80, 80), 0xFFFFFF);
  near(rgb(*image, 110, 80), 0x000000);
}

TEST_F(VisualEffectRasterTest, CanvasDrawsThroughThePainter) {
  const auto image = render([] {
    return placed(
        components::Canvas([](components::GraphicsContext& context,
                              composition::Size size) {
          context.translate_by(size.width * 0.5F, 0.0F);
          context.fill(
              graphics::Path{composition::Rect{
                  .x = 0.0F, .y = 0.0F, .width = 40.0F, .height = 40.0F}},
              graphics::ShapeStyle{theme::Colour::hex(source_rgb)});
        }).frame({.width = 100.0F, .height = 100.0F}));
  });
  ASSERT_TRUE(image.has_value());
  near(rgb(*image, 85, 45), source_rgb);
  near(rgb(*image, 45, 45), 0xFFFFFF);
}

TEST_F(VisualEffectRasterTest, ShaderEffectsDrawTheirLayerInTheSoftwareRaster) {
  const auto image = render([] {
    return placed(swatch(source_rgb)
                      .color_effect(graphics::Shader{
                          graphics::ShaderFunction{
                              0, graphics::ShaderFunction::Kind::colour},
                          {12.0F}}));
  });
  ASSERT_TRUE(image.has_value());
  near(rgb(*image, 80, 80), source_rgb);
}

TEST_F(VisualEffectRasterTest, BackgroundExtensionMirrorsBeyondTheView) {
  const auto image = render([] {
    return placed(swatch(0x000000, 40.0F, 40.0F).background_extension_effect());
  });
  ASSERT_TRUE(image.has_value());
  near(rgb(*image, 80, 80), 0x000000);
  EXPECT_LT(rgb(*image, 50, 80)[0], 200);
}

TEST_F(VisualEffectRasterTest, ScrollEdgeEffectFadesTheScrolledEdge) {
  const auto image = render([] {
    return placed(
        components::ScrollView(swatch(0x000000, 100.0F, 400.0F))
            .scroll_edge_effect_style(composition::ScrollEdgeEffectStyle::soft,
                                      theme::Edge::bottom)
            .frame({.width = 100.0F, .height = 100.0F}));
  });
  ASSERT_TRUE(image.has_value());
  EXPECT_LT(rgb(*image, 80, 60)[0], 30);
  EXPECT_GT(rgb(*image, 80, 128)[0], 120);
}

TEST_F(VisualEffectRasterTest, ScrollEdgeEffectHiddenKeepsTheEdgeSharp) {
  const auto image = render([] {
    return placed(
        components::ScrollView(swatch(0x000000, 100.0F, 400.0F))
            .scroll_edge_effect_style(composition::ScrollEdgeEffectStyle::soft,
                                      theme::Edge::bottom)
            .scroll_edge_effect_hidden(true, theme::Edge::bottom)
            .frame({.width = 100.0F, .height = 100.0F}));
  });
  ASSERT_TRUE(image.has_value());
  EXPECT_LT(rgb(*image, 80, 60)[0], 30);
  EXPECT_LT(rgb(*image, 80, 128)[0], 30);
}

TEST_F(VisualEffectRasterTest, AnAncestorEdgeEffectStyleReachesTheScrollView) {
  const auto image = render([] {
    return placed(
        components::ZStack(
            components::ScrollView(swatch(0x000000, 100.0F, 400.0F))
                .frame({.width = 100.0F, .height = 100.0F}))
            .scroll_edge_effect_style(composition::ScrollEdgeEffectStyle::soft,
                                      theme::Edge::bottom));
  });
  ASSERT_TRUE(image.has_value());
  EXPECT_LT(rgb(*image, 80, 60)[0], 30);
  EXPECT_GT(rgb(*image, 80, 128)[0], 120);
}

TEST_F(VisualEffectRasterTest, MarginPlacementMovesOnlyTheChosenIndicators) {
  const auto shot = [](composition::ContentMarginPlacement placement) {
    return render([placement] {
      return placed(
          components::ScrollView(swatch(0xFFFFFF, 100.0F, 400.0F))
              .content_margins(theme::Edge::vertical, 20.0F, placement)
              .content_margins(theme::Edge::trailing, 12.0F, placement)
              .scroll_indicators_flash(true)
              .frame({.width = 100.0F, .height = 100.0F}));
    });
  };
  const auto automatic = shot(composition::ContentMarginPlacement::automatic);
  ASSERT_TRUE(automatic.has_value());
  EXPECT_LT(rgb(*automatic, 113, 65)[0], 235);
  EXPECT_GT(rgb(*automatic, 125, 45)[0], 250);
  const auto content =
      shot(composition::ContentMarginPlacement::scroll_content);
  ASSERT_TRUE(content.has_value());
  EXPECT_LT(rgb(*content, 125, 45)[0], 235);
  EXPECT_GT(rgb(*content, 113, 65)[0], 250);
  const auto indicators =
      shot(composition::ContentMarginPlacement::scroll_indicators);
  ASSERT_TRUE(indicators.has_value());
  EXPECT_LT(rgb(*indicators, 113, 65)[0], 235);
  EXPECT_GT(rgb(*indicators, 125, 45)[0], 250);
}

TEST_F(VisualEffectRasterTest, ProjectionEffectsMoveTheDrawing) {
  const auto image = render([] {
    return placed(
        swatch(0x000000).projection_effect(graphics::ProjectionTransform{
            graphics::AffineTransform{1.0F, 0.0F, 0.0F, 1.0F, 20.0F, 0.0F}}));
  });
  ASSERT_TRUE(image.has_value());
  near(rgb(*image, 110, 80), 0x000000);
  near(rgb(*image, 70, 80), 0xFFFFFF);
}

TEST_F(VisualEffectRasterTest, PerspectiveRotationMatchesTheRotation3DEffect) {
  const auto edges = [](bool perspective) {
    const auto image = render([perspective] {
      composition::Element swatched = swatch(0x000000, 80.0F, 60.0F);
      return placed(perspective
                        ? std::move(swatched).perspective_rotation_effect(
                              motion::Angle::degrees(45.0),
                              {.x = 0.0F, .y = 1.0F, .z = 0.0F})
                        : std::move(swatched).rotation_3d_effect(
                              motion::Angle::degrees(45.0),
                              {.x = 0.0F, .y = 1.0F, .z = 0.0F}));
    });
    std::pair<std::uint32_t, std::uint32_t> found{160, 0};
    if (!image.has_value()) {
      return found;
    }
    for (std::uint32_t x = 0; x < 160; ++x) {
      if (rgb(*image, x, 80)[0] < 128) {
        found.first = std::min(found.first, x);
        found.second = std::max(found.second, x + 1);
      }
    }
    return found;
  };
  const auto rotated = edges(false);
  const auto perspective = edges(true);
  EXPECT_EQ(perspective, rotated);
  EXPECT_NEAR(static_cast<double>(perspective.first), 36.0, 1.5);
}

TEST_F(VisualEffectRasterTest, FlipsForRightToLeftMirrorsTheDrawing) {
  const auto left_ink = [](bool rtl, bool flips) {
    const auto image = render([rtl, flips] {
      return placed(
          components::Image(halves(0x000000, 0xFFFFFF, 40, 40))
              .resizable()
              .frame({.width = 40.0F, .height = 40.0F})
              .flips_for_right_to_left_layout_direction(flips)
              .environment(components::Environment::layout_direction,
                           rtl ? components::LayoutDirection::right_to_left
                               : components::LayoutDirection::left_to_right));
    });
    return image.has_value() ? rgb(*image, 65, 80)[0] : -1;
  };
  EXPECT_LT(left_ink(false, true), 30);
  EXPECT_LT(left_ink(true, false), 30);
  EXPECT_GT(left_ink(true, true), 220);
}

TEST_F(VisualEffectRasterTest, ImageRendererRendersASubtree) {
  runtime::ImageRenderer renderer{
      components::VStack(swatch(source_rgb, 40.0F, 20.0F)).padding(10.0F)};
  renderer.set_scale(2.0F);
  const auto made = renderer.image();
  ASSERT_TRUE(made.has_value());
  EXPECT_EQ(made->width(), 120U);
  EXPECT_EQ(made->height(), 80U);
  const auto inside = made->pixel(60, 40);
  ASSERT_TRUE(inside.has_value());
  near({(*inside)[0], (*inside)[1], (*inside)[2]}, source_rgb);
  EXPECT_EQ((*inside)[3], 255);
  const auto corner = made->pixel(2, 2);
  ASSERT_TRUE(corner.has_value());
  EXPECT_EQ((*corner)[3], 0);
}

TEST_F(VisualEffectRasterTest, DegenerateEffectRegionsNeverFailTheFrame) {
  const float nan = std::numeric_limits<float>::quiet_NaN();
  const float inf = std::numeric_limits<float>::infinity();
  const std::array<std::pair<std::string_view, Builder>, 18> cases{{
      {"blur on an empty view",
       [] { return placed(swatch(source_rgb, 0.0F, 0.0F).blur(5.0F)); }},
      {"blur on a subpixel view",
       [] { return placed(swatch(source_rgb, 0.3F, 0.3F).blur(5.0F)); }},
      {"blur scaled to nothing",
       [] { return placed(swatch(source_rgb).scale_effect(0.0F).blur(5.0F)); }},
      {"blur scaled by NaN",
       [nan] {
         return placed(swatch(source_rgb).scale_effect(nan).blur(5.0F));
       }},
      {"blur moved by NaN",
       [nan] {
         return placed(swatch(source_rgb).offset(nan, nan).blur(5.0F));
       }},
      {"blur moved to infinity",
       [inf] {
         return placed(swatch(source_rgb).offset(inf, 0.0F).blur(5.0F));
       }},
      {"blur moved off screen",
       [] {
         return placed(swatch(source_rgb).offset(1.0e9F, 0.0F).blur(5.0F));
       }},
      {"blur radius NaN",
       [nan] { return placed(swatch(source_rgb).blur(nan)); }},
      {"blur radius infinite",
       [inf] { return placed(swatch(source_rgb).blur(inf)); }},
      {"glass on an empty view",
       [] { return placed(swatch(source_rgb, 0.0F, 0.0F).glass_effect()); }},
      {"glass on a subpixel view",
       [] { return placed(swatch(source_rgb, 0.3F, 0.3F).glass_effect()); }},
      {"glass scaled to nothing",
       [] {
         return placed(swatch(source_rgb).scale_effect(0.0F).glass_effect());
       }},
      {"glass moved by NaN",
       [nan] {
         return placed(swatch(source_rgb).offset(nan, nan).glass_effect());
       }},
      {"glass moved to infinity",
       [inf] {
         return placed(swatch(source_rgb).offset(inf, 0.0F).glass_effect());
       }},
      {"text moved by NaN",
       [nan] { return placed(components::Text("Ab").offset(nan, nan)); }},
      {"union member with no frame",
       [] {
         return placed(components::GlassEffectContainer(components::HStack(
             swatch(source_rgb).glass_effect(),
             swatch(source_rgb, 0.0F, 0.0F).glass_effect())));
       }},
      {"union member moved by NaN",
       [nan] {
         return placed(components::GlassEffectContainer(components::HStack(
             swatch(source_rgb).glass_effect(),
             swatch(source_rgb).offset(nan, nan).glass_effect())));
       }},
      {"union member scaled to nothing",
       [] {
         return placed(components::GlassEffectContainer(components::HStack(
             swatch(source_rgb).glass_effect(),
             swatch(source_rgb).scale_effect(0.0F).glass_effect())));
       }},
  }};
  for (const auto& [name, builder] : cases) {
    SCOPED_TRACE(name);
    EXPECT_TRUE(render(builder).has_value());
  }
}

TEST_F(VisualEffectRasterTest,
       ARejectedEffectPassDrawsTheSubtreeWithoutTheEffect) {
  for (const std::size_t rejected : {0U, 1U, 2U}) {
    SCOPED_TRACE(rejected);
    const Rendered result = rendered(
        [] {
          return components::ZStack(swatch(0x000000, 60.0F, 60.0F).blur(10.0F))
              .frame({.width = 160.0F, .height = 160.0F})
              .background(theme::Colour::hex(0xFFFFFF),
                          theme::Shape::rectangle());
        },
        [rejected](provider::headless::NullGpu& gpu) {
          gpu.reject_draws(provider::headless::NullGpu::DrawRejection{
              .skipped = rejected, .count = 1, .passes = true});
        });
    ASSERT_TRUE(result.image.has_value());
    near(rgb(*result.image, 45, 80), 0xFFFFFF);
    near(rgb(*result.image, 80, 80), 0x000000);
    near(rgb(*result.image, 55, 80), 0x000000);
    EXPECT_TRUE(std::ranges::any_of(
        result.reports, [](const provider::Diagnostic& report) {
          return report.message.find("draws without the effect") !=
                     std::string::npos &&
                 report.message.find("for node") != std::string::npos &&
                 report.message.find("draw_rejected at effect layer pass") !=
                     std::string::npos;
        }));
  }
}

TEST_F(VisualEffectRasterTest,
       AnEffectStageWithoutARenderTargetDrawsTheSubtreeWithoutTheEffect) {
  const Rendered result = rendered(
      [] {
        return components::ZStack(swatch(0x000000, 60.0F, 60.0F).blur(10.0F))
            .frame({.width = 160.0F, .height = 160.0F})
            .background(theme::Colour::hex(0xFFFFFF),
                        theme::Shape::rectangle());
      },
      [](provider::headless::NullGpu& gpu) {
        gpu.limit_render_targets(provider::headless::NullGpu::RenderTargetLimit{
            .extent = provider::Texture::maximum_extent,
            .bytes = std::numeric_limits<std::size_t>::max(),
            .count = 1});
      });
  ASSERT_TRUE(result.image.has_value());
  near(rgb(*result.image, 80, 80), 0x000000);
  near(rgb(*result.image, 45, 80), 0xFFFFFF);
}

TEST_F(VisualEffectRasterTest, ARejectedMainDrawEndsTheFrameWithTheWholeCause) {
  provider::headless::HeadlessProviderFactory providers;
  const provider::SurfaceSize size{.width = provider::SurfaceWidth{160},
                                   .height = provider::SurfaceHeight{160}};
  providers.surface().resize(size);
  providers.gpu().rasterize(size);
  auto created = Application::create(
      providers, Scene{[] { return placed(swatch(source_rgb)); }});
  ASSERT_TRUE(created.has_value());
  Application application = std::move(*created);
  providers.gpu().reject_draws(provider::headless::NullGpu::DrawRejection{
      .skipped = 0, .count = 1, .passes = false});
  ASSERT_TRUE(
      providers.clock().advance(std::chrono::milliseconds{16}).has_value());
  const auto framed = application.frame();
  ASSERT_FALSE(framed.has_value());
  ASSERT_TRUE(framed.error().paint().has_value());
  EXPECT_EQ(framed.error().paint()->stage(), rendering::PaintStage::main_draw);
  EXPECT_EQ(framed.error().paint()->code(),
            rendering::PaintErrorCode::draw_rejected);
  ASSERT_TRUE(framed.error().cause().has_value());
  EXPECT_EQ(framed.error().cause()->detail(),
            provider::ProviderErrorDetail::unknown_texture);
  EXPECT_EQ(framed.error().explanation(),
            "paint_failed (the painter could not build or draw the frame): "
            "draw_rejected at main draw (the frame rejected the painter's "
            "draw); provider invalid_argument (invalid argument): a command "
            "samples a texture that is not live");
}

}
