#pragma once

#include <arisa/provider/diagnostics/provider.hpp>
#include <arisa/provider/gpu/external_texture.hpp>
#include <arisa/provider/gpu/frame_hook.hpp>
#include <arisa/provider/gpu/frame_hooks.hpp>
#include <arisa/provider/gpu/frame_info.hpp>
#include <arisa/provider/gpu/frame_source.hpp>
#include <arisa/provider/gpu/gpu_surface.hpp>
#include <arisa/provider/gpu/gpu_surfaces.hpp>
#include <arisa/provider/gpu/gpu_takeover.hpp>
#include <arisa/provider/gpu/provider.hpp>
#include <arisa/provider/gpu/swap_interval.hpp>
#include <arisa/provider/vulkan/offscreen_target.hpp>
#include <arisa/provider/vulkan/ready_semaphore.hpp>
#include <arisa/provider/vulkan/vulkan_device.hpp>
#include <arisa/provider/vulkan/vulkan_target.hpp>
#include <arisa/support/scoped_callback.hpp>

#include <vulkan/vulkan.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>

namespace arisa::provider::vulkan {

class VulkanGpu final : public GpuProvider {
  class Renderer;
  class OverlayState;

 public:
  struct FrameTarget {
    VkImage image{VK_NULL_HANDLE};
    VkImageView view{VK_NULL_HANDLE};
    VkFormat format{VK_FORMAT_UNDEFINED};
    VkExtent2D extent{};
    VkImageLayout layout{VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    std::uint32_t index{0};
    std::uint64_t generation{0};
  };

  struct LentImage {
    FrameTarget target{};
    bool waits_on_acquire{false};
    VkSemaphore finished{VK_NULL_HANDLE};
    VkImageLayout present_layout{VK_IMAGE_LAYOUT_PRESENT_SRC_KHR};
    VkImageUsageFlags usage{VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT};
  };

  struct OverlayFence {
    VkSemaphore semaphore{VK_NULL_HANDLE};
    std::uint64_t value{0};
  };

  class Overlay {
   public:
    struct Statistics {
      std::size_t framebuffers{0};
      std::size_t retired_framebuffers{0};
    };

    Overlay(Overlay&& other) noexcept;
    auto operator=(Overlay&& other) noexcept -> Overlay&;
    Overlay(const Overlay&) = delete;
    auto operator=(const Overlay&) -> Overlay& = delete;
    ~Overlay();

    auto held() const noexcept -> bool;

    auto frames() const -> FrameSource;

    [[nodiscard]] auto record(VkCommandBuffer buffer, const FrameTarget& target)
        -> std::expected<std::optional<OverlayFence>, ProviderError>;

    auto statistics() const -> Statistics;

    void release() noexcept;

   private:
    friend class VulkanGpu;

    Overlay(std::shared_ptr<Renderer> renderer,
            std::shared_ptr<OverlayState> state) noexcept;

    std::shared_ptr<Renderer> renderer_;
    std::shared_ptr<OverlayState> state_;
  };

  class Takeover {
   public:
    Takeover(Takeover&& other) noexcept;
    auto operator=(Takeover&& other) noexcept -> Takeover&;
    Takeover(const Takeover&) = delete;
    auto operator=(const Takeover&) -> Takeover& = delete;
    ~Takeover();

    auto held() const noexcept -> bool { return taken_.held(); }

    auto device() const noexcept
        -> std::optional<std::reference_wrapper<const VulkanDevice>>;

    auto image_count() const noexcept -> std::uint32_t;

    [[nodiscard]] auto acquire(VkSemaphore acquired)
        -> std::expected<LentImage, ProviderError>;

    [[nodiscard]] auto present(std::uint32_t index)
        -> std::expected<void, ProviderError>;

    void set_swap_interval(SwapInterval interval);

    auto presents() const noexcept -> std::uint64_t;

    [[nodiscard]] auto overlay() -> std::expected<Overlay, ProviderError>;

    void release() noexcept { taken_.release(); }

   private:
    friend class VulkanGpu;

    Takeover(std::shared_ptr<Renderer> renderer, GpuTakeover taken) noexcept;

    std::shared_ptr<Renderer> renderer_;
    GpuTakeover taken_;
  };

  using Hooks = FrameHooks<VkCommandBuffer, const FrameTarget&>;
  using FrameHookFunction = Hooks::Function;
  using Imported = ExternalTexture<ReadySemaphore>;
  using Surfaces = GpuSurfaces<VkCommandBuffer, const FrameTarget&>;
  using SurfaceFunction = Surfaces::Function;

  static constexpr VkImageLayout hook_layout =
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  static constexpr VkImageLayout import_layout =
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  static constexpr VkImageLayout host_import_layout = VK_IMAGE_LAYOUT_GENERAL;

