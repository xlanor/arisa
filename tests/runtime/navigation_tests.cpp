#include <arisa/components/button.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/navigation_destination.hpp>
#include <arisa/composition/navigation_path.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/presentation/navigation_link.hpp>
#include <arisa/presentation/navigation_stack.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/structure/navigation_split_view.hpp>

#include <scroll_fixture.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <format>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace arisa::runtime {

using namespace std::chrono_literals;
using composition::Element;
using provider::Button;

class NavigationTest : public ScrollFixture {
 protected:
  struct Probe {
    std::optional<composition::StateHandle<std::vector<int>>> path;
    std::optional<composition::StateHandle<composition::NavigationPath>> erased;
    std::vector<std::string> events;
  };

  static auto link(int value, std::string key) -> Element {
    return Element{presentation::NavigationLink(value, key)}
        .frame({.width = 120.0F, .height = 40.0F})
        .id(std::move(key));
  }

  class Detail final : public composition::Component {
   public:
    static constexpr composition::StateKey<int> count{"detail-count", 0};

    explicit Detail(int value) noexcept : value_(value) {}

    auto operator==(const Detail&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto taps = context.state(count);
      const composition::DismissAction dismiss =
          context.environment(components::Environment::dismiss);
      return components::VStack(
          components::Text(
              std::format("detail {} count {}", value_, taps.get())),
          components::Button("inc", [taps] { taps.set(taps.get() + 1); })
              .frame({.width = 120.0F, .height = 40.0F})
              .id(std::format("inc-{}", value_)),
          components::Button("dismiss", [dismiss] { dismiss(); })
              .frame({.width = 120.0F, .height = 40.0F})
              .id(std::format("dismiss-{}", value_)),
          link(value_ + 10, std::format("deeper-{}", value_)));
    }

   private:
    int value_;
  };

  class Stack final : public composition::Component {
   public:
    static const composition::StateKey<std::vector<int>> chapters;

    explicit Stack(Probe* probe) noexcept : probe_(probe) {}

    auto operator==(const Stack&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto path = context.state(chapters);
      probe_->path = path;
      Probe* probe = probe_;
      return presentation::NavigationStack(path, [probe] {
        return components::VStack(link(1, "a"), link(2, "b"), link(3, "c"))
            .on_appear([probe] { probe->events.emplace_back("appear root"); })
            .on_disappear(
                [probe] { probe->events.emplace_back("disappear root"); })
            .navigation_title("Root")
            .navigation_destination(
                composition::for_type<int>, [probe](const int& value) {
                  return Element{Detail{value}}
                      .on_appear([probe, value] {
                        probe->events.push_back(
                            std::format("appear {}", value));
                      })
                      .on_disappear([probe, value] {
                        probe->events.push_back(
                            std::format("disappear {}", value));
                      });
                });
      });
    }

   private:
    Probe* probe_;
  };

  class Erased final : public composition::Component {
   public:
    static const composition::StateKey<composition::NavigationPath> chapters;

    explicit Erased(Probe* probe) noexcept : probe_(probe) {}

    auto operator==(const Erased&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto path = context.state(chapters);
      probe_->erased = path;
      return presentation::NavigationStack(path, [] {
        return components::VStack(link(1, "a"))
            .navigation_destination(
                composition::for_type<int>,
                [](const int& value) {
                  return components::Text(std::format("number {}", value));
                })
            .navigation_destination(
                composition::for_type<std::string>,
                [](const std::string& value) {
                  return components::Text(std::format("word {}", value));
                });
      });
    }

   private:
    Probe* probe_;
  };

  static void settle(provider::headless::HeadlessProviderFactory& providers,
                     Application& application) {
    for (int index = 0; index < 40; ++index) {
      step(providers, application, 32ms);
    }
  }

  static auto count_text(Application& application, std::string_view text)
      -> std::size_t {
    return static_cast<std::size_t>(
        std::ranges::count(texts(application), text));
  }

