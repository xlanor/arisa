#pragma once

#include <arisa/provider/gpu/image.hpp>
#include <arisa/provider/vulkan/offscreen_target.hpp>
#include <arisa/provider/vulkan/vulkan_gpu.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <format>
#include <string>

namespace arisa::provider::vulkan {

class VulkanTest : public ::testing::Test {
 protected:
  static constexpr int level_threshold = 16;
  static constexpr double maximum_off_fraction = 0.01;
  static constexpr double maximum_mean_difference = 1.0;

  struct Difference {
    std::size_t off{0};
    double mean{0.0};
    int worst{0};
    std::size_t changed{0};
    bool sized{true};

    auto within_tolerance(std::size_t pixels) const noexcept -> bool {
      return sized &&
             static_cast<double>(off) <=
                 static_cast<double>(pixels) * maximum_off_fraction &&
             mean <= maximum_mean_difference;
    }

    auto describe() const -> std::string {
      return std::format(
          "{} pixels differ at all, {} by more than {}, worst channel {}, "
          "mean difference {:.4f}",
          changed, off, level_threshold, worst, mean);
    }
  };

  static auto offscreen(OffscreenTarget::Options options)
      -> std::expected<VulkanGpu, ProviderError> {
    return VulkanGpu::create(options);
  }

  static auto skip_reason(const ProviderError& error) -> std::string {
    return std::format(
        "no Vulkan device through the loader: {} ({})", error.description(),
        static_cast<std::int32_t>(error.native_result().value_or(0)));
  }

  static auto compare(const Image& seen, const Image& expected) -> Difference {
    if (seen.width() != expected.width() ||
        seen.height() != expected.height()) {
      return Difference{
          .off = 0, .mean = 0.0, .worst = 0, .changed = 0, .sized = false};
    }
    const auto left = seen.pixels();
    const auto right = expected.pixels();
    std::size_t off = 0;
    std::size_t changed = 0;
    int worst_seen = 0;
    std::uint64_t total = 0;
    for (std::size_t at = 0; at + 3 < left.size(); at += 4) {
      int worst = 0;
      for (std::size_t channel = 0; channel < 3; ++channel) {
        const int difference =
            std::abs(std::to_integer<int>(left[at + channel]) -
                     std::to_integer<int>(right[at + channel]));
        total += static_cast<std::uint64_t>(difference);
        worst = std::max(worst, difference);
      }
      if (worst > level_threshold) {
        ++off;
      }
      if (worst > 0) {
        ++changed;
      }
      worst_seen = std::max(worst_seen, worst);
    }
    const std::size_t pixels =
        static_cast<std::size_t>(seen.width()) * seen.height();
    return Difference{
        .off = off,
        .mean = static_cast<double>(total) / static_cast<double>(pixels * 3),
        .worst = worst_seen,
        .changed = changed,
        .sized = true};
  }
};

}