  struct Statistics {
    std::uint64_t frames_presented{0};
    std::uint64_t fence_waits{0};
    std::uint64_t vertex_bytes_copied{0};
    std::uint64_t texture_bytes_uploaded{0};
    std::uint64_t offscreen_passes{0};
    std::uint64_t timestamp_reads{0};
    std::uint64_t timestamp_stalls{0};
    std::uint64_t overlapping_queue_use{0};
    std::uint64_t imports{0};
    std::uint64_t semaphore_waits{0};
    std::uint64_t hooked_frames{0};
    std::uint64_t surface_calls{0};
    std::uint64_t allocation_waits{0};
    std::uint64_t replays{0};
    std::uint64_t captures{0};
    std::uint64_t interval_changes{0};
    std::uint32_t present_interval{1};

    friend constexpr auto operator==(const Statistics&, const Statistics&)
        -> bool = default;
  };

  using Diagnostics =
      std::optional<std::reference_wrapper<DiagnosticsProvider>>;

  [[nodiscard]] static auto create(std::shared_ptr<const VulkanDevice> device,
                                   std::unique_ptr<VulkanTarget> target,
                                   Diagnostics diagnostics = std::nullopt)
      -> std::expected<VulkanGpu, ProviderError>;

  [[nodiscard]] static auto create(OffscreenTarget::Options offscreen,
                                   Diagnostics diagnostics = std::nullopt)
      -> std::expected<VulkanGpu, ProviderError>;

  auto startup_report() const -> std::string;

  [[nodiscard]] auto create_texture(TextureDescription description)
      -> std::expected<Texture, ProviderError> override;

  [[nodiscard]] auto update_texture(Texture& texture, TextureRegion region,
                                    PixelRows pixels)
      -> std::expected<void, ProviderError> override;

  [[nodiscard]] auto import_texture(VkImage image,
                                    ImportDescription description,
                                    support::ScopedCallback released = {})
      -> std::expected<Imported, ProviderError>;

  [[nodiscard]] auto import_texture(std::span<const VkImage> planes,
                                    ImportDescription description,
                                    support::ScopedCallback released = {})
      -> std::expected<Imported, ProviderError>;

  [[nodiscard]] auto import_texture(VkImage image,
                                    ImportDescription description,
                                    VkImageLayout layout,
                                    support::ScopedCallback released = {})
      -> std::expected<Imported, ProviderError>;

  [[nodiscard]] auto import_texture(std::span<const VkImage> planes,
                                    ImportDescription description,
                                    VkImageLayout layout,
                                    support::ScopedCallback released = {})
      -> std::expected<Imported, ProviderError>;

  [[nodiscard]] auto begin_frame()
      -> std::expected<GpuFrame, ProviderError> override;

  [[nodiscard]] auto begin_replay()
      -> std::expected<GpuFrame, ProviderError> override;

  [[nodiscard]] auto capture() -> std::expected<Image, ProviderError> override;

  auto resources() const -> GpuResources override;

  auto inventory() const -> GpuInventory override;

  auto timings() const -> GpuTimings override;

  auto presents_off_thread() const noexcept -> bool override { return true; }

  auto frame_schedule() noexcept
      -> std::optional<std::reference_wrapper<FrameSchedule>> override;

  auto publications() const noexcept -> std::optional<
      std::reference_wrapper<const TexturePublications>> override;

  auto add_frame_hook(FrameStage stage, FrameHookFunction function,
                      FrameHookOptions options = {}) -> FrameHook;

  [[nodiscard]] auto add_surface(SurfaceOptions options,
                                 SurfaceFunction function)
      -> std::expected<GpuSurface, ProviderError>;

  auto surfaces() noexcept
      -> std::optional<std::reference_wrapper<SurfaceDirectory>> override;

  auto render_queue() noexcept
      -> std::optional<std::reference_wrapper<RenderQueue>> override;

  [[nodiscard]] auto take_over() -> std::expected<Takeover, ProviderError>;

  auto takeover_counters() const -> GpuTakeover::Counters;

  auto completed(GpuSerial serial) const -> bool;

  auto last_damage() const -> std::optional<ClipRect>;

  auto statistics() const -> Statistics;

  auto device() const noexcept -> const VulkanDevice&;

  auto frames_presented() const -> std::uint64_t;

  auto missed_vsyncs() const -> std::uint64_t;

  auto live_textures() const -> std::size_t;

 private:
  class Frame;

  explicit VulkanGpu(std::shared_ptr<Renderer> renderer) noexcept;

  static constexpr std::size_t frame_count = 3;

  std::shared_ptr<Renderer> renderer_;
  std::array<std::shared_ptr<Frame>, frame_count> frames_{};
  std::shared_ptr<Frame> replay_;
};

}
