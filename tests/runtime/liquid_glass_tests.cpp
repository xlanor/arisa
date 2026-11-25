#include <arisa/components/glass_effect_container.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/shape_view.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/drag_gesture.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/identity.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/namespace.hpp>
#include <arisa/composition/node_id.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/graphics/gradient.hpp>
#include <arisa/graphics/path.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/graphics/stroke_style.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/provider/gpu/image.hpp>
#include <arisa/provider/gpu/shape_vertex.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/provider.hpp>
#include <arisa/provider/surface/surface_scale.hpp>
#include <arisa/provider/surface/surface_size.hpp>
#include <arisa/rendering/compositor.hpp>
#include <arisa/rendering/glass_lens.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/material.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/theme.hpp>

#include <focus_fixture.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace arisa::runtime {

using composition::Element;
using rendering::Compositor;

class LiquidGlassTest : public FocusFixture {
 protected:
  static constexpr provider::SurfaceSize extent{
      .width = provider::SurfaceWidth{200},
      .height = provider::SurfaceHeight{120}};

  class Scene final : public composition::Component {
   public:
    static constexpr composition::StateKey<int> tick{"scene-tick", 0};
    static constexpr composition::StateKey<bool> grey{"scene-grey", false};

    auto operator==(const Scene&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto ticking = context.state(tick);
      const auto greyed = context.state(grey);
      ticks() = ticking;
      greys() = greyed;
      const theme::Colour right = greyed.get() ? theme::Colour::hex(0x808080)
                                               : theme::Colour::hex(0xFFFFFF);
      return components::ZStack(
          components::HStack(
              components::HStackOptions{.spacing = theme::Space{0.0F}},
              components::ZStack()
                  .frame({.width = 100.0F, .height = 120.0F})
                  .background(theme::Colour::hex(0x000000)),
              components::ZStack()
                  .frame({.width = 100.0F, .height = 120.0F})
                  .background(right)),
          components::ZStack()
              .frame({.width = 6.0F, .height = 6.0F})
              .background(ticking.get() % 2 == 0 ? theme::Colour::hex(0xFF0000)
                                                 : theme::Colour::hex(0x00FF00))
              .offset(-90.0F, -54.0F),
          components::ZStack()
              .frame({.width = 120.0F, .height = 60.0F})
              .background(theme::Material::ultra_thin));
    }

    static auto ticks() -> std::optional<composition::StateHandle<int>>& {
      static std::optional<composition::StateHandle<int>> held;
      return held;
    }

    static auto greys() -> std::optional<composition::StateHandle<bool>>& {
      static std::optional<composition::StateHandle<bool>> held;
      return held;
    }
  };

  class Fading final : public composition::Component {
   public:
    static constexpr composition::StateKey<bool> dim{"fading-dim", false};

    auto operator==(const Fading&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto dimmed = context.state(dim);
      held() = dimmed;
      return components::VStack(
                 components::VStackOptions{.spacing = theme::Space{4.0F}},
                 components::ZStack()
                     .frame({.width = 60.0F, .height = 20.0F})
                     .background(theme::Colour::hex(0x2060C0)),
                 components::ZStack()
                     .frame({.width = 60.0F, .height = 20.0F})
                     .background(theme::Colour::hex(0xC02060)),
                 components::ZStack()
                     .frame({.width = 60.0F, .height = 20.0F})
                     .background(theme::Colour::hex(0x20C060)))
          .opacity(dimmed.get() ? 0.2F : 1.0F)
          .id("fading");
    }

    static auto held() -> std::optional<composition::StateHandle<bool>>& {
      static std::optional<composition::StateHandle<bool>> handle;
      return handle;
    }
  };

  static constexpr composition::Namespace merged{"liquid-glass-union"};

  static auto lens(float refraction, float rim) -> theme::Theme {
    theme::LiquidGlassStyle style = theme::Theme::defaults().liquid_glass();
    style.rim = rim;
    style.refraction = refraction;
    style.clear_blur = 0.0F;
    style.shadow = theme::ElevationStyle{};
    return theme::Theme::defaults().with(style);
  }

