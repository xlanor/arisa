#include <arisa/components/button.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/editable_tokens.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/search_environment.hpp>
#include <arisa/composition/search_field_placement.hpp>
#include <arisa/composition/search_suggestions_placement.hpp>
#include <arisa/composition/search_toolbar_behavior.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/controls/searchable.hpp>
#include <arisa/presentation/navigation_stack.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/structure/navigation_split_view.hpp>

#include <scroll_fixture.hpp>

#include <gtest/gtest.h>

#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace arisa::runtime {

using composition::Element;
using composition::SearchFieldPlacement;

class SearchParityTest : public ScrollFixture {
 protected:
  struct Probe {
    std::optional<composition::StateHandle<std::string>> query;
    std::optional<composition::StateHandle<bool>> presented;
    std::optional<composition::StateHandle<std::vector<int>>> tokens;
    std::optional<composition::SearchSuggestionsPlacement> placement;
  };

  static auto probe() -> Probe& {
    static Probe value;
    return value;
  }

  using Build = std::function<Element(Element)>;

  static constexpr composition::StateKey<std::string> query{"parity-search",
                                                            ""};
  static constexpr composition::StateKey<bool> presented{"parity-presented",
                                                         false};
  static inline const composition::StateKey<std::vector<int>> tokens{
      "parity-tokens", std::vector<int>{3}};

  class Host final : public composition::Component {
   public:
    explicit Host(
        std::shared_ptr<const std::function<Element(composition::Context&)>>
            build)
        : build_(std::move(build)) {}

    auto operator==(const Host& other) const -> bool {
      return build_ == other.build_;
    }

    auto body(composition::Context& context) const -> Element override {
      probe().query = context.state(query);
      probe().presented = context.state(presented);
      probe().tokens = context.state(tokens);
      return (*build_)(context);
    }

   private:
    std::shared_ptr<const std::function<Element(composition::Context&)>> build_;
  };

  class PlacementReader final : public composition::Component {
   public:
    auto operator==(const PlacementReader&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      probe().placement = context.environment(
          composition::SearchEnvironment::suggestions_placement);
      return components::Text("Suggested").id("suggested");
    }
  };

  static auto host(std::function<Element(composition::Context&)> build)
      -> Element {
    return Host{
        std::make_shared<const std::function<Element(composition::Context&)>>(
            std::move(build))};
  }

  static auto stacked(Element page) -> Element {
    auto held = std::make_shared<composition::PlaceholderContentView::Source>(
        std::move(page));
    return presentation::NavigationStack(
        [held] { return Element{composition::PlaceholderContentView(held)}; });
  }

  static auto rows() -> Element {
    std::vector<Element> lines;
    for (int index = 0; index < 40; ++index) {
      lines.push_back(
          components::Text(std::format("result {}", index))
              .frame({.height = 44.0F, .max_width = composition::infinity}));
    }
    return components::ScrollView(components::VStack(std::move(lines)))
        .frame({.max_width = composition::infinity,
                .max_height = composition::infinity});
  }

  static auto token(int value) -> Element {
    return components::Text(std::format("Tag {}", value));
  }
};

TEST_F(SearchParityTest, IsPresentedBindingActivatesAndDismisses) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers, host([](composition::Context&) {
              return stacked(
                  components::Text("body").navigation_title("Find").searchable(
                      *probe().query, *probe().presented));
            }));
  frames(application, 3);
  EXPECT_FALSE(node_of(application, "arisa.search.cancel").has_value());
  EXPECT_TRUE(
      node_of(application, presentation::NavigationStack::bar_id).has_value());
  probe().presented->set(true);
  settle(providers, application);
  EXPECT_TRUE(node_of(application, "arisa.search.cancel").has_value());
  EXPECT_FALSE(
      node_of(application, presentation::NavigationStack::bar_id).has_value());
  ASSERT_TRUE(application.root().activate(
      composition::ElementKey{"arisa.search.cancel"}));
  settle(providers, application);
  EXPECT_FALSE(probe().presented->get());
  EXPECT_FALSE(node_of(application, "arisa.search.cancel").has_value());
}

