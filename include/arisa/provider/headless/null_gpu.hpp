#pragma once

#include <arisa/provider/gpu/external_texture.hpp>
#include <arisa/provider/gpu/frame_hook.hpp>
#include <arisa/provider/gpu/frame_hooks.hpp>
#include <arisa/provider/gpu/frame_planner.hpp>
#include <arisa/provider/gpu/frame_slots.hpp>
#include <arisa/provider/gpu/frame_source.hpp>
#include <arisa/provider/gpu/gpu_surface.hpp>
#include <arisa/provider/gpu/gpu_surfaces.hpp>
#include <arisa/provider/gpu/gpu_takeover.hpp>
#include <arisa/provider/gpu/lent_swapchain.hpp>
#include <arisa/provider/gpu/provider.hpp>
#include <arisa/provider/gpu/render_queue.hpp>
#include <arisa/provider/gpu/swap_interval.hpp>
#include <arisa/provider/gpu/texture_registry.hpp>
#include <arisa/provider/headless/software_raster.hpp>
#include <arisa/provider/surface/surface_size.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <vector>

namespace arisa::provider::headless {

class NullGpu final : public GpuProvider {
  struct Counters;
  class OverlayFrame;
  class OverlaySeat;
  class OverlayState;

 public:
  struct RenderTargetLimit {
    std::uint32_t extent{Texture::maximum_extent};
    std::size_t bytes{std::numeric_limits<std::size_t>::max()};
    std::size_t count{std::numeric_limits<std::size_t>::max()};
  };

  struct DrawRejection {
    std::size_t skipped{0};
    std::size_t count{std::numeric_limits<std::size_t>::max()};
    bool passes{true};
  };

  using FrameCanvas =
      std::optional<std::reference_wrapper<SoftwareRaster::Canvas>>;
  using Hooks = FrameHooks<FrameCanvas>;
  using FrameHookFunction = Hooks::Function;
  using Imported = ExternalTexture<>;
  using Surfaces = GpuSurfaces<FrameCanvas>;
  using SurfaceFunction = Surfaces::Function;

  struct ShapeRecord {
    std::vector<ShapeVertex> vertices;
    std::vector<Index> indices;
    std::vector<DrawCommand> commands;
  };

  class Overlay {
   public:
    struct Statistics {
      std::uint64_t published{0};
      std::uint64_t empty{0};
      std::uint64_t refused{0};
      std::uint64_t recorded{0};
      std::uint64_t idle{0};
      std::uint64_t fresh{0};

      friend constexpr auto operator==(const Statistics&, const Statistics&)
          -> bool = default;
    };

    Overlay(Overlay&& other) noexcept = default;
    auto operator=(Overlay&& other) noexcept -> Overlay&;
    Overlay(const Overlay&) = delete;
    auto operator=(const Overlay&) -> Overlay& = delete;
    ~Overlay();

    auto held() const noexcept -> bool;

    auto frames() const -> FrameSource;

    [[nodiscard]] auto record(FrameCanvas target)
        -> std::expected<bool, ProviderError>;

    auto current_slot() const -> std::optional<std::uint32_t>;

    auto statistics() const -> Statistics;

    void release() noexcept;

   private:
    friend class NullGpu;

    Overlay(std::shared_ptr<OverlaySeat> seat,
            std::shared_ptr<OverlayState> state) noexcept;

    std::shared_ptr<OverlaySeat> seat_;
    std::shared_ptr<OverlayState> state_;
  };

  class Takeover {
   public:
    Takeover(Takeover&& other) noexcept = default;
    auto operator=(Takeover&& other) noexcept -> Takeover& = default;
    Takeover(const Takeover&) = delete;
    auto operator=(const Takeover&) -> Takeover& = delete;
    ~Takeover() = default;

    auto held() const noexcept -> bool { return taken_.held(); }

    auto image_count() const -> std::uint32_t;

    [[nodiscard]] auto acquire() -> std::expected<std::uint32_t, ProviderError>;

    auto target(std::uint32_t index) -> FrameCanvas;

    [[nodiscard]] auto present(std::uint32_t index)
        -> std::expected<void, ProviderError>;

    void set_swap_interval(SwapInterval interval);

    auto generation() const -> std::uint64_t;

    [[nodiscard]] auto overlay() -> std::expected<Overlay, ProviderError>;

    void release() noexcept { taken_.release(); }

   private:
    friend class NullGpu;

    Takeover(GpuTakeover taken, std::shared_ptr<Counters> counters,
             std::shared_ptr<OverlaySeat> seat) noexcept;

