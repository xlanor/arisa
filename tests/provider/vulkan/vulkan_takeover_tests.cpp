#include <arisa/provider/gpu/swap_interval.hpp>
#include <arisa/provider/vulkan/offscreen_target.hpp>
#include <arisa/provider/vulkan/vulkan_device.hpp>
#include <arisa/provider/vulkan/vulkan_gpu.hpp>
#include <arisa/provider/vulkan/vulkan_instance.hpp>

#include "vulkan_test.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace arisa::provider::vulkan {

class VulkanTakeoverTest : public VulkanTest {
 protected:
  static constexpr SurfaceSize small{.width = SurfaceWidth{64},
                                     .height = SurfaceHeight{48}};
  static constexpr const char* bogus = "VK_ARISA_not_an_extension";

  static auto instance() -> std::shared_ptr<const VulkanInstance> {
    auto created = VulkanInstance::create();
    if (!created.has_value()) {
      return nullptr;
    }
    return std::make_shared<const VulkanInstance>(std::move(*created));
  }
};

TEST_F(VulkanTakeoverTest, TakeoverLendsTheImagesAndHandsBack) {
  auto gpu = offscreen({.size = small, .images = 2});
  if (!gpu.has_value()) {
    GTEST_SKIP() << skip_reason(gpu.error());
  }
  {
    auto taken = gpu->take_over();
    ASSERT_TRUE(taken.has_value());
    EXPECT_TRUE(gpu->render_queue()->get().parked());
    EXPECT_FALSE(gpu->take_over().has_value());
    EXPECT_EQ(taken->image_count(), 2U);
    ASSERT_TRUE(taken->device().has_value());
    EXPECT_EQ(taken->device()->get().handle(), gpu->device().handle());
    taken->set_swap_interval(SwapInterval::of<0>());
    const auto image = taken->acquire(VK_NULL_HANDLE);
    ASSERT_TRUE(image.has_value());
    EXPECT_NE(image->target.image, VK_NULL_HANDLE);
    EXPECT_NE(image->target.view, VK_NULL_HANDLE);
    EXPECT_EQ(image->target.extent.width, 64U);
    EXPECT_FALSE(image->waits_on_acquire);
    ASSERT_TRUE(taken->present(image->target.index).has_value());
    EXPECT_EQ(taken->presents(), 1U);
  }
  EXPECT_FALSE(gpu->render_queue()->get().parked());
  EXPECT_EQ(gpu->takeover_counters().returned, 1U);
  EXPECT_EQ(gpu->statistics().present_interval, 1U);
}

TEST_F(VulkanTakeoverTest, OnlyAcquiredImagesPresentAndHandBackSettlesTheRest) {
  auto gpu = offscreen({.size = small, .images = 2});
  if (!gpu.has_value()) {
    GTEST_SKIP() << skip_reason(gpu.error());
  }
  auto taken = gpu->take_over();
  ASSERT_TRUE(taken.has_value());
  const auto first = taken->acquire(VK_NULL_HANDLE);
  ASSERT_TRUE(first.has_value());
  const std::uint32_t other = (first->target.index + 1) % 2;
  EXPECT_FALSE(taken->present(other).has_value());
  const auto second = taken->acquire(VK_NULL_HANDLE);
  ASSERT_TRUE(second.has_value());
  EXPECT_EQ(second->target.index, other);
  const auto third = taken->acquire(VK_NULL_HANDLE);
  ASSERT_FALSE(third.has_value());
  EXPECT_EQ(third.error().detail(), ProviderErrorDetail::image_unavailable);
  ASSERT_TRUE(taken->present(first->target.index).has_value());
  EXPECT_FALSE(taken->present(first->target.index).has_value());
  EXPECT_EQ(taken->presents(), 1U);
  taken->release();
  EXPECT_FALSE(taken->device().has_value());
  EXPECT_EQ(taken->image_count(), 0U);
  EXPECT_FALSE(gpu->render_queue()->get().parked());
  auto again = gpu->take_over();
  ASSERT_TRUE(again.has_value());
  const auto fresh = again->acquire(VK_NULL_HANDLE);
  ASSERT_TRUE(fresh.has_value());
  ASSERT_TRUE(again->present(fresh->target.index).has_value());
}

TEST_F(VulkanTakeoverTest, ALentImageReportsThatItCanBeCopied) {
  auto gpu = offscreen({.size = small, .images = 2});
  if (!gpu.has_value()) {
    GTEST_SKIP() << skip_reason(gpu.error());
  }
  auto taken = gpu->take_over();
  ASSERT_TRUE(taken.has_value());
  const auto image = taken->acquire(VK_NULL_HANDLE);
  ASSERT_TRUE(image.has_value());
  EXPECT_NE(image->usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 0U);
  EXPECT_NE(image->usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 0U);
  ASSERT_TRUE(taken->present(image->target.index).has_value());
}

TEST_F(VulkanTakeoverTest, MissingRequiredExtensionIsReportedByName) {
  const auto made = instance();
  if (made == nullptr) {
    GTEST_SKIP() << "no Vulkan instance";
  }
  const DeviceRequest request{.presents_to = VK_NULL_HANDLE,
                              .extensions = {bogus},
                              .optional_extensions = {},
                              .features = {},
                              .vulkan13 = {}};
  const auto device = VulkanDevice::create(made, request);
  ASSERT_FALSE(device.has_value());
  EXPECT_EQ(device.error().detail(), ProviderErrorDetail::missing_extension);
  const std::vector<std::string> lacking =
      VulkanDevice::unsupported(made, request);
  EXPECT_TRUE(std::ranges::contains(lacking, std::string{bogus}));
}

TEST_F(VulkanTakeoverTest, OptionalExtensionsAreEnabledOnlyWhenOffered) {
  const auto made = instance();
  if (made == nullptr) {
    GTEST_SKIP() << "no Vulkan instance";
  }
  const auto device =
      VulkanDevice::create(made, DeviceRequest{.presents_to = VK_NULL_HANDLE,
                                               .extensions = {},
                                               .optional_extensions = {bogus},
                                               .features = {},
                                               .vulkan13 = {}});
  if (!device.has_value()) {
    GTEST_SKIP() << skip_reason(device.error());
  }
  EXPECT_FALSE((*device)->has_extension(bogus));
  EXPECT_TRUE(VulkanDevice::unsupported(made, DeviceRequest{}).empty());
}

TEST_F(VulkanTakeoverTest, RequestedVulkan13FeaturesAreCheckedAndEnabled) {
  const auto made = instance();
  if (made == nullptr) {
    GTEST_SKIP() << "no Vulkan instance";
  }
  DeviceRequest request{};
  request.vulkan13.dynamicRendering = VK_TRUE;
  const std::vector<std::string> lacking =
      VulkanDevice::unsupported(made, request);
  const auto device = VulkanDevice::create(made, request);
  if (!lacking.empty()) {
    ASSERT_FALSE(device.has_value());
    EXPECT_EQ(device.error().detail(), ProviderErrorDetail::missing_feature);
    return;
  }
  ASSERT_TRUE(device.has_value());
  EXPECT_EQ((*device)->vulkan13().dynamicRendering, VK_TRUE);
}

}
