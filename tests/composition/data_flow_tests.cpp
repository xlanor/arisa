#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/app_storage.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/layer.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/composition/overlay_layer.hpp>
#include <arisa/composition/preference_key.hpp>
#include <arisa/composition/preference_observer.hpp>
#include <arisa/composition/preference_reader.hpp>
#include <arisa/composition/scene_storage.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/tree.hpp>
#include <arisa/composition/user_defaults.hpp>
#include <arisa/composition/view_modifier.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace arisa::composition {

class DataFlowTest : public ::testing::Test {
 protected:
  using Values = std::shared_ptr<std::vector<int>>;

  static const PreferenceKey<int> last;
  static const PreferenceKey<std::vector<int>> sequence;
  static constexpr EnvironmentKey<int> counter{"counter", 0};

  static auto owner() -> Tree::OwnerCheck {
    return [] { return true; };
  }

  static auto blank() -> Element {
    return components::VStack().frame({.width = 10.0F, .height = 10.0F});
  }

  class Inner final : public Component {
   public:
    static constexpr StateKey<int> value{"inner-value", 3};

    auto operator==(const Inner&) const -> bool = default;

    auto body(Context& context) const -> Element override {
      const auto current = context.state(value);
      return blank().preference(last, current.get()).id("inner");
    }
  };

  class Outer final : public Component {
   public:
    auto operator==(const Outer&) const -> bool = default;

    auto body(Context&) const -> Element override {
      return components::VStack(Inner{});
    }
  };

  class Optional final : public Component {
   public:
    explicit Optional(std::shared_ptr<Observable<bool>> shown)
        : shown_(std::move(shown)) {}

    auto operator==(const Optional&) const -> bool = default;

    auto body(Context& context) const -> Element override {
      std::optional<Element> declared;
      if (context.read(*shown_)) {
        declared = blank().preference(last, 7);
      }
      return components::VStack(std::move(declared), blank().id("anchor"));
    }

   private:
    std::shared_ptr<Observable<bool>> shown_;
  };

  class Reader final : public Component {
   public:
    explicit Reader(std::shared_ptr<int> seen) : seen_(std::move(seen)) {}

    auto operator==(const Reader&) const -> bool = default;

    auto body(Context& context) const -> Element override {
      *seen_ = context.environment(counter);
      return blank();
    }

   private:
    std::shared_ptr<int> seen_;
  };

  class Bumped final : public Component {
   public:
    Bumped(std::shared_ptr<Observable<int>> base, std::shared_ptr<int> seen)
        : base_(std::move(base)), seen_(std::move(seen)) {}

    auto operator==(const Bumped&) const -> bool = default;

    auto body(Context& context) const -> Element override {
      return components::VStack(Element{Reader{seen_}}.transform_environment(
                                    counter, [](int& value) { value += 1; }))
          .environment(counter, context.read(*base_));
    }

   private:
    std::shared_ptr<Observable<int>> base_;
    std::shared_ptr<int> seen_;
  };

  class Card final : public ViewModifier {
   public:
    static constexpr StateKey<int> uses{"card-uses", 0};

    explicit Card(std::shared_ptr<int> built) : built_(std::move(built)) {}

    auto operator==(const Card&) const -> bool = default;

    auto body(Context& context, Content content) const -> Element override {
      ++*built_;
      static_cast<void>(context.state(uses));
      return components::VStack(Element{std::move(content)}).id("card");
    }

   private:
    std::shared_ptr<int> built_;
  };

  class Stored final : public Component {
   public:
    static constexpr AppStorage<bool> flag{"flag", false};
    static constexpr SceneStorage<int> tab{"tab", 2};

    explicit Stored(std::shared_ptr<std::vector<std::pair<bool, int>>> seen)
        : seen_(std::move(seen)) {}

    auto operator==(const Stored&) const -> bool = default;

    auto body(Context& context) const -> Element override {
      Binding<bool> on = context.app_storage(flag);
      Binding<int> selected = context.scene_storage(tab);
      seen_->emplace_back(on.get(), selected.get());
      if (!on.get()) {
        on.set(true);
        selected.set(4);
      }
      return blank();
    }

   private:
    std::shared_ptr<std::vector<std::pair<bool, int>>> seen_;
  };

  static auto find(const Tree& tree, std::string_view key) -> const Node& {
    return tree.node(*tree.find(ElementKey{key}))->get();
  }
};

