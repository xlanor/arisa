#include <arisa/components/button.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/command_hint.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/dismiss_search_action.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/focus_state.hpp>
#include <arisa/composition/search_field_placement.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/submit_triggers.hpp>
#include <arisa/controls/searchable.hpp>
#include <arisa/provider/error.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>

#include <scroll_fixture.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <expected>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace arisa::runtime {

using composition::Element;
using provider::Button;

class SearchTest : public ScrollFixture {
 protected:
  struct Probe {
    std::optional<composition::StateHandle<std::string>> query;
    std::optional<composition::StateHandle<std::vector<int>>> tokens;
    std::optional<composition::FocusBinding<bool>> focus;
    std::vector<std::string> seen;
    bool searching{false};
    composition::DismissSearchAction dismiss;
    int searches{0};
    int texts{0};
  };

  using Build = std::function<Element(std::shared_ptr<Probe>, Element)>;

  class Watcher final : public composition::Component {
   public:
    explicit Watcher(std::shared_ptr<Probe> probe) noexcept
        : probe_(std::move(probe)) {}

    auto operator==(const Watcher& other) const -> bool {
      return probe_ == other.probe_;
    }

    auto body(composition::Context& context) const -> Element override {
      probe_->searching =
          context.environment(components::Environment::is_searching);
      probe_->dismiss =
          context.environment(components::Environment::dismiss_search);
      return components::Text(probe_->searching ? "searching" : "idle")
          .id("watcher");
    }

   private:
    std::shared_ptr<Probe> probe_;
  };

  class Host final : public composition::Component {
   public:
    Host(std::shared_ptr<Probe> probe,
         std::shared_ptr<const Build> build) noexcept
        : probe_(std::move(probe)), build_(std::move(build)) {}

    auto operator==(const Host& other) const -> bool {
      return probe_ == other.probe_;
    }

    auto body(composition::Context& context) const -> Element override {
      const auto query = context.state(query_key);
      probe_->query = query;
      probe_->tokens = context.state(tokens_key);
      probe_->focus = context.state(focus_key);
      if (probe_->seen.empty() || probe_->seen.back() != query.get()) {
        probe_->seen.push_back(query.get());
      }
      Element content = components::VStack(
          Element{Watcher{probe_}},
          components::Button("Elsewhere", [] {}).id("elsewhere"));
      return (*build_)(probe_, std::move(content))
          .on_submit(composition::SubmitTriggers::search,
                     [probe = probe_] { ++probe->searches; })
          .on_submit([probe = probe_] { ++probe->texts; });
    }

   private:
    std::shared_ptr<Probe> probe_;
    std::shared_ptr<const Build> build_;
  };

  static auto plain() -> Build {
    return [](std::shared_ptr<Probe> probe, Element content) {
      return std::move(content).searchable(*probe->query);
    };
  }

  static auto host(std::shared_ptr<Probe> probe, Build build) -> Element {
    return Host{std::move(probe),
                std::make_shared<const Build>(std::move(build))};
  }

  static auto hinted(Application& application, Button button,
                     std::string_view label) -> bool {
    return std::ranges::any_of(
        application.interaction().hints(),
        [button, label](const composition::CommandHint& hint) {
          return hint.button == button && hint.label == label;
        });
  }

  static constexpr composition::StateKey<std::string> query_key{"query",
                                                                std::string{}};
  static const composition::StateKey<std::vector<int>> tokens_key;
  static constexpr composition::FocusState<bool> focus_key{"search-focus"};
  static constexpr composition::StateKey<int> scope_key{"scope", 0};
};

const composition::StateKey<std::vector<int>> SearchTest::tokens_key{
    "tokens", std::vector<int>{}};

TEST_F(SearchTest, TheFieldFiltersLiveFromKeyboardDraftsAndSubmitsASearch) {
  provider::headless::HeadlessProviderFactory providers;
  auto probe = std::make_shared<Probe>();
  Application application = start(providers, host(probe, plain()));
  EXPECT_EQ(focused_key(application), "arisa.search.field");
  providers.text_input().push(std::string{"star"},
                              std::vector<std::string>{"s", "st", "sta"});
  press(providers, application, Button::a);
  frames(application, 12);
  EXPECT_EQ(probe->seen,
            (std::vector<std::string>{"", "s", "st", "sta", "star"}));
  ASSERT_EQ(providers.text_input().requests().size(), 1U);
  EXPECT_TRUE(providers.text_input().requests().front().live);
  EXPECT_EQ(providers.text_input().requests().front().traits.submit_label,
            provider::SubmitLabel::search);
  EXPECT_EQ(probe->searches, 1);
  EXPECT_EQ(probe->texts, 0);
  EXPECT_TRUE(probe->searching);
}

