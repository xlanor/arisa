#include <arisa/components/environment.hpp>
#include <arisa/components/for_each.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/index_set.hpp>
#include <arisa/components/label.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/status/battery.hpp>
#include <arisa/components/status/clock.hpp>
#include <arisa/components/status/wireless.hpp>
#include <arisa/components/system_symbols.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/navigation_destination.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/node_lifecycle_phase.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/toolbar_item.hpp>
#include <arisa/controls/toggle.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/presentation.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/presentation/navigation_link.hpp>
#include <arisa/presentation/navigation_stack.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/structure/cell_style.hpp>
#include <arisa/structure/form.hpp>
#include <arisa/structure/list.hpp>
#include <arisa/structure/list_cell.hpp>
#include <arisa/structure/list_style.hpp>
#include <arisa/structure/scaffold.hpp>
#include <arisa/structure/section.hpp>
#include <arisa/structure/tab.hpp>
#include <arisa/structure/tab_view.hpp>
#include <arisa/structure/tab_view_style.hpp>
#include <arisa/theme/typography.hpp>

#include <focus_fixture.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <format>
#include <functional>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::runtime {

using composition::Element;
using provider::Button;

class StructureTest : public FocusFixture {
 protected:
  struct Probe {
    std::optional<composition::StateHandle<std::vector<std::string>>> items;
    std::optional<composition::StateHandle<int>> tab;
    std::optional<composition::StateHandle<bool>> flag;
  };

  struct Seen {
    bool first{false};
    bool last{false};
    bool lifted{false};
    bool disclosure{false};
    structure::ListStyle::Kind style{structure::ListStyle::Kind::plain};
  };

  struct RecordingCell {
    std::vector<Seen>* log{nullptr};

    auto make_body(structure::CellStyle::Configuration configuration) const
        -> Element {
      log->push_back(Seen{.first = configuration.is_first,
                          .last = configuration.is_last,
                          .lifted = configuration.is_lifted,
                          .disclosure = configuration.shows_disclosure,
                          .style = configuration.list_style.kind()});
      return std::move(configuration.content);
    }

    friend auto operator==(const RecordingCell&, const RecordingCell&)
        -> bool = default;
  };

  struct CountingCell {
    int* count{nullptr};

    auto make_body(structure::CellStyle::Configuration configuration) const
        -> Element {
      ++*count;
      return structure::CellStyle::automatic().make_body(
          std::move(configuration));
    }

    friend auto operator==(const CountingCell&, const CountingCell&)
        -> bool = default;
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

  static const composition::StateKey<std::vector<std::string>> items_key;
  static constexpr composition::StateKey<int> tab_key{"tab", 1};
  static constexpr composition::StateKey<bool> flag_key{"flag", false};

  static auto letters() -> std::vector<std::string> {
    return {"a", "b", "c", "d"};
  }

  static auto editable(composition::StateHandle<std::vector<std::string>> items)
      -> components::ForEach {
    return components::ForEach(
               items.get(), [](const std::string& item) { return item; },
               [](const std::string& item) {
                 return components::Text(std::format("row {}", item));
               })
        .on_move([items](const components::IndexSet& from, std::size_t to) {
          items.update([&](std::vector<std::string>& values) {
            from.move_from(values, to);
          });
        })
        .on_delete([items](const components::IndexSet& offsets) {
          items.update([&](std::vector<std::string>& values) {
            offsets.remove_from(values);
          });
        });
  }

  static void settle(provider::headless::HeadlessProviderFactory& providers,
                     Application& application, int steps = 40) {
    for (int step = 0; step < steps; ++step) {
      ASSERT_TRUE(providers.clock().advance(frame_step).has_value());
      frames(application, 1);
    }
  }

  static auto frame_of(Application& application, std::string_view key)
      -> composition::Rect {
    const auto node = node_named(application, key);
    if (!node.has_value()) {
      return composition::Rect{};
    }
    return node->get().presented().has_value() ? node->get().presented()->bounds
                                               : node->get().frame();
  }

  static auto count_texts(const composition::Node& node,
                          std::string_view prefix) -> int {
    int found = 0;
    if (const auto text = node.content_as<components::Text::Content>()) {
      found += text->get().text().starts_with(prefix) ? 1 : 0;
    }
    for (const composition::Node& child : node.children()) {
      found += count_texts(child, prefix);
    }
    return found;
  }

  static auto capsule_on(Application& application, std::size_t index) -> bool {
    const auto& tree = application.root().tree();
    const auto capsule = tree.node(
        *tree.find(composition::ElementKey{structure::TabView::capsule_id}));
    const auto bar = tree.node(
        *tree.find(composition::ElementKey{structure::TabView::bar_id}));
    const auto item =
        tree.node(*tree.find(structure::TabView::item_key(index)));
    const composition::GeometryEffects& moved =
        capsule->get().modifiers().effects;
    return std::abs(bar->get().frame().y + moved.offset_y -
                    item->get().frame().y) < 1.0F &&
           std::abs(capsule->get().frame().height -
                    item->get().frame().height) < 1.0F;
  }

