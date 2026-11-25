#include <arisa/provider/vulkan/vulkan_surface.hpp>

#include <utility>

namespace arisa::provider::vulkan {

VulkanSurface::VulkanSurface(std::shared_ptr<const VulkanInstance> instance,
                             VkSurfaceKHR surface) noexcept
    : instance_(std::move(instance)),
      surface_(SurfaceTraits::handle_type{.instance = instance_->handle(),
                                          .surface = surface}) {}

}