  static auto panel() -> Element {
    return components::ZStack()
        .frame({.width = 144.0F, .height = 100.0F})
        .glass_effect(theme::Glass::clear(), theme::Shape::rounded(18.0F));
  }

  static auto top_edge(provider::headless::HeadlessProviderFactory& providers,
                       std::uint32_t x, float level) -> float {
    float previous = luma(pixel(providers, x, 0));
    for (std::uint32_t y = 1; y < extent.height.value() / 2; ++y) {
      const float current = luma(pixel(providers, x, y));
      if (previous < level && current >= level) {
        return static_cast<float>(y - 1) +
               ((level - previous) / (current - previous));
      }
      previous = current;
    }
    return static_cast<float>(extent.height.value());
  }

  static auto quiet() -> theme::Theme {
    theme::LiquidGlassStyle style = theme::Theme::defaults().liquid_glass();
    style.rim = 0.0F;
    style.refraction = 12.0F;
    style.clear_blur = 0.0F;
    style.shadow = theme::ElevationStyle{};
    return theme::Theme::defaults().with(style);
  }

  static auto launch(provider::headless::HeadlessProviderFactory& providers,
                     Element content) -> Application {
    providers.surface().resize(extent);
    providers.surface().set_scale(provider::SurfaceScale::unit());
    providers.gpu().rasterize(extent);
    return start(providers, std::move(content));
  }

  static auto pixel(provider::headless::HeadlessProviderFactory& providers,
                    std::uint32_t x, std::uint32_t y) -> std::array<float, 4> {
    const auto image = providers.gpu().capture();
    EXPECT_TRUE(image.has_value());
    if (!image.has_value()) {
      return {};
    }
    const auto found = image->pixel(x, y);
    EXPECT_TRUE(found.has_value());
    if (!found.has_value()) {
      return {};
    }
    return {static_cast<float>((*found)[0]) / 255.0F,
            static_cast<float>((*found)[1]) / 255.0F,
            static_cast<float>((*found)[2]) / 255.0F,
            static_cast<float>((*found)[3]) / 255.0F};
  }

  static auto luma(const std::array<float, 4>& colour) -> float {
    return (colour[0] * 0.2126F) + (colour[1] * 0.7152F) +
           (colour[2] * 0.0722F);
  }

  static auto spread(provider::headless::HeadlessProviderFactory& providers,
                     std::uint32_t row, std::uint32_t from, std::uint32_t to)
      -> int {
    std::vector<float> values;
    for (std::uint32_t x = from; x < to; ++x) {
      values.push_back(luma(pixel(providers, x, row)));
    }
    const auto [low, high] = std::ranges::minmax(values);
    const float span = high - low;
    const auto first = std::ranges::find_if(
        values, [&](float value) { return value > low + (span * 0.2F); });
    const auto last = std::ranges::find_if(
        values, [&](float value) { return value > low + (span * 0.8F); });
    return static_cast<int>(last - first);
  }

  static auto gradient_backdrop() -> Element {
    return components::ShapeView(theme::Shape::rectangle())
        .fill(graphics::ShapeStyle{graphics::LinearGradient{
            .gradient = graphics::Gradient{std::vector<theme::ColourValue>{
                theme::Colour::hex(0x000000), theme::Colour::hex(0xFFFFFF)}},
            .start_point = motion::UnitPoint::leading,
            .end_point = motion::UnitPoint::trailing}})
        .frame({.width = 200.0F, .height = 120.0F});
  }

  static auto pane(theme::Glass glass) -> Element {
    return components::ZStack()
        .frame({.width = 100.0F, .height = 60.0F})
        .glass_effect(glass, theme::Shape::rectangle());
  }

  static auto flat(theme::Colour colour) -> Element {
    return components::ZStack()
        .frame({.width = 200.0F, .height = 120.0F})
        .background(colour);
  }

  class Writing final : public composition::Component {
   public:
    static constexpr composition::StateKey<bool> written{"writing-written",
                                                         false};

    explicit Writing(bool glassy) : glassy_(glassy) {}