  static void drag_path(provider::headless::HeadlessProviderFactory& providers,
                        Application& application, provider::PixelPoint from,
                        provider::PixelPoint to) {
    const auto touch = [](provider::PixelPoint at, provider::TouchPhase phase) {
      provider::InputFrame frame;
      frame.touches.push_back(provider::Touch{
          .id = provider::TouchId{9}, .position = at, .phase = phase});
      return frame;
    };
    providers.input().push(touch(from, provider::TouchPhase::began));
    frames(application, 1);
    for (int step = 1; step <= 6; ++step) {
      const float share = static_cast<float>(step) / 6.0F;
      providers.input().push(
          touch(provider::PixelPoint{.x = from.x + ((to.x - from.x) * share),
                                     .y = from.y + ((to.y - from.y) * share)},
                provider::TouchPhase::moved));
      frames(application, 1);
    }
    providers.input().push(touch(to, provider::TouchPhase::ended));
    frames(application, 3);
  }
};

const composition::StateKey<std::vector<std::string>> StructureTest::items_key{
    "items", StructureTest::letters()};

TEST_F(StructureTest, ListShowsHeadersFootersAndRows) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe&, composition::Context&) -> Element {
    return structure::List(
        components::Text("Loose row"),
        structure::Section(
            structure::Section::Labels{.header = components::Text("Head"),
                                       .footer = components::Text("Foot")},
            components::Text("Inside"), components::Text("Second")));
  };
  Application application = start(providers, Host{&probe, &build});
  for (const std::string_view text :
       {"Loose row", "Head", "Foot", "Inside", "Second"}) {
    EXPECT_TRUE(shows(application, text)) << text;
  }
}

TEST_F(StructureTest, CellsLearnTheirPositionStyleAndDisclosure) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  std::vector<Seen> seen;
  const Build build = [&seen](Probe&, composition::Context&) -> Element {
    return presentation::NavigationStack([&seen] {
      return structure::Form(structure::Section(components::Text("one"),
                                                components::Text("two"),
                                                components::Text("three")),
                             structure::Section(presentation::NavigationLink(
                                 1, Element{components::Text("link")})))
          .list_style(structure::ListStyle::plain())
          .cell_style(RecordingCell{&seen});
    });
  };
  Application application = start(providers, Host{&probe, &build});
  ASSERT_GE(seen.size(), 4U);
  const std::vector<Seen> last(seen.end() - 4, seen.end());
  EXPECT_TRUE(last[0].first);
  EXPECT_FALSE(last[0].last);
  EXPECT_FALSE(last[1].first);
  EXPECT_FALSE(last[1].last);
  EXPECT_TRUE(last[2].last);
  EXPECT_TRUE(last[3].first && last[3].last);
  EXPECT_TRUE(last[3].disclosure);
  EXPECT_FALSE(last[0].disclosure);
  for (const Seen& each : last) {
    EXPECT_EQ(each.style, structure::ListStyle::Kind::inset_grouped);
  }
}

TEST_F(StructureTest, PlainListsPassTheirStyleToCells) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  std::vector<Seen> seen;
  const Build build = [&seen](Probe&, composition::Context&) -> Element {
    return structure::List(components::Text("row"))
        .list_style(structure::SidebarListStyle{})
        .cell_style(RecordingCell{&seen});
  };
  Application application = start(providers, Host{&probe, &build});
  ASSERT_FALSE(seen.empty());
  EXPECT_EQ(seen.back().style, structure::ListStyle::Kind::sidebar);
}

TEST_F(StructureTest, DetailRowsPushThroughTheNavigationStack) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe&, composition::Context&) -> Element {
    return presentation::NavigationStack([] {
      return Element{structure::Form(presentation::NavigationLink(
                                         7, Element{components::Text("Open")})
                                         .id("link"))}
          .navigation_destination(std::type_identity<int>{}, [](int value) {
            return components::Text(std::format("Pushed {}", value));
          });
    });
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_EQ(focused_key(application), "link");
  press(providers, application, Button::a);
  settle(providers, application);
  EXPECT_TRUE(shows(application, "Pushed 7"));
}

TEST_F(StructureTest, LongListsOnlyBuildRowsNearTheScreen) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe&, composition::Context&) -> Element {
    return structure::List(components::ForEach(
        std::views::iota(0, 1000), [](int row) { return row; },
        [](int row) { return components::Text(std::format("item {}", row)); }));
  };
  Application application = start(providers, Host{&probe, &build});
  const int built = count_texts(application.root().tree().root(), "item ");
  EXPECT_GT(built, 3);
  EXPECT_LT(built, 80);
}

