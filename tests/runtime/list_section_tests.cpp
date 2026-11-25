#include <arisa/components/button.hpp>
#include <arisa/components/button_role.hpp>
#include <arisa/components/edit_mode.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/for_each.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/lazy_vstack.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/badge_prominence.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/list_environment.hpp>
#include <arisa/composition/list_row_separator_alignment.hpp>
#include <arisa/composition/list_section_spacing.hpp>
#include <arisa/composition/sidebar_row_size.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/vertical_edges.hpp>
#include <arisa/composition/view_dimensions.hpp>
#include <arisa/composition/visibility.hpp>
#include <arisa/controls/context_menu.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/structure/disclosure_group.hpp>
#include <arisa/structure/list.hpp>
#include <arisa/structure/list_style.hpp>
#include <arisa/structure/section.hpp>
#include <arisa/structure/swipe_row.hpp>
#include <arisa/theme/colour.hpp>

#include <scroll_fixture.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace arisa::runtime {

using composition::Element;
using provider::Button;

class ListSectionTest : public ScrollFixture {
 protected:
  using Body = std::function<Element(composition::Context&)>;

  class Host final : public composition::Component {
   public:
    explicit Host(std::shared_ptr<const Body> body) noexcept
        : body_(std::move(body)) {}

    auto operator==(const Host& other) const -> bool {
      return body_ == other.body_;
    }

    auto body(composition::Context& context) const -> Element override {
      return (*body_)(context);
    }

   private:
    std::shared_ptr<const Body> body_;
  };

  static auto host(Body body) -> Element {
    return Host{std::make_shared<const Body>(std::move(body))};
  }

  static void run(provider::headless::HeadlessProviderFactory& providers,
                  Application& application, int count) {
    for (int index = 0; index < count; ++index) {
      ScrollFixture::step(providers, application);
    }
  }

  static auto keyed(const composition::Node& node, std::string_view key)
      -> std::size_t {
    std::size_t found =
        node.key().has_value() && node.key()->description() == key ? 1 : 0;
    for (const composition::Node& child : node.children()) {
      found += keyed(child, key);
    }
    return found;
  }
};

TEST_F(ListSectionTest, CollapsibleSectionsHideTheirRows) {
  provider::headless::HeadlessProviderFactory providers;
  static constexpr composition::StateKey<bool> open{"list-test-open", true};
  auto handle =
      std::make_shared<std::optional<composition::StateHandle<bool>>>();
  Application application = start(
      providers, host([handle](composition::Context& context) {
        const auto expanded = context.state(open);
        *handle = expanded;
        return structure::List(
            structure::Section("Bound", expanded,
                               components::Text("Bound row")),
            structure::Section(components::Text("Plain row")).collapsible(true),
            structure::Section(
                structure::Section::Labels{.header = components::Text("Fixed"),
                                           .footer = std::nullopt},
                expanded, components::Text("Fixed row"))
                .collapsible(false),
            structure::Section(expanded, components::Text("Unlabelled")));
      }));
  run(providers, application, 6);
  ASSERT_TRUE(handle->has_value());
  EXPECT_TRUE(shows(application, "Bound row"));
  EXPECT_TRUE(node_of(application, "arisa.list.section-chevron").has_value());
  tap(providers, application, centre(application, "arisa.list.section-toggle"));
  run(providers, application, 30);
  EXPECT_FALSE((*handle)->get());
  EXPECT_FALSE(shows(application, "Bound row"));
  EXPECT_TRUE(shows(application, "Fixed row"));
  EXPECT_FALSE(shows(application, "Unlabelled"));
  EXPECT_TRUE(shows(application, "Plain row"));
  (*handle)->set(true);
  run(providers, application, 30);
  EXPECT_TRUE(shows(application, "Bound row"));
}

