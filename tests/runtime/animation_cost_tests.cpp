#include <arisa/components/button.hpp>
#include <arisa/components/for_each.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/navigation_destination.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/controls/progress_view.hpp>
#include <arisa/controls/progress_view_style.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/presentation.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/presentation/navigation_link.hpp>
#include <arisa/presentation/navigation_stack.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/runtime/frame_timings.hpp>
#include <arisa/structure/list.hpp>
#include <arisa/structure/tab.hpp>
#include <arisa/structure/tab_view.hpp>
#include <arisa/structure/tab_view_style.hpp>

#include <focus_fixture.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <format>
#include <functional>
#include <optional>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace arisa::runtime {

using composition::Element;
using provider::Button;

class AnimationCostTest : public FocusFixture {
 protected:
  struct Cost {
    int frames{0};
    int presented{0};
    int animated{0};
    int laid_out{0};
    std::uint64_t evaluations{0};
    std::size_t shaped{0};
    std::uint32_t culled{0};
    std::uint32_t occluded{0};
    std::optional<double> frame_rate;
    bool animating{false};
  };

  struct Probe {
    std::optional<composition::StateHandle<bool>> flag;
    std::optional<composition::StateHandle<std::vector<int>>> items;
  };

  using Build = std::function<Element(Probe&, composition::Context&)>;

  class Host final : public composition::Component {
   public:
    Host(Probe* probe, const Build* build) noexcept
        : probe_(probe), build_(build) {}

    auto operator==(const Host&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      return (*build_)(*probe_, context);
    }

   private:
    Probe* probe_;
    const Build* build_;
  };

  static constexpr composition::StateKey<bool> flag_key{"flag", false};
  static const composition::StateKey<std::vector<int>> items_key;
  static constexpr composition::StateKey<float> width_key{"width", 100.0F};

  static auto measure(provider::headless::HeadlessProviderFactory& providers,
                      Application& application, int count) -> Cost {
    Cost cost;
    for (int index = 0; index < count; ++index) {
      EXPECT_TRUE(providers.clock().advance(frame_step).has_value());
      EXPECT_TRUE(application.frame().has_value());
      const FrameTimings& timings = application.last_frame();
      ++cost.frames;
      cost.presented += timings.presented ? 1 : 0;
      cost.animated += timings.kind == FrameKind::animation ? 1 : 0;
      cost.laid_out += timings.laid_out ? 1 : 0;
      cost.evaluations += timings.evaluations;
      cost.shaped += timings.shaped;
      cost.culled = std::max(cost.culled, timings.culled);
      cost.occluded = std::max(cost.occluded, timings.occluded);
      if (timings.presented) {
        cost.frame_rate = timings.frame_rate;
        cost.animating = timings.animating;
      }
    }
    return cost;
  }

  static void expect_paint_only(const Cost& cost) {
    EXPECT_GT(cost.presented, 0);
    EXPECT_EQ(cost.animated, cost.presented);
    EXPECT_EQ(cost.laid_out, 0);
    EXPECT_EQ(cost.evaluations, 0U);
    EXPECT_EQ(cost.shaped, 0U);
  }

  static auto filler() -> std::vector<Element> {
    std::vector<Element> rows;
    for (int index = 0; index < 12; ++index) {
      rows.push_back(components::Text(std::format("static text {}", index)));
    }
    return rows;
  }
};

const composition::StateKey<std::vector<int>> AnimationCostTest::items_key{
    "items", {1, 2, 3, 4}};

TEST_F(AnimationCostTest, SpinnerRepaintsWithoutBodyLayoutOrShaping) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe&, composition::Context&) -> Element {
    return components::VStack(components::VStack(filler()),
                              controls::ProgressView());
  };
  Application application = start(providers, Host{&probe, &build});
  static_cast<void>(measure(providers, application, 4));
  const Cost cost = measure(providers, application, 60);
  expect_paint_only(cost);
  EXPECT_TRUE(cost.animating);
  EXPECT_EQ(cost.frame_rate,
            controls::ProgressViewStyle::indeterminate_frame_rate);
  EXPECT_LE(cost.presented, 33);
  EXPECT_GE(cost.presented, 27);
}

TEST_F(AnimationCostTest, IndeterminateSweepRepaintsOnly) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe&, composition::Context&) -> Element {
    return components::VStack(
        components::VStack(filler()),
        controls::ProgressView()
            .progress_view_style(controls::LinearProgressViewStyle{})
            .frame({.width = 300.0F}));
  };
  Application application = start(providers, Host{&probe, &build});
  static_cast<void>(measure(providers, application, 6));
  const Cost cost = measure(providers, application, 60);
  expect_paint_only(cost);
  EXPECT_EQ(cost.frame_rate,
            controls::ProgressViewStyle::indeterminate_frame_rate);
}