TEST_F(StructureTest, SelectionStepReevaluatesOnlyTheRowsItChanges) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  int cells = 0;
  int builds = 0;
  std::vector<int> fired;
  const Build build = [&cells, &builds, &fired](
                          Probe& found,
                          composition::Context& context) -> Element {
    const auto selection = context.state(tab_key);
    found.tab = selection;
    const int built = ++builds;
    std::vector<Element> rows;
    rows.push_back(components::Button("Action", [&fired, built] {
                     fired.push_back(built);
                   }).id("action"));
    for (int row = 1; row < 50; ++row) {
      rows.push_back(components::Label(std::format("Row {}", row),
                                       components::Image::system_name("star"))
                         .tag(row));
    }
    return Element{structure::List(selection, structure::Section(
                                                  structure::Section::Labels{},
                                                  std::move(rows)))}
        .list_style(structure::ListStyle::sidebar())
        .cell_style(CountingCell{&cells});
  };
  Application application = start(providers, Host{&probe, &build});
  settle(providers, application);
  ASSERT_EQ(probe.tab->get(), 1);
  cells = 0;
  const int before = builds;
  press(providers, application, Button::dpad_down);
  settle(providers, application);
  EXPECT_EQ(probe.tab->get(), 2);
  EXPECT_EQ(builds, before + 1);
  EXPECT_GE(cells, 1);
  EXPECT_LE(cells, 2);
  EXPECT_TRUE(shows(application, "Row 49"));
  press(providers, application, Button::dpad_up);
  settle(providers, application);
  EXPECT_EQ(probe.tab->get(), 1);
  press(providers, application, Button::dpad_up);
  settle(providers, application);
  press(providers, application, Button::a);
  settle(providers, application);
  ASSERT_FALSE(fired.empty());
  EXPECT_EQ(fired.back(), builds);
}

TEST_F(StructureTest, SectionRowsTakeNewContentWithoutRebuildingCells) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  std::vector<Seen> seen;
  const Build build = [&seen](Probe& found,
                              composition::Context& context) -> Element {
    const auto flag = context.state(flag_key);
    found.flag = flag;
    const std::string_view mode = flag.get() ? "on" : "off";
    std::vector<Element> rows;
    for (int row = 0; row < 6; ++row) {
      rows.push_back(components::Text(std::format("row {} {}", row, mode)));
    }
    return Element{
        structure::List(structure::Section(
            structure::Section::Labels{
                .header = components::Text(std::format("head {}", mode)),
                .footer = std::nullopt},
            std::move(rows)))}
        .cell_style(RecordingCell{&seen});
  };
  Application application = start(providers, Host{&probe, &build});
  settle(providers, application);
  ASSERT_TRUE(shows(application, "row 5 off"));
  seen.clear();
  probe.flag->set(true);
  settle(providers, application);
  EXPECT_TRUE(shows(application, "row 0 on"));
  EXPECT_TRUE(shows(application, "row 5 on"));
  EXPECT_TRUE(shows(application, "head on"));
  EXPECT_FALSE(shows(application, "row 0 off"));
  EXPECT_TRUE(seen.empty());
}

TEST_F(StructureTest, SectionsRememberTheirFocus) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe&, composition::Context&) -> Element {
    const auto column = [](std::string_view side) {
      std::vector<Element> rows;
      for (int index = 0; index < 4; ++index) {
        const std::string name = std::format("{}{}", side, index);
        rows.push_back(components::Button(name, [] {}).id(name));
      }
      return Element{structure::List(structure::Section(
                         structure::Section::Labels{}, std::move(rows)))}
          .frame({.width = 300.0F, .height = 400.0F});
    };
    return components::HStack(column("left"), column("right"));
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_EQ(focused_key(application), "left0");
  press(providers, application, Button::dpad_right);
  EXPECT_EQ(focused_key(application), "right0");
  press(providers, application, Button::dpad_down);
  press(providers, application, Button::dpad_down);
  EXPECT_EQ(focused_key(application), "right2");
  press(providers, application, Button::dpad_left);
  EXPECT_EQ(focused_key(application), "left0");
  press(providers, application, Button::dpad_right);
  EXPECT_EQ(focused_key(application), "right2");
}

TEST_F(StructureTest, TabViewSwitchesWithShoulderButtonsAndTouch) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe&, composition::Context&) -> Element {
    return structure::TabView(
               structure::Tab(
                   "One", "star",
                   [] { return Element{components::Text("page 1")}; }),
               structure::Tab(
                   "Two", "wifi",
                   [] { return Element{components::Text("page 2")}; }),
               structure::Tab(
                   "Three", "home",
                   [] { return Element{components::Text("page 3")}; }))
        .tab_view_style(structure::SidebarAdaptableTabViewStyle{});
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_TRUE(shows(application, "page 1"));
  EXPECT_TRUE(capsule_on(application, 0));
  EXPECT_FALSE(capsule_on(application, 1));
  press(providers, application, Button::r);
  settle(providers, application);
  EXPECT_TRUE(shows(application, "page 2"));
  EXPECT_TRUE(capsule_on(application, 1));
  EXPECT_FALSE(capsule_on(application, 0));
  press(providers, application, Button::l);
  press(providers, application, Button::l);
  settle(providers, application);
  EXPECT_TRUE(shows(application, "page 3"));
  tap(providers, application,
      centre(application, structure::TabView::item_key(1).description()));
  settle(providers, application);
  EXPECT_TRUE(shows(application, "page 2"));
  const composition::Rect sidebar = frame_of(application, "arisa.tab.sidebar");
  EXPECT_FLOAT_EQ(sidebar.width, structure::TabView::sidebar_width);
}