  static auto hinted(Application& application, Button button,
                     std::string_view label) -> bool {
    return std::ranges::any_of(
        application.interaction().hints(),
        [button, label](const composition::CommandHint& hint) {
          return hint.button == button && hint.label == label;
        });
  }
};

const composition::StateKey<std::vector<int>> NavigationTest::Stack::chapters{
    "chapters", {}};

const composition::StateKey<composition::NavigationPath>
    NavigationTest::Erased::chapters{"erased-chapters",
                                     composition::NavigationPath{}};

TEST_F(NavigationTest, LinkPushesAndBackPopsWithFocusRestored) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  Application application = start(providers, Stack{&probe});
  EXPECT_EQ(focused_key(application), "a");
  press(providers, application, Button::dpad_down);
  EXPECT_EQ(focused_key(application), "b");
  press(providers, application, Button::a);
  ASSERT_EQ(probe.path->get(), std::vector<int>{2});
  frames(application, 2);
  EXPECT_TRUE(shows(application, "detail 2 count 0"));
  EXPECT_EQ(focused_key(application), "inc-2");
  EXPECT_TRUE(hinted(application, Button::b, "Back"));
  press(providers, application, Button::b);
  EXPECT_TRUE(probe.path->get().empty());
  frames(application, 2);
  EXPECT_EQ(focused_key(application), "b");
  EXPECT_FALSE(hinted(application, Button::b, "Back"));
  settle(providers, application);
  EXPECT_FALSE(shows(application, "detail 2 count 0"));
  EXPECT_EQ(application.root().tree().departing(), 0U);
}

TEST_F(NavigationTest, ADestinationLinkBuildsItsViewWhenPushed) {
  provider::headless::HeadlessProviderFactory providers;
  auto built = std::make_shared<int>(0);
  Application application = start(
      providers, presentation::NavigationStack([built] {
        return components::VStack(
            Element{presentation::NavigationLink(
                        "Open",
                        [built] {
                          ++*built;
                          return Element{components::Text("pushed detail")};
                        })}
                .frame({.width = 120.0F, .height = 40.0F})
                .id("open"),
            Element{
                presentation::NavigationLink(
                    [] { return Element{components::Text("labelled detail")}; },
                    components::Text("Labelled"))}
                .frame({.width = 120.0F, .height = 40.0F})
                .id("labelled"));
      }));
  EXPECT_EQ(*built, 0);
  EXPECT_EQ(focused_key(application), "open");
  press(providers, application, Button::a);
  frames(application, 2);
  EXPECT_EQ(*built, 1);
  EXPECT_TRUE(shows(application, "pushed detail"));
  press(providers, application, Button::b);
  settle(providers, application);
  EXPECT_FALSE(shows(application, "pushed detail"));
  ASSERT_TRUE(application.root().activate(composition::ElementKey{"labelled"}));
  frames(application, 2);
  EXPECT_TRUE(shows(application, "labelled detail"));
}

TEST_F(NavigationTest, ADestinationLinkInASplitViewSidebarReplacesTheDetail) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers,
      structure::NavigationSplitView(
          components::VStack(
              Element{
                  presentation::NavigationLink(
                      "First",
                      [] { return Element{components::Text("first detail")}; })}
                  .frame({.width = 120.0F, .height = 40.0F})
                  .id("first"),
              Element{presentation::NavigationLink(
                          "Second",
                          [] {
                            return Element{components::Text("second detail")};
                          })}
                  .frame({.width = 120.0F, .height = 40.0F})
                  .id("second")),
          components::Text("placeholder detail")));
  settle(providers, application);
  EXPECT_TRUE(shows(application, "placeholder detail"));
  ASSERT_TRUE(application.root().activate(composition::ElementKey{"first"}));
  settle(providers, application);
  EXPECT_TRUE(shows(application, "first detail"));
  EXPECT_FALSE(shows(application, "placeholder detail"));
  ASSERT_TRUE(application.root().activate(composition::ElementKey{"second"}));
  settle(providers, application);
  EXPECT_TRUE(shows(application, "second detail"));
  EXPECT_FALSE(shows(application, "first detail"));
}

