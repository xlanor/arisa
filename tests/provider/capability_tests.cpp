#include <arisa/provider/capability.hpp>

#include <gtest/gtest.h>

namespace arisa::provider {

TEST(CapabilityTest, CombinesAndContains) {
  const Capability set = Capability::clock | Capability::audio;
  EXPECT_TRUE(contains(set, Capability::clock));
  EXPECT_TRUE(contains(set, Capability::audio));
  EXPECT_FALSE(contains(set, Capability::browser));
  EXPECT_FALSE(contains(set, Capability::clock | Capability::browser));
}

TEST(CapabilityTest, NoneIsNeverContained) {
  EXPECT_FALSE(contains(Capability::all, Capability::none));
}

TEST(CapabilityTest, RequiredAndOptionalPartitionAll) {
  EXPECT_EQ(Capability::required | Capability::optional, Capability::all);
  EXPECT_EQ(Capability::required & Capability::optional, Capability::none);
  EXPECT_EQ(~Capability::required, Capability::optional);
}

TEST(CapabilityTest, FontsAreOptional) {
  EXPECT_TRUE(contains(Capability::optional, Capability::fonts));
  EXPECT_FALSE(contains(Capability::required, Capability::fonts));
}

TEST(CapabilityTest, PresentationCapabilitiesAreOptional) {
  for (const Capability capability :
       {Capability::surface, Capability::input, Capability::gpu}) {
    EXPECT_TRUE(contains(Capability::optional, capability));
    EXPECT_FALSE(contains(Capability::required, capability));
  }
}

TEST(CapabilityTest, TextInputIsOptional) {
  EXPECT_TRUE(contains(Capability::optional, Capability::text_input));
  EXPECT_FALSE(contains(Capability::required, Capability::text_input));
}

TEST(CapabilityTest, CompoundAssignment) {
  Capability set = Capability::none;
  set |= Capability::haptics;
  set |= Capability::lifecycle;
  set &= Capability::haptics;
  EXPECT_EQ(set, Capability::haptics);
  EXPECT_EQ(set ^ Capability::haptics, Capability::none);
}

}
