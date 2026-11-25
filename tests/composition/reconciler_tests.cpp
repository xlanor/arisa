#include <arisa/components/button.hpp>
#include <arisa/components/for_each.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/tree.hpp>

#include <gtest/gtest.h>

#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace arisa::composition {

class ReconcileTest : public ::testing::Test {
 protected:
  struct Entry {
    int key;
    std::string label;

    friend auto operator==(const Entry&, const Entry&) -> bool = default;
  };

  struct Journal {
    std::vector<std::string> events;
    std::vector<DepartureHold> holds;
    bool hold_departures{false};
  };

  using Entries = Observable<std::vector<Entry>>;

  class Item final : public Component {
   public:
    static constexpr StateKey<int> taps{"taps", 0};

    explicit Item(std::string label) : label_(std::move(label)) {}

    auto operator==(const Item&) const -> bool = default;

    auto body(Context& context) const -> Element override {
      const auto tapped = context.state(taps);
      return components::HStack(
          components::Text(std::format("{} {}", label_, tapped.get())),
          components::Button(
              "tap", [tapped] { tapped.update([](int& value) { ++value; }); }));
    }

   private:
    std::string label_;
  };

  class List final : public Component {
   public:
    List(std::shared_ptr<Entries> entries, std::shared_ptr<Journal> journal)
        : entries_(std::move(entries)), journal_(std::move(journal)) {}

    auto operator==(const List&) const -> bool = default;

    auto body(Context& context) const -> Element override {
      const std::vector<Entry>& entries = context.read(*entries_);
      return components::VStack(components::ForEach(
          entries, [](const Entry& entry) { return entry.key; },
          [journal = journal_](const Entry& entry) {
            Element item = Element{Item{entry.label}}.on_disappear(
                [journal, label = entry.label] {
                  journal->events.push_back("-" + label);
                });
            if (!journal->hold_departures) {
              return item;
            }
            return std::move(item).on_depart([journal](DepartureHold hold) {
              journal->holds.push_back(std::move(hold));
            });
          }));
    }

   private:
    std::shared_ptr<Entries> entries_;
    std::shared_ptr<Journal> journal_;
  };

  class Switcher final : public Component {
   public:
    explicit Switcher(std::shared_ptr<Observable<int>> mode)
        : mode_(std::move(mode)) {}

    auto operator==(const Switcher&) const -> bool = default;

    auto body(Context& context) const -> Element override {
      const int mode = context.read(*mode_);
      if (mode == 0) {
        return components::VStack(components::Text("first"),
                                  components::Text("second"));
      }
      if (mode == 1) {
        return components::VStack(components::Text("changed"),
                                  components::Text("second"));
      }
      return components::VStack(components::Button("button", {}),
                                components::Text("second"));
    }

   private:
    std::shared_ptr<Observable<int>> mode_;
  };

  class Aliased final : public Component {
   public:
    explicit Aliased(std::shared_ptr<Entries> entries)
        : entries_(std::move(entries)) {}

    auto operator==(const Aliased&) const -> bool = default;

    auto body(Context& context) const -> Element override {
      const std::vector<Entry>& entries = context.read(*entries_);
      return components::VStack(components::ForEach(
          entries, [](const Entry& entry) { return entry.key; },
          [](const Entry& entry) {
            return components::Text(entry.label).id(entry.label);
          }));
    }

   private:
    std::shared_ptr<Entries> entries_;
  };

  static auto owner() -> Tree::OwnerCheck {
    return [] { return true; };
  }

  static auto children(const Node& node)
      -> std::vector<std::reference_wrapper<const Node>> {
    return node.children() | std::views::transform([](const Node& child) {
             return std::cref(child);
           }) |
           std::ranges::to<std::vector>();
  }

  static auto first(const Node& node) -> const Node& {
    return children(node).front().get();
  }

  static auto items(const Tree& tree)
      -> std::vector<std::reference_wrapper<const Node>> {
    return children(first(first(first(tree.root()))));
  }

  static auto ids(const Tree& tree) -> std::vector<NodeId> {
    return items(tree) |
           std::views::transform([](const Node& node) { return node.id(); }) |
           std::ranges::to<std::vector>();
  }

  static auto keys(const Tree& tree) -> std::vector<ElementKey> {
    return items(tree) |
           std::views::transform([](const Node& node) { return *node.key(); }) |
           std::ranges::to<std::vector>();
  }

