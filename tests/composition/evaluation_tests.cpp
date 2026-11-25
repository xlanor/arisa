#include <arisa/components/button.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/label.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/composition/retained.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/tree.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::composition {

class EvaluationTest : public ::testing::Test {
 protected:
  class Calls {
   public:
    void record(std::string_view name) { ++counts_[std::string{name}]; }

    auto count(std::string_view name) const -> int {
      const auto found = counts_.find(name);
      return found == counts_.end() ? 0 : found->second;
    }

   private:
    std::map<std::string, int, std::less<>> counts_;
  };

  class Probe final : public Component {
   public:
    Probe(std::string name, std::shared_ptr<Calls> calls)
        : name_(std::move(name)), calls_(std::move(calls)) {}

    auto operator==(const Probe&) const -> bool = default;

    auto body(Context&) const -> Element override {
      calls_->record(name_);
      return components::Text(name_);
    }

   private:
    std::string name_;
    std::shared_ptr<Calls> calls_;
  };

  class Stateful final : public Component {
   public:
    static constexpr StateKey<int> clicks{"clicks", 0};

    Stateful(std::string name, std::shared_ptr<Calls> calls)
        : name_(std::move(name)), calls_(std::move(calls)) {}

    auto operator==(const Stateful&) const -> bool = default;

    auto body(Context& context) const -> Element override {
      calls_->record(name_);
      const auto count = context.state(clicks);
      return components::VStack(
          components::Button(name_, [count] { count.set(count.get() + 1); })
              .id(name_),
          Probe{std::format("{}.child", name_), calls_});
    }

   private:
    std::string name_;
    std::shared_ptr<Calls> calls_;
  };

  class Parent final : public Component {
   public:
    static constexpr StateKey<int> revision{"revision", 0};

    explicit Parent(std::shared_ptr<Calls> calls) : calls_(std::move(calls)) {}

    auto operator==(const Parent&) const -> bool = default;

    auto body(Context& context) const -> Element override {
      calls_->record("parent");
      const auto current = context.state(revision);
      return components::VStack(
          components::Button(
              "bump",
              [current] { current.update([](int& value) { ++value; }); })
              .id("bump"),
          Probe{"fixed", calls_},
          Probe{std::format("varying{}", current.get()), calls_},
          Stateful{"left", calls_}, Stateful{"right", calls_});
    }

   private:
    std::shared_ptr<Calls> calls_;
  };

  class Reader final : public Component {
   public:
    Reader(std::shared_ptr<Observable<int>> source,
           std::shared_ptr<Observable<bool>> enabled,
           std::shared_ptr<Calls> calls)
        : source_(std::move(source)),
          enabled_(std::move(enabled)),
          calls_(std::move(calls)) {}

    auto operator==(const Reader&) const -> bool = default;

    auto body(Context& context) const -> Element override {
      calls_->record("reader");
      if (!context.read(*enabled_)) {
        return components::Text("off");
      }
      return components::Text(std::format("value {}", context.read(*source_)));
    }

   private:
    std::shared_ptr<Observable<int>> source_;
    std::shared_ptr<Observable<bool>> enabled_;
    std::shared_ptr<Calls> calls_;
  };

  class Host final : public Component {
   public:
    Host(std::shared_ptr<Observable<int>> source,
         std::shared_ptr<Observable<bool>> enabled,
         std::shared_ptr<Observable<bool>> present,
         std::shared_ptr<Calls> calls)
        : source_(std::move(source)),
          enabled_(std::move(enabled)),
          present_(std::move(present)),
          calls_(std::move(calls)) {}

    auto operator==(const Host&) const -> bool = default;

    auto body(Context& context) const -> Element override {
      calls_->record("host");
      if (!context.read(*present_)) {
        return components::VStack(Probe{"bystander", calls_});
      }
      return components::VStack(Reader{source_, enabled_, calls_},
                                Probe{"bystander", calls_});
    }