TEST_F(NavigationTest, ProgrammaticPushPopAndReplace) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  Application application = start(providers, Stack{&probe});
  probe.path->set({1, 11});
  frames(application, 3);
  EXPECT_TRUE(shows(application, "detail 11 count 0"));
  EXPECT_TRUE(shows(application, "detail 1 count 0"));
  EXPECT_EQ(focused_key(application), "inc-11");
  probe.path->set({1, 12});
  frames(application, 3);
  settle(providers, application);
  EXPECT_FALSE(shows(application, "detail 11 count 0"));
  EXPECT_TRUE(shows(application, "detail 12 count 0"));
  EXPECT_EQ(focused_key(application), "inc-12");
  probe.path->update([](std::vector<int>& values) { values.pop_back(); });
  frames(application, 3);
  EXPECT_EQ(focused_key(application), "inc-1");
  settle(providers, application);
  EXPECT_FALSE(shows(application, "detail 12 count 0"));
}

TEST_F(NavigationTest, StateSurvivesWhileCovered) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  Application application = start(providers, Stack{&probe});
  press(providers, application, Button::a);
  frames(application, 2);
  EXPECT_EQ(focused_key(application), "inc-1");
  press(providers, application, Button::a);
  press(providers, application, Button::a);
  EXPECT_TRUE(shows(application, "detail 1 count 2"));
  probe.path->update([](std::vector<int>& values) { values.push_back(11); });
  frames(application, 3);
  settle(providers, application);
  EXPECT_TRUE(shows(application, "detail 1 count 2"));
  EXPECT_EQ(focused_key(application), "inc-11");
  press(providers, application, Button::b);
  frames(application, 2);
  EXPECT_EQ(focused_key(application), "inc-1");
  EXPECT_TRUE(shows(application, "detail 1 count 2"));
}

TEST_F(NavigationTest, CoveredScreensAreUnreachableByFocus) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  Application application = start(providers, Stack{&probe});
  press(providers, application, Button::a);
  frames(application, 2);
  settle(providers, application);
  for (const Button direction :
       {Button::dpad_up, Button::dpad_left, Button::dpad_down,
        Button::dpad_down, Button::dpad_down, Button::dpad_right}) {
    press(providers, application, direction);
    const std::string key = focused_key(application);
    EXPECT_NE(key, "a");
    EXPECT_NE(key, "b");
    EXPECT_NE(key, "c");
  }
}

TEST_F(NavigationTest, RapidMutationMidTransitionStaysConsistent) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  Application application = start(providers, Stack{&probe});
  for (int round = 0; round < 6; ++round) {
    probe.path->set({1});
    frames(application, 1);
    step(providers, application, 16ms);
    probe.path->set({});
    frames(application, 1);
    probe.path->set({1, 11});
    frames(application, 1);
    probe.path->set({2});
    frames(application, 1);
  }
  probe.path->set({1});
  frames(application, 2);
  EXPECT_EQ(count_text(application, "detail 1 count 0"), 1U);
  EXPECT_EQ(focused_key(application), "inc-1");
  settle(providers, application);
  EXPECT_EQ(application.root().tree().departing(), 0U);
  EXPECT_EQ(count_text(application, "detail 1 count 0"), 1U);
  EXPECT_FALSE(shows(application, "detail 2 count 0"));
  EXPECT_FALSE(shows(application, "detail 11 count 0"));
  press(providers, application, Button::b);
  frames(application, 2);
  EXPECT_EQ(focused_key(application), "a");
  settle(providers, application);
  EXPECT_EQ(application.root().tree().departing(), 0U);
}

