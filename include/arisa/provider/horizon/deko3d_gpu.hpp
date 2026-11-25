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
#include <arisa/provider/horizon/deko3d_memory.hpp>
#include <arisa/provider/horizon/gpu_options.hpp>
#include <arisa/provider/horizon/horizon_surface.hpp>

#include <deko3d.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>

namespace arisa::provider::horizon {

class Deko3dGpu final : public GpuProvider {
  class Device;
  class OverlayState;

 public:
  struct FrameTarget {
    dk::Image& image;
    DkImageView view;
    DkImageFormat format;
    std::uint32_t width;
    std::uint32_t height;
    std::uint64_t generation{0};
  };

  using Hooks = FrameHooks<dk::CmdBuf&, const FrameTarget&>;
  using FrameHookFunction = Hooks::Function;
  using Imported = ExternalTexture<dk::Fence>;
  using Surfaces = GpuSurfaces<dk::CmdBuf&, const FrameTarget&>;
  using SurfaceFunction = Surfaces::Function;

  class Overlay {
   public:
    Overlay(Overlay&& other) noexcept;
    auto operator=(Overlay&& other) noexcept -> Overlay&;
    Overlay(const Overlay&) = delete;
    auto operator=(const Overlay&) -> Overlay& = delete;
    ~Overlay();

    auto held() const noexcept -> bool;

    auto frames() const -> FrameSource;

    [[nodiscard]] auto record(dk::CmdBuf& buffer, const FrameTarget& target)
        -> std::expected<bool, ProviderError>;

    void release() noexcept;

   private:
    friend class Deko3dGpu;

    Overlay(std::shared_ptr<Device> device,
            std::shared_ptr<OverlayState> state) noexcept;

    std::shared_ptr<Device> device_;
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

    auto device() const noexcept -> std::optional<dk::Device>;

    auto queue() const noexcept -> std::optional<dk::Queue>;

    auto memory() noexcept
        -> std::optional<std::reference_wrapper<Deko3dMemory>>;

    auto image_count() const noexcept -> std::uint32_t;

    auto target(std::uint32_t index) -> std::optional<FrameTarget>;

    [[nodiscard]] auto acquire() -> std::expected<std::uint32_t, ProviderError>;

    [[nodiscard]] auto present(std::uint32_t index)
        -> std::expected<void, ProviderError>;

    void set_swap_interval(SwapInterval interval);

    auto presents() const noexcept -> std::uint64_t;

    auto generation() const noexcept -> std::uint64_t;

    [[nodiscard]] auto overlay() -> std::expected<Overlay, ProviderError>;

    void release() noexcept { taken_.release(); }

   private:
    friend class Deko3dGpu;

    Takeover(std::shared_ptr<Device> device, GpuTakeover taken) noexcept;

    std::shared_ptr<Device> device_;
    GpuTakeover taken_;
  };

  static constexpr DkImageFormat target_format = DkImageFormat_RGBA8_Unorm;
  static constexpr std::uint32_t max_textures = 4096;

  [[nodiscard]] static auto create(const HorizonSurface& surface,
                                   GpuOptions options = {})
      -> std::expected<Deko3dGpu, ProviderError>;

  [[nodiscard]] static auto create(const HorizonSurface& surface,
                                   GpuOptions options,
                                   DiagnosticsProvider& diagnostics)
      -> std::expected<Deko3dGpu, ProviderError>;

  [[nodiscard]] auto create_texture(TextureDescription description)
      -> std::expected<Texture, ProviderError> override;

  [[nodiscard]] auto update_texture(Texture& texture, TextureRegion region,
                                    PixelRows pixels)
      -> std::expected<void, ProviderError> override;

  [[nodiscard]] auto import_texture(const dk::Image& image,
                                    ImportDescription description,
                                    support::ScopedCallback released = {})
      -> std::expected<Imported, ProviderError>;

  [[nodiscard]] auto import_texture(std::span<const dk::Image* const> planes,
                                    ImportDescription description,
                                    support::ScopedCallback released = {})
      -> std::expected<Imported, ProviderError>;

  [[nodiscard]] auto begin_frame()
      -> std::expected<GpuFrame, ProviderError> override;

  [[nodiscard]] auto begin_replay()
      -> std::expected<GpuFrame, ProviderError> override;

  [[nodiscard]] auto capture() -> std::expected<Image, ProviderError> override;

  auto timings() const -> GpuTimings override;

  auto resources() const -> GpuResources override;

  auto inventory() const -> GpuInventory override;

  auto startup_report() const -> std::string;

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

  auto swap_interval_changes() const noexcept -> std::uint64_t;

  auto completed(GpuSerial serial) const noexcept -> bool;

  void set_swap_interval(SwapInterval interval);

  auto options() const noexcept -> GpuOptions;

  auto device() const noexcept -> dk::Device;

  auto queue() const noexcept -> dk::Queue;

  auto memory() noexcept -> Deko3dMemory&;

  auto frames_presented() const noexcept -> std::uint64_t;

  auto missed_vsyncs() const noexcept -> std::uint64_t;

  auto report() const -> std::string;

  auto live_textures() const -> std::size_t;

 private:
  class Frame;

  explicit Deko3dGpu(std::shared_ptr<Device> device) noexcept;

  std::shared_ptr<Device> device_;
  static constexpr std::size_t frame_count = 3;

  std::array<std::shared_ptr<Frame>, frame_count> frames_{};
  std::shared_ptr<Frame> replay_;
};

}