   private:
    std::shared_ptr<Observable<int>> source_;
    std::shared_ptr<Observable<bool>> enabled_;
    std::shared_ptr<Observable<bool>> present_;
    std::shared_ptr<Calls> calls_;
  };

  static constexpr EnvironmentKey<int> theme{"theme", -1};

  class Leaf final : public Component {
   public:
    Leaf(std::string name, std::shared_ptr<Calls> calls,
         std::shared_ptr<std::map<std::string, int>> seen)
        : name_(std::move(name)),
          calls_(std::move(calls)),
          seen_(std::move(seen)) {}

    auto operator==(const Leaf&) const -> bool = default;

    auto body(Context& context) const -> Element override {
      calls_->record(name_);
      (*seen_)[name_] = context.environment(theme);
      return components::Text(name_);
    }

   private:
    std::string name_;
    std::shared_ptr<Calls> calls_;
    std::shared_ptr<std::map<std::string, int>> seen_;
  };

  class Middle final : public Component {
   public:
    Middle(std::shared_ptr<Calls> calls,
           std::shared_ptr<std::map<std::string, int>> seen)
        : calls_(std::move(calls)), seen_(std::move(seen)) {}

    auto operator==(const Middle&) const -> bool = default;

    auto body(Context&) const -> Element override {
      calls_->record("middle");
      return components::VStack(
          Leaf{"open", calls_, seen_},
          components::VStack(Leaf{"shadowed", calls_, seen_})
              .environment(theme, 99));
    }

   private:
    std::shared_ptr<Calls> calls_;
    std::shared_ptr<std::map<std::string, int>> seen_;
  };

  class Themed final : public Component {
   public:
    Themed(std::shared_ptr<Observable<int>> value, std::shared_ptr<Calls> calls,
           std::shared_ptr<std::map<std::string, int>> seen)
        : value_(std::move(value)),
          calls_(std::move(calls)),
          seen_(std::move(seen)) {}

    auto operator==(const Themed&) const -> bool = default;

    auto body(Context& context) const -> Element override {
      calls_->record("themed");
      const int value = context.read(*value_);
      return components::VStack(Middle{calls_, seen_},
                                Leaf{"outside", calls_, seen_})
          .environment(theme, value);
    }

   private:
    std::shared_ptr<Observable<int>> value_;
    std::shared_ptr<Calls> calls_;
    std::shared_ptr<std::map<std::string, int>> seen_;
  };

  class Kept final : public Component {
   public:
    static constexpr StateKey<int> phase{"kept-phase", 0};
    static constexpr std::array<std::string_view, 3> names{"a", "b", "z"};

    explicit Kept(std::shared_ptr<Calls> calls) : calls_(std::move(calls)) {}

    auto operator==(const Kept&) const -> bool = default;

    auto body(Context& context) const -> Element override {
      const auto current = context.state(phase);
      std::vector<Element> children;
      children.push_back(components::Button("advance", [current] {
                           current.update([](int& value) { ++value; });
                         }).id("advance"));
      if (current.get() == 0) {
        for (const std::string_view name : {"a", "b", "c"}) {
          children.push_back(
              Element{Probe{std::string{name}, calls_}}.id(ElementKey{name}));
        }
      } else {
        children.push_back(Retained{
            names.size(),
            [](std::size_t index) { return ElementKey{names[index]}; },
            [calls = calls_](std::size_t index) {
              calls->record("built");
              return Element{Probe{std::string{names[index]}, calls}}.id(
                  ElementKey{names[index]});
            }});
      }
      return components::VStack(components::VStackOptions{},
                                std::move(children));
    }

   private:
    std::shared_ptr<Calls> calls_;
  };

  class Rows final : public Component {
   public:
    static constexpr StateKey<int> revision{"rows-revision", 0};

    explicit Rows(std::shared_ptr<std::vector<int>> fired)
        : fired_(std::move(fired)) {}

    auto operator==(const Rows&) const -> bool = default;