TEST_F(AnimationCostTest, OffscreenSpinnerPausesAndLetsTheLoopIdle) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe&, composition::Context&) -> Element {
    std::vector<Element> rows;
    for (int index = 0; index < 40; ++index) {
      rows.push_back(components::Text(std::format("row {}", index))
                         .frame({.height = 60.0F}));
    }
    rows.push_back(controls::ProgressView().id("spinner"));
    return components::ScrollView(components::VStack(std::move(rows)))
        .frame({.height = 300.0F});
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_GT(application.last_frame().culled, 0U);
  static_cast<void>(measure(providers, application, 6));
  const Cost cost = measure(providers, application, 30);
  EXPECT_FALSE(cost.animating);
  EXPECT_LE(cost.presented, 1);
}

TEST_F(AnimationCostTest, TabCapsuleSpringsWithoutLayout) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe&, composition::Context&) -> Element {
    return structure::TabView(
               structure::Tab("One", "star",
                              [] { return Element{components::Text("one")}; }),
               structure::Tab("Two", "wifi",
                              [] { return Element{components::Text("two")}; }))
        .tab_view_style(structure::SidebarAdaptableTabViewStyle{});
  };
  Application application = start(providers, Host{&probe, &build});
  static_cast<void>(measure(providers, application, 4));
  press(providers, application, Button::r);
  static_cast<void>(measure(providers, application, 3));
  const Cost cost = measure(providers, application, 12);
  expect_paint_only(cost);
}

TEST_F(AnimationCostTest, ListMovesAnimateFramesWithoutRelayout) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen,
                         composition::Context& context) -> Element {
    const auto items = context.state(items_key);
    seen.items = items;
    return structure::List(components::ForEach(
        items.get(), [](int item) { return item; },
        [](int item) {
          return components::Text(std::format("item {}", item));
        }));
  };
  Application application = start(providers, Host{&probe, &build});
  static_cast<void>(measure(providers, application, 4));
  motion::with_animation(motion::Animation::smooth(0.5), [&probe] {
    probe.items->update(
        [](std::vector<int>& values) { std::ranges::reverse(values); });
  });
  static_cast<void>(measure(providers, application, 3));
  const Cost cost = measure(providers, application, 10);
  expect_paint_only(cost);
}

TEST_F(AnimationCostTest, NavigationPushSlidesWithoutRelayout) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe&, composition::Context&) -> Element {
    return presentation::NavigationStack([] {
      return Element{
          components::VStack(
              presentation::NavigationLink(1, std::string{"Open"}).id("open"))}
          .navigation_destination(std::type_identity<int>{}, [](int value) {
            return components::Text(std::format("Detail {}", value));
          });
    });
  };
  Application application = start(providers, Host{&probe, &build});
  press(providers, application, Button::a);
  static_cast<void>(measure(providers, application, 3));
  const Cost cost = measure(providers, application, 10);
  expect_paint_only(cost);
}

TEST_F(AnimationCostTest, CoveredScreenPausesUntilRevealed) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe&, composition::Context&) -> Element {
    return presentation::NavigationStack([] {
      return Element{
          components::VStack(
              presentation::NavigationLink(1, std::string{"Open"}).id("open"),
              controls::ProgressView().id("spinner"))}
          .navigation_destination(std::type_identity<int>{}, [](int value) {
            return components::Text(std::format("Detail {}", value));
          });
    });
  };
  Application application = start(providers, Host{&probe, &build});
  static_cast<void>(measure(providers, application, 4));
  const auto spinner = node_of(application, "spinner");
  ASSERT_TRUE(spinner.has_value());
  const auto presentation = [&]() -> const motion::Presentation& {
    return application.root().tree().presentation(
        application.root().tree().node(*spinner)->get());
  };
  EXPECT_TRUE(measure(providers, application, 4).animating);
  press(providers, application, Button::a);
  EXPECT_GT(measure(providers, application, 60).occluded, 0U);
  EXPECT_TRUE(presentation().suspended());
  const Cost covered = measure(providers, application, 30);
  EXPECT_FALSE(covered.animating);
  EXPECT_LE(covered.presented, 1);
  press(providers, application, Button::b);
  static_cast<void>(measure(providers, application, 60));
  EXPECT_FALSE(presentation().suspended());
  const Cost revealed = measure(providers, application, 4);
  EXPECT_TRUE(revealed.animating);
  EXPECT_EQ(revealed.occluded, 0U);
}