TEST_F(SearchParityTest, AvoidHidingContentKeepsTheNavigationBar) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers, host([](composition::Context&) {
        return stacked(components::Text("body")
                           .navigation_title("Find")
                           .searchable(*probe().query, *probe().presented)
                           .search_presentation_toolbar_behavior(
                               composition::SearchPresentationToolbarBehavior::
                                   avoid_hiding_content));
      }));
  probe().presented->set(true);
  settle(providers, application);
  EXPECT_TRUE(node_of(application, "arisa.search.cancel").has_value());
  EXPECT_TRUE(
      node_of(application, presentation::NavigationStack::bar_id).has_value());
}

TEST_F(SearchParityTest, TokenOverloadsTakeIsPresented) {
  provider::headless::HeadlessProviderFactory providers;
  static const composition::StateKey<std::vector<int>> suggested{
      "parity-suggested", std::vector<int>{5, 6}};
  Application application = start(
      providers, host([](composition::Context& context) {
        return components::VStack(
            components::Text("one").searchable(
                *probe().query, *probe().tokens, *probe().presented,
                [](int value) { return token(value); }),
            components::Text("two").searchable(
                *probe().query, *probe().tokens, context.state(suggested),
                *probe().presented, [](int value) { return token(value); }));
      }));
  frames(application, 3);
  EXPECT_TRUE(shows(application, "Tag 3"));
  probe().presented->set(true);
  settle(providers, application);
  EXPECT_TRUE(node_of(application, "arisa.search.cancel").has_value());
  EXPECT_TRUE(shows(application, "Tag 5"));
}

TEST_F(SearchParityTest, EditableTokensBindEachChip) {
  static std::optional<composition::Binding<int>> captured;
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers, host([](composition::Context&) {
        return components::VStack(
            components::Text("one").searchable(
                *probe().query, composition::EditableTokens{*probe().tokens},
                [](composition::Binding<int> each) {
                  captured = each;
                  return components::Text(std::format("Chip {}", each.get()));
                }),
            components::Text("two").searchable(
                *probe().query, composition::EditableTokens{*probe().tokens},
                *probe().presented, [](composition::Binding<int> each) {
                  return components::Text(std::format("Other {}", each.get()));
                }));
      }));
  frames(application, 3);
  EXPECT_TRUE(shows(application, "Chip 3"));
  EXPECT_TRUE(shows(application, "Other 3"));
  ASSERT_TRUE(captured.has_value());
  captured->set(13);
  settle(providers, application);
  EXPECT_EQ(probe().tokens->get(), std::vector<int>{13});
  EXPECT_TRUE(shows(application, "Chip 13"));
  EXPECT_TRUE(shows(application, "Other 13"));
}

TEST_F(SearchParityTest, AutomaticDrawerHidesOnScrollAlwaysStays) {
  const auto run = [](SearchFieldPlacement placement) {
    provider::headless::HeadlessProviderFactory providers;
    Application application =
        start(providers, host([placement](composition::Context&) {
                return rows().searchable(*probe().query, placement);
              }));
    frames(application, 3);
    const bool before = node_of(application, "arisa.search.bar").has_value();
    drag(providers, application, {.x = 640.0F, .y = 600.0F},
         {.x = 640.0F, .y = 200.0F}, 8);
    settle(providers, application);
    return std::pair{before,
                     node_of(application, "arisa.search.bar").has_value()};
  };
  const auto automatic = run(SearchFieldPlacement::navigation_bar_drawer);
  EXPECT_TRUE(automatic.first);
  EXPECT_FALSE(automatic.second);
  const auto always = run(SearchFieldPlacement::navigation_bar_drawer(
      SearchFieldPlacement::NavigationBarDrawerDisplayMode::always));
  EXPECT_TRUE(always.first);
  EXPECT_TRUE(always.second);
  const SearchFieldPlacement drawer =
      SearchFieldPlacement::navigation_bar_drawer;
  EXPECT_EQ(drawer.display_mode(),
            SearchFieldPlacement::NavigationBarDrawerDisplayMode::automatic);
}