    auto operator==(const Writing&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto state = context.state(written);
      held() = state;
      graphics::Path line;
      line.move(composition::Point{.x = 20.0F, .y = 60.0F});
      line.add_line(composition::Point{.x = 180.0F, .y = 60.0F});
      components::ShapeView pen =
          components::ShapeView(std::move(line))
              .trim(0.0, state.get() ? 1.0 : 0.0)
              .stroke(
                  graphics::ShapeStyle{graphics::LinearGradient{
                      .gradient =
                          graphics::Gradient{std::vector<theme::ColourValue>{
                              theme::Colour::hex(0x3060FF),
                              theme::Colour::hex(0xFF3060)}},
                      .start_point = motion::UnitPoint::leading,
                      .end_point = motion::UnitPoint::trailing}},
                  graphics::StrokeStyle{
                      .line_width = 14.0F,
                      .line_cap = graphics::LineCap::round,
                      .line_join = graphics::LineJoin::round});
      if (glassy_) {
        pen = std::move(pen).glass_effect(theme::Glass::clear());
      }
      return components::ZStack(
          stripes(),
          Element{std::move(pen)}.frame({.width = 200.0F, .height = 120.0F}));
    }

    static auto held() -> std::optional<composition::StateHandle<bool>>& {
      static std::optional<composition::StateHandle<bool>> handle;
      return handle;
    }

   private:
    bool glassy_{false};
  };

  static auto stripes() -> Element {
    std::vector<Element> bars;
    for (int index = 0; index < 20; ++index) {
      bars.push_back(Element{
          components::ZStack()
              .frame({.width = 10.0F, .height = 120.0F})
              .background(index % 2 == 0 ? theme::Colour::hex(0x101010)
                                         : theme::Colour::hex(0xF0F0F0))});
    }
    return Element{components::HStack(
        components::HStackOptions{.spacing = theme::Space{0.0F}},
        std::move(bars))};
  }

  static auto glass_reports(
      provider::headless::HeadlessProviderFactory& providers)
      -> std::vector<provider::Diagnostic> {
    std::vector<provider::Diagnostic> found;
    for (provider::Diagnostic& each : providers.diagnostics().snapshot()) {
      if (each.category == "glass") {
        found.push_back(std::move(each));
      }
    }
    return found;
  }

  static auto count(provider::headless::HeadlessProviderFactory& providers,
                    provider::ShapeKind kind) -> std::size_t {
    return static_cast<std::size_t>(
        std::ranges::count_if(providers.gpu().last_shapes().vertices,
                              [kind](const provider::ShapeVertex& vertex) {
                                return vertex.kind == kind;
                              }));
  }
};

TEST_F(LiquidGlassTest, MaterialBlurSpreadsAnEdgeTheBackdropKeepsSharp) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = launch(providers, Element{Scene{}});
  EXPECT_LE(spread(providers, 10, 80, 120), 2);
  EXPECT_GE(spread(providers, 60, 50, 150), 8);
  EXPECT_EQ(count(providers, provider::ShapeKind::material), 4U);
  static_cast<void>(application);
}

TEST_F(LiquidGlassTest, BlurIsCachedWhileTheBackdropStaysStill) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = launch(providers, Element{Scene{}});
  std::uint32_t blurs = 0;
  std::uint32_t hits = 0;
  for (int index = 0; index < 6; ++index) {
    ASSERT_TRUE(application.frame().has_value());
    blurs += application.last_frame().blurs;
  }
  EXPECT_EQ(blurs, 0U);
  for (int index = 1; index <= 6; ++index) {
    Scene::ticks()->set(index);
    ASSERT_TRUE(application.frame().has_value());
    ASSERT_TRUE(application.last_frame().presented);
    blurs += application.last_frame().blurs;
    hits += application.last_frame().blur_hits;
  }
  EXPECT_EQ(blurs, 0U);
  EXPECT_EQ(hits, 6U);
  Scene::greys()->set(true);
  ASSERT_TRUE(application.frame().has_value());
  EXPECT_EQ(application.last_frame().blurs, 1U);
  Scene::ticks().reset();
  Scene::greys().reset();
}