  static auto label(const Node& item) -> std::string {
    const Node& text = first(first(item));
    return std::string{
        text.content_as<components::Text::Content>()->get().text()};
  }

  static auto tap(Tree& tree, const Node& item) -> bool {
    const NodeId button = children(first(item)).back().get().id();
    return tree.activate(button);
  }

  static auto entries(std::initializer_list<int> keys) -> std::vector<Entry> {
    return keys | std::views::transform([](int key) {
             return Entry{.key = key, .label = std::format("item{}", key)};
           }) |
           std::ranges::to<std::vector>();
  }

  void SetUp() override {
    source_->set(entries({1, 2, 3}));
    tree_.reconcile();
  }

  std::shared_ptr<Entries> source_{std::make_shared<Entries>()};
  std::shared_ptr<Journal> journal_{std::make_shared<Journal>()};
  Tree tree_{List{source_, journal_}, owner()};
};

TEST_F(ReconcileTest, MountsKeyedChildrenInOrder) {
  ASSERT_EQ(items(tree_).size(), 3U);
  EXPECT_EQ(keys(tree_), (std::vector<ElementKey>{1, 2, 3}));
  EXPECT_EQ(label(items(tree_)[1]), "item2 0");
  for (const Node& item : items(tree_)) {
    EXPECT_EQ(item.phase(), NodeLifecyclePhase::live);
    EXPECT_EQ(item.state_slots(), 1U);
  }
}

TEST_F(ReconcileTest, InsertsWithoutDisturbingExistingNodes) {
  const std::vector<NodeId> before = ids(tree_);
  source_->set(entries({1, 4, 2, 3}));
  tree_.reconcile();
  const std::vector<NodeId> after = ids(tree_);
  ASSERT_EQ(after.size(), 4U);
  EXPECT_EQ(keys(tree_), (std::vector<ElementKey>{1, 4, 2, 3}));
  EXPECT_EQ(after[0], before[0]);
  EXPECT_EQ(after[2], before[1]);
  EXPECT_EQ(after[3], before[2]);
  EXPECT_NE(after[1], before[0]);
  EXPECT_NE(after[1], before[1]);
  EXPECT_NE(after[1], before[2]);
}

TEST_F(ReconcileTest, UpdatesPropsInPlace) {
  const std::vector<NodeId> before = ids(tree_);
  auto changed = entries({1, 2, 3});
  changed[1].label = "renamed";
  source_->set(std::move(changed));
  tree_.reconcile();
  EXPECT_EQ(ids(tree_), before);
  EXPECT_EQ(label(items(tree_)[1]), "renamed 0");
  EXPECT_EQ(label(items(tree_)[0]), "item1 0");
}

TEST_F(ReconcileTest, MovesKeyedChildrenWithTheirState) {
  ASSERT_TRUE(tap(tree_, items(tree_)[2]));
  ASSERT_TRUE(tap(tree_, items(tree_)[2]));
  tree_.reconcile();
  ASSERT_EQ(label(items(tree_)[2]), "item3 2");
  const std::vector<NodeId> before = ids(tree_);
  source_->set(entries({3, 1, 2}));
  tree_.reconcile();
  EXPECT_EQ(keys(tree_), (std::vector<ElementKey>{3, 1, 2}));
  EXPECT_EQ(ids(tree_), (std::vector<NodeId>{before[2], before[0], before[1]}));
  EXPECT_EQ(label(items(tree_)[0]), "item3 2");
  EXPECT_EQ(label(items(tree_)[1]), "item1 0");
}

TEST_F(ReconcileTest, RemovesChildrenOnceTheirDepartureCompletes) {
  const std::vector<NodeId> before = ids(tree_);
  const std::size_t nodes = tree_.size();
  source_->set(entries({1, 3}));
  tree_.reconcile();
  EXPECT_EQ(ids(tree_), (std::vector<NodeId>{before[0], before[2]}));
  EXPECT_FALSE(tree_.node(before[1]).has_value());
  EXPECT_EQ(tree_.departing(), 0U);
  EXPECT_LT(tree_.size(), nodes);
  EXPECT_EQ(journal_->events, (std::vector<std::string>{"-item2"}));
}

