#include <arisa/components/environment.hpp>
#include <arisa/components/shape_view.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/layout_direction_behavior.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/graphics/any_shape.hpp>
#include <arisa/graphics/gradient.hpp>
#include <arisa/graphics/offset_shape.hpp>
#include <arisa/graphics/path.hpp>
#include <arisa/graphics/rotated_shape.hpp>
#include <arisa/graphics/scaled_shape.hpp>
#include <arisa/graphics/shadow_style.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/graphics/transformed_shape.hpp>
#include <arisa/motion/angle.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/frame_timings.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::runtime {

class ShapeRasterTest : public ::testing::Test {
 protected:
  static constexpr std::uint32_t side = 160;

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

  struct Counting {
    std::shared_ptr<int> calls;

    auto path(composition::Rect rect) const -> graphics::Path {
      ++*calls;
      return graphics::Path::ellipse_in(rect);
    }

    friend auto operator==(const Counting&, const Counting&) -> bool = default;
  };

  class Resizer final : public composition::Component {
   public:
    static constexpr composition::StateKey<float> width{"shape-width", 60.0F};

    Resizer(
        std::shared_ptr<int> calls,
        std::shared_ptr<std::optional<composition::StateHandle<float>>> slot)
        : calls_(std::move(calls)), slot_(std::move(slot)) {}

    auto operator==(const Resizer&) const -> bool { return true; }

    auto body(composition::Context& context) const
        -> composition::Element override {
      const auto handle = context.state(width);
      *slot_ = handle;
      return components::ZStack(
          composition::Element{components::ShapeView(Counting{calls_})
                                   .fill(theme::Colour::hex(0x000000))}
              .frame({.width = handle.get(), .height = 40.0F}));
    }

   private:
    std::shared_ptr<int> calls_;
    std::shared_ptr<std::optional<composition::StateHandle<float>>> slot_;
  };

  class Toggled final : public composition::Component {
   public:
    using Make = std::function<composition::Element(bool)>;

    Toggled(const composition::Observable<bool>& on, Make make)
        : on_(on), make_(std::move(make)) {}

    auto operator==(const Toggled& other) const -> bool {
      return this == &other;
    }

    auto body(composition::Context& context) const
        -> composition::Element override {
      return make_(context.read(on_.get()));
    }

   private:
    std::reference_wrapper<const composition::Observable<bool>> on_;
    Make make_;
  };

  struct Churn {
    std::uint64_t tessellations{0};
    std::uint64_t textures{0};
    std::uint64_t shadow_misses{0};
    int animated{0};
    std::optional<provider::Image> settled;
  };

  static auto churn(const Toggled::Make& make) -> Churn {
    provider::headless::HeadlessProviderFactory providers;
    const provider::SurfaceSize size{.width = provider::SurfaceWidth{side},
                                     .height = provider::SurfaceHeight{side}};
    providers.surface().resize(size);
    providers.gpu().rasterize(size);
    composition::Observable<bool> on{false};
    auto created = Application::create(providers, Toggled{on, make});
    EXPECT_TRUE(created.has_value());
    if (!created.has_value()) {
      return {};
    }
    Application application = std::move(*created);
    const auto step = [&] {
      EXPECT_TRUE(
          providers.clock().advance(std::chrono::milliseconds{16}).has_value());
      EXPECT_TRUE(application.frame().has_value());
      return application.last_frame();
    };
    for (int frame = 0; frame < 4; ++frame) {
      step();
    }
    motion::with_animation(motion::Animation::linear(0.4),
                           [&on] { on.set(true); });
    Churn totals;
    for (int frame = 0; frame < 20; ++frame) {
      const FrameTimings timings = step();
      totals.tessellations += timings.tessellations;
      totals.textures += timings.textures_created;
      totals.shadow_misses += timings.shadow_misses;
      totals.animated += timings.animating ? 1 : 0;
    }
    for (int frame = 0; frame < 20; ++frame) {
      step();
    }
    auto image = providers.gpu().capture();
    EXPECT_TRUE(image.has_value());
    if (image.has_value()) {
      totals.settled = std::move(*image);
    }
    return totals;
  }

  static auto same_pixels(const provider::Image& left,
                          const provider::Image& right) -> bool {
    return left.width() == right.width() && left.height() == right.height() &&
           std::ranges::equal(left.pixels(), right.pixels());
  }