TEST_F(StructureTest, TabCapsuleSpringsBetweenItems) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe&, composition::Context&) -> Element {
    return structure::TabView(
               structure::Tab("One", "star",
                              [] { return Element{components::Text("1")}; }),
               structure::Tab("Two", "wifi",
                              [] { return Element{components::Text("2")}; }))
        .tab_view_style(structure::SidebarAdaptableTabViewStyle{});
  };
  Application application = start(providers, Host{&probe, &build});
  auto& tree = application.root().tree();
  const auto capsule = [&tree]() -> motion::Presentation& {
    return tree.presentation(tree.node(*tree.find(composition::ElementKey{
                                           structure::TabView::capsule_id}))
                                 ->get());
  };
  EXPECT_FALSE(capsule().in_flight(motion::Presentation::Property::offset));
  providers.input().push(pad({Button::r}));
  providers.input().push(pad({}));
  frames(application, 2);
  ASSERT_TRUE(providers.clock().advance(frame_step).has_value());
  frames(application, 1);
  EXPECT_TRUE(capsule_on(application, 1));
  EXPECT_TRUE(capsule().in_flight(motion::Presentation::Property::offset));
  EXPECT_TRUE(application.last_frame().animating);
  settle(providers, application, 90);
  EXPECT_FALSE(application.last_frame().animating);
}

TEST_F(StructureTest, TabViewDrivesABoundSelection) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen,
                         composition::Context& context) -> Element {
    const auto tab = context.state(tab_key);
    seen.tab = tab;
    return structure::TabView(
        tab,
        structure::Tab("One", "star", 1,
                       [] { return Element{components::Text("first")}; }),
        structure::Tab("Two", "wifi", 2,
                       [] { return Element{components::Text("second")}; }));
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_TRUE(shows(application, "first"));
  press(providers, application, Button::r);
  EXPECT_EQ(probe.tab->get(), 2);
  probe.tab->set(1);
  settle(providers, application);
  EXPECT_TRUE(shows(application, "first"));
}

class TabCounter final : public composition::Component {
 public:
  static constexpr composition::StateKey<int> count{"tab-counter", 0};

  explicit TabCounter(
      std::optional<composition::StateHandle<int>>* seen) noexcept
      : seen_(seen) {}

  auto operator==(const TabCounter&) const -> bool = default;

  auto body(composition::Context& context) const -> Element override {
    const auto held = context.state(count);
    *seen_ = held;
    return components::Text(std::format("count {}", held.get()));
  }

 private:
  std::optional<composition::StateHandle<int>>* seen_;
};

TEST_F(StructureTest, FastSidebarSweepBuildsOnlyTheRestingPage) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  std::vector<std::size_t> built;
  constexpr std::size_t total = 8;
  const Build build = [&built](Probe&, composition::Context&) -> Element {
    std::vector<Element> tabs;
    for (std::size_t index = 0; index < total; ++index) {
      tabs.push_back(
          structure::Tab(std::format("Tab {}", index), "star", [&built, index] {
            built.push_back(index);
            return Element{components::Text(std::format("page {}", index))};
          }));
    }
    return Element{structure::TabView(std::nullopt, std::move(tabs))}
        .tab_view_style(structure::SidebarAdaptableTabViewStyle{});
  };
  Application application = start(providers, Host{&probe, &build});
  settle(providers, application);
  ASSERT_EQ(focused_key(application),
            structure::TabView::item_key(0).description());
  built.clear();
  for (std::size_t index = 1; index < total; ++index) {
    providers.input().push(pad({Button::dpad_down}));
    providers.input().push(pad({}));
    frames(application, 2);
    ASSERT_TRUE(providers.clock().advance(frame_step).has_value());
    frames(application, 1);
    EXPECT_EQ(focused_key(application),
              structure::TabView::item_key(index).description());
    EXPECT_TRUE(capsule_on(application, index));
    ASSERT_TRUE(providers.clock().advance(frame_step * 3).has_value());
    frames(application, 1);
  }
  EXPECT_TRUE(
      std::ranges::all_of(built, [](std::size_t index) { return index == 0; }));
  EXPECT_TRUE(shows(application, "page 0"));
  settle(providers, application);
  EXPECT_TRUE(shows(application, std::format("page {}", total - 1)));
  EXPECT_TRUE(std::ranges::all_of(built, [](std::size_t index) {
    return index == 0 || index == total - 1;
  }));
  EXPECT_TRUE(std::ranges::contains(built, total - 1));
}

TEST_F(StructureTest, CachedTabPagesKeepTheirState) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  std::optional<composition::StateHandle<int>> first;
  const Build build = [&first](Probe&, composition::Context&) -> Element {
    return structure::TabView(
               structure::Tab("One", "star",
                              [&first] { return Element{TabCounter{&first}}; }),
               structure::Tab(
                   "Two", "wifi",
                   [] { return Element{components::Text("page 2")}; }))
        .tab_view_style(structure::SidebarAdaptableTabViewStyle{});
  };
  Application application = start(providers, Host{&probe, &build});
  settle(providers, application);
  ASSERT_TRUE(first.has_value());
  first->set(5);
  settle(providers, application);
  EXPECT_TRUE(shows(application, "count 5"));
  press(providers, application, Button::r);
  settle(providers, application);
  EXPECT_TRUE(capsule_on(application, 1));
  const auto& tree = application.root().tree();
  const auto hidden = tree.node(
      *tree.find(composition::ElementKey{static_cast<std::int64_t>(0)}));
  ASSERT_TRUE(hidden.has_value());
  EXPECT_FLOAT_EQ(hidden->get().modifiers().opacity, 0.0F);
  press(providers, application, Button::l);
  settle(providers, application);
  EXPECT_TRUE(shows(application, "count 5"));
  EXPECT_EQ(first->get(), 5);
}

