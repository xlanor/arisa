#include <arisa/async/awaitables.hpp>
#include <arisa/async/task.hpp>
#include <arisa/components/button.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/refresh_action.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/shape_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/controls/refreshable.hpp>
#include <arisa/graphics/stroke_style.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/provider/gpu/shape_vertex.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/headless/null_gpu.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/runtime/frame_timings.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/shape.hpp>

#include <scroll_fixture.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace arisa::runtime {

using composition::Element;

class PaginationTest : public ScrollFixture {
 protected:
  struct Probe {
    std::optional<composition::StateHandle<bool>> flag;
    std::optional<composition::StateHandle<std::optional<int>>> position;
    std::vector<bool> shown;
    std::vector<std::vector<int>> targets;
    int refreshed{0};
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
  static constexpr composition::StateKey<std::optional<int>> position_key{
      "position", std::nullopt};
  static constexpr float row_height = 40.0F;

  static auto rows(int count) -> std::vector<Element> {
    std::vector<Element> found;
    for (int index = 0; index < count; ++index) {
      found.push_back(components::Text(std::format("row {}", index))
                          .frame({.height = row_height})
                          .id(static_cast<std::int64_t>(index)));
    }
    return found;
  }

  static auto advance(provider::headless::HeadlessProviderFactory& providers,
                      Application& application, int count) {
    for (int index = 0; index < count; ++index) {
      ScrollFixture::step(providers, application);
    }
  }

  static auto refresh(Probe* probe) -> async::Task<void> {
    for (int frame = 0; frame < 3; ++frame) {
      co_await async::next_frame();
    }
    ++probe->refreshed;
  }
};

TEST_F(PaginationTest, TrimAnimationRepaintsWithoutLayoutOrBody) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen,
                         composition::Context& context) -> Element {
    const auto on = context.state(flag_key);
    seen.flag = on;
    return components::VStack(
        components::Text("static"),
        Element{components::ShapeView(theme::Circle{})
                    .trim(on.get() ? 0.4 : 0.0, on.get() ? 1.0 : 0.1)
                    .stroke(theme::Colour::hex(0x0AB9E6),
                            graphics::StrokeStyle{.line_width = 6.0F})}
            .frame({.width = 80.0F, .height = 80.0F}));
  };
  Application application = start(providers, Host{&probe, &build});
  advance(providers, application, 4);
  motion::with_animation(motion::Animation::linear(1.0),
                         [&probe] { probe.flag->set(true); });
  advance(providers, application, 2);
  int laid = 0;
  int presented = 0;
  std::uint64_t evaluations = 0;
  for (int index = 0; index < 30; ++index) {
    ScrollFixture::step(providers, application);
    const FrameTimings& timings = application.last_frame();
    laid += timings.laid_out ? 1 : 0;
    presented += timings.presented ? 1 : 0;
    evaluations += timings.evaluations;
  }
  EXPECT_GT(presented, 20);
  EXPECT_EQ(laid, 0);
  EXPECT_EQ(evaluations, 0U);
}

TEST_F(PaginationTest, DelayedTrimHoldsItsStartUntilTheDelayElapses) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen,
                         composition::Context& context) -> Element {
    const auto on = context.state(flag_key);
    seen.flag = on;
    return Element{components::ShapeView(theme::Circle{})
                       .trim(0.0, on.get() ? 1.0 : 0.0)
                       .stroke(theme::Colour::hex(0xFFFFFF),
                               graphics::StrokeStyle{.line_width = 6.0F})}
        .frame({.width = 80.0F, .height = 80.0F});
  };
  const auto drawn = [&providers] {
    std::size_t found = 0;
    for (const provider::ShapeVertex& vertex :
         providers.gpu().last_shapes().vertices) {
      found += vertex.kind == provider::ShapeKind::path ? 1U : 0U;
    }
    return found;
  };
  Application application = start(providers, Host{&probe, &build});
  advance(providers, application, 4);
  EXPECT_EQ(drawn(), 0U);
  motion::with_animation(motion::Animation::linear(0.5).delay(1.0),
                         [&probe] { probe.flag->set(true); });
  advance(providers, application, 40);
  EXPECT_EQ(drawn(), 0U);
  advance(providers, application, 60);
  EXPECT_GT(drawn(), 0U);
}

TEST_F(PaginationTest, OffsetBringsAnOffscreenLayoutBackIntoView) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe&, composition::Context&) -> Element {
    return components::ScrollView(
        components::VStack(components::VStackOptions{.spacing = 0.0F},
                           components::Text("top").frame({.height = 3000.0F}),
                           Element{components::ShapeView(theme::Rectangle{})
                                       .fill(theme::Colour::hex(0x00FF00))}
                               .frame({.width = 50.0F, .height = 50.0F})
                               .offset(0.0F, -2900.0F)));
  };
  Application application = start(providers, Host{&probe, &build});
  advance(providers, application, 4);
  std::size_t drawn = 0;
  for (const provider::ShapeVertex& vertex :
       providers.gpu().last_shapes().vertices) {
    drawn += vertex.kind == provider::ShapeKind::path ? 1U : 0U;
  }
  EXPECT_GT(drawn, 0U);
}