    auto body(Context& context) const -> Element override {
      const auto current = context.state(revision);
      const int captured = current.get();
      return components::VStack(
          components::Button(
              [fired = fired_, captured] { fired->push_back(captured); },
              components::Label("Row", components::Image::system_name("star")))
              .id("row"),
          components::Button(
              [fired = fired_, captured] { fired->push_back(-captured); },
              components::Text("fixed"))
              .id("plain"),
          components::Button(
              [] {}, components::Text(std::format("revision {}", captured)))
              .id("titled"),
          components::Button("bump", [current] {
            current.update([](int& value) { ++value; });
          }).id("bump"));
    }

   private:
    std::shared_ptr<std::vector<int>> fired_;
  };

  class Draft final : public Component {
   public:
    inline static const StateKey<std::string> text{"draft", "unseeded"};

    explicit Draft(std::string seed) : seed_(std::move(seed)) {}

    auto operator==(const Draft&) const -> bool = default;

    auto body(Context& context) const -> Element override {
      const auto draft = context.state(text, seed_);
      return components::Text(draft.get());
    }

   private:
    std::string seed_;
  };

  class Editor final : public Component {
   public:
    Editor(std::shared_ptr<Observable<std::string>> seed,
           std::shared_ptr<Observable<int>> identity)
        : seed_(std::move(seed)), identity_(std::move(identity)) {}

    auto operator==(const Editor&) const -> bool = default;

    auto body(Context& context) const -> Element override {
      return components::VStack(Element{Draft{context.read(*seed_)}}.id(
          static_cast<std::int64_t>(context.read(*identity_))));
    }

   private:
    std::shared_ptr<Observable<std::string>> seed_;
    std::shared_ptr<Observable<int>> identity_;
  };

  static auto owner() -> Tree::OwnerCheck {
    return [] { return true; };
  }

  static auto shows(const Node& node, std::string_view wanted) -> bool {
    if (const auto text = node.content_as<components::Text::Content>();
        text.has_value() && text->get().text() == wanted) {
      return true;
    }
    return std::ranges::any_of(node.children(), [wanted](const Node& child) {
      return shows(child, wanted);
    });
  }

  std::shared_ptr<Calls> calls_{std::make_shared<Calls>()};
};

TEST_F(EvaluationTest, SkipsComponentsWhosePropsAreEqual) {
  Tree tree{Parent{calls_}, owner()};
  tree.reconcile();
  EXPECT_EQ(calls_->count("parent"), 1);
  EXPECT_EQ(calls_->count("fixed"), 1);
  EXPECT_EQ(calls_->count("varying0"), 1);
  ASSERT_TRUE(tree.activate(*tree.find("bump")));
  EXPECT_TRUE(tree.dirty());
  tree.reconcile();
  EXPECT_EQ(calls_->count("parent"), 2);
  EXPECT_EQ(calls_->count("fixed"), 1);
  EXPECT_EQ(calls_->count("varying1"), 1);
  EXPECT_EQ(calls_->count("left"), 1);
  EXPECT_EQ(calls_->count("right"), 1);
  EXPECT_FALSE(tree.dirty());
}

TEST_F(EvaluationTest, RetainedRunsKeepTheirNodesAndBuildOnlyTheMissing) {
  Tree tree{Kept{calls_}, owner()};
  tree.reconcile();
  const auto first = tree.find(ElementKey{"a"});
  const auto second = tree.find(ElementKey{"b"});
  ASSERT_TRUE(first.has_value() && second.has_value());
  ASSERT_TRUE(tree.activate(*tree.find("advance")));
  tree.reconcile();
  EXPECT_EQ(tree.find(ElementKey{"a"}), first);
  EXPECT_EQ(tree.find(ElementKey{"b"}), second);
  EXPECT_EQ(calls_->count("built"), 1);
  EXPECT_EQ(calls_->count("a"), 1);
  EXPECT_EQ(calls_->count("z"), 1);
  EXPECT_TRUE(shows(tree.root(), "a"));
  EXPECT_TRUE(shows(tree.root(), "z"));
  ASSERT_TRUE(tree.activate(*tree.find("advance")));
  tree.reconcile();
  EXPECT_EQ(calls_->count("built"), 1);
  EXPECT_EQ(tree.find(ElementKey{"a"}), first);
}