TEST_F(SearchTest, IsSearchingFollowsTheFieldAndDismissSearchClearsIt) {
  provider::headless::HeadlessProviderFactory providers;
  auto probe = std::make_shared<Probe>();
  Application application = start(providers, host(probe, plain()));
  EXPECT_FALSE(probe->searching);
  EXPECT_FALSE(node_of(application, "arisa.search.cancel").has_value());
  providers.text_input().push(std::string{"kart"});
  press(providers, application, Button::a);
  frames(application, 8);
  EXPECT_TRUE(probe->searching);
  EXPECT_EQ(probe->query->get(), "kart");
  EXPECT_TRUE(node_of(application, "arisa.search.cancel").has_value());
  EXPECT_TRUE(hinted(application, Button::b, "Cancel"));
  ASSERT_TRUE(probe->dismiss.available());
  probe->dismiss();
  frames(application, 4);
  EXPECT_FALSE(probe->searching);
  EXPECT_EQ(probe->query->get(), "");
  EXPECT_FALSE(node_of(application, "arisa.search.cancel").has_value());
}

TEST_F(SearchTest, TheCancelButtonAndBEndTheSearch) {
  provider::headless::HeadlessProviderFactory providers;
  auto probe = std::make_shared<Probe>();
  Application application = start(providers, host(probe, plain()));
  providers.text_input().push(std::string{"kart"});
  press(providers, application, Button::a);
  frames(application, 8);
  tap(providers, application, centre(application, "arisa.search.cancel"));
  frames(application, 4);
  EXPECT_FALSE(probe->searching);
  providers.text_input().push(std::string{"puzzle"});
  press(providers, application, Button::a);
  frames(application, 8);
  EXPECT_TRUE(probe->searching);
  press(providers, application, Button::b);
  frames(application, 4);
  EXPECT_FALSE(probe->searching);
  EXPECT_EQ(probe->query->get(), "");
}

TEST_F(SearchTest, CancellingTheKeyboardRestoresTheTextBeforeTheEdit) {
  provider::headless::HeadlessProviderFactory providers;
  auto probe = std::make_shared<Probe>();
  Application application = start(providers, host(probe, plain()));
  probe->query->set("ab");
  frames(application, 2);
  ASSERT_EQ(probe->query->get(), "ab");
  providers.text_input().push(std::unexpected(provider::ProviderError{
                                  provider::ProviderErrorCode::cancelled}),
                              std::vector<std::string>{"abx", "abxy"});
  press(providers, application, Button::a);
  frames(application, 10);
  EXPECT_NE(std::ranges::find(probe->seen, "abxy"), probe->seen.end());
  EXPECT_EQ(probe->query->get(), "ab");
  EXPECT_EQ(probe->searches, 0);
}

TEST_F(SearchTest, ScopesShowWhileSearchingAndBindTheirSelection) {
  provider::headless::HeadlessProviderFactory providers;
  auto probe = std::make_shared<Probe>();
  auto scope = std::make_shared<std::optional<composition::StateHandle<int>>>();
  class Scoped final : public composition::Component {
   public:
    Scoped(std::shared_ptr<Probe> probe,
           std::shared_ptr<std::optional<composition::StateHandle<int>>> scope)
        : probe_(std::move(probe)), scope_(std::move(scope)) {}

    auto operator==(const Scoped& other) const -> bool {
      return probe_ == other.probe_;
    }

    auto body(composition::Context& context) const -> Element override {
      const auto chosen = context.state(scope_key);
      *scope_ = chosen;
      return host(
          probe_, [chosen](std::shared_ptr<Probe> seen, Element content) {
            return std::move(content)
                .searchable(*seen->query)
                .search_scopes(chosen, [] {
                  return Element{components::Group(
                      components::Text("All").tag(0),
                      components::Text("Installed").tag(1).id("installed"))};
                });
          });
    }

   private:
    std::shared_ptr<Probe> probe_;
    std::shared_ptr<std::optional<composition::StateHandle<int>>> scope_;
  };
  Application application = start(providers, Scoped{probe, scope});
  EXPECT_FALSE(node_of(application, "arisa.search.scopes").has_value());
  providers.text_input().push(std::string{""});
  press(providers, application, Button::a);
  frames(application, 8);
  ASSERT_TRUE(node_of(application, "arisa.search.scopes").has_value());
  tap(providers, application, centre(application, "installed"));
  frames(application, 4);
  EXPECT_EQ((*scope)->get(), 1);
}

