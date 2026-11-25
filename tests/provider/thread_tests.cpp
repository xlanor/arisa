#include <arisa/provider/threading/thread.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <utility>

namespace arisa::provider {

class FakeThread final : public Thread::Implementation {
 public:
  FakeThread(int& stops, int& destroyed) noexcept
      : stops_(stops), destroyed_(destroyed) {}

  FakeThread(const FakeThread&) = delete;
  FakeThread(FakeThread&&) = delete;
  auto operator=(const FakeThread&) -> FakeThread& = delete;
  auto operator=(FakeThread&&) -> FakeThread& = delete;

  ~FakeThread() override { ++destroyed_; }

  void request_stop() noexcept override { ++stops_; }

  auto id() const noexcept -> ThreadId override { return ThreadId{77}; }

 private:
  int& stops_;
  int& destroyed_;
};

TEST(ThreadTest, ForwardsToImplementationAndReleasesIt) {
  int stops = 0;
  int destroyed = 0;
  {
    Thread thread{std::make_unique<FakeThread>(stops, destroyed)};
    EXPECT_TRUE(thread.joinable());
    EXPECT_EQ(thread.id(), ThreadId{77});
    thread.request_stop();
    EXPECT_EQ(stops, 1);
  }
  EXPECT_EQ(destroyed, 1);
}

TEST(ThreadTest, MovedFromHandleIsEmpty) {
  int stops = 0;
  int destroyed = 0;
  Thread source{std::make_unique<FakeThread>(stops, destroyed)};
  Thread target{std::move(source)};
  EXPECT_FALSE(source.joinable());
  EXPECT_EQ(source.id(), ThreadId{0});
  source.request_stop();
  EXPECT_EQ(stops, 0);
  EXPECT_TRUE(target.joinable());
}

}