TEST_F(StructureTest, SidebarStepRebuildsTheSelectionOwnerOnceInOneFrame) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  int built = 0;
  const Build build = [&built](Probe& seen,
                               composition::Context& context) -> Element {
    ++built;
    const auto tab = context.state(tab_key);
    seen.tab = tab;
    return Element{
        structure::TabView(
            tab,
            structure::Tab("One", "star", 1,
                           [] { return Element{components::Text("first")}; }),
            structure::Tab("Two", "wifi", 2,
                           [] { return Element{components::Text("second")}; }),
            structure::Tab("Three", "bolt", 3,
                           [] { return Element{components::Text("third")}; }))}
        .tab_view_style(structure::SidebarAdaptableTabViewStyle{});
  };
  Application application = start(providers, Host{&probe, &build});
  settle(providers, application);
  ASSERT_EQ(focused_key(application),
            structure::TabView::item_key(0).description());
  built = 0;
  providers.input().push(pad({Button::dpad_down}));
  providers.input().push(pad({}));
  int working = 0;
  for (int frame = 0; frame < 6; ++frame) {
    frames(application, 1);
    working += application.last_frame().evaluations > 0 ? 1 : 0;
  }
  EXPECT_EQ(working, 1);
  EXPECT_EQ(built, 1);
  EXPECT_EQ(probe.tab->get(), 2);
  EXPECT_EQ(focused_key(application),
            structure::TabView::item_key(1).description());
  EXPECT_TRUE(capsule_on(application, 1));
}

TEST_F(StructureTest, SidebarStepLeavesARootWithoutTheSelectionAlone) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  int built = 0;
  const Build build = [&built](Probe&, composition::Context&) -> Element {
    ++built;
    return Element{
        structure::TabView(
            structure::Tab("One", "star",
                           [] { return Element{components::Text("first")}; }),
            structure::Tab("Two", "wifi",
                           [] { return Element{components::Text("second")}; }),
            structure::Tab("Three", "bolt",
                           [] { return Element{components::Text("third")}; }))}
        .tab_view_style(structure::SidebarAdaptableTabViewStyle{});
  };
  Application application = start(providers, Host{&probe, &build});
  settle(providers, application);
  ASSERT_EQ(focused_key(application),
            structure::TabView::item_key(0).description());
  built = 0;
  providers.input().push(pad({Button::dpad_down}));
  providers.input().push(pad({}));
  int working = 0;
  for (int frame = 0; frame < 6; ++frame) {
    frames(application, 1);
    working += application.last_frame().evaluations > 0 ? 1 : 0;
  }
  EXPECT_EQ(working, 1);
  EXPECT_EQ(built, 0);
  EXPECT_TRUE(capsule_on(application, 1));
  settle(providers, application);
  EXPECT_TRUE(shows(application, "second"));
}

TEST_F(StructureTest, HiddenKeptPagesWaitUntilShownToRebuild) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  std::vector<int> built;
  const Build build = [&built](Probe& seen,
                               composition::Context& context) -> Element {
    const auto flag = context.state(flag_key);
    seen.flag = flag;
    const std::string_view mode = flag.get() ? "on" : "off";
    const auto page = [&built, mode](int index) {
      return [&built, mode, index] {
        built.push_back(index);
        return Element{
            components::Text(std::format("page {} {}", index, mode))};
      };
    };
    return Element{structure::TabView(structure::Tab("One", "star", page(0)),
                                      structure::Tab("Two", "wifi", page(1)))}
        .tab_view_style(structure::SidebarAdaptableTabViewStyle{});
  };
  Application application = start(providers, Host{&probe, &build});
  settle(providers, application);
  press(providers, application, Button::r);
  settle(providers, application);
  ASSERT_TRUE(shows(application, "page 1 off"));
  built.clear();
  probe.flag->set(true);
  settle(providers, application);
  EXPECT_EQ(built, std::vector<int>{1});
  EXPECT_TRUE(shows(application, "page 1 on"));
  press(providers, application, Button::l);
  settle(providers, application);
  EXPECT_TRUE(shows(application, "page 0 on"));
  EXPECT_FALSE(shows(application, "page 0 off"));
}

TEST_F(StructureTest, ScaffoldPlacesTitleToolbarStatusAndHints) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe&, composition::Context&) -> Element {
    return structure::Scaffold(
        Element{components::Button("Body", [] {}).id("body")}
            .navigation_title("Title")
            .toolbar(composition::ToolbarItem(
                         composition::ToolbarItemPlacement::top_bar_leading,
                         components::Text("Lead").id("lead")),
                     composition::ToolbarItem(
                         composition::ToolbarItemPlacement::principal,
                         components::Text("Middle").id("middle")),
                     composition::ToolbarItem(
                         composition::ToolbarItemPlacement::top_bar_trailing,
                         components::Text("Trail").id("trail"))));
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_TRUE(shows(application, "Title"));
  EXPECT_TRUE(shows(application, "OK"));
  const float lead = frame_of(application, "lead").x;
  const float middle = frame_of(application, "middle").x;
  const float trail = frame_of(application, "trail").x;
  const float status = frame_of(application, "arisa.scaffold.status").x;
  EXPECT_LT(lead, middle);
  EXPECT_LT(middle, trail);
  EXPECT_LT(trail, status);
  const composition::Rect header =
      frame_of(application, structure::Scaffold::header_key);
  const composition::Rect content =
      frame_of(application, structure::Scaffold::content_key);
  const composition::Rect footer =
      frame_of(application, structure::Scaffold::footer_key);
  EXPECT_NEAR(header.y, 0.0F, 1.0F);
  EXPECT_NEAR(header.y + header.height + 0.5F, content.y, 1.0F);
  EXPECT_NEAR(content.y + content.height + 0.5F, footer.y, 1.0F);
  EXPECT_NEAR(footer.y + footer.height, 720.0F, 1.0F);
}

