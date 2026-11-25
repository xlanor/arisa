#include <arisa/components/button.hpp>
#include <arisa/components/for_each.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/focus_scope_behavior.hpp>
#include <arisa/composition/focus_state.hpp>
#include <arisa/composition/move_command_direction.hpp>
#include <arisa/composition/namespace.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/provider/audio/provider.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>

#include <focus_fixture.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace arisa::runtime {

using provider::Button;

class FocusTest : public FocusFixture {
 protected:
  class Grid final : public composition::Component {
   public:
    auto operator==(const Grid&) const -> bool = default;

    auto body(composition::Context&) const -> composition::Element override {
      std::vector<composition::Element> rows;
      for (int row = 0; row < 3; ++row) {
        std::vector<composition::Element> cells;
        for (int column = 0; column < 3; ++column) {
          cells.push_back(button(std::format("c{}{}", row, column)));
        }
        rows.push_back(components::HStack(std::move(cells)));
      }
      return components::VStack(std::move(rows));
    }
  };

  class Uneven final : public composition::Component {
   public:
    auto operator==(const Uneven&) const -> bool = default;

    auto body(composition::Context&) const -> composition::Element override {
      return components::VStack(
          components::HStack(button("wide", 300.0F), button("tail")),
          components::HStack(button("left"), button("mid"), button("right")));
    }
  };

  class Transformed final : public composition::Component {
   public:
    explicit Transformed(int* taps) noexcept : taps_(taps) {}

    auto operator==(const Transformed&) const -> bool = default;

    auto body(composition::Context&) const -> composition::Element override {
      int* taps = taps_;
      return components::VStack(
          button("top"),
          components::Button("moved", [taps] { ++*taps; })
              .frame({.width = 100.0F, .height = 50.0F})
              .id("moved")
              .offset(300.0F, -40.0F),
          components::Button("big", [taps] { *taps += 10; })
              .frame({.width = 60.0F, .height = 40.0F})
              .id("big")
              .scale_effect(2.0F));
    }

   private:
    int* taps_;
  };

  class Sections final : public composition::Component {
   public:
    auto operator==(const Sections&) const -> bool = default;

    auto body(composition::Context&) const -> composition::Element override {
      return components::HStack(
          components::VStack(button("a1"), button("a2"), button("a3"))
              .focus_section(),
          components::VStack(button("b1"), button("b2"), button("b3"))
              .focus_section());
    }
  };

  class Chosen final : public composition::Component {
   public:
    static constexpr composition::FocusState<int> field{"field"};

    explicit Chosen(bool declares) noexcept : declares_(declares) {}

    auto operator==(const Chosen&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      const auto focus = context.state(field);
      std::vector<composition::Element> rows;
      constexpr std::array names{"one", "two", "three"};
      for (int index = 1; index <= 3; ++index) {
        rows.push_back(button(names[static_cast<std::size_t>(index - 1)])
                           .focused(focus, index));
      }
      composition::Element choices = components::VStack(std::move(rows));
      if (declares_) {
        choices = std::move(choices).default_focus(focus, 2);
      }
      const std::optional<int> value = focus.get();
      return components::VStack(
          std::move(choices),
          components::Button("jump", [focus] { focus.set(3); })
              .frame({.width = 100.0F, .height = 50.0F})
              .id("jump"),
          components::Button("clear", [focus] { focus.set(std::nullopt); })
              .frame({.width = 100.0F, .height = 50.0F})
              .id("clear"),
          components::Text(value.has_value() ? std::format("value {}", *value)
                                             : std::string{"value none"}));
    }

   private:
    bool declares_;
  };

  class Preferred final : public composition::Component {
   public:
    static constexpr composition::Namespace scope{"scope"};

    auto operator==(const Preferred&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      const composition::Namespace::ID id = context.namespace_id(scope);
      return components::VStack(button("first"),
                                button("chosen").prefers_default_focus(id))
          .focus_scope(id);
    }
  };

