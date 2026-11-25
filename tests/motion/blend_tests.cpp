#include <arisa/motion/animatable_vector.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/animation_track.hpp>
#include <arisa/motion/spring.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::motion {

class BlendTest : public ::testing::Test {
 protected:
  static constexpr double retarget_at = 0.2;
  static constexpr double start_response = 0.6;

  static auto reference() -> const std::map<std::string, double>& {
    static const std::map<std::string, double> rows = [] {
      std::map<std::string, double> found;
      std::ifstream stream{ARISA_SWIFTUI_BLEND};
      std::string line;
      while (std::getline(stream, line)) {
        if (line.empty() || line.front() == '#') {
          continue;
        }
        std::istringstream fields{line};
        std::string key;
        std::string light;
        std::getline(fields, key, '\t');
        std::getline(fields, light, '\t');
        if (key.starts_with("motion.blend_")) {
          found[key] = std::stod(light);
        }
      }
      return found;
    }();
    return rows;
  }

  static auto nanoseconds(double seconds) -> std::chrono::nanoseconds {
    return std::chrono::nanoseconds{std::llround(seconds * 1.0e9)};
  }

  static auto retargeted(const Animation& next, double time) -> double {
    const AnimationTrack first{Animation::spring(start_response, 1.0),
                               Animation::Span{.from = AnimatableVector{0.0},
                                               .to = AnimatableVector{1.0},
                                               .velocity = {},
                                               .precision = 1.0e-9},
                               nanoseconds(0.0), nullptr};
    if (time < retarget_at) {
      return first.sample(nanoseconds(time)).value[0];
    }
    const Animation::Sample current = first.sample(nanoseconds(retarget_at));
    const AnimationTrack second{next,
                                Animation::Span{.from = current.value,
                                                .to = AnimatableVector{2.0},
                                                .velocity = current.velocity,
                                                .precision = 1.0e-9},
                                nanoseconds(retarget_at), nullptr,
                                first.spring_at(nanoseconds(retarget_at))};
    return second.sample(nanoseconds(time)).value[0];
  }

  static auto error(const Animation& next, std::string_view series) -> double {
    double worst = 0.0;
    std::size_t matched = 0;
    const std::string prefix = std::format("motion.blend_{}.sample.", series);
    for (const auto& [key, value] : reference()) {
      if (!key.starts_with(prefix)) {
        continue;
      }
      const double time = std::stod(key.substr(prefix.size())) / 1000.0;
      worst = std::max(worst, std::abs(retargeted(next, time) - value));
      ++matched;
    }
    EXPECT_GT(matched, 50U) << series;
    return worst;
  }
};

TEST_F(BlendTest, RetargetingWithoutABlendMatchesSwiftUI) {
  EXPECT_LT(error(Animation::spring(start_response, 1.0), "same_spring"), 0.05);
  EXPECT_LT(error(Animation::spring(0.2, 1.0, 0.0), "response_0_2_blend_0"),
            0.07);
}

TEST_F(BlendTest, BlendDurationInterpolatesTheResponseAsSwiftUIDoes) {
  EXPECT_LT(error(Animation::spring(0.2, 1.0, 0.25), "response_0_2_blend_0_25"),
            0.05);
  EXPECT_LT(error(Animation::spring(0.2, 1.0, 0.5), "response_0_2_blend_0_5"),
            0.05);
  EXPECT_LT(error(Animation::spring(0.2, 1.0, 1.0), "response_0_2_blend_1"),
            0.05);
}

TEST_F(BlendTest, DampingSwitchesAtTheRetargetWhileTheResponseBlends) {
  EXPECT_LT(error(Animation::spring(0.6, 0.5, 0.5), "damping_0_5_blend_0_5"),
            0.05);
}

TEST_F(BlendTest, ABlendChangesTheCurveItWasIgnoredOnBefore) {
  double widest = 0.0;
  for (double time = retarget_at; time < 1.2; time += 0.02) {
    widest = std::max(
        widest, std::abs(retargeted(Animation::spring(0.2, 1.0, 0.5), time) -
                         retargeted(Animation::spring(0.2, 1.0, 0.0), time)));
  }
  EXPECT_GT(widest, 0.2);
}

TEST_F(BlendTest, ABlendedTrackSettlesOnItsTarget) {
  EXPECT_NEAR(retargeted(Animation::spring(0.2, 1.0, 0.5), 3.0), 2.0, 1.0e-3);
}

TEST_F(BlendTest, ATrackWithoutAPreviousSpringIgnoresTheBlend) {
  const AnimationTrack plain{Animation::spring(0.2, 1.0, 0.5),
                             Animation::Span{.from = AnimatableVector{0.0},
                                             .to = AnimatableVector{1.0},
                                             .velocity = {},
                                             .precision = 1.0e-9},
                             nanoseconds(0.0), nullptr};
  const AnimationTrack reference_track{
      Animation::spring(0.2, 1.0),
      Animation::Span{.from = AnimatableVector{0.0},
                      .to = AnimatableVector{1.0},
                      .velocity = {},
                      .precision = 1.0e-9},
      nanoseconds(0.0), nullptr};
  EXPECT_FALSE(plain.blended_from().has_value());
  EXPECT_DOUBLE_EQ(plain.sample(nanoseconds(0.1)).value[0],
                   reference_track.sample(nanoseconds(0.1)).value[0]);
}

}
