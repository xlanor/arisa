#include <arisa/provider/standard/std_threading.hpp>

#include <gtest/gtest.h>

#include <pthread.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <optional>
#include <thread>
#include <utility>

namespace arisa::provider::standard {

TEST(StdThreadingProviderTest, ThreadHandleJoinsWhenItLeavesScope) {
  StdThreadingProvider threading;
  std::atomic<bool> finished{false};
  {
    const auto thread = threading.start(
        ThreadSpec{.name = "join-test", .role = ThreadRole::background},
        [&finished](const std::stop_token&) {
          std::this_thread::sleep_for(std::chrono::milliseconds{20});
          finished.store(true);
        });
    ASSERT_TRUE(thread.has_value());
    EXPECT_TRUE(thread->joinable());
  }
  EXPECT_TRUE(finished.load());
}

TEST(StdThreadingProviderTest, DestructionRequestsStop) {
  StdThreadingProvider threading;
  std::atomic<bool> observed{false};
  {
    const auto thread = threading.start(
        ThreadSpec{.name = "stop-test", .role = ThreadRole::blocking},
        [&observed](const std::stop_token& token) {
          while (!token.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
          }
          observed.store(true);
        });
    ASSERT_TRUE(thread.has_value());
  }
  EXPECT_TRUE(observed.load());
}

TEST(StdThreadingProviderTest, RejectsEmptyNameAndEmptyBody) {
  StdThreadingProvider threading;
  const auto unnamed =
      threading.start(ThreadSpec{.name = "", .role = ThreadRole::ui},
                      [](const std::stop_token&) {});
  ASSERT_FALSE(unnamed.has_value());
  EXPECT_EQ(unnamed.error().code(), ProviderErrorCode::invalid_argument);
  const auto bodiless = threading.start(
      ThreadSpec{.name = "empty", .role = ThreadRole::ui}, ThreadBody{});
  ASSERT_FALSE(bodiless.has_value());
  EXPECT_EQ(bodiless.error().code(), ProviderErrorCode::invalid_argument);
}

TEST(StdThreadingProviderTest, IdentifiesThreads) {
  StdThreadingProvider threading;
  std::atomic<std::uint64_t> seen{0};
  std::optional<ThreadId> handle_id;
  {
    const auto thread =
        threading.start(ThreadSpec{.name = "id-test", .role = ThreadRole::app},
                        [&threading, &seen](const std::stop_token&) {
                          seen.store(threading.current_thread().value());
                        });
    ASSERT_TRUE(thread.has_value());
    handle_id = thread->id();
  }
  EXPECT_EQ(ThreadId{seen.load()}, handle_id);
  EXPECT_NE(threading.current_thread(), handle_id);
  EXPECT_EQ(threading.current_thread(), threading.current_thread());
}

TEST(StdThreadingProviderTest, KeepsTheGivenPolicy) {
  const auto custom = ThreadPlacement::create(
      CoreMask{0b0001}, ThreadPriority::of<20>(), StackSize{0x10000});
  ASSERT_TRUE(custom.has_value());
  const ThreadPlacementPolicy policy =
      ThreadPlacementPolicy{}.with(ThreadRole::ui, *custom);
  const StdThreadingProvider threading{policy};
  EXPECT_EQ(threading.placement(), policy);
}

TEST(StdThreadingProviderTest, RunsEachThreadOnTheStackItsRoleAsksFor) {
  constexpr std::size_t asked = std::size_t{4} << 20U;
  const auto placement = ThreadPlacement::create(
      CoreMask{0b0001}, ThreadPriority::of<20>(), StackSize{asked});
  ASSERT_TRUE(placement.has_value());
  StdThreadingProvider threading{
      ThreadPlacementPolicy{}.with(ThreadRole::background, *placement)};
  std::atomic<std::size_t> measured{0};
  {
    const auto thread = threading.start(
        ThreadSpec{.name = "stack-test", .role = ThreadRole::background},
        [&measured](const std::stop_token&) {
#if defined(__APPLE__)
          measured.store(pthread_get_stacksize_np(pthread_self()));
#else
          pthread_attr_t attributes;
          std::size_t size = 0;
          if (pthread_getattr_np(pthread_self(), &attributes) == 0) {
            pthread_attr_getstacksize(&attributes, &size);
            pthread_attr_destroy(&attributes);
          }
          measured.store(size);
#endif
        });
    ASSERT_TRUE(thread.has_value());
  }
  EXPECT_GE(measured.load(), asked);
}

TEST(StdThreadingProviderTest, GivesEveryRoleTheDefaultThreadStack) {
  const ThreadPlacementPolicy policy;
  for (const ThreadRole role : all_thread_roles) {
    EXPECT_EQ(policy.placement(role).stack().value(), 0x20000U);
  }
}

class StdThreadingSelfDestructionTest : public ::testing::Test {
 protected:
  static auto wait_for(const std::atomic<bool>& flag) -> bool {
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds{5};
    while (!flag.load()) {
      if (std::chrono::steady_clock::now() > deadline) {
        return false;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    return true;
  }

  static auto start_self_destroying(StdThreadingProvider& threading,
                                    std::optional<Thread>& handle,
                                    std::atomic<bool>& handed_over,
                                    std::atomic<bool>& destroyed,
                                    std::atomic<bool>& exited,
                                    std::chrono::milliseconds linger) -> bool {
    auto thread = threading.start(
        ThreadSpec{.name = "self-test", .role = ThreadRole::background},
        [&, linger](const std::stop_token& token) {
          if (!wait_for(handed_over)) {
            return;
          }
          handle.reset();
          destroyed.store(token.stop_requested());
          std::this_thread::sleep_for(linger);
          exited.store(true);
        });
    if (!thread.has_value()) {
      return false;
    }
    handle.emplace(std::move(*thread));
    handed_over.store(true);
    return true;
  }
};

TEST_F(StdThreadingSelfDestructionTest, HandsTheThreadToTheOrphanList) {
  StdThreadingProvider threading;
  std::optional<Thread> handle;
  std::atomic<bool> handed_over{false};
  std::atomic<bool> destroyed{false};
  std::atomic<bool> exited{false};
  ASSERT_TRUE(start_self_destroying(threading, handle, handed_over, destroyed,
                                    exited, std::chrono::milliseconds{0}));
  ASSERT_TRUE(wait_for(exited));
  EXPECT_TRUE(destroyed.load());
  EXPECT_FALSE(handle.has_value());
  EXPECT_EQ(threading.orphans(), 1U);
}

TEST_F(StdThreadingSelfDestructionTest, StartReapsFinishedOrphans) {
  StdThreadingProvider threading;
  std::optional<Thread> handle;
  std::atomic<bool> handed_over{false};
  std::atomic<bool> destroyed{false};
  std::atomic<bool> exited{false};
  ASSERT_TRUE(start_self_destroying(threading, handle, handed_over, destroyed,
                                    exited, std::chrono::milliseconds{0}));
  ASSERT_TRUE(wait_for(exited));
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds{5};
  while (threading.orphans() > 0 &&
         std::chrono::steady_clock::now() < deadline) {
    const auto reaper = threading.start(
        ThreadSpec{.name = "reaper", .role = ThreadRole::background},
        [](const std::stop_token&) {});
    ASSERT_TRUE(reaper.has_value());
  }
  EXPECT_EQ(threading.orphans(), 0U);
}

TEST_F(StdThreadingSelfDestructionTest, ProviderJoinsRemainingOrphans) {
  std::optional<Thread> handle;
  std::atomic<bool> handed_over{false};
  std::atomic<bool> destroyed{false};
  std::atomic<bool> exited{false};
  {
    StdThreadingProvider threading;
    ASSERT_TRUE(start_self_destroying(threading, handle, handed_over, destroyed,
                                      exited, std::chrono::milliseconds{50}));
    ASSERT_TRUE(wait_for(destroyed));
  }
  EXPECT_TRUE(exited.load());
}

}