  class Hero final : public composition::Component {
   public:
    static constexpr composition::StateKey<std::optional<int>> chosen{
        "chosen", std::nullopt};
    static constexpr composition::Namespace hero{"hero"};

    auto operator==(const Hero&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      const auto pick = context.state(chosen);
      const composition::Namespace::ID space = context.namespace_id(hero);
      std::vector<composition::Element> row;
      for (int index = 0; index < 3; ++index) {
        if (pick.get() == index) {
          row.push_back(components::Text("slot").frame(
              {.width = 100.0F, .height = 50.0F}));
          continue;
        }
        row.push_back(components::Button(std::format("card{}", index),
                                         [pick, index] { pick.set(index); })
                          .frame({.width = 100.0F, .height = 50.0F})
                          .id(std::format("card{}", index))
                          .matched_geometry_effect(index, space));
      }
      std::vector<composition::Element> items;
      items.push_back(components::HStack(std::move(row)));
      items.push_back(components::Text("gap").frame({.height = 80.0F}));
      if (pick.get().has_value()) {
        items.push_back(
            components::Button("detail", [pick] { pick.set(std::nullopt); })
                .frame({.width = 300.0F, .height = 120.0F})
                .id("detail")
                .matched_geometry_effect(*pick.get(), space));
      }
      return components::VStack(std::move(items));
    }
  };

  class Swapping final : public composition::Component {
   public:
    static constexpr composition::StateKey<bool> playing{"playing", false};

    auto operator==(const Swapping&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      const auto state = context.state(playing);
      const std::string name = state.get() ? "pause" : "play";
      return components::HStack(
          components::Button(name, [state] { state.set(!state.get()); })
              .frame({.width = 100.0F, .height = 50.0F})
              .id(name),
          button("other"));
    }
  };

  class Departing final : public composition::Component {
   public:
    static constexpr composition::StateKey<bool> gone{"gone", false};

    auto operator==(const Departing&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      const auto removed = context.state(gone);
      std::vector<composition::Element> items;
      items.push_back(button("a"));
      if (!removed.get()) {
        items.push_back(
            components::Button("b", [removed] { removed.set(true); })
                .frame({.width = 100.0F, .height = 50.0F})
                .id("b"));
      }
      items.push_back(components::Text("gap").frame({.height = 240.0F}));
      items.push_back(button("c"));
      return components::VStack(std::move(items));
    }
  };

  class SectionDefault final : public composition::Component {
   public:
    static constexpr composition::StateKey<bool> gone{"gone", false};
    static constexpr composition::FocusState<int> field{"field"};

    auto operator==(const SectionDefault&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      const auto removed = context.state(gone);
      const auto focus = context.state(field);
      std::vector<composition::Element> items;
      if (!removed.get()) {
        items.push_back(components::HStack(
            components::Button("b", [removed] { removed.set(true); })
                .frame({.width = 100.0F, .height = 50.0F})
                .id("b")));
      }
      items.push_back(button("x").focused(focus, 1));
      items.push_back(button("y").focused(focus, 2));
      return components::VStack(std::move(items))
          .default_focus(focus, 2)
          .focus_section();
    }
  };

  class Nested final : public composition::Component {
   public:
    static constexpr composition::StateKey<bool> gone{"gone", false};

    auto operator==(const Nested&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      const auto removed = context.state(gone);
      std::vector<composition::Element> outer;
      if (!removed.get()) {
        outer.push_back(
            components::VStack(
                components::Button("b", [removed] { removed.set(true); })
                    .frame({.width = 100.0F, .height = 50.0F})
                    .id("b"))
                .focus_section());
      }
      outer.push_back(components::Text("gap").frame({.height = 200.0F}));
      outer.push_back(button("z"));
      return components::VStack(
          button("far"), components::VStack(std::move(outer)).focus_section());
    }
  };