TEST_F(StructureTest, OverlayBarsFloatAndInsetTheScrolledContent) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe&, composition::Context&) -> Element {
    std::vector<Element> rows;
    for (int index = 0; index < 30; ++index) {
      const std::string name = std::format("row{}", index);
      rows.push_back(components::Button(name, [] {})
                         .frame({.width = 300.0F, .height = 60.0F})
                         .id(name));
    }
    return structure::Scaffold(
        structure::ScaffoldOptions{
            .shows_status = true,
            .shows_hints = true,
            .header = composition::BarPlacement::overlay,
            .footer = composition::BarPlacement::overlay},
        Element{components::ScrollView(components::VStack(std::move(rows)))}
            .navigation_title("Scroll"));
  };
  Application application = start(providers, Host{&probe, &build});
  const composition::Rect content =
      frame_of(application, structure::Scaffold::content_key);
  const composition::Rect header =
      frame_of(application, structure::Scaffold::header_key);
  EXPECT_NEAR(content.y, 0.0F, 1.0F);
  EXPECT_NEAR(content.height, 720.0F, 1.0F);
  EXPECT_GE(frame_of(application, "row0").y, header.y + header.height - 0.5F);
  for (int step = 0; step < 29; ++step) {
    press(providers, application, Button::dpad_down);
  }
  settle(providers, application, 60);
  const composition::Rect footer =
      frame_of(application, structure::Scaffold::footer_key);
  const composition::Rect last = frame_of(application, "row29");
  EXPECT_LE(last.y + last.height, footer.y + 0.5F);
}

TEST_F(StructureTest, SwitchingBarPlacementRelaysOutOnce) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen,
                         composition::Context& context) -> Element {
    const auto floating = context.state(flag_key);
    seen.flag = floating;
    return structure::Scaffold(
               Element{components::Text("body")}.navigation_title("Bars"))
        .scaffold_bar_placement(theme::Edge::top,
                                floating.get()
                                    ? composition::BarPlacement::overlay
                                    : composition::BarPlacement::in_layout);
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_GT(frame_of(application, structure::Scaffold::content_key).y, 1.0F);
  probe.flag->set(true);
  frames(application, 1);
  EXPECT_TRUE(application.last_frame().laid_out);
  int layouts = 0;
  for (int index = 0; index < 10; ++index) {
    frames(application, 1);
    layouts += application.last_frame().laid_out ? 1 : 0;
  }
  EXPECT_EQ(layouts, 0);
  EXPECT_NEAR(frame_of(application, structure::Scaffold::content_key).y, 0.0F,
              1.0F);
}

TEST_F(StructureTest, ScaffoldContentClearsTheHeaderAndHintBar) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe&, composition::Context&) -> Element {
    std::vector<Element> rows;
    for (int index = 0; index < 30; ++index) {
      const std::string name = std::format("row{}", index);
      rows.push_back(components::Button(name, [] {})
                         .frame({.width = 300.0F, .height = 60.0F})
                         .id(name));
    }
    return structure::Scaffold(
        Element{components::ScrollView(components::VStack(std::move(rows)))}
            .navigation_title("Scroll"));
  };
  Application application = start(providers, Host{&probe, &build});
  const auto bottom_of = [](const composition::Rect& rect) {
    return rect.y + rect.height;
  };
  const composition::Rect header =
      frame_of(application, structure::Scaffold::header_key);
  EXPECT_GE(frame_of(application, "row0").y, bottom_of(header) - 0.5F);
  for (int step = 0; step < 29; ++step) {
    press(providers, application, Button::dpad_down);
  }
  settle(providers, application, 60);
  EXPECT_EQ(focused_key(application), "row29");
  const composition::Rect footer =
      frame_of(application, structure::Scaffold::footer_key);
  const composition::Rect last = frame_of(application, "row29");
  EXPECT_LE(bottom_of(last), footer.y + 0.5F);
  EXPECT_GE(last.y, bottom_of(header));
  for (int step = 0; step < 29; ++step) {
    press(providers, application, Button::dpad_up);
  }
  settle(providers, application, 60);
  EXPECT_GE(frame_of(application, "row0").y, bottom_of(header) - 0.5F);
}

