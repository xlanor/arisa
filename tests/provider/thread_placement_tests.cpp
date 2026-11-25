#include <arisa/provider/threading/thread_placement_policy.hpp>

#include <gtest/gtest.h>

#include <cstdint>

namespace arisa::provider {

TEST(ThreadPlacementTest, AcceptsValidPlacement) {
  const auto placement = ThreadPlacement::create(
      CoreMask{0b0011}, ThreadPriority::of<0x2C>(), StackSize{0x8000});
  ASSERT_TRUE(placement.has_value());
  EXPECT_EQ(placement->cores(), CoreMask{0b0011});
  EXPECT_EQ(placement->priority().value(), 0x2C);
  EXPECT_EQ(placement->stack(), StackSize{0x8000});
}

TEST(ThreadPlacementTest, RejectsEmptyCoreMask) {
  const auto placement = ThreadPlacement::create(
      CoreMask{0}, ThreadPriority::of<0x2C>(), StackSize{0x8000});
  ASSERT_FALSE(placement.has_value());
  EXPECT_EQ(placement.error().code(), ProviderErrorCode::invalid_argument);
}

TEST(ThreadPlacementTest, RejectsSmallOrUnalignedStacks) {
  EXPECT_FALSE(ThreadPlacement::create(CoreMask{1}, ThreadPriority::of<0>(),
                                       StackSize{0x1000})
                   .has_value());
  EXPECT_FALSE(ThreadPlacement::create(CoreMask{1}, ThreadPriority::of<0>(),
                                       StackSize{0x8001})
                   .has_value());
}

TEST(ThreadPriorityTest, RejectsOutOfRange) {
  EXPECT_FALSE(ThreadPriority::create(64).has_value());
  EXPECT_FALSE(ThreadPriority::create(-1).has_value());
  EXPECT_TRUE(ThreadPriority::create(63).has_value());
}

TEST(ThreadPlacementPolicyTest, DefaultsPlaceEveryRole) {
  const ThreadPlacementPolicy policy;
  for (const ThreadRole role : all_thread_roles) {
    EXPECT_NE(policy.placement(role).cores().value(), 0U);
    EXPECT_GE(policy.placement(role).stack().value(),
              ThreadPlacement::minimum_stack);
  }
  EXPECT_EQ(policy.placement(ThreadRole::ui).cores(), CoreMask{0b0001});
}

TEST(ThreadPlacementPolicyTest, WithReplacesOneRoleAndLeavesOriginal) {
  const ThreadPlacementPolicy policy;
  const auto custom = ThreadPlacement::create(
      CoreMask{0b1000}, ThreadPriority::of<10>(), StackSize{0x10000});
  ASSERT_TRUE(custom.has_value());
  const ThreadPlacementPolicy updated =
      policy.with(ThreadRole::background, *custom);
  EXPECT_EQ(updated.placement(ThreadRole::background), *custom);
  EXPECT_NE(policy.placement(ThreadRole::background), *custom);
  EXPECT_EQ(updated.placement(ThreadRole::ui),
            policy.placement(ThreadRole::ui));
  EXPECT_NE(updated, policy);
}

TEST(ThreadPlacementPolicyTest, RestrictedToKeepsRolesInsideAvailableCores) {
  const ThreadPlacementPolicy policy;
  const ThreadPlacementPolicy restricted =
      policy.restricted_to(CoreMask{0b0011});
  for (const ThreadRole role : all_thread_roles) {
    const std::uint64_t cores = restricted.placement(role).cores().value();
    EXPECT_NE(cores, 0U);
    EXPECT_EQ(cores & ~std::uint64_t{0b0011}, 0U);
    EXPECT_EQ(restricted.placement(role).priority(),
              policy.placement(role).priority());
    EXPECT_EQ(restricted.placement(role).stack(),
              policy.placement(role).stack());
  }
  EXPECT_EQ(restricted.placement(ThreadRole::ui).cores(), CoreMask{0b0001});
}

TEST(ThreadPlacementPolicyTest, RestrictedToFallsBackToEveryAvailableCore) {
  const ThreadPlacementPolicy restricted =
      ThreadPlacementPolicy{}.restricted_to(CoreMask{0b1000});
  for (const ThreadRole role : all_thread_roles) {
    EXPECT_EQ(restricted.placement(role).cores(), CoreMask{0b1000});
  }
}

TEST(ThreadPlacementPolicyTest, RestrictedToWiderOrEmptyMaskChangesNothing) {
  const ThreadPlacementPolicy policy;
  EXPECT_EQ(policy.restricted_to(CoreMask{0xFF}), policy);
  EXPECT_EQ(policy.restricted_to(CoreMask{0}), policy);
}

}