TEST_F(LiquidGlassTest, RefractionBendsTheBackdropNearTheEdgeNotTheCentre) {
  provider::headless::HeadlessProviderFactory plain;
  Application baseline =
      launch(plain, Element{components::ZStack(gradient_backdrop(),
                                               pane(theme::Glass::identity()))}
                        .theme_override(quiet()));
  provider::headless::HeadlessProviderFactory glassy;
  Application refracted =
      launch(glassy, Element{components::ZStack(gradient_backdrop(),
                                                pane(theme::Glass::clear()))}
                         .theme_override(quiet()));
  const float left = luma(pixel(glassy, 52, 60)) - luma(pixel(plain, 52, 60));
  const float right =
      luma(pixel(glassy, 147, 60)) - luma(pixel(plain, 147, 60));
  const float centre =
      luma(pixel(glassy, 100, 60)) - luma(pixel(plain, 100, 60));
  EXPECT_GT(left - centre, 0.02F);
  EXPECT_LT(right - centre, -0.02F);
  static_cast<void>(baseline);
  static_cast<void>(refracted);
}

TEST_F(LiquidGlassTest, ClearGlassLiftsTheBackdropEvenly) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = launch(
      providers, Element{components::ZStack(flat(theme::Colour::hex(0x808080)),
                                            pane(theme::Glass::clear()))}
                     .theme_override(quiet()));
  const std::array<float, 4> seen = pixel(providers, 100, 60);
  const float lifted = (128.0F / 255.0F * rendering::GlassLens::clear_gain) +
                       rendering::GlassLens::clear_lift;
  EXPECT_NEAR(seen[0], lifted, 0.02F);
  EXPECT_NEAR(seen[1], lifted, 0.02F);
  EXPECT_NEAR(seen[2], lifted, 0.02F);
  EXPECT_EQ(count(providers, provider::ShapeKind::glass), 4U);
  static_cast<void>(application);
}

TEST_F(LiquidGlassTest, ClearGlassStaysNeutralOverGreyWithTheDefaultTheme) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = launch(
      providers, Element{components::ZStack(flat(theme::Colour::hex(0x808080)),
                                            pane(theme::Glass::clear()))});
  for (const std::uint32_t x : {52U, 60U, 80U, 100U, 120U, 140U, 147U}) {
    for (const std::uint32_t y : {32U, 45U, 60U, 75U, 88U}) {
      const std::array<float, 4> seen = pixel(providers, x, y);
      EXPECT_NEAR(seen[0], seen[1], 1.5F / 255.0F) << x << "," << y;
      EXPECT_NEAR(seen[1], seen[2], 1.5F / 255.0F) << x << "," << y;
    }
  }
  static_cast<void>(application);
}

TEST_F(LiquidGlassTest, ClearGlassKeepsTheBackdropChroma) {
  provider::headless::HeadlessProviderFactory providers;
  const theme::Colour pink = theme::Colour::hex(0xC08090);
  Application application = launch(
      providers,
      Element{components::ZStack(flat(pink), pane(theme::Glass::clear()))}
          .theme_override(quiet()));
  const std::array<float, 4> seen = pixel(providers, 100, 60);
  const float chroma = rendering::GlassLens::clear_chroma;
  EXPECT_NEAR(seen[0] - seen[1], (pink.red - pink.green) * chroma,
              3.0F / 255.0F);
  EXPECT_NEAR(seen[2] - seen[1], (pink.blue - pink.green) * chroma,
              3.0F / 255.0F);
  static_cast<void>(application);
}

TEST_F(LiquidGlassTest, TintShiftsTheHue) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      launch(providers,
             Element{components::ZStack(
                         flat(theme::Colour::hex(0x808080)),
                         pane(theme::Glass::regular().tint(theme::ColourValue{
                             theme::Colour::hex(0xFF0000).with_alpha(0.5F)})))}
                 .theme_override(quiet()));
  const std::array<float, 4> seen = pixel(providers, 100, 60);
  EXPECT_GT(seen[0] - seen[1], 0.15F);
  EXPECT_GT(seen[0] - seen[2], 0.15F);
  static_cast<void>(application);
}