TEST_F(ReconcileTest, HeldDepartureKeepsTheNodeRenderingUntilReleased) {
  journal_->hold_departures = true;
  source_->set(entries({1, 2, 3, 4}));
  tree_.reconcile();
  const std::vector<NodeId> before = ids(tree_);
  source_->set(entries({1, 3, 4}));
  tree_.reconcile();
  ASSERT_EQ(journal_->holds.size(), 1U);
  EXPECT_EQ(ids(tree_), before);
  EXPECT_EQ(items(tree_)[1].get().phase(), NodeLifecyclePhase::departing);
  EXPECT_EQ(tree_.departing(), 1U);
  EXPECT_EQ(journal_->events, (std::vector<std::string>{"-item2"}));
  EXPECT_FALSE(tap(tree_, items(tree_)[1]));

  source_->set(entries({4, 1, 3}));
  tree_.reconcile();
  EXPECT_EQ(items(tree_).size(), 4U);
  EXPECT_EQ(tree_.node(before[1])->get().phase(),
            NodeLifecyclePhase::departing);

  journal_->holds.clear();
  tree_.reconcile();
  EXPECT_EQ(tree_.departing(), 0U);
  EXPECT_FALSE(tree_.node(before[1]).has_value());
  EXPECT_EQ(keys(tree_), (std::vector<ElementKey>{4, 1, 3}));
  EXPECT_EQ(journal_->events, (std::vector<std::string>{"-item2"}));
}

TEST_F(ReconcileTest, FindFollowsInsertRemoveAndDeparture) {
  journal_->hold_departures = true;
  source_->set(entries({1, 2, 3}));
  tree_.reconcile();
  const std::vector<NodeId> before = ids(tree_);
  EXPECT_EQ(tree_.find(ElementKey{2}), before[1]);
  EXPECT_FALSE(tree_.find(ElementKey{9}).has_value());
  source_->set(entries({1, 4, 2, 3}));
  tree_.reconcile();
  const NodeId inserted = ids(tree_)[1];
  EXPECT_EQ(tree_.find(ElementKey{4}), inserted);
  EXPECT_EQ(tree_.find(ElementKey{2}), before[1]);
  source_->set(entries({1, 4, 3}));
  tree_.reconcile();
  ASSERT_EQ(tree_.departing(), 1U);
  EXPECT_FALSE(tree_.find(ElementKey{2}).has_value());
  EXPECT_EQ(tree_.find(ElementKey{3}), before[2]);
  journal_->holds.clear();
  tree_.reconcile();
  EXPECT_EQ(tree_.departing(), 0U);
  EXPECT_FALSE(tree_.find(ElementKey{2}).has_value());
  source_->set(entries({1, 2, 4, 3}));
  tree_.reconcile();
  const auto returned = tree_.find(ElementKey{2});
  ASSERT_TRUE(returned.has_value());
  EXPECT_NE(*returned, before[1]);
  EXPECT_EQ(tree_.node(*returned)->get().phase(), NodeLifecyclePhase::live);
  EXPECT_EQ(tree_.find(ElementKey{4}), inserted);
  EXPECT_EQ(tree_.find(ElementKey{1}, first(first(tree_.root())).id()),
            before[0]);
  EXPECT_EQ(tree_.searches(), 0U);
}

TEST_F(ReconcileTest, ReportsDuplicateKeysAndKeepsIdentityStable) {
  static_cast<void>(tree_.take_duplicate_keys());
  source_->set(std::vector<Entry>{{.key = 1, .label = "a"},
                                  {.key = 1, .label = "b"},
                                  {.key = 2, .label = "c"}});
  tree_.reconcile();
  const auto duplicates = tree_.take_duplicate_keys();
  ASSERT_EQ(duplicates.size(), 1U);
  EXPECT_EQ(duplicates.front().key, ElementKey{1});
  EXPECT_EQ(duplicates.front().parent, first(first(first(tree_.root()))).id());
  const std::vector<NodeId> before = ids(tree_);
  ASSERT_EQ(before.size(), 3U);
  EXPECT_EQ(label(items(tree_)[1]), "b 0");

  source_->set(std::vector<Entry>{{.key = 1, .label = "a"},
                                  {.key = 1, .label = "b2"},
                                  {.key = 2, .label = "c"},
                                  {.key = 3, .label = "d"}});
  tree_.reconcile();
  const std::vector<NodeId> after = ids(tree_);
  ASSERT_EQ(after.size(), 4U);
  EXPECT_EQ(after[0], before[0]);
  EXPECT_EQ(after[1], before[1]);
  EXPECT_EQ(after[2], before[2]);
  EXPECT_EQ(label(items(tree_)[1]), "b2 0");
  EXPECT_EQ(tree_.take_duplicate_keys().size(), 1U);
}