TEST_F(EvaluationTest, UnchangedBuiltInsSkipWhileTheNewestActionFires) {
  auto fired = std::make_shared<std::vector<int>>();
  Tree tree{Rows{fired}, owner()};
  tree.reconcile();
  ASSERT_TRUE(tree.activate(*tree.find("row")));
  EXPECT_EQ(*fired, std::vector<int>{0});
  const std::uint64_t before = tree.evaluations();
  ASSERT_TRUE(tree.activate(*tree.find("bump")));
  tree.reconcile();
  EXPECT_EQ(tree.evaluations() - before, 1U);
  ASSERT_TRUE(tree.activate(*tree.find("row")));
  ASSERT_TRUE(tree.activate(*tree.find("plain")));
  EXPECT_EQ(*fired, (std::vector<int>{0, 1, -1}));
  EXPECT_TRUE(shows(tree.root(), "revision 1"));
  EXPECT_FALSE(shows(tree.root(), "revision 0"));
  ASSERT_TRUE(tree.activate(*tree.find("bump")));
  tree.reconcile();
  ASSERT_TRUE(tree.activate(*tree.find("row")));
  EXPECT_EQ(fired->back(), 2);
  EXPECT_TRUE(shows(tree.root(), "revision 2"));
  EXPECT_FALSE(tree.dirty());
}

TEST_F(EvaluationTest, StateWriteReevaluatesOnlyThatSubtree) {
  Tree tree{Parent{calls_}, owner()};
  tree.reconcile();
  ASSERT_TRUE(tree.activate(*tree.find("left")));
  ASSERT_TRUE(tree.activate(*tree.find("left")));
  tree.reconcile();
  EXPECT_EQ(calls_->count("parent"), 1);
  EXPECT_EQ(calls_->count("left"), 2);
  EXPECT_EQ(calls_->count("left.child"), 1);
  EXPECT_EQ(calls_->count("right"), 1);
  EXPECT_EQ(calls_->count("fixed"), 1);
  tree.reconcile();
  EXPECT_EQ(calls_->count("left"), 2);
}

TEST_F(EvaluationTest, ObservableReadsAreTrackedAndReleased) {
  auto source = std::make_shared<Observable<int>>(1);
  auto enabled = std::make_shared<Observable<bool>>(true);
  auto present = std::make_shared<Observable<bool>>(true);
  Tree tree{Host{source, enabled, present, calls_}, owner()};
  tree.reconcile();
  EXPECT_EQ(source->subscribers(), 1U);
  EXPECT_EQ(calls_->count("reader"), 1);

  source->set(2);
  EXPECT_TRUE(tree.dirty());
  tree.reconcile();
  EXPECT_EQ(calls_->count("reader"), 2);
  EXPECT_EQ(calls_->count("host"), 1);
  EXPECT_EQ(calls_->count("bystander"), 1);

  source->set(2);
  EXPECT_FALSE(tree.dirty());

  enabled->set(false);
  tree.reconcile();
  EXPECT_EQ(calls_->count("reader"), 3);
  EXPECT_EQ(source->subscribers(), 0U);
  source->set(3);
  EXPECT_FALSE(tree.dirty());

  enabled->set(true);
  tree.reconcile();
  EXPECT_EQ(source->subscribers(), 1U);
  EXPECT_EQ(enabled->subscribers(), 1U);

  present->set(false);
  tree.reconcile();
  EXPECT_EQ(source->subscribers(), 0U);
  EXPECT_EQ(enabled->subscribers(), 0U);
  EXPECT_EQ(present->subscribers(), 1U);
}

