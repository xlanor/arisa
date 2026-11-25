#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/composition/tree.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/transaction.hpp>
#include <arisa/motion/with_animation.hpp>

#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::composition {

class TransactionTest : public ::testing::Test {
 protected:
  using Changes = std::vector<std::pair<int, int>>;

  class Panel final : public Component {
   public:
    Panel(const Observable<int>& model, Changes& changes, bool initial)
        : model_(model), changes_(changes), initial_(initial) {}

    auto operator==(const Panel& other) const -> bool {
      return &model_.get() == &other.model_.get() &&
             &changes_.get() == &other.changes_.get() &&
             initial_ == other.initial_;
    }

    auto body(Context& context) const -> Element override {
      const int value = context.read(model_.get());
      return components::VStack(
                 components::Text("plain")
                     .opacity(value > 0 ? 0.5F : 1.0F)
                     .id("plain"),
                 components::Text("implicit")
                     .id("implicit")
                     .animation(motion::Animation::snappy(), value % 2 == 0),
                 components::Text("muted").id("muted").transaction(
                     [](motion::Transaction& transaction) {
                       transaction.set_animation(std::nullopt);
                     }),
                 components::Text("cancelled")
                     .id("cancelled")
                     .animation(std::nullopt, value))
          .on_change(value, initial_, [changes = changes_](int old, int now) {
            changes.get().emplace_back(old, now);
          });
    }

   private:
    std::reference_wrapper<const Observable<int>> model_;
    std::reference_wrapper<Changes> changes_;
    bool initial_;
  };

  static auto animation_of(Tree& tree, std::string_view key)
      -> std::optional<motion::Animation> {
    const auto found = tree.find(ElementKey{key});
    EXPECT_TRUE(found.has_value()) << key;
    return tree.transaction(tree.node(*found)->get()).animation();
  }

  Observable<int> model_{0};
  Changes changes_;
};

TEST_F(TransactionTest, OnlyWritesInsideWithAnimationAnimate) {
  Tree tree{Panel{model_, changes_, false}, [] { return true; }};
  tree.reconcile();
  tree.close_transactions();
  motion::with_animation(motion::Animation::linear(1.0),
                         [this] { model_.set(1); });
  tree.reconcile();
  EXPECT_EQ(animation_of(tree, "plain"), motion::Animation::linear(1.0));
  EXPECT_EQ(animation_of(tree, "implicit"), motion::Animation::snappy());
  EXPECT_FALSE(animation_of(tree, "muted").has_value());
  EXPECT_FALSE(animation_of(tree, "cancelled").has_value());
  tree.close_transactions();
  EXPECT_FALSE(animation_of(tree, "plain").has_value());
  model_.set(3);
  tree.reconcile();
  EXPECT_FALSE(animation_of(tree, "plain").has_value());
  EXPECT_FALSE(animation_of(tree, "implicit").has_value());
  tree.close_transactions();
}

TEST_F(TransactionTest, ImplicitAnimationFollowsItsValue) {
  Tree tree{Panel{model_, changes_, false}, [] { return true; }};
  tree.reconcile();
  tree.close_transactions();
  model_.set(1);
  tree.reconcile();
  EXPECT_EQ(animation_of(tree, "implicit"), motion::Animation::snappy());
  EXPECT_FALSE(animation_of(tree, "plain").has_value());
  tree.close_transactions();
  model_.set(3);
  tree.reconcile();
  EXPECT_FALSE(animation_of(tree, "implicit").has_value());
  tree.close_transactions();
  model_.set(4);
  tree.reconcile();
  EXPECT_EQ(animation_of(tree, "implicit"), motion::Animation::snappy());
  tree.close_transactions();
}

TEST_F(TransactionTest, DisablesAnimationsBlocksImplicitAnimation) {
  Tree tree{Panel{model_, changes_, false}, [] { return true; }};
  tree.reconcile();
  tree.close_transactions();
  motion::Transaction quiet;
  quiet.set_disables_animations(true);
  motion::with_transaction(quiet, [this] { model_.set(1); });
  tree.reconcile();
  EXPECT_FALSE(animation_of(tree, "implicit").has_value());
  tree.close_transactions();
  motion::Transaction explicit_only{motion::Animation::linear(0.5)};
  explicit_only.set_disables_animations(true);
  motion::with_transaction(explicit_only, [this] { model_.set(2); });
  tree.reconcile();
  EXPECT_EQ(animation_of(tree, "implicit"), motion::Animation::linear(0.5));
  EXPECT_EQ(animation_of(tree, "plain"), motion::Animation::linear(0.5));
  tree.close_transactions();
}

TEST_F(TransactionTest, OnChangeReportsOldAndNewValues) {
  Tree tree{Panel{model_, changes_, false}, [] { return true; }};
  tree.reconcile();
  EXPECT_TRUE(changes_.empty());
  model_.set(5);
  tree.reconcile();
  model_.set(5);
  tree.reconcile();
  model_.set(9);
  tree.reconcile();
  EXPECT_EQ(changes_, (Changes{{0, 5}, {5, 9}}));
}

TEST_F(TransactionTest, OnChangeWithInitialFiresOnMount) {
  model_.set(3);
  Tree tree{Panel{model_, changes_, true}, [] { return true; }};
  tree.reconcile();
  EXPECT_EQ(changes_, (Changes{{3, 3}}));
  model_.set(4);
  tree.reconcile();
  EXPECT_EQ(changes_, (Changes{{3, 3}, {3, 4}}));
}

TEST_F(TransactionTest, CompletionQueuesOnTheTreeAndRunsOnce) {
  Tree tree{Panel{model_, changes_, false}, [] { return true; }};
  tree.reconcile();
  tree.close_transactions();
  int completed = 0;
  motion::with_animation(
      motion::Animation::linear(1.0), [this] { model_.set(1); },
      [&completed] { ++completed; });
  EXPECT_EQ(completed, 0);
  tree.reconcile();
  tree.close_transactions();
  EXPECT_TRUE(tree.completions_pending());
  EXPECT_EQ(tree.run_completions(), 1U);
  EXPECT_EQ(completed, 1);
  EXPECT_EQ(tree.run_completions(), 0U);
}

}
