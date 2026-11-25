#include <arisa/async/awaitables.hpp>
#include <arisa/async/frame_timeline.hpp>
#include <arisa/async/manual_executor.hpp>
#include <arisa/async/task.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/composition/tree.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <memory>
#include <utility>

namespace arisa::composition {

using namespace std::chrono_literals;

class NodeTaskTest : public ::testing::Test {
 protected:
  struct Ledger {
    int starts{0};
    int finishes{0};
    int destroyed{0};
  };

  struct Setup {
    bool show{true};
    int key{1};
    bool keyed{true};
    bool finite{false};

    friend auto operator==(const Setup&, const Setup&) -> bool = default;
  };

  using Shared = std::shared_ptr<Ledger>;

  class Lifetime {
   public:
    explicit Lifetime(Shared ledger) noexcept : ledger_(std::move(ledger)) {}

    Lifetime(const Lifetime&) = delete;
    auto operator=(const Lifetime&) -> Lifetime& = delete;
    Lifetime(Lifetime&&) = delete;
    auto operator=(Lifetime&&) -> Lifetime& = delete;

    ~Lifetime() { ++ledger_->destroyed; }

   private:
    Shared ledger_;
  };

  static auto loop(Shared ledger, std::reference_wrapper<async::Executor> root,
                   bool finite) -> async::Task<void> {
    const Lifetime lifetime{ledger};
    ++ledger->starts;
    if (finite) {
      ++ledger->finishes;
      co_return;
    }
    while (true) {
      co_await async::on(root.get());
    }
  }

  static auto nap(Shared ledger) -> async::Task<void> {
    co_await async::sleep(1s);
    ++ledger->finishes;
  }

  class Ticker final : public Component {
   public:
    Ticker(std::shared_ptr<Observable<Setup>> setup, Shared ledger)
        : setup_(std::move(setup)), ledger_(std::move(ledger)) {}

    auto operator==(const Ticker&) const -> bool = default;

    auto body(Context& context) const -> Element override {
      const Setup& setup = context.read(*setup_);
      if (!setup.show) {
        return components::Text("idle");
      }
      const std::reference_wrapper<async::Executor> root =
          context.services().root;
      auto start = [ledger = ledger_, root, finite = setup.finite] {
        return loop(ledger, root, finite);
      };
      Element ticking = components::Text("ticking").id("ticking");
      if (setup.keyed) {
        return std::move(ticking).task(setup.key, std::move(start));
      }
      return std::move(ticking).task(std::move(start));
    }

   private:
    std::shared_ptr<Observable<Setup>> setup_;
    Shared ledger_;
  };

  class Napper final : public Component {
   public:
    explicit Napper(Shared ledger) : ledger_(std::move(ledger)) {}

    auto operator==(const Napper&) const -> bool = default;

    auto body(Context&) const -> Element override {
      return components::Text("napping").task(
          [ledger = ledger_] { return nap(ledger); });
    }

   private:
    Shared ledger_;
  };

  auto services() -> RootServices {
    return RootServices{.root = manual_,
                        .frames = timeline_,
                        .executors = async::Executors{.background = manual_,
                                                      .blocking = manual_}};
  }

  static auto owner() -> Tree::OwnerCheck {
    return [] { return true; };
  }

  std::shared_ptr<std::chrono::nanoseconds> now_{
      std::make_shared<std::chrono::nanoseconds>(0)};
  async::ManualExecutor manual_;
  async::FrameTimeline timeline_{[now = now_] { return *now; }};
  Shared ledger_{std::make_shared<Ledger>()};
  std::shared_ptr<Observable<Setup>> setup_{
      std::make_shared<Observable<Setup>>(Setup{})};
};

TEST_F(NodeTaskTest, StartsOnceTheNodeIsLive) {
  Tree tree{Ticker{setup_, ledger_}, owner(), services()};
  EXPECT_EQ(ledger_->starts, 0);
  tree.reconcile();
  EXPECT_EQ(ledger_->starts, 1);
  EXPECT_EQ(tree.running_tasks(), 1U);
  const auto ticking = tree.find("ticking");
  ASSERT_TRUE(ticking.has_value());
  EXPECT_EQ(tree.node(*ticking)->get().phase(), NodeLifecyclePhase::live);
  EXPECT_EQ(tree.node(*ticking)->get().running_tasks(), 1U);
  EXPECT_TRUE(manual_.step());
  EXPECT_EQ(manual_.pending(), 1U);
  EXPECT_EQ(ledger_->starts, 1);
}

TEST_F(NodeTaskTest, CancelsWhenTheNodeDeparts) {
  Tree tree{Ticker{setup_, ledger_}, owner(), services()};
  tree.reconcile();
  setup_->update([](Setup& setup) { setup.show = false; });
  tree.reconcile();
  EXPECT_EQ(ledger_->destroyed, 1);
  EXPECT_EQ(tree.running_tasks(), 0U);
  EXPECT_EQ(manual_.pending(), 0U);
}

TEST_F(NodeTaskTest, RestartsWhenTheKeyChangesAndOnlyThen) {
  Tree tree{Ticker{setup_, ledger_}, owner(), services()};
  tree.reconcile();
  setup_->update([](Setup& setup) { setup.finite = false; });
  tree.reconcile();
  EXPECT_EQ(ledger_->starts, 1);
  setup_->update([](Setup& setup) { setup.key = 2; });
  tree.reconcile();
  EXPECT_EQ(ledger_->starts, 2);
  EXPECT_EQ(ledger_->destroyed, 1);
  EXPECT_EQ(tree.running_tasks(), 1U);
  EXPECT_EQ(manual_.pending(), 1U);
}

TEST_F(NodeTaskTest, UnkeyedTasksSurviveUpdates) {
  setup_->set(Setup{.show = true, .key = 1, .keyed = false, .finite = false});
  Tree tree{Ticker{setup_, ledger_}, owner(), services()};
  tree.reconcile();
  setup_->update([](Setup& setup) { setup.key = 7; });
  tree.reconcile();
  EXPECT_EQ(ledger_->starts, 1);
  EXPECT_EQ(ledger_->destroyed, 0);
}

TEST_F(NodeTaskTest, FinishedTasksAreReleased) {
  setup_->set(Setup{.show = true, .key = 1, .keyed = true, .finite = true});
  Tree tree{Ticker{setup_, ledger_}, owner(), services()};
  tree.reconcile();
  EXPECT_EQ(ledger_->finishes, 1);
  tree.reconcile();
  EXPECT_EQ(tree.running_tasks(), 0U);
  EXPECT_EQ(ledger_->destroyed, 1);
}

TEST_F(NodeTaskTest, TasksSleepOnTheTreeScheduler) {
  Tree tree{Napper{ledger_}, owner(), services()};
  tree.reconcile();
  timeline_.fire();
  EXPECT_EQ(ledger_->finishes, 0);
  *now_ = 1s;
  timeline_.fire();
  EXPECT_EQ(ledger_->finishes, 1);
}

TEST_F(NodeTaskTest, CancelTasksStopsEveryNodeTask) {
  Tree tree{Ticker{setup_, ledger_}, owner(), services()};
  tree.reconcile();
  tree.cancel_tasks();
  EXPECT_EQ(ledger_->destroyed, 1);
  EXPECT_EQ(tree.running_tasks(), 0U);
}

TEST_F(NodeTaskTest, DestroyingTheTreeCancelsItsTasks) {
  {
    Tree tree{Ticker{setup_, ledger_}, owner(), services()};
    tree.reconcile();
  }
  EXPECT_EQ(ledger_->destroyed, 1);
  EXPECT_EQ(manual_.pending(), 0U);
}

}