TEST_F(LiquidGlassTest, IdentityGlassDrawsNothing) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = launch(
      providers, Element{components::ZStack(flat(theme::Colour::hex(0x336699)),
                                            pane(theme::Glass::identity()))}
                     .theme_override(quiet()));
  EXPECT_EQ(count(providers, provider::ShapeKind::glass), 0U);
  EXPECT_EQ(providers.gpu().passes(), 0U);
  EXPECT_EQ(application.last_frame().backdrops, 0U);
  const std::array<float, 4> seen = pixel(providers, 100, 60);
  EXPECT_NEAR(seen[0], 0x33 / 255.0F, 0.01F);
  EXPECT_NEAR(seen[2], 0x99 / 255.0F, 0.01F);
}

TEST_F(LiquidGlassTest, ContainerSharesOneBlurForItsChildren) {
  provider::headless::HeadlessProviderFactory providers;
  const auto bubble = [] {
    return components::ZStack()
        .frame({.width = 40.0F, .height = 30.0F})
        .glass_effect();
  };
  Application application = launch(
      providers,
      Element{components::ZStack(
          flat(theme::Colour::hex(0x406080)),
          components::GlassEffectContainer(
              theme::Space{20.0F},
              components::HStack(
                  components::HStackOptions{.spacing = theme::Space{6.0F}},
                  bubble(), bubble(), bubble())))});
  EXPECT_EQ(count(providers, provider::ShapeKind::glass_union), 4U);
  EXPECT_EQ(providers.gpu().passes(),
            1U + (2U * Compositor::levels_for(
                           theme::Theme::defaults().liquid_glass().blur)));
  static_cast<void>(application);
  EXPECT_EQ(count(providers, provider::ShapeKind::glass), 0U);
}

TEST_F(LiquidGlassTest, InteractiveHighlightFollowsFocus) {
  provider::headless::HeadlessProviderFactory providers;
  const auto chip = [](std::string key) {
    return components::ZStack()
        .frame({.width = 60.0F, .height = 30.0F})
        .glass_effect(theme::Glass::regular().interactive(),
                      theme::Shape::capsule())
        .focusable()
        .id(std::move(key));
  };
  Application application = launch(
      providers, Element{components::HStack(
                     components::HStackOptions{.spacing = theme::Space{20.0F}},
                     chip("left"), chip("right"))});
  const auto lit = [&providers](bool leftmost) {
    float best = leftmost ? 1.0e9F : -1.0e9F;
    float strength = -1.0F;
    for (const provider::ShapeVertex& vertex :
         providers.gpu().last_shapes().vertices) {
      if (vertex.kind != provider::ShapeKind::glass) {
        continue;
      }
      const float x = vertex.position[0];
      if (leftmost ? x < best : x > best) {
        best = x;
        strength = vertex.detail[2];
      }
    }
    return strength;
  };
  press(providers, application, provider::Button::dpad_right);
  const std::string first = focused_key(application);
  ASSERT_TRUE(first == "left" || first == "right") << first;
  EXPECT_GT(lit(first == "left"), 0.0F);
  EXPECT_EQ(lit(first != "left"), 0.0F);
  press(providers, application,
        first == "left" ? provider::Button::dpad_right
                        : provider::Button::dpad_left);
  const std::string second = focused_key(application);
  ASSERT_NE(first, second);
  EXPECT_GT(lit(second == "left"), 0.0F);
  EXPECT_EQ(lit(second != "left"), 0.0F);
}

