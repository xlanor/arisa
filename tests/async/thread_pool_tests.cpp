#include <arisa/async/manual_executor.hpp>
#include <arisa/async/thread_pool.hpp>
#include <arisa/provider/standard/std_threading.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>
#include <vector>

namespace arisa::async {

using namespace std::chrono_literals;

class ThreadPoolTest : public ::testing::Test {
 protected:
  static auto eventually(const std::function<bool()>& condition) -> bool {
    for (int attempt = 0; attempt < 5000; ++attempt) {
      if (condition()) {
        return true;
      }
      std::this_thread::sleep_for(1ms);
    }
    return condition();
  }

  auto single() -> ThreadPool {
    auto pool = ThreadPool::create(
        threading_, ThreadPool::Config{.workers = WorkerCount::of<1>(),
                                       .role = provider::ThreadRole::blocking,
                                       .name = "arisa-test"});
    EXPECT_TRUE(pool.has_value());
    return std::move(*pool);
  }

  class Gate {
   public:
    void open() {
      {
        const std::scoped_lock lock{mutex_};
        open_ = true;
      }
      changed_.notify_all();
    }

    void pass() {
      std::unique_lock lock{mutex_};
      changed_.wait(lock, [this] { return open_; });
    }

   private:
    std::mutex mutex_;
    std::condition_variable changed_;
    bool open_{false};
  };