TEST_F(PaginationTest, ScrollVisibilityCallbacksReportCrossings) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context&) -> Element {
    std::vector<Element> items = rows(30);
    items[20] = std::move(items[20]).on_scroll_visibility_change(
        [&seen](bool visible) { seen.shown.push_back(visible); });
    return components::ScrollView(
               components::VStack(components::VStackOptions{.spacing = 0.0F},
                                  std::move(items))
                   .scroll_target_layout())
        .on_scroll_target_visibility_change(
            std::type_identity<int>{}, 0.5,
            [&seen](const std::vector<int>& visible) {
              seen.targets.push_back(visible);
            })
        .frame({.width = 300.0F, .height = 400.0F})
        .id("scroller");
  };
  Application application = start(providers, Host{&probe, &build});
  advance(providers, application, 4);
  EXPECT_TRUE(probe.shown.empty());
  ASSERT_FALSE(probe.targets.empty());
  EXPECT_EQ(probe.targets.back().front(), 0);
  EXPECT_EQ(probe.targets.back().size(), 10U);
  const provider::PixelPoint middle = centre(application, "scroller");
  drag(providers, application, middle,
       provider::PixelPoint{.x = middle.x, .y = middle.y - 500.0F}, 20);
  static_cast<void>(settle(providers, application));
  ASSERT_FALSE(probe.shown.empty());
  EXPECT_TRUE(probe.shown.back());
  EXPECT_GT(probe.targets.back().front(), 5);
}

TEST_F(PaginationTest, ScrollPositionScrollsToTheBoundIdAndRestoresOnRemount) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen,
                         composition::Context& context) -> Element {
    const auto position = context.state(position_key);
    const auto swapped = context.state(flag_key);
    seen.position = position;
    seen.flag = swapped;
    return components::ScrollView(
               components::VStack(components::VStackOptions{.spacing = 0.0F},
                                  rows(60))
                   .scroll_target_layout())
        .scroll_position(position)
        .frame({.width = 300.0F, .height = 400.0F})
        .id(swapped.get() ? "second" : "first");
  };
  Application application = start(providers, Host{&probe, &build});
  advance(providers, application, 4);
  probe.position->set(25);
  advance(providers, application, 6);
  EXPECT_NEAR(offset(application, "first").y, 25.0F * row_height, 1.0F);
  probe.flag->set(true);
  advance(providers, application, 6);
  EXPECT_NEAR(offset(application, "second").y, 25.0F * row_height, 1.0F);
  const provider::PixelPoint middle = centre(application, "second");
  drag(providers, application, middle,
       provider::PixelPoint{.x = middle.x, .y = middle.y + 200.0F}, 10);
  static_cast<void>(settle(providers, application));
  ASSERT_TRUE(probe.position->get().has_value());
  EXPECT_LT(*probe.position->get(), 25);
}

TEST_F(PaginationTest, RefreshableRunsOnControllerAndPull) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context&) -> Element {
    return components::ScrollView(
               components::VStack(components::VStackOptions{.spacing = 0.0F},
                                  rows(5))
                   .frame({.width = 300.0F})
                   .focusable())
        .frame({.width = 300.0F, .height = 400.0F})
        .id("scroller")
        .refreshable([&seen] { return refresh(&seen); });
  };
  Application application = start(providers, Host{&probe, &build});
  advance(providers, application, 4);
  providers.input().push(pad({controls::Refreshable::button}));
  providers.input().push(pad({}));
  advance(providers, application, 2);
  EXPECT_TRUE(node_of(application, "arisa.refresh.indicator").has_value());
  EXPECT_EQ(probe.refreshed, 0);
  advance(providers, application, 30);
  EXPECT_EQ(probe.refreshed, 1);
  EXPECT_FALSE(node_of(application, "arisa.refresh.indicator").has_value());
  const provider::PixelPoint middle = centre(application, "scroller");
  drag(providers, application, middle,
       provider::PixelPoint{.x = middle.x, .y = middle.y + 260.0F}, 20);
  advance(providers, application, 40);
  EXPECT_EQ(probe.refreshed, 2);
}

TEST_F(PaginationTest, TheRefreshActionInTheEnvironmentRunsTheRefresh) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  class Puller final : public composition::Component {
   public:
    auto operator==(const Puller&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const std::optional<components::RefreshAction>& refresh =
          context.environment(components::Environment::refresh);
      return components::Button("Reload",
                                [refresh] {
                                  if (refresh.has_value()) {
                                    (*refresh)();
                                  }
                                })
          .frame({.width = 120.0F, .height = 40.0F})
          .id("reload");
    }
  };
  const Build build = [](Probe& seen, composition::Context&) -> Element {
    return components::ScrollView(
               components::VStack(Element{Puller{}}).frame({.width = 300.0F}))
        .frame({.width = 300.0F, .height = 400.0F})
        .refreshable([&seen] { return refresh(&seen); });
  };
  Application application = start(providers, Host{&probe, &build});
  advance(providers, application, 4);
  ASSERT_TRUE(application.root().activate(composition::ElementKey{"reload"}));
  advance(providers, application, 30);
  EXPECT_EQ(probe.refreshed, 1);
}

TEST_F(PaginationTest, OptionalChildrenBuildLikeAnIfInAViewBuilder) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen,
                         composition::Context& context) -> Element {
    const auto on = context.state(flag_key);
    seen.flag = on;
    std::optional<Element> extra;
    if (on.get()) {
      extra = components::Text("shown");
    }
    return components::VStack(components::Text("always"), std::move(extra));
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_FALSE(shows(application, "shown"));
  probe.flag->set(true);
  frames(application, 2);
  EXPECT_TRUE(shows(application, "shown"));
  EXPECT_TRUE(shows(application, "always"));
}

}