TEST_F(StructureTest, StatusWidgetsReadTheProvidersAndRefresh) {
  provider::headless::HeadlessProviderFactory providers;
  providers.system_info().set_battery(provider::BatteryState{
      .percent = provider::BatteryPercent::of<42>(), .charging = true});
  providers.system_info().set_network(
      provider::NetworkState{.kind = provider::NetworkKind::ethernet,
                             .signal_bars = provider::SignalBars::of<3>(),
                             .address = {}});
  providers.clock().set_wall(
      std::chrono::sys_days{std::chrono::year{2026} / std::chrono::May / 2} +
      std::chrono::hours{13} + std::chrono::minutes{5});
  providers.clock().set_local_offset(std::chrono::hours{1});
  Probe probe;
  const Build build = [](Probe&, composition::Context&) -> Element {
    return components::HStack(components::status::Wireless{},
                              components::status::Battery{},
                              components::status::Clock{});
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_TRUE(shows(application, "42%"));
  EXPECT_TRUE(shows(application, "14:05"));
  EXPECT_TRUE(shows(application,
                    *components::SystemSymbols::text("battery_2_bar")));
  EXPECT_TRUE(shows(application, *components::SystemSymbols::text("lan")));
  providers.system_info().set_battery(provider::BatteryState{
      .percent = provider::BatteryPercent::of<41>(), .charging = false});
  for (int step = 0; step < 70; ++step) {
    ASSERT_TRUE(providers.clock().advance(std::chrono::seconds{1}).has_value());
    frames(application, 1);
  }
  EXPECT_TRUE(shows(application, "14:06"));
  EXPECT_TRUE(shows(application, "41%"));
}

TEST_F(StructureTest, StatusWidgetsTakeTheFontTheyAreGiven) {
  provider::headless::HeadlessProviderFactory providers;
  providers.system_info().set_battery(provider::BatteryState{
      .percent = provider::BatteryPercent::of<42>(), .charging = true});
  providers.system_info().set_network(
      provider::NetworkState{.kind = provider::NetworkKind::ethernet,
                             .signal_bars = provider::SignalBars::of<3>(),
                             .address = {}});
  providers.clock().set_wall(
      std::chrono::sys_days{std::chrono::year{2026} / std::chrono::May / 2} +
      std::chrono::hours{13} + std::chrono::minutes{5});
  providers.clock().set_local_offset(std::chrono::hours{1});
  Probe probe;
  const Build build = [](Probe&, composition::Context&) -> Element {
    return components::HStack(
        components::status::Wireless{theme::Font::footnote},
        components::status::Battery{theme::Font::caption2,
                                    theme::Font::caption},
        components::status::Clock{theme::Font::caption2});
  };
  Application application = start(providers, Host{&probe, &build});
  const composition::Tree& tree = application.root().tree();
  const auto font = [&tree](const std::string& text) {
    const composition::Node* found = nullptr;
    const auto search = [&found, &text](const auto& self,
                                        const composition::Node& node) -> void {
      if (const auto content = node.content_as<components::Text::Content>();
          content.has_value() && content->get().text() == text) {
        found = &node;
      }
      for (const composition::Node& child : node.children()) {
        self(self, child);
      }
    };
    search(search, tree.root());
    std::optional<theme::Font> chosen;
    if (found != nullptr) {
      chosen =
          found->content_as<components::Text::Content>()->get().explicit_font();
    }
    while (found != nullptr && !chosen.has_value()) {
      if (const auto declared =
              found->override_of(components::Environment::font)) {
        chosen = declared->get();
      }
      const auto parent = found->parent();
      found = parent.has_value() ? &tree.node(*parent)->get() : nullptr;
    }
    return chosen;
  };
  EXPECT_EQ(font("14:05"), std::optional{theme::Font::caption2});
  EXPECT_EQ(font("42%"), std::optional{theme::Font::caption2});
  EXPECT_EQ(font(*components::SystemSymbols::text("battery_2_bar")),
            std::optional{theme::Font::caption});
  EXPECT_EQ(font(*components::SystemSymbols::text("lan")),
            std::optional{theme::Font::footnote});
}

TEST_F(StructureTest, InsertRemoveAndMoveAnimateInsideForEach) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen,
                         composition::Context& context) -> Element {
    const auto items = context.state(items_key);
    seen.items = items;
    return components::VStack(components::ForEach(
        items.get(), [](const std::string& item) { return item; },
        [](const std::string& item) {
          return components::Text(item).frame(
              {.width = 100.0F, .height = 30.0F});
        }));
  };
  Application application = start(providers, Host{&probe, &build});
  motion::with_animation(motion::Animation::smooth(0.5), [&] {
    probe.items->update([](std::vector<std::string>& values) {
      values.insert(values.begin() + 1, "new");
    });
  });
  frames(application, 1);
  ASSERT_TRUE(providers.clock().advance(frame_step).has_value());
  frames(application, 1);
  const auto inserted = node_named(application, "new");
  ASSERT_TRUE(inserted.has_value());
  EXPECT_TRUE(inserted->get().presentation().animating());
  const auto moved = node_named(application, "b");
  ASSERT_TRUE(moved.has_value());
  EXPECT_TRUE(moved->get().presentation().in_flight(
      motion::Presentation::Property::frame));
  settle(providers, application, 60);
  motion::with_animation(motion::Animation::smooth(0.5), [&] {
    probe.items->update(
        [](std::vector<std::string>& values) { std::erase(values, "a"); });
  });
  frames(application, 2);
  EXPECT_GT(application.root().tree().departing(), 0U);
  EXPECT_FALSE(node_of(application, "a").has_value());
  settle(providers, application, 60);
  EXPECT_EQ(application.root().tree().departing(), 0U);
  motion::with_animation(motion::Animation::smooth(0.5), [&] {
    probe.items->update(
        [](std::vector<std::string>& values) { std::ranges::reverse(values); });
  });
  frames(application, 1);
  ASSERT_TRUE(providers.clock().advance(frame_step).has_value());
  frames(application, 1);
  const auto reversed = node_named(application, "d");
  ASSERT_TRUE(reversed.has_value());
  EXPECT_TRUE(reversed->get().presentation().in_flight(
      motion::Presentation::Property::frame));
}