TEST_F(AnimationCostTest, SpinnerUnderAHiddenAncestorPausesAndResumes) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen,
                         composition::Context& context) -> Element {
    const auto hidden = context.state(flag_key);
    seen.flag = hidden;
    return components::VStack(controls::ProgressView().id("spinner"))
        .opacity(hidden.get() ? 0.0F : 1.0F);
  };
  Application application = start(providers, Host{&probe, &build});
  static_cast<void>(measure(providers, application, 4));
  const auto spinner = node_of(application, "spinner");
  ASSERT_TRUE(spinner.has_value());
  const auto presentation = [&]() -> const motion::Presentation& {
    return application.root().tree().presentation(
        application.root().tree().node(*spinner)->get());
  };
  EXPECT_TRUE(measure(providers, application, 4).animating);
  probe.flag->set(true);
  static_cast<void>(measure(providers, application, 4));
  const Cost hidden = measure(providers, application, 30);
  EXPECT_FALSE(hidden.animating);
  EXPECT_LE(hidden.presented, 1);
  probe.flag->set(false);
  static_cast<void>(measure(providers, application, 4));
  EXPECT_FALSE(presentation().suspended());
  EXPECT_TRUE(measure(providers, application, 4).animating);
}

TEST_F(AnimationCostTest, GeometryFeedbackLoopIsReported) {
  for (const bool looping : {false, true}) {
    provider::headless::HeadlessProviderFactory providers;
    Probe probe;
    const Build build = [looping](Probe&,
                                  composition::Context& context) -> Element {
      const auto width = context.state(width_key);
      return components::VStack(
          components::Text("Measured")
              .frame({.width = width.get(), .height = 30.0F})
              .on_geometry_change(
                  std::type_identity<float>{},
                  [](const composition::GeometryProxy& proxy) {
                    return proxy.size().width;
                  },
                  [width, looping](float value) {
                    width.set(looping ? (value < 110.0F ? 120.0F : 100.0F)
                                      : value);
                  }));
    };
    Application application = start(providers, Host{&probe, &build});
    static_cast<void>(measure(providers, application, 30));
    const auto reports = providers.diagnostics().snapshot();
    const bool reported = std::ranges::any_of(
        reports, [](const provider::Diagnostic& diagnostic) {
          return diagnostic.category == "geometry";
        });
    EXPECT_EQ(reported, looping);
  }
}

TEST_F(AnimationCostTest, SheetTransitionRepaintsOnly) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen,
                         composition::Context& context) -> Element {
    const auto shown = context.state(flag_key);
    seen.flag = shown;
    return components::VStack(components::Text("Base")).sheet(shown, [] {
      return components::Text("Sheet");
    });
  };
  Application application = start(providers, Host{&probe, &build});
  probe.flag->set(true);
  static_cast<void>(measure(providers, application, 3));
  const Cost cost = measure(providers, application, 10);
  expect_paint_only(cost);
}

TEST_F(AnimationCostTest, ScrollFlingMovesContentWithoutRelayout) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe&, composition::Context&) -> Element {
    std::vector<Element> rows;
    for (int index = 0; index < 60; ++index) {
      rows.push_back(components::Text(std::format("row {}", index))
                         .frame({.width = 200.0F, .height = 50.0F}));
    }
    return components::ScrollView(components::VStack(std::move(rows)))
        .frame({.width = 300.0F, .height = 400.0F})
        .id("scroller");
  };
  Application application = start(providers, Host{&probe, &build});
  const provider::PixelPoint from = centre(application, "scroller");
  const auto touch = [](provider::PixelPoint at, provider::TouchPhase phase) {
    provider::InputFrame frame;
    frame.touches.push_back(provider::Touch{
        .id = provider::TouchId{4}, .position = at, .phase = phase});
    return frame;
  };
  providers.input().push(touch(from, provider::TouchPhase::began));
  frames(application, 1);
  for (int step = 1; step <= 5; ++step) {
    ASSERT_TRUE(providers.clock().advance(frame_step).has_value());
    providers.input().push(touch(
        provider::PixelPoint{.x = from.x,
                             .y = from.y - (40.0F * static_cast<float>(step))},
        provider::TouchPhase::moved));
    frames(application, 1);
  }
  providers.input().push(
      touch(provider::PixelPoint{.x = from.x, .y = from.y - 200.0F},
            provider::TouchPhase::ended));
  frames(application, 1);
  const Cost cost = measure(providers, application, 10);
  EXPECT_GT(cost.presented, 0);
  EXPECT_EQ(cost.laid_out, 0);
  EXPECT_EQ(cost.evaluations, 0U);
  EXPECT_EQ(cost.shaped, 0U);
}

TEST_F(AnimationCostTest, SettledPagesStopPresenting) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe&, composition::Context&) -> Element {
    return components::VStack(filler());
  };
  Application application = start(providers, Host{&probe, &build});
  static_cast<void>(measure(providers, application, 6));
  const Cost cost = measure(providers, application, 30);
  EXPECT_EQ(cost.presented, 0);
  EXPECT_FALSE(cost.animating);
}

}
