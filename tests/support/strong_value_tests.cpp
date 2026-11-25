#include <arisa/support/bounded_value.hpp>
#include <arisa/support/strong_value.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

namespace arisa::support {

struct ExampleTag;
struct PercentTag;

using Example = StrongValue<std::string, ExampleTag>;
using Percent = BoundedValue<std::uint8_t, 0, 100, PercentTag>;
using Signed = BoundedValue<int, -5, 5, PercentTag>;

TEST(StrongValueTest, ComparesByValue) {
  EXPECT_EQ(Example{"a"}, Example{"a"});
  EXPECT_LT(Example{"a"}, Example{"b"});
  EXPECT_EQ(Example{"arisa"}.value(), "arisa");
}

TEST(BoundedValueTest, AcceptsValuesInRange) {
  ASSERT_TRUE(Percent::create(0).has_value());
  ASSERT_TRUE(Percent::create(100).has_value());
  EXPECT_EQ(Percent::create(42)->value(), 42);
}

TEST(BoundedValueTest, RejectsValuesOutOfRange) {
  EXPECT_FALSE(Percent::create(101).has_value());
  EXPECT_FALSE(Signed::create(-6).has_value());
  EXPECT_FALSE(Signed::create(6).has_value());
  EXPECT_TRUE(Signed::create(-5).has_value());
}

TEST(BoundedValueTest, CompileTimeConstantMatchesCreate) {
  constexpr Percent fifty = Percent::of<50>();
  EXPECT_EQ(fifty, *Percent::create(50));
  EXPECT_EQ(Percent::minimum(), 0);
  EXPECT_EQ(Percent::maximum(), 100);
}

}