TEST_F(ListSectionTest, DisclosureGroupsFlattenIntoRows) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers, host([](composition::Context&) {
              return structure::List(structure::Section(
                  structure::DisclosureGroup(
                      "Display",
                      [] {
                        return Element{components::Group(
                            components::Text("Brightness"),
                            structure::DisclosureGroup("Advanced", [] {
                              return Element{components::Text("Gamma")};
                            }))};
                      }),
                  components::Text("After")));
            }));
  run(providers, application, 6);
  EXPECT_FALSE(shows(application, "Brightness"));
  press(providers, application, Button::a);
  run(providers, application, 30);
  EXPECT_TRUE(shows(application, "Brightness"));
  EXPECT_TRUE(node_of(application, "arisa.list.outline.0.0").has_value());
  EXPECT_TRUE(node_of(application, "arisa.list.outline.0.1").has_value());
  const composition::Rect label =
      presented_of(application, "arisa.list.outline.0");
  const composition::Rect child =
      presented_of(application, "arisa.list.outline.0.0");
  EXPECT_GE(child.y, label.y + label.height - 1.0F);
  EXPECT_FALSE(shows(application, "Gamma"));
  tap(providers, application, centre(application, "arisa.list.outline.0.1"));
  run(providers, application, 30);
  EXPECT_TRUE(shows(application, "Gamma"));
}

TEST_F(ListSectionTest, SectionSeparatorsMarginsAndSpacing) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers, host([](composition::Context&) {
        return structure::List(
                   structure::Section("One", components::Text("A").id("a"),
                                      components::Text("B"))
                       .list_section_separator(composition::Visibility::visible)
                       .list_section_separator_tint(
                           theme::ColourValue{theme::ColourRole::red},
                           composition::VerticalEdges::bottom),
                   structure::Section("Two", components::Text("C").id("c"))
                       .list_section_margins(theme::Edge::horizontal, 60.0F)
                       .list_section_margins(theme::Edge::top, 12.0F),
                   structure::Section(
                       "Three",
                       components::Text("D")
                           .list_row_insets(theme::Edge::leading, 40.0F)
                           .id("d"))
                       .list_section_separator(composition::Visibility::hidden,
                                               composition::VerticalEdges::top))
            .list_section_spacing(composition::ListSectionSpacing::compact)
            .default_min_list_header_height(40.0F)
            .frame({.width = 500.0F, .height = 600.0F});
      }));
  run(providers, application, 6);
  EXPECT_EQ(
      keyed(application.root().tree().root(), "arisa.list.section-separator"),
      2U);
  const composition::Rect a = presented_of(application, "a");
  const composition::Rect c = presented_of(application, "c");
  const composition::Rect d = presented_of(application, "d");
  EXPECT_GT(c.x, a.x + 30.0F);
  EXPECT_GT(d.x, a.x + 20.0F);
  const auto headers = node_named(application, "arisa.list.header");
  ASSERT_TRUE(headers.has_value());
  EXPECT_GE(headers->get().frame().height, 40.0F);
}

TEST_F(ListSectionTest, BadgeProminenceStylesTheBadge) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers, host([](composition::Context&) {
              return structure::List(
                  components::Text("Plain").badge(1).id("plain"),
                  components::Text("Loud").badge(2).badge_prominence(
                      composition::BadgeProminence::increased),
                  components::Text("Quiet").badge(3).badge_prominence(
                      composition::BadgeProminence::decreased));
            }));
  run(providers, application, 6);
  EXPECT_TRUE(shows(application, "2"));
  EXPECT_TRUE(shows(application, "3"));
  bool capsule = false;
  const std::function<void(const composition::Node&, bool)> visit =
      [&](const composition::Node& node, bool inside) {
        const bool badge =
            inside || (node.key().has_value() &&
                       node.key()->description() == "arisa.list.badge");
        if (badge && node.modifiers().frame.min_height.has_value()) {
          capsule = true;
        }
        for (const composition::Node& child : node.children()) {
          visit(child, badge);
        }
      };
  visit(application.root().tree().root(), false);
  EXPECT_TRUE(capsule);
}