  class Reloading final : public composition::Component {
   public:
    static constexpr composition::StateKey<int> generation{"generation", 0};
    static constexpr composition::StateKey<bool> loaded{"loaded", true};

    auto operator==(const Reloading&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      const auto loads = context.state(generation);
      const auto present = context.state(loaded);
      std::vector<composition::Element> items;
      if (present.get()) {
        items.push_back(
            components::VStack(
                components::ForEach(
                    std::views::iota(0, 4),
                    [](int row) { return std::format("r{}", row); },
                    [](int row) { return button(std::format("row {}", row)); }))
                .id(loads.get()));
      }
      items.push_back(components::Button("bump", [loads] {
                        loads.set(loads.get() + 1);
                      }).id("bump"));
      items.push_back(components::Button("unload", [present] {
                        present.set(false);
                      }).id("unload"));
      items.push_back(components::Button("load", [present] {
                        present.set(true);
                      }).id("load"));
      return components::VStack(std::move(items));
    }
  };

  class Trap final : public composition::Component {
   public:
    static constexpr composition::StateKey<bool> open{"open", false};
    static constexpr composition::Namespace modal{"modal"};

    explicit Trap(int* taps) noexcept : taps_(taps) {}

    auto operator==(const Trap&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      const auto shown = context.state(open);
      int* taps = taps_;
      std::vector<composition::Element> items;
      items.push_back(button("base1"));
      items.push_back(components::Button("base2", [taps] { ++*taps; })
                          .frame({.width = 100.0F, .height = 50.0F})
                          .id("base2"));
      if (shown.get()) {
        items.push_back(
            components::VStack(
                button("m1"),
                components::Button("m2", [shown] { shown.set(false); })
                    .frame({.width = 100.0F, .height = 50.0F})
                    .id("m2"))
                .focus_scope(context.namespace_id(modal),
                             composition::FocusScopeBehavior::confined));
      }
      items.push_back(components::Button("opener", [shown] { shown.set(true); })
                          .frame({.width = 100.0F, .height = 50.0F})
                          .id("opener"));
      return components::VStack(std::move(items));
    }

   private:
    int* taps_;
  };

  class Lazy final : public composition::Element {
   public:
    class Content final : public composition::ElementContent {
     public:
      explicit Content(std::function<void()> grow) noexcept
          : grow_(std::move(grow)) {}

      auto type() const noexcept -> composition::ElementType override {
        return composition::ElementType::of<Content>();
      }

      auto materialize(composition::Context&, const composition::Node&,
                       composition::MoveCommandDirection direction)
          -> bool override {
        if (direction != composition::MoveCommandDirection::down || !grow_) {
          return false;
        }
        grow_();
        return true;
      }

     private:
      std::function<void()> grow_;
    };

    Lazy(std::function<void()> grow, std::vector<composition::Element> items)
        : Element(std::make_unique<Content>(std::move(grow)),
                  std::move(items)) {}
  };

  class Growing final : public composition::Component {
   public:
    static constexpr composition::StateKey<int> count{"count", 1};

    auto operator==(const Growing&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      const auto items = context.state(count);
      std::vector<composition::Element> built;
      for (int index = 0; index < items.get(); ++index) {
        built.push_back(button(std::format("item{}", index)));
      }
      return components::VStack(
          Lazy([items] { items.set(items.get() + 1); }, std::move(built)));
    }
  };

  static auto played(provider::headless::HeadlessProviderFactory& providers,
                     provider::SoundRole role) -> std::size_t {
    const std::vector<provider::SoundRole> sounds = providers.audio().played();
    return static_cast<std::size_t>(std::ranges::count(sounds, role));
  }
};

