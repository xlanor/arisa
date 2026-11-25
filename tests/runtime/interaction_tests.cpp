#include <arisa/components/button.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/alignment.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/modifiers.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/provider.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/introspection.hpp>
#include <arisa/runtime/root.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/space.hpp>

#include <gtest/gtest.h>

#include <format>
#include <string>

namespace arisa::runtime {

class InteractionTest : public ::testing::Test {
 protected:
  class Pair final : public composition::Component {
   public:
    static constexpr composition::StateKey<int> first{"first", 0};
    static constexpr composition::StateKey<int> second{"second", 0};

    auto operator==(const Pair&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      const auto one = context.state(first);
      const auto two = context.state(second);
      return components::VStack(
          components::Button("one", [one] { one.set(one.get() + 1); })
              .id("one"),
          components::Button("two", [two] { two.set(two.get() + 1); })
              .id("two"),
          components::Text(std::format("{} {}", one.get(), two.get()))
              .id("label"));
    }
  };

  class Covered final : public composition::Component {
   public:
    static constexpr composition::StateKey<int> taps{"taps", 0};

    auto operator==(const Covered&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      const auto count = context.state(taps);
      return components::ZStack(
          components::Button("under", [count] { count.set(count.get() + 1); })
              .frame({.max_width = composition::infinity,
                      .max_height = composition::infinity})
              .id("under"),
          components::HStack(components::ZStack()
                                 .frame({.width = 60.0F, .height = 20.0F})
                                 .background(theme::ColourRole::accent)
                                 .id("hint"))
              .padding(theme::space::lg)
              .frame({.max_width = composition::infinity,
                      .max_height = composition::infinity,
                      .alignment = composition::Alignment::bottom_trailing})
              .id("bar"),
          components::Text(std::format("{}", count.get()))
              .padding(theme::space::xl)
              .frame({.max_width = composition::infinity,
                      .max_height = composition::infinity,
                      .alignment = composition::Alignment::top_leading})
              .id("label"));
    }
  };

  static void tap(provider::headless::HeadlessProviderFactory& providers,
                  provider::PixelPoint target) {
    provider::InputFrame down;
    down.touches.push_back(
        provider::Touch{.id = provider::TouchId{3},
                        .position = target,
                        .phase = provider::TouchPhase::began});
    provider::InputFrame up;
    up.touches.push_back(provider::Touch{.id = provider::TouchId{3},
                                         .position = target,
                                         .phase = provider::TouchPhase::ended});
    providers.input().push(down);
    providers.input().push(up);
  }

  static auto label(Root& root) -> std::string {
    const auto node = root.tree().find(composition::ElementKey{"label"});
    return std::string{root.tree()
                           .node(*node)
                           ->get()
                           .content_as<components::Text::Content>()
                           ->get()
                           .text()};
  }

  static auto centre(Root& root, std::string_view key) -> provider::PixelPoint {
    const auto node = root.tree().find(composition::ElementKey{key});
    const composition::Rect frame = root.tree().node(*node)->get().frame();
    return provider::PixelPoint{.x = frame.x + (frame.width * 0.5F),
                                .y = frame.y + (frame.height * 0.5F)};
  }

  static auto pad(provider::Buttons buttons) -> provider::InputFrame {
    provider::InputFrame frame;
    frame.pads[0] = provider::PadState{
        .connected = true, .buttons = buttons, .left = {}, .right = {}};
    return frame;
  }