TEST_F(NavigationTest, CoveredAndRevealedScreensGetLifecycleHooks) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  Application application = start(providers, Stack{&probe});
  EXPECT_EQ(probe.events, std::vector<std::string>{"appear root"});
  probe.events.clear();
  probe.path->set({1});
  frames(application, 2);
  EXPECT_EQ(probe.events,
            (std::vector<std::string>{"disappear root", "appear 1"}));
  probe.events.clear();
  probe.path->set({1, 11});
  frames(application, 2);
  EXPECT_EQ(probe.events,
            (std::vector<std::string>{"disappear 1", "appear 11"}));
  probe.events.clear();
  press(providers, application, Button::b);
  frames(application, 2);
  EXPECT_EQ(probe.events,
            (std::vector<std::string>{"disappear 11", "appear 1"}));
  probe.events.clear();
  probe.path->set({});
  frames(application, 2);
  EXPECT_EQ(probe.events,
            (std::vector<std::string>{"disappear 1", "appear root"}));
}

TEST_F(NavigationTest, PoppingTwoLevelsFiresEachHookOnce) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  Application application = start(providers, Stack{&probe});
  probe.path->set({1, 11});
  frames(application, 2);
  probe.events.clear();
  probe.path->set({});
  frames(application, 2);
  EXPECT_EQ(probe.events,
            (std::vector<std::string>{"disappear 11", "appear root"}));
}

TEST_F(NavigationTest, DismissEnvironmentPopsTheScreen) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  Application application = start(providers, Stack{&probe});
  probe.path->set({1, 11});
  frames(application, 3);
  ASSERT_TRUE(
      application.root().activate(composition::ElementKey{"dismiss-11"}));
  frames(application, 2);
  EXPECT_EQ(probe.path->get(), std::vector<int>{1});
}

TEST_F(NavigationTest, BackLabelAndEdgeSwipePop) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  Application application = start(providers, Stack{&probe});
  probe.path->set({1, 11});
  frames(application, 3);
  settle(providers, application);
  drag(providers, application, provider::PixelPoint{.x = 6.0F, .y = 360.0F},
       provider::PixelPoint{.x = 420.0F, .y = 360.0F}, 8);
  frames(application, 2);
  EXPECT_EQ(probe.path->get(), std::vector<int>{1});
  settle(providers, application);
  drag(providers, application, provider::PixelPoint{.x = 6.0F, .y = 360.0F},
       provider::PixelPoint{.x = 40.0F, .y = 360.0F}, 4);
  frames(application, 2);
  EXPECT_EQ(probe.path->get(), std::vector<int>{1});
  settle(providers, application);
  std::optional<provider::PixelPoint> back;
  const auto search = [&](const auto& self,
                          const composition::Node& node) -> void {
    if (node.phase() != composition::NodeLifecyclePhase::live) {
      return;
    }
    if (const auto text = node.content_as<components::Text::Content>();
        text.has_value() && text->get().text() == "‹ Back" &&
        node.presented().has_value() && node.presented()->visible) {
      const composition::Rect bounds = node.presented()->bounds;
      back = provider::PixelPoint{.x = bounds.x + (bounds.width * 0.5F),
                                  .y = bounds.y + (bounds.height * 0.5F)};
    }
    for (const composition::Node& child : node.children()) {
      self(self, child);
    }
  };
  search(search, application.root().tree().root());
  ASSERT_TRUE(back.has_value());
  tap(providers, application, *back);
  frames(application, 2);
  EXPECT_TRUE(probe.path->get().empty());
}

TEST_F(NavigationTest, TypeErasedPathRoutesEachValueType) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  Application application = start(providers, Erased{&probe});
  press(providers, application, Button::a);
  frames(application, 2);
  EXPECT_TRUE(shows(application, "number 1"));
  probe.erased->update([](composition::NavigationPath& path) {
    path.append(std::string{"hello"});
  });
  frames(application, 2);
  EXPECT_TRUE(shows(application, "word hello"));
  EXPECT_EQ(probe.erased->get().count(), 2U);
  press(providers, application, Button::b);
  press(providers, application, Button::b);
  EXPECT_TRUE(probe.erased->get().empty());
}

}