const PreferenceKey<int> DataFlowTest::last{
    "last", 0, [](int& value, int next) { value = next; }};

const PreferenceKey<std::vector<int>> DataFlowTest::sequence{
    "sequence", {}, [](std::vector<int>& value, std::vector<int> next) {
      value.insert(value.end(), next.begin(), next.end());
    }};

TEST_F(DataFlowTest, APreferenceSetInsideAComponentBodyReachesTheObserver) {
  const Values seen = std::make_shared<std::vector<int>>();
  Tree tree{Element{Outer{}}.on_preference_change(
                last, [seen](const int& value) { seen->push_back(value); }),
            owner()};
  tree.reconcile();
  EXPECT_EQ(*seen, (std::vector<int>{3}));
  tree.reconcile();
  EXPECT_EQ(*seen, (std::vector<int>{3}));
}

TEST_F(DataFlowTest, AnOuterPreferenceReplacesTheValuesInside) {
  const Values seen = std::make_shared<std::vector<int>>();
  Tree tree{components::VStack(blank().preference(last, 1))
                .preference(last, 2)
                .on_preference_change(
                    last, [seen](const int& value) { seen->push_back(value); }),
            owner()};
  tree.reconcile();
  EXPECT_EQ(*seen, (std::vector<int>{2}));
}

TEST_F(DataFlowTest, SiblingsReduceInTreeOrderAndSilentViewsAddNothing) {
  auto ordered = std::make_shared<std::vector<int>>();
  Tree tree{components::VStack(blank().preference(sequence, {1}), blank(),
                               blank().preference(sequence, {2}))
                .on_preference_change(sequence,
                                      [ordered](const std::vector<int>& value) {
                                        *ordered = value;
                                      }),
            owner()};
  tree.reconcile();
  EXPECT_EQ(*ordered, (std::vector<int>{1, 2}));
  const Values seen = std::make_shared<std::vector<int>>();
  Tree silent{
      components::VStack(blank().preference(last, 5), blank())
          .on_preference_change(
              last, [seen](const int& value) { seen->push_back(value); }),
      owner()};
  silent.reconcile();
  EXPECT_EQ(*seen, (std::vector<int>{5}));
}

TEST_F(DataFlowTest, AnUnsetKeyNeverCallsTheAction) {
  const Values seen = std::make_shared<std::vector<int>>();
  Tree tree{components::VStack(blank()).on_preference_change(
                last, [seen](const int& value) { seen->push_back(value); }),
            owner()};
  tree.reconcile();
  EXPECT_TRUE(seen->empty());
}

TEST_F(DataFlowTest, TransformPreferenceEditsTheValueOrTheDefault) {
  const Values seen = std::make_shared<std::vector<int>>();
  Tree tree{components::VStack(blank().preference(last, 1).transform_preference(
                                   last, [](int& value) { value += 10; }))
                .on_preference_change(
                    last, [seen](const int& value) { seen->push_back(value); }),
            owner()};
  tree.reconcile();
  EXPECT_EQ(*seen, (std::vector<int>{11}));
  const Values bare = std::make_shared<std::vector<int>>();
  Tree unset{
      components::VStack(
          blank().transform_preference(last, [](int& value) { value += 10; }))
          .on_preference_change(
              last, [bare](const int& value) { bare->push_back(value); }),
      owner()};
  unset.reconcile();
  EXPECT_EQ(*bare, (std::vector<int>{10}));
}

TEST_F(DataFlowTest, RemovingTheDeclaringViewRestoresTheDefault) {
  const Values seen = std::make_shared<std::vector<int>>();
  const auto shown = std::make_shared<Observable<bool>>(true);
  Tree tree{Element{Optional{shown}}.on_preference_change(
                last, [seen](const int& value) { seen->push_back(value); }),
            owner()};
  tree.reconcile();
  ASSERT_EQ(*seen, (std::vector<int>{7}));
  shown->set(false);
  tree.reconcile();
  EXPECT_EQ(*seen, (std::vector<int>{7, 0}));
}