TEST_F(StructureTest, ListRowsUseTheirDefaultOrOverriddenTransition) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen,
                         composition::Context& context) -> Element {
    const auto items = context.state(items_key);
    seen.items = items;
    return structure::List(components::ForEach(
        items.get(), [](const std::string& item) { return item; },
        [](const std::string& item) -> Element {
          if (item == "slide") {
            return components::Text(item).transition(
                motion::Transition::move(theme::Edge::leading));
          }
          return components::Text(item);
        }));
  };
  Application application = start(providers, Host{&probe, &build});
  probe.items->update(
      [](std::vector<std::string>& values) { values.emplace_back("slide"); });
  frames(application, 2);
  const auto plain = node_named(application, "a");
  ASSERT_TRUE(plain.has_value());
  EXPECT_EQ(plain->get().transition(),
            structure::ListCell::default_transition());
  const auto slid = node_named(application, "slide");
  ASSERT_TRUE(slid.has_value());
  EXPECT_EQ(slid->get().transition(),
            motion::Transition::move(theme::Edge::leading));
}

TEST_F(StructureTest, ControllerReordersAndDeletesRows) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen,
                         composition::Context& context) -> Element {
    const auto items = context.state(items_key);
    seen.items = items;
    return structure::List(editable(items));
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_EQ(focused_key(application), "unkeyed");
  press(providers, application, Button::dpad_down);
  press(providers, application, structure::ListCell::move_button);
  press(providers, application, Button::dpad_down);
  settle(providers, application);
  EXPECT_EQ(probe.items->get(), (std::vector<std::string>{"a", "c", "b", "d"}));
  press(providers, application, Button::dpad_down);
  settle(providers, application);
  EXPECT_EQ(probe.items->get(), (std::vector<std::string>{"a", "c", "d", "b"}));
  press(providers, application, Button::a);
  settle(providers, application);
  press(providers, application, Button::dpad_up);
  settle(providers, application);
  EXPECT_EQ(probe.items->get(), (std::vector<std::string>{"a", "c", "d", "b"}));
  press(providers, application, structure::ListCell::delete_button);
  settle(providers, application, 60);
  EXPECT_EQ(probe.items->get(), (std::vector<std::string>{"a", "c", "b"}));
}

TEST_F(StructureTest, TouchDragsTheHandleToReorder) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen,
                         composition::Context& context) -> Element {
    const auto items = context.state(items_key);
    seen.items = items;
    return structure::List(editable(items));
  };
  Application application = start(providers, Host{&probe, &build});
  const auto handles = [&application] {
    std::vector<composition::Rect> found;
    const std::function<void(const composition::Node&)> visit =
        [&](const composition::Node& node) {
          if (node.key() == composition::ElementKey{"arisa.list.handle"}) {
            found.push_back(node.frame());
          }
          for (const composition::Node& child : node.children()) {
            visit(child);
          }
        };
    visit(application.root().tree().root());
    return found;
  };
  const std::vector<composition::Rect> found = handles();
  ASSERT_EQ(found.size(), 4U);
  const float row = found[1].y - found[0].y;
  const provider::PixelPoint from{.x = found[0].x + (found[0].width * 0.5F),
                                  .y = found[0].y + (found[0].height * 0.5F)};
  drag_path(providers, application, from,
            provider::PixelPoint{.x = from.x, .y = from.y + (row * 2.0F)});
  settle(providers, application);
  EXPECT_EQ(probe.items->get(), (std::vector<std::string>{"b", "c", "a", "d"}));
}

TEST_F(StructureTest, EditModeShowsDeleteButtons) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen,
                         composition::Context& context) -> Element {
    const auto items = context.state(items_key);
    seen.items = items;
    return structure::List(editable(items))
        .environment(components::Environment::edit_mode,
                     components::EditMode::active);
  };
  Application application = start(providers, Host{&probe, &build});
  const auto first_delete =
      [&application]() -> std::optional<composition::Rect> {
    std::optional<composition::Rect> found;
    const std::function<void(const composition::Node&)> visit =
        [&](const composition::Node& node) {
          if (!found.has_value() &&
              node.key() == composition::ElementKey{"arisa.list.delete"}) {
            found = node.frame();
          }
          for (const composition::Node& child : node.children()) {
            visit(child);
          }
        };
    visit(application.root().tree().root());
    return found;
  };
  const auto button_frame = first_delete();
  ASSERT_TRUE(button_frame.has_value());
  tap(providers, application,
      provider::PixelPoint{
          .x = button_frame->x + (button_frame->width * 0.5F),
          .y = button_frame->y + (button_frame->height * 0.5F)});
  settle(providers, application, 60);
  EXPECT_EQ(probe.items->get(), (std::vector<std::string>{"b", "c", "d"}));
}

}