TEST_F(SearchTest, CompletionsFillTheTextAndTokenCompletionsAddChips) {
  provider::headless::HeadlessProviderFactory providers;
  auto probe = std::make_shared<Probe>();
  Application application = start(
      providers, host(probe, [](std::shared_ptr<Probe> seen, Element content) {
        return std::move(content)
            .searchable(*seen->query, *seen->tokens,
                        [](int token) {
                          return components::Text(std::format("Tag {}", token));
                        })
            .search_suggestions([] {
              return components::VStack(
                  components::Text("Star").search_completion("Star").id(
                      "suggest-text"),
                  components::Text("Seven").search_completion(7).id(
                      "suggest-token"));
            });
      }));
  EXPECT_FALSE(node_of(application, "suggest-text").has_value());
  providers.text_input().push(std::string{""});
  press(providers, application, Button::a);
  frames(application, 8);
  ASSERT_TRUE(node_of(application, "suggest-text").has_value());
  const int before = probe->searches;
  tap(providers, application, centre(application, "suggest-text"));
  frames(application, 4);
  EXPECT_EQ(probe->query->get(), "Star");
  EXPECT_EQ(probe->searches, before + 1);
  tap(providers, application, centre(application, "suggest-token"));
  frames(application, 4);
  EXPECT_EQ(probe->tokens->get(), (std::vector<int>{7}));
  EXPECT_EQ(probe->query->get(), "");
  EXPECT_TRUE(shows(application, "Tag 7"));
}

TEST_F(SearchTest, SuggestedTokensAppearWhileSearchingAndMinusRemovesTheLast) {
  provider::headless::HeadlessProviderFactory providers;
  auto probe = std::make_shared<Probe>();
  static const composition::StateKey<std::vector<int>> offered{
      "offered", std::vector<int>{1, 2, 3}};
  class Offering final : public composition::Component {
   public:
    explicit Offering(std::shared_ptr<Probe> probe)
        : probe_(std::move(probe)) {}

    auto operator==(const Offering& other) const -> bool {
      return probe_ == other.probe_;
    }

    auto body(composition::Context& context) const -> Element override {
      const auto suggested = context.state(offered);
      return host(probe_,
                  [suggested](std::shared_ptr<Probe> seen, Element content) {
                    return std::move(content).searchable(
                        *seen->query, *seen->tokens, suggested, [](int token) {
                          return components::Text(std::format("Tag {}", token));
                        });
                  });
    }

   private:
    std::shared_ptr<Probe> probe_;
  };
  Application application = start(providers, Offering{probe});
  providers.text_input().push(std::string{""});
  press(providers, application, Button::a);
  frames(application, 8);
  ASSERT_TRUE(node_of(application, "arisa.search.tokens").has_value());
  EXPECT_TRUE(shows(application, "Tag 2"));
  probe->tokens->set(std::vector<int>{1, 2});
  frames(application, 4);
  EXPECT_TRUE(hinted(application, Button::minus, "Delete"));
  press(providers, application, Button::minus);
  frames(application, 4);
  EXPECT_EQ(probe->tokens->get(), (std::vector<int>{1}));
}

TEST_F(SearchTest, TheClearCommandEmptiesTheField) {
  provider::headless::HeadlessProviderFactory providers;
  auto probe = std::make_shared<Probe>();
  Application application = start(providers, host(probe, plain()));
  probe->query->set("abc");
  frames(application, 3);
  ASSERT_EQ(probe->query->get(), "abc");
  EXPECT_TRUE(hinted(application, Button::x, "Clear"));
  press(providers, application, Button::x);
  frames(application, 3);
  EXPECT_EQ(probe->query->get(), "");
  EXPECT_FALSE(hinted(application, Button::x, "Clear"));
}

TEST_F(SearchTest, SearchFocusedMovesFocusToTheField) {
  provider::headless::HeadlessProviderFactory providers;
  auto probe = std::make_shared<Probe>();
  Application application = start(
      providers, host(probe, [](std::shared_ptr<Probe> seen, Element content) {
        return std::move(content)
            .searchable(*seen->query)
            .search_focused(*seen->focus);
      }));
  press(providers, application, Button::dpad_down);
  frames(application, 3);
  EXPECT_EQ(focused_key(application), "elsewhere");
  probe->focus->set(true);
  frames(application, 4);
  EXPECT_EQ(focused_key(application), "arisa.search.field");
}

}
