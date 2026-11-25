#include <arisa/async/awaitables.hpp>
#include <arisa/async/frame_pool.hpp>
#include <arisa/async/frame_timeline.hpp>
#include <arisa/async/manual_executor.hpp>
#include <arisa/async/task.hpp>
#include <arisa/async/thread_pool.hpp>
#include <arisa/provider/standard/std_threading.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <print>
#include <random>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

namespace arisa::async {

using namespace std::chrono_literals;

class TaskTest : public ::testing::Test {
 protected:
  using Counter = std::shared_ptr<std::atomic<int>>;
  using ExecutorRef = std::reference_wrapper<Executor>;

  class Lifetime {
   public:
    explicit Lifetime(Counter destroyed) noexcept
        : destroyed_(std::move(destroyed)) {}

    Lifetime(const Lifetime&) = delete;
    auto operator=(const Lifetime&) -> Lifetime& = delete;
    Lifetime(Lifetime&&) = delete;
    auto operator=(Lifetime&&) -> Lifetime& = delete;

    ~Lifetime() { ++*destroyed_; }

   private:
    Counter destroyed_;
  };

  static auto counter() -> Counter {
    return std::make_shared<std::atomic<int>>(0);
  }

  static auto eventually(const std::function<bool()>& condition) -> bool {
    for (int attempt = 0; attempt < 10000; ++attempt) {
      if (condition()) {
        return true;
      }
      std::this_thread::sleep_for(1ms);
    }
    return condition();
  }

  static auto answer() -> Task<int> { co_return 42; }

  static auto add(int left, int right) -> Task<int> {
    const int base = co_await answer();
    co_return base + left + right;
  }

  static auto flag(std::shared_ptr<bool> ran) -> Task<void> {
    *ran = true;
    co_return;
  }

  static auto checked(int value) -> Task<std::expected<int, std::string>> {
    if (value < 0) {
      co_return std::unexpected(std::string{"negative"});
    }
    co_return value * 2;
  }

  static auto forward(int value) -> Task<std::expected<int, std::string>> {
    auto doubled = co_await checked(value);
    if (!doubled.has_value()) {
      co_return std::unexpected("forwarded " + doubled.error());
    }
    co_return *doubled + 1;
  }

  static auto hop(ExecutorRef executor, Counter destroyed,
                  std::shared_ptr<std::atomic<bool>> resumed) -> Task<void> {
    const Lifetime lifetime{std::move(destroyed)};
    co_await on(executor.get());
    resumed->store(true);
  }

  static auto parent(ExecutorRef executor, Counter destroyed) -> Task<void> {
    const Lifetime lifetime{destroyed};
    co_await hop(executor, destroyed, std::make_shared<std::atomic<bool>>());
  }

  static auto where(
      ExecutorRef executor,
      std::reference_wrapper<const provider::ThreadingProvider> threading)
      -> Task<provider::ThreadId> {
    co_await on(executor.get());
    co_return threading.get().current_thread();
  }

  static auto chain(int depth) -> Task<int> {
    if (depth == 0) {
      co_return 0;
    }
    const int below = co_await chain(depth - 1);
    co_return below + 1;
  }

  static auto parked_chain(int depth, ExecutorRef executor, Counter destroyed)
      -> Task<int> {
    const Lifetime lifetime{destroyed};
    if (depth == 0) {
      co_await on(executor.get());
      co_return 0;
    }
    const int below = co_await parked_chain(depth - 1, executor, destroyed);
    co_return below + 1;
  }

  static auto doze(std::chrono::nanoseconds delay, Counter destroyed,
                   std::shared_ptr<std::atomic<bool>> woke) -> Task<void> {
    const Lifetime lifetime{std::move(destroyed)};
    if (delay.count() < 0) {
      co_await next_frame();
    } else {
      co_await sleep(delay);
    }
    woke->store(true);
  }