  static auto placed(composition::Element content,
                     std::uint32_t backdrop = 0xFFFFFF)
      -> composition::Element {
    return components::ZStack(std::move(content))
        .frame({.width = static_cast<float>(side),
                .height = static_cast<float>(side)})
        .background(theme::Colour::hex(backdrop), theme::Shape::rectangle());
  }

  static auto black() -> graphics::ShapeStyle {
    return graphics::ShapeStyle{theme::Colour::hex(0x000000)};
  }

  static auto render(Builder builder) -> std::optional<provider::Image> {
    provider::headless::HeadlessProviderFactory providers;
    const provider::SurfaceSize size{.width = provider::SurfaceWidth{side},
                                     .height = provider::SurfaceHeight{side}};
    providers.surface().resize(size);
    providers.gpu().rasterize(size);
    auto created = Application::create(providers, Scene{std::move(builder)});
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

  static auto level(const provider::Image& image, int x, int y) -> float {
    return static_cast<float>(rgb(image, x, y)[0]) / 255.0F;
  }

  static auto diagonal(const provider::Image& image, int x, int y, int dx,
                       int dy) -> int {
    int steps = 0;
    while (steps < 100 &&
           rgb(image, x + (steps * dx), y + (steps * dy))[0] >= 128) {
      ++steps;
    }
    return steps;
  }

  static void corners(const provider::Image& image, std::array<int, 4> wanted,
                      int slack = 1) {
    const int left = (side - 100) / 2;
    const int top = (side - 80) / 2;
    const int right = left + 99;
    const int bottom = top + 79;
    const std::array<int, 4> seen{diagonal(image, left, top, 1, 1),
                                  diagonal(image, right, top, -1, 1),
                                  diagonal(image, left, bottom, 1, -1),
                                  diagonal(image, right, bottom, -1, -1)};
    for (std::size_t corner = 0; corner < seen.size(); ++corner) {
      EXPECT_LE(std::abs(seen[corner] - wanted[corner]), slack)
          << "corner " << corner << " saw " << seen[corner] << " wanted "
          << wanted[corner];
    }
  }

  static auto boxed(composition::Element content) -> composition::Element {
    return std::move(content).frame({.width = 100.0F, .height = 80.0F});
  }

  static auto contained(composition::Element shape, theme::Insets insets)
      -> composition::Element {
    return boxed(Element{components::ZStack(std::move(shape)).padding(insets)})
        .container_shape(theme::RoundedRectangle(30.0F));
  }

  using Element = composition::Element;

  static void near(const std::array<int, 3>& seen, std::array<int, 3> wanted,
                   int slack = 3) {
    for (std::size_t channel = 0; channel < 3; ++channel) {
      EXPECT_LE(std::abs(seen[channel] - wanted[channel]), slack)
          << "channel " << channel << " saw " << seen[channel] << " wanted "
          << wanted[channel];
    }
  }
};

TEST_F(ShapeRasterTest, EllipseFillsItsBoundingEllipse) {
  for (const bool sdf : {false, true}) {
    const auto image = render([sdf] {
      Element shape =
          sdf ? Element{components::VStack().background(
                    theme::Colour::hex(0x000000), theme::Ellipse{})}
              : Element{components::ShapeView(theme::Ellipse{}).fill(black())};
      return placed(std::move(shape).frame({.width = 120.0F, .height = 60.0F}));
    });
    ASSERT_TRUE(image.has_value());
    int min_x = side;
    int max_x = -1;
    for (int x = 0; x < static_cast<int>(side); ++x) {
      if (level(*image, x, 80) < 0.5F) {
        min_x = std::min(min_x, x);
        max_x = std::max(max_x, x);
      }
    }
    EXPECT_NEAR(min_x, 20, 1) << sdf;
    EXPECT_NEAR(max_x + 1, 140, 1) << sdf;
    EXPECT_LT(level(*image, 50, 65), 0.1F) << sdf;
    EXPECT_GT(level(*image, 23, 53), 0.9F) << sdf;
  }
}

TEST_F(ShapeRasterTest, UnevenCornersMatchSwiftUI) {
  const theme::UnevenRoundedRectangle uneven{0.0F, 10.0F, 20.0F, 30.0F};
  for (const bool sdf : {false, true}) {
    const auto image = render([sdf, uneven] {
      return placed(
          boxed(sdf ? Element{components::VStack().background(
                          theme::Colour::hex(0x000000), uneven)}
                    : Element{components::ShapeView(uneven).fill(black())}));
    });
    ASSERT_TRUE(image.has_value());
    corners(*image, {0, 9, 3, 6});
  }
  const auto mirrored = render([uneven] {
    return placed(
        boxed(Element{components::VStack().background(
                  theme::Colour::hex(0x000000), uneven)})
            .environment(components::Environment::layout_direction,
                         composition::LayoutDirection::right_to_left));
  });
  ASSERT_TRUE(mirrored.has_value());
  corners(*mirrored, {9, 0, 6, 3});
}

TEST_F(ShapeRasterTest, ContainerRelativeShapeInsetsTheContainerShape) {
  const auto relative = [] {
    return Element{
        components::ShapeView(theme::ContainerRelativeShape{}).fill(black())};
  };
  const auto padded = render(
      [&] { return placed(contained(relative(), theme::Insets::all(10.0F))); });
  ASSERT_TRUE(padded.has_value());
  corners(*padded, {16, 16, 16, 16});
  const auto offset = render([&] {
    return placed(contained(relative(), theme::Insets{.top = 5.0F,
                                                      .leading = 20.0F,
                                                      .bottom = 10.0F,
                                                      .trailing = 10.0F}));
  });
  ASSERT_TRUE(offset.has_value());
  corners(*offset, {21, 15, 23, 17});
  const auto alone = render([&] {
    return placed(boxed(Element{
        components::ZStack(relative()).padding(theme::Insets::all(10.0F))}));
  });
  ASSERT_TRUE(alone.has_value());
  corners(*alone, {10, 10, 10, 10});
  const auto background = render([] {
    return placed(
        contained(Element{components::VStack()
                              .frame({.max_width = composition::infinity,
                                      .max_height = composition::infinity})
                              .background(theme::Colour::hex(0x000000),
                                          theme::ContainerRelativeShape{})},
                  theme::Insets::all(10.0F)));
  });
  ASSERT_TRUE(background.has_value());
  corners(*background, {16, 16, 16, 16});
}

TEST_F(ShapeRasterTest, ConcentricRectangleMatchesSwiftUI) {
  const auto concentric = [](theme::ConcentricRectangle shape) {
    return Element{components::ShapeView(shape).fill(black())};
  };
  const theme::Insets offset{
      .top = 5.0F, .leading = 20.0F, .bottom = 10.0F, .trailing = 10.0F};
  const auto check = [&](theme::ConcentricRectangle shape, theme::Insets insets,
                         std::array<int, 4> wanted) {
    const auto image =
        render([&] { return placed(contained(concentric(shape), insets)); });
    ASSERT_TRUE(image.has_value());
    corners(*image, wanted);
  };
  check(theme::ConcentricRectangle{}, theme::Insets::all(10.0F),
        {16, 16, 16, 16});
  check(theme::ConcentricRectangle{}, offset, {20, 14, 20, 16});
  check(theme::ConcentricRectangle{theme::ConcentricCorner::concentric(25.0F)},
        theme::Insets::all(10.0F), {17, 17, 17, 17});
  check(theme::ConcentricRectangle{theme::ConcentricCorner::fixed(5.0F)},
        theme::Insets::all(10.0F), {11, 11, 11, 11});
  check(theme::ConcentricRectangle{theme::ConcentricCorner::concentric(), true},
        offset, {21, 14, 22, 16});
  const auto alone = render([&] {
    return placed(boxed(
        Element{components::ZStack(concentric(theme::ConcentricRectangle{}))
                    .padding(theme::Insets::all(10.0F))}));
  });
  ASSERT_TRUE(alone.has_value());
  corners(*alone, {10, 10, 10, 10});
  const auto small = render([&] {
    return placed(Element{
        components::ZStack(concentric(theme::ConcentricRectangle{})
                               .frame({.width = 40.0F, .height = 30.0F}))}
                      .frame({.width = 100.0F,
                              .height = 80.0F,
                              .alignment = composition::Alignment::top_leading})
                      .container_shape(theme::RoundedRectangle(30.0F)));
  });
  ASSERT_TRUE(small.has_value());
  EXPECT_NEAR(diagonal(*small, (side - 100) / 2, (side - 80) / 2, 1, 1), 4, 1);
}

TEST_F(ShapeRasterTest, StrokeBorderInsetAndTransformsMatchSwiftUI) {
  const auto extent = [](const provider::Image& image, int row) {
    int min_x = side;
    int max_x = -1;
    for (int x = 0; x < static_cast<int>(side); ++x) {
      if (level(image, x, row) < 0.5F) {
        min_x = std::min(min_x, x);
        max_x = std::max(max_x, x);
      }
    }
    return std::array<int, 2>{min_x, max_x + 1};
  };
  const auto square = [](components::ShapeView shape, float size = 80.0F) {
    return placed(
        Element{std::move(shape)}.frame({.width = size, .height = size}));
  };
  const auto border = render([&] {
    return square(
        components::ShapeView(theme::Circle{}).stroke_border(black(), 10.0F));
  });
  ASSERT_TRUE(border.has_value());
  EXPECT_NEAR(extent(*border, 80)[0], 40, 1);
  EXPECT_NEAR(extent(*border, 80)[1], 120, 1);
  EXPECT_GT(level(*border, 51, 80), 0.5F);
  EXPECT_LT(level(*border, 48, 80), 0.5F);
  const auto stroked = render([&] {
    return square(
        components::ShapeView(theme::Circle{}).stroke(black(), 10.0F));
  });
  ASSERT_TRUE(stroked.has_value());
  EXPECT_NEAR(extent(*stroked, 80)[0], 35, 1);
  EXPECT_NEAR(extent(*stroked, 80)[1], 125, 1);
  const auto inset = render([&] {
    return square(
        components::ShapeView(theme::Rectangle{}).inset(10.0F).fill(black()));
  });
  ASSERT_TRUE(inset.has_value());
  EXPECT_NEAR(extent(*inset, 80)[0], 50, 1);
  const auto scaled = render([&] {
    return square(
        components::ShapeView(theme::Rectangle{}).scale(0.5F).fill(black()));
  });
  ASSERT_TRUE(scaled.has_value());
  EXPECT_NEAR(extent(*scaled, 80)[0], 60, 1);
  EXPECT_NEAR(extent(*scaled, 80)[1], 100, 1);
  const auto offset = render([&] {
    return square(components::ShapeView(
                      graphics::OffsetShape{theme::Rectangle{},
                                            {.width = 10.0F, .height = 5.0F}})
                      .fill(black()));
  });
  ASSERT_TRUE(offset.has_value());
  EXPECT_NEAR(extent(*offset, 80)[0], 50, 1);
  int top = side;
  for (int y = 0; y < static_cast<int>(side); ++y) {
    if (level(*offset, 80, y) < 0.5F) {
      top = std::min(top, y);
    }
  }
  EXPECT_NEAR(top, 45, 1);
  const auto rotated = render([&] {
    return square(components::ShapeView(theme::Rectangle{})
                      .rotation(motion::Angle::degrees(45.0))
                      .fill(black()),
                  60.0F);
  });
  ASSERT_TRUE(rotated.has_value());
  EXPECT_NEAR(extent(*rotated, 80)[0], 38, 1);
  EXPECT_NEAR(extent(*rotated, 80)[1], 122, 1);
}

TEST_F(ShapeRasterTest, HierarchicalStylesFollowTheLabelRolesAndSwiftUI) {
  using graphics::HierarchicalShapeStyle;
  const std::array<HierarchicalShapeStyle, 5> tiers{
      HierarchicalShapeStyle::primary, HierarchicalShapeStyle::secondary,
      HierarchicalShapeStyle::tertiary, HierarchicalShapeStyle::quaternary,
      HierarchicalShapeStyle::quinary};
  const std::array<theme::ColourRole, 5> roles{
      theme::ColourRole::label, theme::ColourRole::secondary_label,
      theme::ColourRole::tertiary_label, theme::ColourRole::quaternary_label,
      theme::ColourRole::quinary_label};
  const std::array<float, 5> derived{1.0F, 0.5F, 0.25F, 0.18F, 0.18F};
  for (std::size_t index = 0; index < tiers.size(); ++index) {
    const auto tier = render([&] {
      return placed(
          Element{components::ShapeView(theme::Rectangle{}).fill(tiers[index])}
              .frame({.width = 40.0F, .height = 40.0F}));
    });
    const auto role = render([&] {
      return placed(
          Element{components::ShapeView(theme::Rectangle{}).fill(roles[index])}
              .frame({.width = 40.0F, .height = 40.0F}));
    });
    ASSERT_TRUE(tier.has_value());
    ASSERT_TRUE(role.has_value());
    near(rgb(*tier, 80, 80), rgb(*role, 80, 80), 1);
    const auto red = render([&] {
      return placed(
          Element{components::ShapeView(theme::Rectangle{}).fill(tiers[index])}
              .frame({.width = 40.0F, .height = 40.0F})
              .foreground_style(theme::Colour::hex(0xFF0000)));
    });
    ASSERT_TRUE(red.has_value());
    const int fade =
        static_cast<int>(std::lround(255.0F * (1.0F - derived[index])));
    near(rgb(*red, 80, 80), {255, fade, fade});
  }
  const auto explicit_levels = [&](HierarchicalShapeStyle tier, bool three) {
    return render([tier, three] {
      Element swatch =
          Element{components::ShapeView(theme::Rectangle{}).fill(tier)}.frame(
              {.width = 40.0F, .height = 40.0F});
      return placed(three ? std::move(swatch).foreground_style(
                                theme::Colour::hex(0xFF0000),
                                theme::Colour::hex(0x0000FF),
                                theme::Colour::hex(0x00FF00))
                          : std::move(swatch).foreground_style(
                                theme::Colour::hex(0xFF0000),
                                theme::Colour::hex(0x0000FF)));
    });
  };
  near(rgb(*explicit_levels(HierarchicalShapeStyle::secondary, true), 80, 80),
       {0, 0, 255});
  near(rgb(*explicit_levels(HierarchicalShapeStyle::tertiary, true), 80, 80),
       {0, 255, 0});
  near(rgb(*explicit_levels(HierarchicalShapeStyle::quaternary, true), 80, 80),
       {0, 255, 0});
  near(rgb(*explicit_levels(HierarchicalShapeStyle::tertiary, false), 80, 80),
       {0, 0, 255});
}

TEST_F(ShapeRasterTest, BackgroundStyleAndOverlayInShape) {
  const auto styled = render([] {
    return placed(Element{components::VStack()
                              .frame({.width = 40.0F, .height = 40.0F})
                              .background()
                              .background_style(theme::Colour::hex(0x0000FF))});
  });
  ASSERT_TRUE(styled.has_value());
  near(rgb(*styled, 80, 80), {0, 0, 255});
  const auto filled = render([] {
    return placed(Element{components::ShapeView(theme::Rectangle{})
                              .fill(graphics::ShapeStyle::background())}
                      .frame({.width = 40.0F, .height = 40.0F})
                      .background_style(theme::Colour::hex(0xFF0000)));
  });
  ASSERT_TRUE(filled.has_value());
  near(rgb(*filled, 80, 80), {255, 0, 0});
  const auto overlaid = render([] {
    return placed(
        Element{components::VStack()
                    .frame({.width = 40.0F, .height = 40.0F})
                    .background(theme::Colour::hex(0xFFFFFF))
                    .overlay(theme::Colour::hex(0xFF0000), theme::Circle{})},
        0x000000);
  });
  ASSERT_TRUE(overlaid.has_value());
  near(rgb(*overlaid, 80, 80), {255, 0, 0});
  near(rgb(*overlaid, 62, 62), {255, 255, 255});
}

TEST_F(ShapeRasterTest, EllipticalGradientMatchesSwiftUIGeometry) {
  const auto ramp = [](float from, float to) {
    return graphics::EllipticalGradient{
        .gradient = graphics::Gradient{std::vector<theme::ColourValue>{
            theme::Colour::hex(0x000000), theme::Colour::hex(0xFFFFFF)}},
        .start_radius_fraction = from,
        .end_radius_fraction = to};
  };
  for (const bool sdf : {false, true}) {
    const auto image = render([&] {
      Element shape =
          sdf ? Element{components::VStack().background(ramp(0.0F, 0.5F))}
              : Element{components::ShapeView(theme::Rectangle{})
                            .fill(ramp(0.0F, 0.5F))};
      return placed(std::move(shape).frame({.width = 160.0F, .height = 80.0F}));
    });
    ASSERT_TRUE(image.has_value());
    const int top = 40;
    EXPECT_NEAR(level(*image, 80, top + 40), 0.0F, 0.03F) << sdf;
    EXPECT_NEAR(level(*image, 120, top + 40), 0.506F, 0.03F) << sdf;
    EXPECT_NEAR(level(*image, 80, top + 60), 0.5125F, 0.03F) << sdf;
    EXPECT_NEAR(level(*image, 159, top + 40), 1.0F, 0.03F) << sdf;
  }
  const auto fraction = render([&] {
    return placed(Element{
        components::ShapeView(theme::Rectangle{}).fill(ramp(0.1F, 0.3F))}
                      .frame({.width = 160.0F, .height = 80.0F}));
  });
  ASSERT_TRUE(fraction.has_value());
  EXPECT_NEAR(level(*fraction, 112, 80), 0.516F, 0.03F);
  EXPECT_NEAR(level(*fraction, 96, 80), 0.0F, 0.03F);
  EXPECT_NEAR(level(*fraction, 128, 80), 1.0F, 0.03F);
}

TEST_F(ShapeRasterTest, InnerAndDropShadowStylesMatchSwiftUI) {
  const auto inner = [](float radius, float x, float y) {
    return graphics::ShapeStyle{theme::Colour::hex(0xFFFFFF)}.shadow(
        graphics::ShadowStyle::inner(theme::Colour::hex(0x000000), radius, x,
                                     y));
  };
  for (const bool sdf : {false, true}) {
    const auto image = render([&] {
      Element shape = sdf ? Element{components::VStack().background(
                                inner(10.0F, 0.0F, 0.0F))}
                          : Element{components::ShapeView(theme::Rectangle{})
                                        .fill(inner(10.0F, 0.0F, 0.0F))};
      return placed(std::move(shape).frame({.width = 100.0F, .height = 100.0F}),
                    0x808080);
    });
    ASSERT_TRUE(image.has_value());
    const std::array<std::pair<int, float>, 7> profile{{{0, 0.518F},
                                                        {2, 0.596F},
                                                        {5, 0.702F},
                                                        {10, 0.851F},
                                                        {15, 0.945F},
                                                        {20, 0.988F},
                                                        {30, 1.0F}}};
    for (const auto& [distance, expected] : profile) {
      EXPECT_NEAR(level(*image, 30 + distance, 80), expected, 0.04F)
          << "inner distance " << distance << " sdf " << sdf;
    }
  }
  const auto offset = render([&] {
    return placed(Element{components::ShapeView(theme::Rectangle{})
                              .fill(inner(4.0F, 6.0F, 6.0F))}
                      .frame({.width = 100.0F, .height = 100.0F}),
                  0x808080);
  });
  ASSERT_TRUE(offset.has_value());
  const std::array<std::pair<int, float>, 5> left{
      {{0, 0.082F}, {3, 0.271F}, {6, 0.549F}, {9, 0.804F}, {12, 0.953F}}};
  for (const auto& [distance, expected] : left) {
    EXPECT_NEAR(level(*offset, 30 + distance, 80), expected, 0.04F)
        << "left " << distance;
  }
  EXPECT_NEAR(level(*offset, 129, 80), 0.953F, 0.04F);
  const auto drop = render([] {
    return placed(
        Element{components::VStack().background(
                    graphics::ShapeStyle{theme::Colour::hex(0xFFFFFF)}.shadow(
                        graphics::ShadowStyle::drop(
                            theme::Colour::hex(0x000000), 10.0F)))}
            .frame({.width = 60.0F, .height = 60.0F}));
  });
  ASSERT_TRUE(drop.has_value());
  const std::array<std::pair<int, float>, 4> outside{
      {{0, 0.518F}, {5, 0.702F}, {10, 0.851F}, {20, 0.988F}}};
  for (const auto& [distance, expected] : outside) {
    EXPECT_NEAR(level(*drop, 49 - distance, 80), expected, 0.04F)
        << "drop " << distance;
  }
}

TEST_F(ShapeRasterTest, CustomShapePathIsEvaluatedOncePerRect) {
  provider::headless::HeadlessProviderFactory providers;
  const provider::SurfaceSize size{.width = provider::SurfaceWidth{side},
                                   .height = provider::SurfaceHeight{side}};
  providers.surface().resize(size);
  providers.gpu().rasterize(size);
  auto calls = std::make_shared<int>(0);
  auto slot =
      std::make_shared<std::optional<composition::StateHandle<float>>>();
  auto created = Application::create(providers, Resizer{calls, slot});
  ASSERT_TRUE(created.has_value());
  Application application = std::move(*created);
  const auto step = [&] {
    EXPECT_TRUE(
        providers.clock().advance(std::chrono::milliseconds{16}).has_value());
    EXPECT_TRUE(application.frame().has_value());
    return application.last_frame().tessellations;
  };
  for (int frame = 0; frame < 3; ++frame) {
    step();
  }
  EXPECT_EQ(*calls, 1);
  for (int frame = 0; frame < 5; ++frame) {
    EXPECT_EQ(step(), 0U);
  }
  EXPECT_EQ(*calls, 1);
  ASSERT_TRUE(slot->has_value());
  (*slot)->set(90.0F);
  std::uint32_t rebuilt = 0;
  for (int frame = 0; frame < 3; ++frame) {
    rebuilt += step();
  }
  EXPECT_EQ(rebuilt, 1U);
  EXPECT_EQ(*calls, 2);
  const graphics::AnyShape shape{Counting{calls}};
  EXPECT_EQ(shape, graphics::AnyShape{Counting{calls}});
  EXPECT_FALSE(shape == graphics::AnyShape{Counting{std::make_shared<int>(0)}});
}

struct RasterWedge {
  auto path(composition::Rect rect) const -> graphics::Path {
    graphics::Path made;
    made.move(composition::Point{.x = rect.x, .y = rect.y});
    made.add_line(
        composition::Point{.x = rect.x + (rect.width * 0.5F), .y = rect.y});
    made.add_line(composition::Point{.x = rect.x, .y = rect.y + rect.height});
    made.close_subpath();
    return made;
  }