TEST_F(DataFlowTest, OverlayPreferenceValueRebuildsInTheSameReconcile) {
  const Values built = std::make_shared<std::vector<int>>();
  Tree tree{Element{Outer{}}.overlay_preference_value(
                last,
                [built](const int& value) -> Element {
                  built->push_back(value);
                  return blank().id("overlay");
                }),
            owner()};
  tree.reconcile();
  ASSERT_FALSE(built->empty());
  EXPECT_EQ(built->back(), 3);
  const std::size_t settled = built->size();
  tree.reconcile();
  EXPECT_EQ(built->size(), settled);
}

TEST_F(DataFlowTest, BackgroundPreferenceValueBuildsBehindTheContent) {
  const Values built = std::make_shared<std::vector<int>>();
  Tree tree{Element{Outer{}}.background_preference_value(
                last,
                [built](const int& value) -> Element {
                  built->push_back(value);
                  return blank().id("behind");
                }),
            owner()};
  tree.reconcile();
  ASSERT_FALSE(built->empty());
  EXPECT_EQ(built->back(), 3);
  const auto behind = tree.find(ElementKey{"behind"});
  ASSERT_TRUE(behind.has_value());
  const auto parent = tree.node(*behind)->get().parent();
  ASSERT_TRUE(parent.has_value());
  const auto siblings = tree.node(*parent)->get().children();
  ASSERT_FALSE(siblings.empty());
  EXPECT_EQ((*siblings.begin()).id(), *behind);
}

TEST_F(DataFlowTest, PresentedLayersDoNotLeakPreferences) {
  const Values seen = std::make_shared<std::vector<int>>();
  Tree tree{components::VStack(blank().overlay(true, OverlayLayer::sheet,
                                               blank().preference(last, 9)))
                .on_preference_change(
                    last, [seen](const int& value) { seen->push_back(value); }),
            owner()};
  tree.reconcile();
  EXPECT_TRUE(seen->empty());
}

TEST_F(DataFlowTest, SettledReadersResolveNothing) {
  Tree tree{Element{Outer{}}.on_preference_change(last, [](const int&) {}),
            owner()};
  tree.reconcile();
  const std::uint64_t resolved = tree.preference_resolutions();
  tree.reconcile();
  tree.reconcile();
  EXPECT_EQ(tree.preference_resolutions(), resolved);
}

TEST_F(DataFlowTest, TransformEnvironmentEditsTheInheritedValue) {
  const auto seen = std::make_shared<int>(0);
  const auto base = std::make_shared<Observable<int>>(5);
  Tree tree{Bumped{base, seen}, owner()};
  tree.reconcile();
  EXPECT_EQ(*seen, 6);
  base->set(9);
  tree.reconcile();
  EXPECT_EQ(*seen, 10);
}

TEST_F(DataFlowTest, TransformEnvironmentFollowsSwiftUIModifierOrder) {
  const auto outer = std::make_shared<int>(0);
  Tree after{Element{Reader{outer}}
                 .transform_environment(counter, [](int& value) { value += 1; })
                 .environment(counter, 5),
             owner()};
  after.reconcile();
  EXPECT_EQ(*outer, 6);
  const auto inner = std::make_shared<int>(0);
  Tree before{
      Element{Reader{inner}}
          .environment(counter, 5)
          .transform_environment(counter, [](int& value) { value += 1; }),
      owner()};
  before.reconcile();
  EXPECT_EQ(*inner, 5);
}

TEST_F(DataFlowTest, AViewModifierWrapsItsContentAsAComponent) {
  const auto built = std::make_shared<int>(0);
  Tree tree{components::VStack(blank().id("content").modifier(Card{built})),
            owner()};
  tree.reconcile();
  EXPECT_EQ(*built, 1);
  const Node& content = find(tree, "content");
  bool wrapped = false;
  std::optional<NodeId> above = content.parent();
  while (above.has_value()) {
    const Node& node = tree.node(*above)->get();
    wrapped = wrapped ||
              (node.key().has_value() && node.key()->description() == "card");
    above = node.parent();
  }
  EXPECT_TRUE(wrapped);
}

TEST_F(DataFlowTest, StorageFallsBackToViewStateWithoutAStore) {
  auto seen = std::make_shared<std::vector<std::pair<bool, int>>>();
  Tree tree{Stored{seen}, owner()};
  tree.reconcile();
  tree.reconcile();
  ASSERT_GE(seen->size(), 2U);
  EXPECT_EQ(seen->front(), (std::pair<bool, int>{false, 2}));
  EXPECT_EQ(seen->back(), (std::pair<bool, int>{true, 4}));
}

}