  static auto spin(ExecutorRef pool, Counter destroyed,
                   std::shared_ptr<std::atomic<bool>> spinning,
                   std::shared_ptr<std::atomic<bool>> observed) -> Task<void> {
    const Lifetime lifetime{std::move(destroyed)};
    co_await on(pool.get());
    const std::stop_token token = co_await cancellation();
    spinning->store(true);
    while (!co_await stop_requested()) {
      std::this_thread::yield();
    }
    observed->store(token.stop_requested());
    co_await on(pool.get());
    observed->store(false);
  }

  static auto ping_pong(
      ExecutorRef pool, ExecutorRef home, int hops,
      std::reference_wrapper<const provider::ThreadingProvider> threading,
      provider::ThreadId home_id, std::shared_ptr<std::atomic<int>> wrong)
      -> Task<int> {
    for (int hop = 0; hop < hops; ++hop) {
      co_await on(pool.get());
      if (threading.get().current_thread() == home_id) {
        ++*wrong;
      }
      co_await on(home.get());
      if (threading.get().current_thread() != home_id) {
        ++*wrong;
      }
    }
    co_return hops;
  }

  static auto churn(ExecutorRef pool, ExecutorRef home, Counter destroyed)
      -> Task<void> {
    const Lifetime lifetime{std::move(destroyed)};
    while (true) {
      co_await on(pool.get());
      co_await on(home.get());
    }
  }

  static auto ordered(ExecutorRef executor, int label,
                      std::shared_ptr<std::vector<int>> order) -> Task<void> {
    for (int round = 0; round < 3; ++round) {
      co_await on(executor.get());
      order->push_back((label * 10) + round);
    }
  }