TEST_F(LiquidGlassTest, InteractiveHighlightFollowsATouchedGesture) {
  provider::headless::HeadlessProviderFactory providers;
  const auto chip = [](std::string key) {
    return components::ZStack()
        .frame({.width = 60.0F, .height = 30.0F})
        .glass_effect(theme::Glass::regular().interactive(),
                      theme::Shape::capsule())
        .gesture(composition::DragGesture())
        .id(std::move(key));
  };
  Application application = launch(
      providers, Element{components::HStack(
                     components::HStackOptions{.spacing = theme::Space{20.0F}},
                     chip("left"), chip("right"))});
  const auto strength = [&providers](bool leftmost) {
    float best = leftmost ? 1.0e9F : -1.0e9F;
    float found = -1.0F;
    for (const provider::ShapeVertex& vertex :
         providers.gpu().last_shapes().vertices) {
      if (vertex.kind != provider::ShapeKind::glass) {
        continue;
      }
      const float x = vertex.position[0];
      if (leftmost ? x < best : x > best) {
        best = x;
        found = vertex.detail[2];
      }
    }
    return found;
  };
  const auto touch = [&providers, &application](provider::PixelPoint at,
                                                provider::TouchPhase phase) {
    provider::InputFrame frame;
    frame.touches.push_back(provider::Touch{
        .id = provider::TouchId{3}, .position = at, .phase = phase});
    providers.input().push(std::move(frame));
    frames(application, 1);
  };
  const provider::PixelPoint at = centre(application, "left");
  EXPECT_EQ(strength(true), 0.0F);
  touch(at, provider::TouchPhase::began);
  EXPECT_GT(strength(true), 0.0F);
  EXPECT_EQ(strength(false), 0.0F);
  touch(provider::PixelPoint{.x = at.x + 24.0F, .y = at.y},
        provider::TouchPhase::moved);
  EXPECT_GT(strength(true), 0.0F);
  EXPECT_EQ(strength(false), 0.0F);
  touch(provider::PixelPoint{.x = at.x + 24.0F, .y = at.y},
        provider::TouchPhase::ended);
  frames(application, 1);
  EXPECT_EQ(strength(true), 0.0F);
}

TEST_F(LiquidGlassTest, FadingSubtreeIsPaintedOnceIntoALayer) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = launch(providers, Element{Fading{}});
  motion::with_animation(motion::Animation::linear(0.4),
                         [] { Fading::held()->set(true); });
  std::uint32_t renders = 0;
  std::uint32_t hits = 0;
  for (int index = 0; index < 20; ++index) {
    ASSERT_TRUE(providers.clock().advance(frame_step).has_value());
    ASSERT_TRUE(application.frame().has_value());
    renders += application.last_frame().layer_renders;
    hits += application.last_frame().layer_hits;
  }
  EXPECT_EQ(renders, 1U);
  EXPECT_GE(hits, 15U);
  Fading::held().reset();
}

TEST_F(LiquidGlassTest, NearContainerChildrenMergeIntoOneSurface) {
  provider::headless::HeadlessProviderFactory providers;
  const auto bubble = [] {
    return components::ZStack()
        .frame({.width = 40.0F, .height = 30.0F})
        .glass_effect(theme::Glass::regular(), theme::Shape::capsule());
  };
  Application application = launch(
      providers,
      Element{components::ZStack(
          flat(theme::Colour::hex(0x406080)),
          components::GlassEffectContainer(
              theme::Space{20.0F},
              components::HStack(
                  components::HStackOptions{.spacing = theme::Space{6.0F}},
                  bubble(), bubble())))});
  const std::array<float, 4> outside = pixel(providers, 10, 60);
  const std::array<float, 4> inside = pixel(providers, 60, 60);
  const std::array<float, 4> neck = pixel(providers, 100, 60);
  const auto difference = [](const std::array<float, 4>& left,
                             const std::array<float, 4>& right) {
    return std::abs(left[0] - right[0]) + std::abs(left[1] - right[1]) +
           std::abs(left[2] - right[2]);
  };
  EXPECT_GT(difference(inside, outside), 0.03F);
  EXPECT_LT(difference(neck, inside), difference(neck, outside));
  static_cast<void>(application);
}

TEST_F(LiquidGlassTest, TrimmedGlassStrokeRefractsMidWrite) {
  provider::headless::HeadlessProviderFactory plain;
  Application baseline = launch(plain, Element{Writing{false}});
  ASSERT_TRUE(Writing::held().has_value());
  Writing::held().reset();
  provider::headless::HeadlessProviderFactory providers;
  Application application = launch(providers, Element{Writing{true}});
  ASSERT_TRUE(Writing::held().has_value());
  motion::with_animation(motion::Animation::linear(1.0),
                         [] { Writing::held()->set(true); });
  for (int index = 0; index < 30; ++index) {
    ASSERT_TRUE(providers.clock().advance(frame_step).has_value());
    ASSERT_TRUE(application.frame().has_value());
  }
  const auto changed = [&](std::uint32_t x) {
    float most = 0.0F;
    for (std::uint32_t y = 52; y <= 68; ++y) {
      most = std::max(most, std::abs(luma(pixel(providers, x, y)) -
                                     luma(pixel(plain, x, y))));
    }
    return most;
  };
  EXPECT_GT(changed(45), 0.08F);
  EXPECT_GT(changed(65), 0.08F);
  EXPECT_LT(changed(170), 0.01F);
  EXPECT_GT(count(providers, provider::ShapeKind::glass_path), 0U);
  Writing::held().reset();
  static_cast<void>(baseline);
}