  provider::standard::StdThreadingProvider threading_;
};

TEST_F(ThreadPoolTest, RunsJobsOnItsWorkers) {
  auto pool = ThreadPool::create(
      threading_, ThreadPool::Config{.workers = WorkerCount::of<3>(),
                                     .role = provider::ThreadRole::background,
                                     .name = "arisa-workers"});
  ASSERT_TRUE(pool.has_value());
  const std::vector<provider::ThreadId> workers = pool->workers();
  EXPECT_EQ(workers.size(), 3U);
  EXPECT_EQ(pool->role(), provider::ThreadRole::background);
  std::mutex mutex;
  std::vector<provider::ThreadId> seen;
  std::vector<Job> jobs;
  for (int index = 0; index < 32; ++index) {
    jobs.push_back(pool->submit([&](const std::stop_token&) {
      const std::scoped_lock lock{mutex};
      seen.push_back(threading_.current_thread());
    }));
  }
  ASSERT_TRUE(pool->wait().has_value());
  ASSERT_EQ(seen.size(), 32U);
  for (const provider::ThreadId id : seen) {
    EXPECT_TRUE(std::ranges::contains(workers, id));
  }
}

TEST_F(ThreadPoolTest, TakesTheHighestPriorityFirstAndFifoWithin) {
  ThreadPool pool = single();
  Gate gate;
  std::vector<int> order;
  std::vector<Job> jobs;
  jobs.push_back(pool.submit([&gate](const std::stop_token&) { gate.pass(); }));
  ASSERT_TRUE(eventually([&pool] { return pool.running() == 1; }));
  const auto record = [&order](int value) {
    return [&order, value](const std::stop_token&) { order.push_back(value); };
  };
  jobs.push_back(pool.submit(record(1), JobPriority::low));
  jobs.push_back(pool.submit(record(2), JobPriority::normal));
  jobs.push_back(pool.submit(record(3), JobPriority::high));
  jobs.push_back(pool.submit(record(4), JobPriority::normal));
  jobs.push_back(pool.submit(record(5), JobPriority::high));
  jobs.push_back(pool.submit(record(6), JobPriority::low));
  EXPECT_EQ(pool.pending(), 6U);
  gate.open();
  ASSERT_TRUE(pool.wait().has_value());
  EXPECT_EQ(order, (std::vector<int>{3, 5, 2, 4, 1, 6}));
}

TEST_F(ThreadPoolTest, ManualExecutorHonoursTheSameOrdering) {
  ManualExecutor executor;
  std::vector<int> order;
  std::vector<Job> jobs;
  const auto record = [&order](int value) {
    return [&order, value](const std::stop_token&) { order.push_back(value); };
  };
  jobs.push_back(executor.submit(record(1), JobPriority::low));
  jobs.push_back(executor.submit(record(2)));
  jobs.push_back(executor.submit(record(3), JobPriority::high));
  jobs.push_back(executor.submit(record(4)));
  jobs.push_back(executor.submit(record(5), JobPriority::high));
  EXPECT_EQ(executor.drain(), 5U);
  EXPECT_EQ(order, (std::vector<int>{3, 5, 2, 4, 1}));
}

TEST_F(ThreadPoolTest, PauseFinishesRunningJobsAndHoldsNewOnes) {
  ThreadPool pool = single();
  Gate gate;
  std::atomic<int> finished{0};
  std::vector<Job> jobs;
  jobs.push_back(pool.submit([&](const std::stop_token&) {
    gate.pass();
    ++finished;
  }));
  ASSERT_TRUE(eventually([&pool] { return pool.running() == 1; }));
  std::optional<ThreadPool::PauseScope> pause{pool.pause()};
  EXPECT_TRUE(pool.paused());
  jobs.push_back(pool.submit([&](const std::stop_token&) { ++finished; }));
  gate.open();
  ASSERT_TRUE(eventually([&] { return finished.load() == 1; }));
  std::this_thread::sleep_for(20ms);
  EXPECT_EQ(finished.load(), 1);
  EXPECT_EQ(pool.pending(), 1U);
  EXPECT_EQ(pool.running(), 0U);
  pause.reset();
  EXPECT_FALSE(pool.paused());
  ASSERT_TRUE(pool.wait().has_value());
  EXPECT_EQ(finished.load(), 2);
}

TEST_F(ThreadPoolTest, NestedPauseScopesResumeWhenTheLastEnds) {
  ThreadPool pool = single();
  std::atomic<int> runs{0};
  std::optional<ThreadPool::PauseScope> outer{pool.pause()};
  std::optional<ThreadPool::PauseScope> inner{pool.pause()};
  Job job = pool.submit([&runs](const std::stop_token&) { ++runs; });
  outer.reset();
  EXPECT_TRUE(pool.paused());
  std::this_thread::sleep_for(20ms);
  EXPECT_EQ(runs.load(), 0);
  inner.reset();
  EXPECT_FALSE(pool.paused());
  ASSERT_TRUE(pool.wait().has_value());
  EXPECT_EQ(runs.load(), 1);
}

TEST_F(ThreadPoolTest, WaitCompletesOnceEveryJobHasRun) {
  ThreadPool pool = single();
  std::atomic<int> runs{0};
  std::vector<Job> jobs;
  for (int index = 0; index < 100; ++index) {
    jobs.push_back(pool.submit([&runs](const std::stop_token&) {
      std::this_thread::sleep_for(50us);
      ++runs;
    }));
  }
  ASSERT_TRUE(pool.wait().has_value());
  EXPECT_EQ(runs.load(), 100);
  EXPECT_EQ(pool.pending(), 0U);
  EXPECT_EQ(pool.running(), 0U);
}

TEST_F(ThreadPoolTest, WaitFromAWorkerIsRejected) {
  ThreadPool pool = single();
  std::optional<std::expected<void, provider::ProviderError>> waited;
  Job job = pool.submit(
      [&pool, &waited](const std::stop_token&) { waited = pool.wait(); });
  ASSERT_TRUE(pool.wait().has_value());
  ASSERT_TRUE(waited.has_value());
  ASSERT_FALSE(waited->has_value());
  EXPECT_EQ(waited->error().code(),
            provider::ProviderErrorCode::invalid_argument);
}

TEST_F(ThreadPoolTest, DestroyingThePoolDropsPendingJobs) {
  auto captured = std::make_shared<int>(0);
  std::optional<ThreadPool::PauseScope> pause;
  std::optional<Job> job;
  {
    ThreadPool pool = single();
    pause.emplace(pool.pause());
    job.emplace(
        pool.submit([captured](const std::stop_token&) { ++*captured; }));
  }
  EXPECT_FALSE(job->started());
  EXPECT_EQ(*captured, 0);
}

}