TEST_F(FocusTest, GridMovesSpatially) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, Grid{});
  EXPECT_EQ(focused_key(application), "c00");
  press(providers, application, Button::dpad_right);
  EXPECT_EQ(focused_key(application), "c01");
  press(providers, application, Button::dpad_down);
  EXPECT_EQ(focused_key(application), "c11");
  press(providers, application, Button::dpad_left);
  EXPECT_EQ(focused_key(application), "c10");
  press(providers, application, Button::dpad_up);
  EXPECT_EQ(focused_key(application), "c00");
  EXPECT_EQ(played(providers, provider::SoundRole::focus_change), 4U);
}

TEST_F(FocusTest, BlockedMoveShakesAndPlaysTheErrorSound) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, Grid{});
  providers.input().push(pad({Button::dpad_up}));
  frames(application, 1);
  EXPECT_EQ(focused_key(application), "c00");
  ASSERT_TRUE(application.interaction().shake().has_value());
  EXPECT_EQ(node_of(application, "c00"),
            application.interaction().shake()->node);
  EXPECT_EQ(application.interaction().shake()->direction,
            composition::MoveCommandDirection::up);
  EXPECT_EQ(played(providers, provider::SoundRole::focus_error), 1U);
  providers.input().push(pad({}));
  frames(application, 1);
  ASSERT_TRUE(providers.clock().advance(std::chrono::milliseconds{400}));
  frames(application, 1);
  EXPECT_FALSE(application.interaction().shake().has_value());
}

TEST_F(FocusTest, FocusEdgeActionsRunWhenAMoveLeavesTheContent) {
  provider::headless::HeadlessProviderFactory providers;
  auto edges = std::make_shared<std::vector<std::string>>();
  Application application = start(
      providers, components::HStack(button("left"), button("right"))
                     .on_focus_edge(composition::MoveCommandDirection::left,
                                    [edges] { edges->emplace_back("left"); })
                     .on_focus_edge(composition::MoveCommandDirection::right,
                                    [edges] { edges->emplace_back("right"); }));
  EXPECT_EQ(focused_key(application), "left");
  press(providers, application, provider::Button::dpad_right);
  EXPECT_EQ(focused_key(application), "right");
  EXPECT_TRUE(edges->empty());
  press(providers, application, provider::Button::dpad_right);
  press(providers, application, provider::Button::dpad_left);
  press(providers, application, provider::Button::dpad_left);
  EXPECT_EQ(*edges, (std::vector<std::string>{"right", "left"}));
}

TEST_F(FocusTest, AFocusFallbackTakesFocusOnlyWhenNothingInsideCan) {
  provider::headless::HeadlessProviderFactory providers;
  Application empty =
      start(providers,
            components::VStack(components::VStack()
                                   .frame({.width = 100.0F, .height = 50.0F})
                                   .focus_fallback()
                                   .id("pane")));
  EXPECT_EQ(focused_key(empty), "pane");
  provider::headless::HeadlessProviderFactory filled_providers;
  Application filled =
      start(filled_providers,
            components::VStack(components::VStack(button("inner"))
                                   .frame({.width = 100.0F, .height = 50.0F})
                                   .focus_fallback()
                                   .id("pane")));
  EXPECT_EQ(focused_key(filled), "inner");
}

TEST_F(FocusTest, UnevenRowsFollowAlignment) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, Uneven{});
  EXPECT_EQ(focused_key(application), "wide");
  press(providers, application, Button::dpad_down);
  EXPECT_EQ(focused_key(application), "left");
  press(providers, application, Button::dpad_right);
  press(providers, application, Button::dpad_right);
  EXPECT_EQ(focused_key(application), "right");
  press(providers, application, Button::dpad_up);
  EXPECT_EQ(focused_key(application), "tail");
}