TEST(ObservableTest, SubscriptionsSurviveEitherSideDyingFirst) {
  int heard = 0;
  std::optional<Subscription> outliving;
  {
    Observable<int> source{0};
    const ObservableVersion version = source.version();
    std::optional<Subscription> early{source.subscribe([&heard] { ++heard; })};
    outliving.emplace(source.subscribe([&heard] { heard += 10; }));
    EXPECT_EQ(source.subscribers(), 2U);
    source.set(1);
    EXPECT_EQ(heard, 11);
    EXPECT_NE(source.version(), version);
    early.reset();
    EXPECT_EQ(source.subscribers(), 1U);
    source.update([](int& value) { value += 1; });
    EXPECT_EQ(heard, 21);
  }
  outliving.reset();
  EXPECT_EQ(heard, 21);
}

TEST(ObservableTest, ListenersMayUnsubscribeWhilePublishing) {
  Observable<int> source{0};
  int heard = 0;
  std::optional<Subscription> second;
  std::optional<Subscription> first{source.subscribe([&] {
    ++heard;
    second.reset();
  })};
  second.emplace(source.subscribe([&heard] { heard += 100; }));
  source.set(1);
  EXPECT_EQ(heard, 1);
  EXPECT_EQ(source.subscribers(), 1U);
  source.set(2);
  EXPECT_EQ(heard, 2);
}

TEST_F(EvaluationTest, EnvironmentOverridesFlowDownAndPropagateChanges) {
  auto value = std::make_shared<Observable<int>>(1);
  auto seen = std::make_shared<std::map<std::string, int>>();
  Tree tree{Themed{value, calls_, seen}, owner()};
  tree.reconcile();
  EXPECT_EQ(seen->at("open"), 1);
  EXPECT_EQ(seen->at("outside"), 1);
  EXPECT_EQ(seen->at("shadowed"), 99);

  value->set(7);
  tree.reconcile();
  EXPECT_EQ(calls_->count("themed"), 2);
  EXPECT_EQ(calls_->count("middle"), 1);
  EXPECT_EQ(calls_->count("open"), 2);
  EXPECT_EQ(calls_->count("outside"), 2);
  EXPECT_EQ(calls_->count("shadowed"), 1);
  EXPECT_EQ(seen->at("open"), 7);
  EXPECT_EQ(seen->at("outside"), 7);
  EXPECT_EQ(seen->at("shadowed"), 99);
  EXPECT_FALSE(tree.dirty());
}

TEST_F(EvaluationTest, EnvironmentFallsBackToTheKeyDefault) {
  auto seen = std::make_shared<std::map<std::string, int>>();
  Tree tree{Leaf{"bare", calls_, seen}, owner()};
  tree.reconcile();
  EXPECT_EQ(seen->at("bare"), -1);
  EXPECT_EQ(theme.name(), "theme");
}

class LifecycleHookTest : public ::testing::Test {
 protected:
  using Log = std::vector<std::string>;

  class Branch final : public Component {
   public:
    Branch(std::string name, std::shared_ptr<Log> log)
        : name_(std::move(name)), log_(std::move(log)) {}

    auto operator==(const Branch&) const -> bool = default;

    auto body(Context&) const -> Element override {
      return components::VStack(components::Text(name_ + ".leaf")
                                    .on_appear([log = log_, name = name_] {
                                      log->push_back("+" + name + ".leaf");
                                    })
                                    .on_disappear([log = log_, name = name_] {
                                      log->push_back("-" + name + ".leaf");
                                    }));
    }

   private:
    std::string name_;
    std::shared_ptr<Log> log_;
  };

  class Scene final : public Component {
   public:
    Scene(std::shared_ptr<Observable<bool>> swapped, std::shared_ptr<Log> log)
        : swapped_(std::move(swapped)), log_(std::move(log)) {}

    auto operator==(const Scene&) const -> bool = default;

    auto body(Context& context) const -> Element override {
      const std::string name = context.read(*swapped_) ? "b" : "a";
      return components::VStack(
          Element{Branch{name, log_}}
              .id(name)
              .on_appear([log = log_, name] { log->push_back("+" + name); })
              .on_disappear(
                  [log = log_, name] { log->push_back("-" + name); }));
    }

   private:
    std::shared_ptr<Observable<bool>> swapped_;
    std::shared_ptr<Log> log_;
  };
};