  static void frames(Application& application, int count) {
    for (int index = 0; index < count; ++index) {
      ASSERT_TRUE(application.frame().has_value());
    }
  }
};

TEST_F(InteractionTest, TapActivatesTheButtonUnderTheFinger) {
  provider::headless::HeadlessProviderFactory providers;
  auto application = Application::create(providers, Pair{});
  ASSERT_TRUE(application.has_value());
  frames(*application, 1);
  const provider::PixelPoint target = centre(application->root(), "two");
  provider::InputFrame down;
  down.touches.push_back(provider::Touch{.id = provider::TouchId{7},
                                         .position = target,
                                         .phase = provider::TouchPhase::began});
  provider::InputFrame up;
  up.touches.push_back(provider::Touch{.id = provider::TouchId{7},
                                       .position = target,
                                       .phase = provider::TouchPhase::ended});
  providers.input().push(down);
  providers.input().push(up);
  frames(*application, 3);
  EXPECT_EQ(label(application->root()), "0 1");
}

TEST_F(InteractionTest, TapReleasedOutsideDoesNotActivate) {
  provider::headless::HeadlessProviderFactory providers;
  auto application = Application::create(providers, Pair{});
  ASSERT_TRUE(application.has_value());
  frames(*application, 1);
  const provider::PixelPoint target = centre(application->root(), "one");
  provider::InputFrame down;
  down.touches.push_back(provider::Touch{.id = provider::TouchId{1},
                                         .position = target,
                                         .phase = provider::TouchPhase::began});
  provider::InputFrame up;
  up.touches.push_back(
      provider::Touch{.id = provider::TouchId{1},
                      .position = provider::PixelPoint{.x = 1.0F, .y = 1.0F},
                      .phase = provider::TouchPhase::ended});
  providers.input().push(down);
  providers.input().push(up);
  frames(*application, 3);
  EXPECT_EQ(label(application->root()), "0 0");
}

TEST_F(InteractionTest, PointerClicksActivateAtTheSurfaceScale) {
  provider::headless::HeadlessProviderFactory providers;
  providers.surface().resize(
      provider::SurfaceSize{.width = provider::SurfaceWidth{1920},
                            .height = provider::SurfaceHeight{1080}});
  providers.surface().set_scale(provider::SurfaceScale::create(1.5F).value());
  auto application = Application::create(providers, Pair{});
  ASSERT_TRUE(application.has_value());
  frames(*application, 1);
  const provider::PixelPoint point = centre(application->root(), "one");
  const provider::PixelPoint pixels{.x = point.x * 1.5F, .y = point.y * 1.5F};
  provider::InputFrame press;
  press.pointer = provider::PointerState{.position = pixels, .left = true};
  provider::InputFrame release;
  release.pointer = provider::PointerState{.position = pixels, .left = false};
  providers.input().push(press);
  providers.input().push(release);
  frames(*application, 3);
  EXPECT_EQ(label(application->root()), "1 0");
}

TEST_F(InteractionTest, DpadMovesFocusAndAActivates) {
  provider::headless::HeadlessProviderFactory providers;
  auto application = Application::create(providers, Pair{});
  ASSERT_TRUE(application.has_value());
  frames(*application, 1);
  providers.input().push(pad({provider::Button::dpad_down}));
  providers.input().push(pad({}));
  providers.input().push(pad({provider::Button::a}));
  providers.input().push(pad({}));
  frames(*application, 6);
  EXPECT_EQ(label(application->root()), "0 1");
  providers.input().push(pad({provider::Button::dpad_up}));
  providers.input().push(pad({}));
  providers.input().push(pad({provider::Button::a}));
  providers.input().push(pad({}));
  frames(*application, 6);
  EXPECT_EQ(label(application->root()), "1 1");
  providers.input().push(pad({provider::Button::dpad_up}));
  providers.input().push(pad({}));
  providers.input().push(pad({provider::Button::a}));
  providers.input().push(pad({}));
  frames(*application, 6);
  EXPECT_EQ(label(application->root()), "2 1");
}

TEST_F(InteractionTest, ApplicationReportsFrameTimings) {
  provider::headless::HeadlessProviderFactory providers;
  auto application = Application::create(providers, Pair{});
  ASSERT_TRUE(application.has_value());
  frames(*application, 1);
  EXPECT_TRUE(application->last_frame().laid_out);
  EXPECT_GT(application->last_frame().quads, 0U);
  frames(*application, 1);
  EXPECT_FALSE(application->last_frame().laid_out);
  EXPECT_FALSE(application->last_frame().presented);
  EXPECT_EQ(providers.gpu().geometry_allocations(), 1U);
}

TEST_F(InteractionTest, EmptySlackOfAFrameOrPaddingDoesNotTakeTheTouch) {
  provider::headless::HeadlessProviderFactory providers;
  auto application = Application::create(providers, Covered{});
  ASSERT_TRUE(application.has_value());
  frames(*application, 1);
  Root& root = application->root();
  const provider::PixelPoint middle = centre(root, "under");
  const auto picked = application->introspection().hit(
      composition::Point{.x = middle.x, .y = middle.y});
  ASSERT_TRUE(picked.has_value());
  EXPECT_NE(picked, root.tree().find(composition::ElementKey{"bar"}));
  EXPECT_NE(picked, root.tree().find(composition::ElementKey{"label"}));
  tap(providers, middle);
  frames(*application, 3);
  EXPECT_EQ(label(root), "1");
  const provider::PixelPoint written = centre(root, "hint");
  EXPECT_EQ(application->introspection().hit(
                composition::Point{.x = written.x, .y = written.y}),
            root.tree().find(composition::ElementKey{"hint"}));
  const auto text = root.tree().find(composition::ElementKey{"label"});
  const composition::Rect drawn = root.tree().node(*text)->get().frame();
  tap(providers,
      provider::PixelPoint{.x = drawn.x + 4.0F, .y = drawn.y + 4.0F});
  frames(*application, 3);
  EXPECT_EQ(label(root), "2");
}
}