  provider::standard::StdThreadingProvider threading_;
};

TEST_F(TaskTest, StartsLazily) {
  auto ran = std::make_shared<bool>(false);
  Task<void> task = flag(ran);
  EXPECT_FALSE(*ran);
  EXPECT_FALSE(task.started());
  task.start();
  EXPECT_TRUE(*ran);
  EXPECT_TRUE(task.done());
}

TEST_F(TaskTest, ReturnsValuesAndChainsChildren) {
  Task<int> task = add(1, 2);
  task.start();
  ASSERT_TRUE(task.done());
  EXPECT_EQ(task.result(), 45);
}

TEST_F(TaskTest, PropagatesExpectedResults) {
  Task<std::expected<int, std::string>> good = forward(4);
  Task<std::expected<int, std::string>> bad = forward(-1);
  good.start();
  bad.start();
  ASSERT_TRUE(good.done());
  ASSERT_TRUE(bad.done());
  EXPECT_EQ(good.result(), 9);
  ASSERT_FALSE(bad.result().has_value());
  EXPECT_EQ(bad.result().error(), "forwarded negative");
}

TEST_F(TaskTest, ResumesOnlyWhenTheManualExecutorSteps) {
  ManualExecutor executor;
  const Counter destroyed = counter();
  auto resumed = std::make_shared<std::atomic<bool>>(false);
  Task<void> task = hop(executor, destroyed, resumed);
  task.start();
  EXPECT_FALSE(task.done());
  EXPECT_EQ(executor.pending(), 1U);
  EXPECT_TRUE(executor.step());
  EXPECT_TRUE(resumed->load());
  EXPECT_TRUE(task.done());
  EXPECT_EQ(destroyed->load(), 1);
}

TEST_F(TaskTest, ManualExecutorInterleavesDeterministically) {
  ManualExecutor executor;
  auto order = std::make_shared<std::vector<int>>();
  Task<void> first = ordered(executor, 1, order);
  Task<void> second = ordered(executor, 2, order);
  first.start();
  second.start();
  EXPECT_EQ(executor.drain(), 6U);
  EXPECT_EQ(*order, (std::vector<int>{10, 20, 11, 21, 12, 22}));
}

TEST_F(TaskTest, ResumesOnTheRequestedPoolThread) {
  auto pool = ThreadPool::create(
      threading_, ThreadPool::Config{.workers = WorkerCount::of<2>(),
                                     .role = provider::ThreadRole::background,
                                     .name = "arisa-task"});
  ASSERT_TRUE(pool.has_value());
  Task<provider::ThreadId> task = where(*pool, std::cref(threading_));
  task.start();
  ASSERT_TRUE(eventually([&task] { return task.done(); }));
  EXPECT_TRUE(std::ranges::contains(pool->workers(), task.result()));
  EXPECT_NE(task.result(), threading_.current_thread());
}

TEST_F(TaskTest, DestroyingATaskParkedOnAnExecutorCancelsIt) {
  ManualExecutor executor;
  const Counter destroyed = counter();
  auto resumed = std::make_shared<std::atomic<bool>>(false);
  {
    Task<void> task = hop(executor, destroyed, resumed);
    task.start();
    EXPECT_EQ(executor.pending(), 1U);
  }
  EXPECT_EQ(destroyed->load(), 1);
  EXPECT_EQ(executor.pending(), 0U);
  EXPECT_FALSE(executor.step());
  EXPECT_FALSE(resumed->load());
}

TEST_F(TaskTest, DestroyingAParentCancelsTheAwaitedChild) {
  ManualExecutor executor;
  const Counter destroyed = counter();
  {
    Task<void> task = parent(executor, destroyed);
    task.start();
  }
  EXPECT_EQ(destroyed->load(), 2);
  EXPECT_FALSE(executor.step());
}

TEST_F(TaskTest, SleepAndNextFrameResumeOnlyFromTheTimeline) {
  auto now = std::make_shared<std::chrono::nanoseconds>(0);
  FrameTimeline timeline{[now] { return *now; }};
  const Counter destroyed = counter();
  auto slept = std::make_shared<std::atomic<bool>>(false);
  auto framed = std::make_shared<std::atomic<bool>>(false);
  Task<void> sleeper = doze(1s, destroyed, slept);
  Task<void> framer = doze(-1ns, destroyed, framed);
  sleeper.start(timeline);
  framer.start(timeline);
  EXPECT_EQ(timeline.pending(), 2U);
  timeline.fire();
  EXPECT_TRUE(framer.done());
  EXPECT_FALSE(sleeper.done());
  *now = 2s;
  timeline.fire();
  EXPECT_TRUE(sleeper.done());
  EXPECT_TRUE(slept->load());
  EXPECT_EQ(destroyed->load(), 2);
}

TEST_F(TaskTest, DestroyingATaskAsleepOrWaitingForAFrameCancelsIt) {
  auto now = std::make_shared<std::chrono::nanoseconds>(0);
  FrameTimeline timeline{[now] { return *now; }};
  const Counter destroyed = counter();
  auto woke = std::make_shared<std::atomic<bool>>(false);
  {
    Task<void> sleeper = doze(1s, destroyed, woke);
    Task<void> framer = doze(-1ns, destroyed, woke);
    sleeper.start(timeline);
    framer.start(timeline);
  }
  EXPECT_EQ(destroyed->load(), 2);
  EXPECT_EQ(timeline.pending(), 0U);
  *now = 5s;
  EXPECT_EQ(timeline.fire(), 0U);
  EXPECT_FALSE(woke->load());
}

TEST_F(TaskTest, RunningTasksObserveTheirStopTokenAndDieAtTheNextSuspension) {
  auto pool = ThreadPool::create(
      threading_, ThreadPool::Config{.workers = WorkerCount::of<1>(),
                                     .role = provider::ThreadRole::background,
                                     .name = "arisa-spin"});
  ASSERT_TRUE(pool.has_value());
  const Counter destroyed = counter();
  auto spinning = std::make_shared<std::atomic<bool>>(false);
  auto observed = std::make_shared<std::atomic<bool>>(false);
  {
    Task<void> task = spin(*pool, destroyed, spinning, observed);
    task.start();
    ASSERT_TRUE(eventually([&spinning] { return spinning->load(); }));
    EXPECT_FALSE(task.stop_token().stop_requested());
  }
  ASSERT_TRUE(eventually([&destroyed] { return destroyed->load() == 1; }));
  EXPECT_TRUE(observed->load());
}

TEST_F(TaskTest, ReusesPooledFrames) {
  const FramePool::Statistics before = FramePool::shared().statistics();
  for (int index = 0; index < 1000; ++index) {
    Task<int> task = add(index, 0);
    task.start();
    ASSERT_EQ(task.result(), 42 + index);
  }
  const FramePool::Statistics after = FramePool::shared().statistics();
  EXPECT_GE(after.frames - before.frames, 2000U);
  EXPECT_LE(after.upstream_allocations - before.upstream_allocations, 16U);
  EXPECT_EQ(after.live_frames, before.live_frames);
  std::println(
      "frame pool: smallest {} bytes, largest {} bytes, {} upstream "
      "allocations for {} frames",
      after.smallest_frame, after.largest_frame, after.upstream_allocations,
      after.frames);
}

TEST_F(TaskTest, SurvivesTenThousandDeepChains) {
  Task<int> task = chain(10000);
  task.start();
  ASSERT_TRUE(task.done());
  EXPECT_EQ(task.result(), 10000);
}

TEST_F(TaskTest, CancelsATenThousandDeepSuspendedChain) {
  ManualExecutor executor;
  const Counter destroyed = counter();
  {
    Task<int> task = parked_chain(10000, executor, destroyed);
    task.start();
    EXPECT_EQ(executor.pending(), 1U);
    EXPECT_EQ(destroyed->load(), 0);
  }
  EXPECT_EQ(destroyed->load(), 10001);
  EXPECT_FALSE(executor.step());
}

TEST_F(TaskTest, HopsAThousandTimesBetweenAPoolAndTheHomeExecutor) {
  auto pool = ThreadPool::create(
      threading_, ThreadPool::Config{.workers = WorkerCount::of<2>(),
                                     .role = provider::ThreadRole::background,
                                     .name = "arisa-hop"});
  ASSERT_TRUE(pool.has_value());
  ManualExecutor home;
  auto wrong = std::make_shared<std::atomic<int>>(0);
  Task<int> task = ping_pong(*pool, home, 1000, std::cref(threading_),
                             threading_.current_thread(), wrong);
  task.start();
  ASSERT_TRUE(eventually([&] {
    home.drain();
    return task.done();
  }));
  EXPECT_EQ(task.result(), 1000);
  EXPECT_EQ(wrong->load(), 0);
}

TEST_F(TaskTest, SurvivesACancellationStorm) {
  auto pool = ThreadPool::create(
      threading_, ThreadPool::Config{.workers = WorkerCount::of<4>(),
                                     .role = provider::ThreadRole::background,
                                     .name = "arisa-storm"});
  ASSERT_TRUE(pool.has_value());
  ManualExecutor home;
  const Counter destroyed = counter();
  constexpr int tasks = 250;
  {
    std::vector<Task<void>> running;
    running.reserve(tasks);
    for (int index = 0; index < tasks; ++index) {
      running.push_back(churn(*pool, home, destroyed));
      running.back().start();
    }
    for (int round = 0; round < 5; ++round) {
      home.drain();
      std::this_thread::yield();
    }
    std::mt19937 shuffle{20261107U};
    std::ranges::shuffle(running, shuffle);
    while (!running.empty()) {
      running.pop_back();
      if (running.size() % 97 == 0) {
        home.drain();
      }
    }
  }
  ASSERT_TRUE(eventually([&] {
    home.drain();
    return destroyed->load() == tasks;
  }));
  ASSERT_TRUE(pool->wait().has_value());
  EXPECT_EQ(home.drain(), 0U);
}

TEST_F(TaskTest, ExhaustingTheFramePoolTerminates) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  EXPECT_DEATH(
      {
        FramePool::shared().limit(FramePool::shared().statistics().live_bytes +
                                  8);
        Task<int> task = answer();
        task.start();
      },
      "frame pool exhausted");
}

TEST_F(TaskTest, AFramePoolLimitWithinBudgetIsHarmless) {
  FramePool::shared().limit(FramePool::shared().statistics().live_bytes +
                            (64U * 1024U));
  Task<int> task = add(1, 1);
  task.start();
  FramePool::shared().limit(std::nullopt);
  EXPECT_EQ(task.result(), 44);
}

}