    static void show(Counters& counters, std::uint32_t index);

    GpuTakeover taken_;
    std::shared_ptr<Counters> counters_;
    std::shared_ptr<OverlaySeat> seat_;
  };

  NullGpu();

  NullGpu(const NullGpu&) = delete;
  auto operator=(const NullGpu&) -> NullGpu& = delete;
  NullGpu(NullGpu&& other) noexcept = default;
  auto operator=(NullGpu&& other) noexcept -> NullGpu& = default;
  ~NullGpu() override;

  [[nodiscard]] auto create_texture(TextureDescription description)
      -> std::expected<Texture, ProviderError> override;

  [[nodiscard]] auto update_texture(Texture& texture, TextureRegion region,
                                    PixelRows pixels)
      -> std::expected<void, ProviderError> override;

  [[nodiscard]] auto import_texture(
      std::shared_ptr<SoftwareRaster::Canvas> image,
      ImportDescription description, support::ScopedCallback released = {})
      -> std::expected<Imported, ProviderError>;

  [[nodiscard]] auto import_texture(
      std::span<const std::shared_ptr<SoftwareRaster::Canvas>> planes,
      ImportDescription description, support::ScopedCallback released = {})
      -> std::expected<Imported, ProviderError>;

  [[nodiscard]] auto begin_frame()
      -> std::expected<GpuFrame, ProviderError> override;

  [[nodiscard]] auto begin_replay()
      -> std::expected<GpuFrame, ProviderError> override;

  [[nodiscard]] auto capture() -> std::expected<Image, ProviderError> override;

  auto timings() const -> GpuTimings override;

  auto resources() const -> GpuResources override;

  auto inventory() const -> GpuInventory override;

  auto presents_off_thread() const noexcept -> bool override { return true; }

  auto frame_schedule() noexcept
      -> std::optional<std::reference_wrapper<FrameSchedule>> override;

  auto publications() const noexcept -> std::optional<
      std::reference_wrapper<const TexturePublications>> override;

  void hold_completion(std::uint64_t frames);

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

  auto takeover_presents() const noexcept -> std::size_t;

  auto takeover_settled() const noexcept -> std::size_t;

  void display(std::optional<std::chrono::nanoseconds> period) noexcept;

  auto swap_interval() const noexcept -> std::uint32_t;

  auto swap_interval_changes() const noexcept -> std::size_t;

  auto captures() const noexcept -> std::size_t;

  auto surface_calls() const noexcept -> std::size_t;

  void rasterize(SurfaceSize extent);

  void swapchain(std::uint32_t images, bool partial);

  void limit_render_targets(RenderTargetLimit limit) noexcept;

  void reject_draws(DrawRejection rejection) noexcept;

  auto rasterizing() const -> bool;

  auto live_textures() const -> std::size_t;
  auto passes() const noexcept -> std::size_t;
  auto render_targets() const noexcept -> std::size_t;
  auto uploads() const noexcept -> std::size_t;
  auto imports() const noexcept -> std::size_t;
  auto uploaded_bytes() const noexcept -> std::size_t;
  auto frames_presented() const noexcept -> std::size_t;
  auto frames_discarded() const noexcept -> std::size_t;
  auto draws() const noexcept -> std::size_t;
  auto commands() const noexcept -> std::size_t;
  auto geometry_allocations() const noexcept -> std::size_t;
  auto geometry_bytes() const noexcept -> std::size_t;
  auto vertex_uploads() const noexcept -> std::size_t;
  auto vertex_uploaded_bytes() const noexcept -> std::size_t;
  auto last_damage() const -> std::optional<ClipRect>;
  auto damaged_pixels() const -> std::size_t;
  auto last_shapes() const -> ShapeRecord;
  auto last_clear() const -> std::optional<Colour>;

 private:
  using Canvas = SoftwareRaster::Canvas;
  using Registry = TextureRegistry<std::shared_ptr<Canvas>>;
  using Publisher = Imported::Publisher;

  class Frame;

  static constexpr std::uint32_t slot_count = 2;

  struct Upload {
    std::shared_ptr<Canvas> canvas;
    TextureFormat format{TextureFormat::rgba8};
    TextureRegion region{};
    std::size_t offset{0};
    std::size_t size{0};
    bool clear{false};
  };

  struct Uploads {
    std::mutex mutex;
    std::vector<Upload> pending;
    std::vector<std::byte> bytes;
  };