  friend auto operator==(const RasterWedge&, const RasterWedge&)
      -> bool = default;
};

struct RasterFixedWedge {
  auto path(composition::Rect rect) const -> graphics::Path {
    return RasterWedge{}.path(rect);
  }

  auto layout_direction_behavior() const
      -> composition::LayoutDirectionBehavior {
    return composition::LayoutDirectionBehavior::fixed();
  }

  friend auto operator==(const RasterFixedWedge&, const RasterFixedWedge&)
      -> bool = default;
};

TEST_F(ShapeRasterTest, ShapesMirrorInRightToLeftUnlessFixed) {
  using Make = std::function<Element()>;
  const auto side_of = [](Make make, bool rtl) -> std::string_view {
    const auto image = render([make, rtl] {
      Element shape = make().frame({.width = 80.0F, .height = 80.0F});
      if (rtl) {
        shape = std::move(shape).environment(
            components::Environment::layout_direction,
            composition::LayoutDirection::right_to_left);
      }
      return placed(std::move(shape));
    });
    EXPECT_TRUE(image.has_value());
    if (!image.has_value()) {
      return "none";
    }
    int left = 0;
    int right = 0;
    for (int x = 40; x < 120; ++x) {
      if (level(*image, x, 50) < 0.5F) {
        (x < 80 ? left : right) += 1;
      }
    }
    return left > right ? "left" : "right";
  };
  const auto filled = [](graphics::AnyShape shape) -> Make {
    return [shape] {
      return Element{components::ShapeView(shape).fill(
          graphics::ShapeStyle{theme::Colour::hex(0x000000)})};
    };
  };
  EXPECT_EQ(side_of(filled(RasterWedge{}), false), "left");
  EXPECT_EQ(side_of(filled(RasterWedge{}), true), "right");
  EXPECT_EQ(side_of(filled(RasterFixedWedge{}), true), "left");
  const Make path = [] {
    return Element{
        components::ShapeView(
            RasterWedge{}.path(composition::Rect{
                .x = 0.0F, .y = 0.0F, .width = 80.0F, .height = 80.0F}))
            .fill(graphics::ShapeStyle{theme::Colour::hex(0x000000)})};
  };
  EXPECT_EQ(side_of(path, false), "left");
  EXPECT_EQ(side_of(path, true), "right");
  EXPECT_EQ(
      side_of(filled(graphics::AnyShape{RasterWedge{}}.offset(0.0F, 0.0F)),
              true),
      "right");
  EXPECT_EQ(
      side_of(filled(graphics::AnyShape{RasterFixedWedge{}}.offset(0.0F, 0.0F)),
              true),
      "left");
  EXPECT_EQ(side_of(filled(graphics::RotatedShape{RasterWedge{},
                                                  motion::Angle::degrees(0.0)}),
                    true),
            "right");
  EXPECT_EQ(side_of(filled(graphics::ScaledShape{
                        RasterWedge{},
                        composition::Size{.width = 1.0F, .height = 1.0F}}),
                    true),
            "right");
  EXPECT_EQ(side_of(filled(graphics::TransformedShape{
                        RasterWedge{}, graphics::AffineTransform{}}),
                    true),
            "right");
}

TEST_F(ShapeRasterTest, ATransformedUnevenRectangleMirrorsAsAWholePath) {
  const theme::UnevenRoundedRectangle uneven{0.0F, 10.0F, 20.0F, 30.0F};
  const auto mirrored = render([uneven] {
    return placed(
        boxed(
            Element{
                components::ShapeView(graphics::TransformedShape{
                                          uneven, graphics::AffineTransform{}})
                    .fill(graphics::ShapeStyle{theme::Colour::hex(0x000000)})})
            .environment(components::Environment::layout_direction,
                         composition::LayoutDirection::right_to_left));
  });
  ASSERT_TRUE(mirrored.has_value());
  corners(*mirrored, {9, 0, 6, 3});
}

TEST_F(ShapeRasterTest, AnimatedGradientStopsReuseOneRampAndTessellation) {
  const auto make = [](bool on) -> Element {
    return placed(Element{
        components::ShapeView(RasterWedge{})
            .fill(graphics::ShapeStyle{graphics::LinearGradient{
                .gradient = graphics::Gradient{std::vector<theme::ColourValue>{
                    theme::Colour::hex(on ? 0x00FF00 : 0xFF0000),
                    theme::Colour::hex(on ? 0xFFFF00 : 0x0000FF)}},
                .start_point = motion::UnitPoint::leading,
                .end_point = motion::UnitPoint::trailing}})}
                      .frame({.width = 80.0F, .height = 80.0F}));
  };
  const Churn seen = churn(make);
  EXPECT_GT(seen.animated, 10);
  EXPECT_EQ(seen.tessellations, 0U);
  EXPECT_LE(seen.textures, 2U);
  const auto fresh = render([&make] { return make(true); });
  ASSERT_TRUE(seen.settled.has_value());
  ASSERT_TRUE(fresh.has_value());
  EXPECT_TRUE(same_pixels(*seen.settled, *fresh));
}

TEST_F(ShapeRasterTest, PathShapesMovedByLayoutAnimationKeepTheirTessellation) {
  const auto make = [](bool on) -> Element {
    return placed(components::VStack(
        Element{components::ZStack()}.frame(
            {.width = 10.0F, .height = on ? 50.0F : 0.0F}),
        Element{components::ShapeView(RasterWedge{}).fill(black())}.frame(
            {.width = 60.0F, .height = 40.0F})));
  };
  const Churn seen = churn(make);
  EXPECT_GT(seen.animated, 10);
  EXPECT_EQ(seen.tessellations, 0U);
  const auto fresh = render([&make] { return make(true); });
  ASSERT_TRUE(seen.settled.has_value());
  ASSERT_TRUE(fresh.has_value());
  EXPECT_TRUE(same_pixels(*seen.settled, *fresh));
}

TEST_F(ShapeRasterTest, AnimatingCornerRadiiDoNotRasteriseAShadowPerFrame) {
  const auto make = [](bool on) -> Element {
    return placed(Element{components::ZStack()}
                      .frame({.width = 80.0F, .height = 80.0F})
                      .background(theme::Colour::hex(0xFFFFFF),
                                  theme::Shape::rounded(on ? 30.0F : 4.0F))
                      .shadow(theme::Colour::hex(0x000000), 8.0F),
                  0xC0C0C0);
  };
  const Churn seen = churn(make);
  EXPECT_GT(seen.animated, 10);
  EXPECT_LE(seen.shadow_misses, 2U);
  const auto fresh = render([&make] { return make(true); });
  ASSERT_TRUE(seen.settled.has_value());
  ASSERT_TRUE(fresh.has_value());
  EXPECT_TRUE(same_pixels(*seen.settled, *fresh));
}

}
