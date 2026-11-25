#include <arisa/provider/vulkan/offscreen_target.hpp>

#include <utility>

namespace arisa::provider::vulkan {

OffscreenTarget::OffscreenTarget(std::shared_ptr<const VulkanDevice> device,
                                 SurfaceSize size,
                                 std::vector<VulkanImage> images,
                                 bool preserved) noexcept
    : device_(std::move(device)), preserved_(preserved) {
  adopt(size, std::move(images));
}

auto OffscreenTarget::create(std::shared_ptr<const VulkanDevice> device,
                             Options options)
    -> std::expected<OffscreenTarget, ProviderError> {
  if (device == nullptr || options.size.empty() ||
      options.images < minimum_images || options.images > maximum_images) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument});
  }
  auto images = OffscreenTarget::images(*device, options.size, options.images);
  if (!images.has_value()) {
    return std::unexpected(images.error());
  }
  return OffscreenTarget{std::move(device), options.size, std::move(*images),
                         options.preserved};
}

auto OffscreenTarget::images(const VulkanDevice& device, SurfaceSize size,
                             std::uint32_t count)
    -> std::expected<std::vector<VulkanImage>, ProviderError> {
  std::vector<VulkanImage> made;
  made.reserve(count);
  for (std::uint32_t index = 0; index < count; ++index) {
    auto image = VulkanImage::create(
        device, VulkanImage::Description{.width = size.width.value(),
                                         .height = size.height.value(),
                                         .format = image_format,
                                         .usage = image_usage,
                                         .swizzle = {}});
    if (!image.has_value()) {
      return std::unexpected(image.error());
    }
    made.push_back(std::move(*image));
  }
  return made;
}

void OffscreenTarget::adopt(SurfaceSize size, std::vector<VulkanImage> images) {
  views_.clear();
  images_ = std::move(images);
  size_ = size;
  views_.reserve(images_.size());
  for (const VulkanImage& image : images_) {
    views_.push_back(image.view());
  }
  drawn_.assign(images_.size(), false);
  presented_.reset();
  next_ = 0;
}

auto OffscreenTarget::prepare(SurfaceSize wanted)
    -> std::expected<bool, ProviderError> {
  if (wanted.empty() || wanted == size_) {
    return true;
  }
  device_->wait_idle();
  auto resized =
      images(*device_, wanted, static_cast<std::uint32_t>(images_.size()));
  if (!resized.has_value()) {
    return std::unexpected(resized.error());
  }
  adopt(wanted, std::move(*resized));
  ++generation_;
  return true;
}

auto OffscreenTarget::acquire(VkSemaphore)
    -> std::expected<std::optional<Image>, ProviderError> {
  const std::uint32_t index = next_;
  next_ = (next_ + 1) % static_cast<std::uint32_t>(images_.size());
  return Image{.image = images_[index].image(),
               .index = index,
               .layout = drawn_[index] && preserved_
                             ? presented_layout()
                             : VK_IMAGE_LAYOUT_UNDEFINED,
               .signals = false};
}

auto OffscreenTarget::finished(std::uint32_t) const noexcept -> VkSemaphore {
  return VK_NULL_HANDLE;
}

auto OffscreenTarget::present(VkQueue, std::uint32_t index)
    -> std::expected<void, ProviderError> {
  if (index >= images_.size()) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument});
  }
  drawn_[index] = true;
  presented_ = index;
  return {};
}

auto OffscreenTarget::readable() const noexcept -> std::optional<Image> {
  if (!presented_.has_value()) {
    return std::nullopt;
  }
  return Image{.image = images_[*presented_].image(),
               .index = *presented_,
               .layout = presented_layout(),
               .signals = false};
}

}