TEST_F(LiquidGlassTest, OversizedBackdropIsCapturedSmallerAndReportedOnce) {
  provider::headless::HeadlessProviderFactory providers;
  providers.gpu().limit_render_targets(
      provider::headless::NullGpu::RenderTargetLimit{.extent = 64});
  Application application =
      launch(providers, Element{components::ZStack(gradient_backdrop(),
                                                   pane(theme::Glass::clear()))}
                            .theme_override(quiet()));
  for (int index = 0; index < 5; ++index) {
    ASSERT_TRUE(providers.clock().advance(frame_step).has_value());
    ASSERT_TRUE(application.frame().has_value());
  }
  EXPECT_EQ(count(providers, provider::ShapeKind::glass), 4U);
  const auto reports = glass_reports(providers);
  ASSERT_EQ(reports.size(), 1U);
  EXPECT_NE(reports.front().message.find("captured at"), std::string::npos);
}

TEST_F(LiquidGlassTest, RefusedBackdropLeavesAVisibleStrokeAndReportsOnce) {
  provider::headless::HeadlessProviderFactory providers;
  providers.gpu().limit_render_targets(
      provider::headless::NullGpu::RenderTargetLimit{.extent = 1});
  Application application = launch(providers, Element{Writing{true}});
  ASSERT_TRUE(Writing::held().has_value());
  Writing::held()->set(true);
  for (int index = 0; index < 5; ++index) {
    ASSERT_TRUE(providers.clock().advance(frame_step).has_value());
    ASSERT_TRUE(application.frame().has_value());
  }
  EXPECT_EQ(count(providers, provider::ShapeKind::glass_path), 0U);
  EXPECT_GT(count(providers, provider::ShapeKind::path), 0U);
  EXPECT_GT(luma(pixel(providers, 45, 60)) - luma(pixel(providers, 45, 20)),
            0.2F);
  const auto reports = glass_reports(providers);
  ASSERT_EQ(reports.size(), 1U);
  EXPECT_NE(reports.front().message.find("flat fallback"), std::string::npos);
  Writing::held().reset();
}

TEST_F(LiquidGlassTest, ARejectedBackdropPassFallsBackToFlatGlass) {
  provider::headless::HeadlessProviderFactory providers;
  providers.gpu().reject_draws(provider::headless::NullGpu::DrawRejection{
      .skipped = 0, .count = 1, .passes = true});
  Application application =
      launch(providers, Element{components::ZStack(gradient_backdrop(),
                                                   pane(theme::Glass::clear()))}
                            .theme_override(quiet()));
  for (int index = 0; index < 5; ++index) {
    ASSERT_TRUE(providers.clock().advance(frame_step).has_value());
    const auto framed = application.frame();
    ASSERT_TRUE(framed.has_value()) << framed.error().explanation();
  }
  EXPECT_EQ(count(providers, provider::ShapeKind::glass), 0U);
  const auto reports = glass_reports(providers);
  ASSERT_EQ(reports.size(), 1U);
  EXPECT_NE(reports.front().message.find("backdrop pass"), std::string::npos);
  EXPECT_NE(reports.front().message.find("for node"), std::string::npos);
  EXPECT_NE(reports.front().message.find("draw_rejected at backdrop capture"),
            std::string::npos);
  EXPECT_NE(reports.front().message.find("flat fallback"), std::string::npos);
}