TEST_F(SearchParityTest, ToolbarPrincipalAndMinimizeBehaviour) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers, host([](composition::Context&) {
              return stacked(
                  components::Text("body").navigation_title("Find").searchable(
                      *probe().query, SearchFieldPlacement::toolbar_principal));
            }));
  frames(application, 4);
  ASSERT_TRUE(node_of(application, "arisa.search.bar").has_value());
  const composition::Rect bar =
      frame_of(application, presentation::NavigationStack::bar_id);
  const composition::Rect field = frame_of(application, "arisa.search.bar");
  EXPECT_GE(field.y, bar.y);
  EXPECT_LE(field.y + field.height, bar.y + bar.height + 1.0F);
  EXPECT_LT(field.x + (field.width * 0.5F), bar.x + (bar.width * 0.5F));
  provider::headless::HeadlessProviderFactory other;
  Application minimized =
      start(other, host([](composition::Context&) {
              return stacked(
                  components::Text("body")
                      .navigation_title("Find")
                      .searchable(*probe().query, SearchFieldPlacement::toolbar)
                      .search_toolbar_behavior(
                          composition::SearchToolbarBehavior::minimize));
            }));
  frames(minimized, 4);
  EXPECT_FALSE(node_of(minimized, "arisa.search.bar").has_value());
  ASSERT_TRUE(node_of(minimized, "arisa.search.button").has_value());
  ASSERT_TRUE(minimized.root().activate(
      composition::ElementKey{"arisa.search.button"}));
  settle(other, minimized);
  EXPECT_TRUE(node_of(minimized, "arisa.search.bar").has_value());
}

TEST_F(SearchParityTest, SidebarPlacementMovesTheFieldIntoTheSplitSidebar) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers, host([](composition::Context&) {
              return Element{structure::NavigationSplitView(
                                 components::Text("Sidebar rows"),
                                 components::Text("Detail page"))}
                  .searchable(*probe().query, SearchFieldPlacement::sidebar);
            }));
  frames(application, 4);
  ASSERT_TRUE(node_of(application, "arisa.split.search").has_value());
  const composition::Rect sidebar =
      frame_of(application, structure::NavigationSplitView::sidebar_id);
  const composition::Rect field = frame_of(application, "arisa.search.bar");
  EXPECT_GE(field.x, sidebar.x - 1.0F);
  EXPECT_LE(field.x + field.width, sidebar.x + sidebar.width + 1.0F);
}

TEST_F(SearchParityTest, SuggestionsFollowTheirPlacementVisibility) {
  const auto run = [](composition::Visibility visibility,
                      composition::SearchSuggestionsPlacement::Set set) {
    provider::headless::HeadlessProviderFactory providers;
    probe().placement.reset();
    Application application = start(
        providers, host([visibility, set](composition::Context&) {
          return components::Text("body")
              .searchable(*probe().query, *probe().presented)
              .search_suggestions([] { return Element{PlacementReader{}}; })
              .search_suggestions(visibility, set);
        }));
    probe().presented->set(true);
    settle(providers, application);
    return node_of(application, "suggested").has_value();
  };
  EXPECT_TRUE(run(composition::Visibility::automatic,
                  composition::SearchSuggestionsPlacement::Set::all));
  EXPECT_EQ(probe().placement,
            std::optional<composition::SearchSuggestionsPlacement>{
                composition::SearchSuggestionsPlacement::content});
  EXPECT_FALSE(run(composition::Visibility::hidden,
                   composition::SearchSuggestionsPlacement::Set::content));
  EXPECT_TRUE(run(composition::Visibility::hidden,
                  composition::SearchSuggestionsPlacement::Set::menu));
  const auto both = composition::SearchSuggestionsPlacement::Set::menu |
                    composition::SearchSuggestionsPlacement::Set::content;
  EXPECT_EQ(both, composition::SearchSuggestionsPlacement::Set::all);
  EXPECT_TRUE(both.contains(composition::SearchSuggestionsPlacement::menu));
  EXPECT_FALSE(composition::SearchSuggestionsPlacement::Set::menu.contains(
      composition::SearchSuggestionsPlacement::content));
}

TEST_F(SearchParityTest, ToolbarSuggestionsUseTheMenu) {
  provider::headless::HeadlessProviderFactory providers;
  probe().placement.reset();
  Application application = start(
      providers, host([](composition::Context&) {
        return stacked(
            components::Text("body")
                .navigation_title("Find")
                .searchable(*probe().query, *probe().presented,
                            SearchFieldPlacement::toolbar)
                .search_suggestions([] { return Element{PlacementReader{}}; }));
      }));
  probe().presented->set(true);
  settle(providers, application);
  EXPECT_TRUE(node_of(application, "suggested").has_value());
  EXPECT_EQ(probe().placement,
            std::optional<composition::SearchSuggestionsPlacement>{
                composition::SearchSuggestionsPlacement::menu});
}

}