TEST_F(LifecycleHookTest, AppearsParentsFirstAndDisappearsBeforeAppearing) {
  auto swapped = std::make_shared<Observable<bool>>(false);
  auto log = std::make_shared<Log>();
  Tree tree{Scene{swapped, log}, [] { return true; }};
  tree.reconcile();
  EXPECT_EQ(*log, (Log{"+a", "+a.leaf"}));
  log->clear();
  swapped->set(true);
  tree.reconcile();
  EXPECT_EQ(*log, (Log{"-a", "-a.leaf", "+b", "+b.leaf"}));
}

class StateHandleTest : public ::testing::Test {
 protected:
  class Holder final : public Component {
   public:
    static constexpr StateKey<int> value{"value", 5};

    explicit Holder(std::shared_ptr<std::optional<StateHandle<int>>> escaped)
        : escaped_(std::move(escaped)) {}

    auto operator==(const Holder&) const -> bool = default;

    auto body(Context& context) const -> Element override {
      escaped_->emplace(context.state(value));
      return components::Text(std::format("{}", (*escaped_)->get()));
    }

   private:
    std::shared_ptr<std::optional<StateHandle<int>>> escaped_;
  };

  class Gate final : public Component {
   public:
    Gate(std::shared_ptr<Observable<bool>> open,
         std::shared_ptr<std::optional<StateHandle<int>>> escaped)
        : open_(std::move(open)), escaped_(std::move(escaped)) {}

    auto operator==(const Gate&) const -> bool = default;

    auto body(Context& context) const -> Element override {
      if (context.read(*open_)) {
        return components::VStack(Holder{escaped_});
      }
      return components::VStack();
    }

   private:
    std::shared_ptr<Observable<bool>> open_;
    std::shared_ptr<std::optional<StateHandle<int>>> escaped_;
  };
};

TEST_F(StateHandleTest, StartsAtTheDeclaredInitialAndSkipsEqualWrites) {
  auto open = std::make_shared<Observable<bool>>(true);
  auto escaped = std::make_shared<std::optional<StateHandle<int>>>();
  Tree tree{Gate{open, escaped}, [] { return true; }};
  tree.reconcile();
  ASSERT_TRUE(escaped->has_value());
  EXPECT_EQ((*escaped)->get(), 5);
  EXPECT_EQ(Holder::value.name(), "value");
  (*escaped)->set(5);
  EXPECT_FALSE(tree.dirty());
  (*escaped)->set(6);
  EXPECT_TRUE(tree.dirty());
  tree.reconcile();
  EXPECT_EQ((*escaped)->get(), 6);
}

TEST_F(StateHandleTest, OutlivesItsNodeSafely) {
  auto open = std::make_shared<Observable<bool>>(true);
  auto escaped = std::make_shared<std::optional<StateHandle<int>>>();
  Tree tree{Gate{open, escaped}, [] { return true; }};
  tree.reconcile();
  const std::size_t nodes = tree.size();
  open->set(false);
  tree.reconcile();
  EXPECT_LT(tree.size(), nodes);
  ASSERT_TRUE(escaped->has_value());
  (*escaped)->update([](int& current) { current += 10; });
  EXPECT_EQ((*escaped)->get(), 15);
  EXPECT_FALSE(tree.dirty());
}

TEST_F(EvaluationTest, SeededStateTakesItsSeedOncePerIdentity) {
  auto seed = std::make_shared<Observable<std::string>>(std::string{"alpha"});
  auto identity = std::make_shared<Observable<int>>(1);
  Tree tree{Editor{seed, identity}, owner()};
  tree.reconcile();
  EXPECT_TRUE(shows(tree.root(), "alpha"));
  seed->set("beta");
  tree.reconcile();
  EXPECT_TRUE(shows(tree.root(), "alpha"));
  EXPECT_FALSE(shows(tree.root(), "beta"));
  identity->set(2);
  tree.reconcile();
  EXPECT_TRUE(shows(tree.root(), "beta"));
  EXPECT_FALSE(shows(tree.root(), "alpha"));
}

}