TEST_F(FocusTest, TransformedFramesSteerSearchAndHits) {
  provider::headless::HeadlessProviderFactory providers;
  int taps = 0;
  Application application = start(providers, Transformed{&taps});
  EXPECT_EQ(focused_key(application), "top");
  press(providers, application, Button::dpad_right);
  EXPECT_EQ(focused_key(application), "moved");
  const auto moved = node_named(application, "moved");
  ASSERT_TRUE(moved.has_value());
  const composition::Rect laid = moved->get().frame();
  tap(providers, application,
      provider::PixelPoint{.x = laid.x + 5.0F, .y = laid.y + 5.0F});
  EXPECT_EQ(taps, 0);
  tap(providers, application, centre(application, "moved"));
  EXPECT_EQ(taps, 1);
  const auto big = node_named(application, "big");
  ASSERT_TRUE(big.has_value());
  const composition::Rect small = big->get().frame();
  tap(providers, application,
      provider::PixelPoint{.x = small.x + small.width + 10.0F,
                           .y = small.y + (small.height * 0.5F)});
  EXPECT_EQ(taps, 11);
  EXPECT_EQ(focused_key(application), "big");
}

TEST_F(FocusTest, SectionsRememberTheirLastFocus) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, Sections{});
  EXPECT_EQ(focused_key(application), "a1");
  press(providers, application, Button::dpad_down);
  EXPECT_EQ(focused_key(application), "a2");
  press(providers, application, Button::dpad_right);
  EXPECT_EQ(focused_key(application), "b2");
  press(providers, application, Button::dpad_down);
  EXPECT_EQ(focused_key(application), "b3");
  press(providers, application, Button::dpad_left);
  EXPECT_EQ(focused_key(application), "a2");
  press(providers, application, Button::dpad_right);
  EXPECT_EQ(focused_key(application), "b3");
}

TEST_F(FocusTest, DefaultFocusPicksTheDeclaredValue) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, Chosen{true});
  EXPECT_EQ(focused_key(application), "two");
  frames(application, 2);
  EXPECT_TRUE(shows(application, "value 2"));
}

TEST_F(FocusTest, PrefersDefaultFocusInsideItsScope) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, Preferred{});
  EXPECT_EQ(focused_key(application), "chosen");
}

TEST_F(FocusTest, FocusStateFollowsTheEngineAndDrivesIt) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, Chosen{false});
  frames(application, 2);
  EXPECT_EQ(focused_key(application), "one");
  EXPECT_TRUE(shows(application, "value 1"));
  press(providers, application, Button::dpad_down);
  EXPECT_EQ(focused_key(application), "two");
  EXPECT_TRUE(shows(application, "value 2"));
  ASSERT_TRUE(application.root().activate(composition::ElementKey{"jump"}));
  frames(application, 3);
  EXPECT_EQ(focused_key(application), "three");
  EXPECT_TRUE(shows(application, "value 3"));
  ASSERT_TRUE(application.root().activate(composition::ElementKey{"clear"}));
  frames(application, 3);
  EXPECT_EQ(focused_key(application), "none");
  EXPECT_TRUE(shows(application, "value none"));
  press(providers, application, Button::dpad_down);
  EXPECT_EQ(focused_key(application), "one");
  EXPECT_TRUE(shows(application, "value 1"));
}

TEST_F(FocusTest, DepartedFocusFallsBackToTheNearestSibling) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, Departing{});
  press(providers, application, Button::dpad_down);
  EXPECT_EQ(focused_key(application), "b");
  press(providers, application, Button::a);
  EXPECT_FALSE(node_of(application, "b").has_value());
  EXPECT_EQ(focused_key(application), "a");
}

TEST_F(FocusTest, DepartedFocusUsesTheSectionDefaultNext) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, SectionDefault{});
  EXPECT_EQ(focused_key(application), "y");
  press(providers, application, Button::dpad_up);
  press(providers, application, Button::dpad_up);
  EXPECT_EQ(focused_key(application), "b");
  press(providers, application, Button::a);
  EXPECT_FALSE(node_of(application, "b").has_value());
  EXPECT_EQ(focused_key(application), "y");
}