TEST_F(ListSectionTest, SectionActionsShowInTheHeader) {
  provider::headless::HeadlessProviderFactory providers;
  auto pressed = std::make_shared<int>(0);
  Application application = start(
      providers, host([pressed](composition::Context&) {
        return structure::List(
            structure::Section("Header", components::Text("Row"))
                .section_actions([pressed] {
                  return Element{components::Button("Add", [pressed] {
                                   ++*pressed;
                                 }).frame({.width = 60.0F, .height = 30.0F})};
                }));
      }));
  run(providers, application, 6);
  ASSERT_TRUE(node_of(application, "arisa.list.section-actions").has_value());
  tap(providers, application,
      centre(application, "arisa.list.section-actions"));
  run(providers, application, 4);
  EXPECT_EQ(*pressed, 1);
}

TEST_F(ListSectionTest, SwipeActionsContainerHostsRowsOutsideAList) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers, host([](composition::Context&) {
        return components::ScrollView(
                   components::LazyVStack(components::ForEach(
                       std::vector<std::string>{"Alpha", "Bravo"},
                       [](const std::string& name) { return name; },
                       [](const std::string& name) {
                         return components::Text(name)
                             .frame({.width = 400.0F, .height = 44.0F})
                             .swipe_actions([] {
                               return Element{components::Button(
                                   "Remove",
                                   components::ButtonRole::destructive, [] {})};
                             });
                       })))
            .swipe_actions_container();
      }));
  run(providers, application, 6);
  EXPECT_FALSE(shows(application, "Remove"));
  const provider::PixelPoint from = centre(application, "Bravo");
  drag(providers, application, from,
       provider::PixelPoint{.x = from.x - 140.0F, .y = from.y}, 12);
  run(providers, application, 30);
  EXPECT_TRUE(shows(application, "Remove"));
}

TEST_F(ListSectionTest, SwipeActionsOutsideAContainerStayInert) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers, host([](composition::Context&) {
              return components::VStack(
                  components::Text("Alone")
                      .id("Alone")
                      .frame({.width = 400.0F, .height = 44.0F})
                      .swipe_actions([] {
                        return Element{components::Button("Remove", [] {})};
                      }));
            }));
  run(providers, application, 6);
  const provider::PixelPoint from = centre(application, "Alone");
  drag(providers, application, from,
       provider::PixelPoint{.x = from.x - 140.0F, .y = from.y}, 12);
  run(providers, application, 30);
  EXPECT_FALSE(shows(application, "Remove"));
}

TEST_F(ListSectionTest, ContextMenuForSelectionUsesTheSelection) {
  provider::headless::HeadlessProviderFactory providers;
  auto primary = std::make_shared<std::set<int>>();
  static const composition::StateKey<std::set<int>> chosen{"list-test-chosen",
                                                           {2, 3}};
  Application application = start(
      providers, host([primary](composition::Context& context) {
        return structure::List(
                   context.state(chosen),
                   structure::Section(components::ForEach(
                       std::vector<int>{1, 2, 3, 4},
                       [](int row) { return row; },
                       [](int row) {
                         return components::Text(std::format("Row {}", row))
                             .tag(row)
                             .id(std::format("Row {}", row));
                       })))
            .context_menu(
                std::type_identity<int>{},
                [](const std::set<int>& rows) {
                  return components::Button(
                      std::format("Act on {}", rows.size()), [] {});
                },
                [primary](const std::set<int>& rows) { *primary = rows; })
            .environment(components::Environment::edit_mode,
                         components::EditMode::inactive);
      }));
  run(providers, application, 6);
  tap(providers, application, centre(application, "Row 1"));
  run(providers, application, 4);
  EXPECT_EQ(*primary, (std::set<int>{1}));
  hold(providers, application, pad({}), std::chrono::milliseconds{16});
  const provider::PixelPoint row = centre(application, "Row 2");
  provider::InputFrame down;
  down.touches.push_back(provider::Touch{.id = provider::TouchId{4},
                                         .position = row,
                                         .phase = provider::TouchPhase::began});
  providers.input().push(down);
  for (int index = 0; index < 50; ++index) {
    ScrollFixture::step(providers, application);
  }
  provider::InputFrame up;
  up.touches.push_back(provider::Touch{.id = provider::TouchId{4},
                                       .position = row,
                                       .phase = provider::TouchPhase::ended});
  providers.input().push(up);
  run(providers, application, 30);
  EXPECT_TRUE(shows(application, "Act on 2"));
}