  struct Buffer {
    std::optional<Canvas> canvas;
  };

  struct Resident {
    std::vector<ShapeVertex> vertices;
    std::vector<Index> indices;
    std::uint64_t generation{1};
  };

  struct ResidentDraw {
    bool offscreen{false};
    std::vector<DrawCommand> commands;
  };

  template <class Draw>
  class DrawSlots {
   public:
    void clear() noexcept { used_ = 0; }

    auto next(std::size_t commands) -> Draw& {
      if (commands > widest_) {
        widest_ = commands;
        for (Draw& slot : slots_) {
          slot.commands.reserve(widest_);
        }
      }
      if (used_ == slots_.size()) {
        slots_.emplace_back().commands.reserve(widest_);
      }
      return slots_[used_++];
    }

    auto active() const noexcept -> std::span<const Draw> {
      return std::span<const Draw>{slots_}.first(used_);
    }

   private:
    std::vector<Draw> slots_;
    std::size_t used_{0};
    std::size_t widest_{0};
  };

  struct Snapshot {
    std::vector<ShapeVertex> vertices;
    std::vector<Index> indices;
    DrawSlots<ResidentDraw> draws;
    std::uint64_t generation{0};
  };

  struct Counters {
    std::atomic<std::size_t> uploads{0};
    std::atomic<std::size_t> imports{0};
    std::atomic<std::uint64_t> held{0};
    std::atomic<std::size_t> uploaded_bytes{0};
    std::atomic<std::size_t> presented{0};
    std::atomic<std::size_t> discarded{0};
    std::atomic<std::size_t> draws{0};
    std::atomic<std::size_t> commands{0};
    std::atomic<std::size_t> geometry_allocations{0};
    std::atomic<std::size_t> geometry_bytes{0};
    std::atomic<std::size_t> passes{0};
    std::atomic<std::size_t> render_targets{0};
    std::atomic<std::uint64_t> textures_created{0};
    std::atomic<std::uint32_t> target_extent{Texture::maximum_extent};
    std::atomic<std::size_t> target_bytes{
        std::numeric_limits<std::size_t>::max()};
    std::atomic<std::size_t> target_count{
        std::numeric_limits<std::size_t>::max()};
    std::atomic<std::size_t> vertex_uploads{0};
    std::atomic<std::size_t> vertex_uploaded_bytes{0};
    std::atomic<std::uint64_t> submitted{0};
    std::atomic<std::int64_t> display_period{0};
    std::atomic<std::uint32_t> swap_interval{1};
    std::atomic<std::size_t> swap_interval_changes{0};
    std::atomic<std::size_t> captures{0};
    std::atomic<std::size_t> takeover_presents{0};
    std::atomic<std::size_t> takeover_settled{0};
    std::atomic<std::size_t> reject_skipped{0};
    std::atomic<std::size_t> reject_count{0};
    std::atomic<bool> reject_passes{true};
    std::atomic<std::size_t> reject_seen{0};
    mutable std::mutex mutex;
    std::optional<std::chrono::nanoseconds> displayed;
    std::vector<Buffer> buffers{1};
    gpu::FramePlanner planner;
    std::size_t next_buffer{0};
    bool partial{true};
    std::optional<ClipRect> last_damage;
    std::size_t damaged_pixels{0};
    ShapeRecord last_shapes;
    Snapshot snapshot;
    std::optional<Colour> last_clear;
    std::optional<SurfaceSize> extent;
    std::uint64_t generation{0};
    std::optional<Image> last_image;
    GpuTimings timings;
    LentSwapchain<> lent;
  };

  static void append(ShapeRecord& record, const Snapshot& snapshot);

  static auto make_texture(const std::shared_ptr<Registry>& registry,
                           Counters& counters, TextureDescription description)
      -> std::expected<Texture, ProviderError>;

  std::shared_ptr<Registry> registry_;
  std::shared_ptr<Counters> counters_;
  std::shared_ptr<Resident> resident_;
  std::shared_ptr<Hooks> hooks_;
  std::shared_ptr<Publisher> publisher_;
  std::shared_ptr<Surfaces> surfaces_;
  std::shared_ptr<FrameSlots> slots_;
  std::shared_ptr<Uploads> uploads_;
  std::shared_ptr<RenderQueue> queue_;
  GpuTakeover::Seat takeover_;
  std::shared_ptr<OverlaySeat> overlay_;
  std::array<std::shared_ptr<Frame>, slot_count> frames_{};
  std::shared_ptr<Frame> replay_;
};

}