TEST_F(LiquidGlassTest, RegularGlassCentreStillShowsTheStripes) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = launch(
      providers, Element{components::ZStack(
                     stripes(), components::ZStack()
                                    .frame({.width = 140.0F, .height = 60.0F})
                                    .glass_effect(theme::Glass::regular(),
                                                  theme::Shape::capsule()))});
  std::vector<float> centre;
  for (std::uint32_t x = 70; x < 130; ++x) {
    centre.push_back(luma(pixel(providers, x, 60)));
  }
  const auto [low, high] = std::ranges::minmax(centre);
  EXPECT_GT(high - low, 0.3F);
  EXPECT_EQ(count(providers, provider::ShapeKind::glass), 4U);
  static_cast<void>(application);
}

TEST_F(LiquidGlassTest, EdgeBandLensesTheStripesAndTheCentreStaysPut) {
  provider::headless::HeadlessProviderFactory flat;
  Application baseline = launch(
      flat, Element{components::ZStack(stripes(), panel())}.theme_override(
                lens(0.0F, 0.0F)));
  provider::headless::HeadlessProviderFactory bent;
  Application lensed = launch(
      bent, Element{components::ZStack(stripes(), panel())}.theme_override(
                lens(8.0F, 0.0F)));
  float edge = 0.0F;
  for (std::uint32_t x = 29; x < 42; ++x) {
    edge += std::abs(luma(pixel(bent, x, 60)) - luma(pixel(flat, x, 60)));
  }
  float centre = 0.0F;
  for (std::uint32_t x = 80; x < 120; ++x) {
    centre += std::abs(luma(pixel(bent, x, 60)) - luma(pixel(flat, x, 60)));
  }
  EXPECT_GT(edge, 1.0F);
  EXPECT_LT(centre, 0.05F);
  static_cast<void>(baseline);
  static_cast<void>(lensed);
}

TEST_F(LiquidGlassTest, SpecularBandRunsAlongTheTopEdge) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = launch(
      providers,
      Element{components::ZStack(flat(theme::Colour::hex(0x606060)), panel())}
          .theme_override(lens(0.0F, 0.5F)));
  const float body = luma(pixel(providers, 100, 60));
  const float top = luma(pixel(providers, 100, 10));
  const float side = luma(pixel(providers, 28, 60));
  EXPECT_GT(top - body, 0.2F);
  EXPECT_GT(top - side, 0.15F);
  EXPECT_GT(luma(pixel(providers, 100, 11)) - body, 0.08F);
  EXPECT_LT(luma(pixel(providers, 100, 14)) - body, 0.06F);
  EXPECT_LT(luma(pixel(providers, 100, 18)) - body, 0.02F);
  static_cast<void>(application);
}

TEST_F(LiquidGlassTest, UnionedShapesDrawOneCapsuleWithAFlatTop) {
  provider::headless::HeadlessProviderFactory providers;
  const composition::Namespace::ID space{composition::Identity{&merged},
                                         composition::NodeId{1}};
  const auto member = [space] {
    return components::ZStack()
        .frame({.width = 40.0F, .height = 30.0F})
        .glass_effect(theme::Glass::regular().tint(
                          theme::ColourValue{theme::Colour::hex(0xFFFFFF)}),
                      theme::Shape::capsule())
        .glass_effect_union(composition::ElementKey{"run"}, space);
  };
  Application application = launch(
      providers,
      Element{components::ZStack(flat(theme::Colour::hex(0x000000)),
                                 components::GlassEffectContainer(
                                     theme::Space{20.0F},
                                     components::HStack(
                                         components::HStackOptions{
                                             .spacing = theme::Space{6.0F}},
                                         member(), member(), member())))}
          .theme_override(quiet()));
  const float level = luma(pixel(providers, 100, 60)) * 0.5F;
  const float reference = top_edge(providers, 100, level);
  for (std::uint32_t x = 55; x <= 145; x += 5) {
    EXPECT_NEAR(top_edge(providers, x, level), reference, 1.0F) << x;
  }
  EXPECT_GT(top_edge(providers, 36, level), reference + 3.0F);
  EXPECT_GT(top_edge(providers, 164, level), reference + 3.0F);
  static_cast<void>(application);
}

}
