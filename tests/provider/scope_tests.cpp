#include <arisa/provider/lifecycle/home_button_block.hpp>
#include <arisa/provider/system_info/dimming_inhibit.hpp>

#include <gtest/gtest.h>

#include <utility>

namespace arisa::provider {

TEST(HomeButtonBlockTest, ReleasesOnceWhenDestroyed) {
  int released = 0;
  {
    HomeButtonBlock block{
        support::ScopedCallback{[&released]() noexcept { ++released; }}};
    EXPECT_TRUE(block.active());
    const HomeButtonBlock moved{std::move(block)};
    EXPECT_FALSE(block.active());
    EXPECT_TRUE(moved.active());
  }
  EXPECT_EQ(released, 1);
}

TEST(DimmingInhibitTest, ReleasesOnceWhenDestroyed) {
  int released = 0;
  {
    const DimmingInhibit inhibit{
        support::ScopedCallback{[&released]() noexcept { ++released; }}};
    EXPECT_TRUE(inhibit.active());
  }
  EXPECT_EQ(released, 1);
}

}