TEST_F(ListSectionTest, BorderedListsAndSidebarRowSizes) {
  const auto height_of = [](composition::SidebarRowSize size) {
    provider::headless::HeadlessProviderFactory providers;
    Application application =
        start(providers, host([size](composition::Context&) {
                return structure::List(components::Text("Row"))
                    .list_style(structure::SidebarListStyle{})
                    .environment(composition::ListEnvironment::sidebar_row_size,
                                 size);
              }));
    run(providers, application, 6);
    return presented_of(application, "arisa.list.row.0").height;
  };
  const float small = height_of(composition::SidebarRowSize::small);
  const float medium = height_of(composition::SidebarRowSize::medium);
  const float large = height_of(composition::SidebarRowSize::large);
  EXPECT_LT(small, medium);
  EXPECT_LT(medium, large);
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers, host([](composition::Context&) {
              return structure::List(components::Text("Bordered"),
                                     components::Text("Second"))
                  .list_style(structure::BorderedListStyle{true});
            }));
  run(providers, application, 6);
  EXPECT_TRUE(shows(application, "Bordered"));
  EXPECT_FALSE(
      node_of(application, "arisa.list.section-separator").has_value());
}

TEST_F(ListSectionTest, RowSeparatorGuidesMoveTheSeparatorEnds) {
  const auto rules = [](bool guided) {
    provider::headless::HeadlessProviderFactory providers;
    Application application = start(
        providers, host([guided](composition::Context&) {
          const auto row = [guided](std::string_view key) {
            Element label = components::VStack()
                                .frame({.width = 60.0F, .height = 20.0F})
                                .id(std::format("{}-text", key));
            if (guided) {
              label =
                  std::move(label)
                      .alignment_guide(
                          composition::list_row_separator_leading,
                          [](const composition::ViewDimensions& dimensions) {
                            return dimensions
                                [composition::HorizontalAlignment::leading];
                          })
                      .alignment_guide(
                          composition::list_row_separator_trailing,
                          [](const composition::ViewDimensions& dimensions) {
                            return dimensions
                                [composition::HorizontalAlignment::trailing];
                          });
            }
            return components::HStack(
                components::VStack().frame({.width = 30.0F, .height = 20.0F}),
                std::move(label));
          };
          return structure::List(row("Apple"), row("Pear"), row("Plum"))
              .frame({.width = 500.0F, .height = 400.0F});
        }));
    run(providers, application, 6);
    std::vector<composition::Rect> found;
    const auto collect = [&found](const auto& self,
                                  const composition::Node& node) -> void {
      if (node.key().has_value() &&
          node.key()->description() == "arisa.list.row-separator" &&
          !node.children().empty()) {
        found.push_back(node.children().front().frame());
      }
      for (const composition::Node& child : node.children()) {
        self(self, child);
      }
    };
    collect(collect, application.root().tree().root());
    const composition::Rect pear = frame_of(application, "Pear-text");
    return std::pair{found, pear};
  };
  const auto [plain, plain_pear] = rules(false);
  const auto [guided, guided_pear] = rules(true);
  ASSERT_EQ(plain.size(), guided.size());
  ASSERT_FALSE(guided.empty());
  bool moved = false;
  for (std::size_t index = 0; index < guided.size(); ++index) {
    if (std::abs(guided[index].y - guided_pear.y) > 30.0F) {
      continue;
    }
    EXPECT_NEAR(guided[index].x, guided_pear.x, 0.5F);
    EXPECT_NEAR(guided[index].x + guided[index].width,
                guided_pear.x + guided_pear.width, 0.5F);
    EXPECT_LT(plain[index].x, guided_pear.x - 10.0F);
    moved = true;
  }
  EXPECT_TRUE(moved);
}

}
