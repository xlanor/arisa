#include <arisa/support/scoped_callback.hpp>
#include <arisa/support/unique_handle.hpp>

#include <gtest/gtest.h>

#include <utility>

namespace arisa::support {

class CountingRelease {
 public:
  using handle_type = int;

  static auto empty() noexcept -> handle_type { return -1; }

  static auto is_valid(const handle_type& handle) noexcept -> bool {
    return handle >= 0;
  }

  static void release(handle_type&) noexcept { ++released; }

  static inline int released = 0;
};

class UniqueHandleTest : public ::testing::Test {
 protected:
  void SetUp() override { CountingRelease::released = 0; }
};

TEST_F(UniqueHandleTest, ReleasesOnDestruction) {
  {
    const UniqueHandle<CountingRelease> handle{3};
    EXPECT_TRUE(handle.valid());
    EXPECT_EQ(handle.get(), 3);
  }
  EXPECT_EQ(CountingRelease::released, 1);
}

TEST_F(UniqueHandleTest, DefaultIsEmptyAndReleasesNothing) {
  {
    const UniqueHandle<CountingRelease> handle;
    EXPECT_FALSE(handle);
  }
  EXPECT_EQ(CountingRelease::released, 0);
}

TEST_F(UniqueHandleTest, MoveTransfersOwnership) {
  UniqueHandle<CountingRelease> source{4};
  UniqueHandle<CountingRelease> target{std::move(source)};
  EXPECT_FALSE(source.valid());
  EXPECT_EQ(target.get(), 4);
  target.reset();
  EXPECT_EQ(CountingRelease::released, 1);
  EXPECT_FALSE(target.valid());
}

TEST_F(UniqueHandleTest, MoveAssignmentReleasesPrevious) {
  UniqueHandle<CountingRelease> first{1};
  UniqueHandle<CountingRelease> second{2};
  first = std::move(second);
  EXPECT_EQ(CountingRelease::released, 1);
  EXPECT_EQ(first.get(), 2);
}

TEST(ScopedCallbackTest, RunsCallbackExactlyOnce) {
  int calls = 0;
  {
    ScopedCallback outer{[&calls]() noexcept { ++calls; }};
    const ScopedCallback inner{std::move(outer)};
    EXPECT_FALSE(outer.valid());
    EXPECT_TRUE(inner.valid());
  }
  EXPECT_EQ(calls, 1);
}

}