TEST_F(FocusTest, DepartedFocusClimbsToTheParentSection) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, Nested{});
  EXPECT_EQ(focused_key(application), "far");
  press(providers, application, Button::dpad_down);
  EXPECT_EQ(focused_key(application), "b");
  press(providers, application, Button::a);
  EXPECT_FALSE(node_of(application, "b").has_value());
  EXPECT_EQ(focused_key(application), "z");
}

TEST_F(FocusTest, ReloadRestoresFocusByKey) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, Reloading{});
  EXPECT_EQ(focused_key(application), "r0");
  press(providers, application, Button::dpad_down);
  press(providers, application, Button::dpad_down);
  EXPECT_EQ(focused_key(application), "r2");
  const composition::NodeId before = *application.interaction().focused();
  ASSERT_TRUE(application.root().activate(composition::ElementKey{"bump"}));
  frames(application, 3);
  EXPECT_EQ(focused_key(application), "r2");
  EXPECT_NE(*application.interaction().focused(), before);
  ASSERT_TRUE(application.root().activate(composition::ElementKey{"unload"}));
  frames(application, 3);
  EXPECT_EQ(focused_key(application), "bump");
  ASSERT_TRUE(application.root().activate(composition::ElementKey{"load"}));
  frames(application, 3);
  EXPECT_EQ(focused_key(application), "r2");
}

TEST_F(FocusTest, ConfinedScopeTrapsMovementAndRestoresFocus) {
  provider::headless::HeadlessProviderFactory providers;
  int taps = 0;
  Application application = start(providers, Trap{&taps});
  EXPECT_EQ(focused_key(application), "base1");
  ASSERT_TRUE(application.root().activate(composition::ElementKey{"opener"}));
  frames(application, 3);
  EXPECT_EQ(focused_key(application), "m1");
  press(providers, application, Button::dpad_down);
  EXPECT_EQ(focused_key(application), "m2");
  press(providers, application, Button::dpad_down);
  EXPECT_EQ(focused_key(application), "m2");
  press(providers, application, Button::dpad_up);
  press(providers, application, Button::dpad_up);
  EXPECT_EQ(focused_key(application), "m1");
  EXPECT_EQ(played(providers, provider::SoundRole::focus_error), 2U);
  tap(providers, application, centre(application, "base2"));
  EXPECT_EQ(taps, 0);
  EXPECT_EQ(focused_key(application), "m1");
  ASSERT_TRUE(application.root().activate(composition::ElementKey{"m2"}));
  frames(application, 3);
  EXPECT_EQ(focused_key(application), "base1");
}

TEST_F(FocusTest, LazyContainersMaterialiseTheNextItem) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, Growing{});
  EXPECT_EQ(focused_key(application), "item0");
  providers.input().push(pad({Button::dpad_down}));
  providers.input().push(pad({}));
  frames(application, 4);
  EXPECT_EQ(focused_key(application), "item1");
  EXPECT_EQ(played(providers, provider::SoundRole::focus_error), 0U);
  EXPECT_EQ(played(providers, provider::SoundRole::focus_change), 1U);
}

TEST_F(FocusTest, DepartedFocusFollowsTheMatchedGeometrySuccessor) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, Hero{});
  EXPECT_EQ(focused_key(application), "card0");
  press(providers, application, Button::dpad_right);
  EXPECT_EQ(focused_key(application), "card1");
  press(providers, application, Button::a);
  EXPECT_EQ(focused_key(application), "detail");
  press(providers, application, Button::a);
  EXPECT_EQ(focused_key(application), "card1");
  press(providers, application, Button::a);
  EXPECT_EQ(focused_key(application), "detail");
}

TEST_F(FocusTest, DepartedFocusMovesToItsInPlaceReplacement) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, Swapping{});
  EXPECT_EQ(focused_key(application), "play");
  press(providers, application, Button::a);
  EXPECT_EQ(focused_key(application), "pause");
  press(providers, application, Button::a);
  EXPECT_EQ(focused_key(application), "play");
}

}