TEST_F(ReconcileTest, ARowKeepsItsExplicitIdAsAnAliasOfTheForEachKey) {
  auto rows = std::make_shared<Entries>(entries({1, 2, 3}));
  Tree tree{Aliased{rows}, owner()};
  tree.reconcile();
  const std::vector<NodeId> before = ids(tree);
  ASSERT_EQ(before.size(), 3U);
  EXPECT_EQ(keys(tree), (std::vector<ElementKey>{1, 2, 3}));
  EXPECT_EQ(tree.find(ElementKey{"item2"}), before[1]);
  EXPECT_EQ(tree.find(ElementKey{2}), before[1]);
  rows->set(entries({3, 1, 2}));
  tree.reconcile();
  EXPECT_EQ(ids(tree), (std::vector<NodeId>{before[2], before[0], before[1]}));
  EXPECT_EQ(tree.find(ElementKey{"item3"}), before[2]);
  EXPECT_EQ(tree.searches(), 0U);
  auto renamed = entries({3, 1, 2});
  renamed[2].label = "renamed";
  rows->set(std::move(renamed));
  tree.reconcile();
  EXPECT_EQ(ids(tree), (std::vector<NodeId>{before[2], before[0], before[1]}));
  EXPECT_FALSE(tree.find(ElementKey{"item2"}).has_value());
  EXPECT_EQ(tree.find(ElementKey{"renamed"}), before[1]);
  rows->set(entries({3, 1}));
  tree.reconcile();
  EXPECT_FALSE(tree.find(ElementKey{"renamed"}).has_value());
}

TEST_F(ReconcileTest, ASharedAliasResolvesToTheFirstRowInTreeOrder) {
  auto rows = std::make_shared<Entries>(
      std::vector<Entry>{{.key = 1, .label = "a"},
                         {.key = 2, .label = "shared"},
                         {.key = 3, .label = "shared"}});
  Tree tree{Aliased{rows}, owner()};
  tree.reconcile();
  const std::vector<NodeId> before = ids(tree);
  ASSERT_EQ(before.size(), 3U);
  EXPECT_EQ(tree.find(ElementKey{"shared"}), before[1]);
  EXPECT_EQ(tree.find(ElementKey{"shared"}), before[1]);
  rows->set(std::vector<Entry>{{.key = 3, .label = "shared"},
                               {.key = 1, .label = "a"},
                               {.key = 2, .label = "shared"}});
  tree.reconcile();
  EXPECT_EQ(tree.find(ElementKey{"shared"}), before[2]);
  EXPECT_TRUE(tree.take_duplicate_keys().empty());
}

TEST_F(ReconcileTest, MatchesUnkeyedChildrenByPositionAndType) {
  auto mode = std::make_shared<Observable<int>>(0);
  Tree tree{Switcher{mode}, owner()};
  tree.reconcile();
  const Node& stack = first(first(tree.root()));
  const auto initial = children(stack);
  ASSERT_EQ(initial.size(), 2U);
  const NodeId leading = initial[0].get().id();
  const NodeId trailing = initial[1].get().id();

  mode->set(1);
  tree.reconcile();
  const auto updated = children(first(first(tree.root())));
  EXPECT_EQ(updated[0].get().id(), leading);
  EXPECT_EQ(
      updated[0].get().content_as<components::Text::Content>()->get().text(),
      "changed");
  EXPECT_EQ(updated[1].get().id(), trailing);

  mode->set(2);
  tree.reconcile();
  const auto replaced = children(first(first(tree.root())));
  ASSERT_EQ(replaced.size(), 2U);
  EXPECT_NE(replaced[0].get().id(), leading);
  EXPECT_TRUE(
      replaced[0].get().content_as<components::Button::Content>().has_value());
  EXPECT_EQ(replaced[1].get().id(), trailing);
  EXPECT_FALSE(tree.node(leading).has_value());
}

}
