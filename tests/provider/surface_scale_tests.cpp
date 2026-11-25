#include <arisa/provider/surface/absent.hpp>
#include <arisa/provider/surface/surface_scale.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

namespace arisa::provider {

class SurfaceScaleTest : public ::testing::Test {
 protected:
  static auto size(std::uint32_t width, std::uint32_t height) -> SurfaceSize {
    return SurfaceSize{.width = SurfaceWidth{width},
                       .height = SurfaceHeight{height}};
  }
};

TEST_F(SurfaceScaleTest, HandheldIsUnitAndDockedIsOneAndAHalf) {
  EXPECT_FLOAT_EQ(SurfaceScale::for_pixels(size(1280, 720)).value(), 1.0F);
  EXPECT_FLOAT_EQ(SurfaceScale::for_pixels(size(1920, 1080)).value(), 1.5F);
  EXPECT_FLOAT_EQ(SurfaceScale::for_pixels(size(2560, 1440)).value(), 2.0F);
}

TEST_F(SurfaceScaleTest, UsesTheTighterAxisAndAFloor) {
  EXPECT_FLOAT_EQ(SurfaceScale::for_pixels(size(2560, 720)).value(), 1.0F);
  EXPECT_FLOAT_EQ(SurfaceScale::for_pixels(size(16, 16)).value(),
                  SurfaceScale::minimum);
  EXPECT_EQ(SurfaceScale::for_pixels(size(0, 720)), SurfaceScale::unit());
}

TEST_F(SurfaceScaleTest, CreateRejectsNonPositiveAndNonFinite) {
  EXPECT_FALSE(SurfaceScale::create(0.0F).has_value());
  EXPECT_FALSE(SurfaceScale::create(-1.0F).has_value());
  EXPECT_FALSE(
      SurfaceScale::create(std::numeric_limits<float>::infinity()).has_value());
  EXPECT_FALSE(SurfaceScale::create(std::numeric_limits<float>::quiet_NaN())
                   .has_value());
  ASSERT_TRUE(SurfaceScale::create(1.25F).has_value());
  EXPECT_FLOAT_EQ(SurfaceScale::create(1.25F)->value(), 1.25F);
}

TEST_F(SurfaceScaleTest, SizeIsEmptyWhenEitherAxisIsZero) {
  EXPECT_TRUE(SurfaceSize{}.empty());
  EXPECT_TRUE(size(10, 0).empty());
  EXPECT_FALSE(size(1, 1).empty());
}

}
