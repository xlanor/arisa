#include <arisa/provider/gpu/frame_planner.hpp>
#include <arisa/provider/gpu/frame_slots.hpp>
#include <arisa/provider/gpu/frame_stopwatch.hpp>
#include <arisa/provider/gpu/lent_swapchain.hpp>
#include <arisa/provider/gpu/overlay_slots.hpp>
#include <arisa/provider/gpu/texture_registry.hpp>
#include <arisa/provider/gpu/upload_arena.hpp>
#include <arisa/provider/horizon/deko3d_gpu.hpp>
#include <arisa/provider/horizon/deko3d_handle.hpp>
#include <provider/horizon/deko3d_shaders.hpp>

#include <switch.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <flat_map>
#include <format>
#include <functional>
#include <initializer_list>
#include <limits>
#include <mutex>
#include <optional>
#include <span>
#include <utility>
#include <variant>
#include <vector>

namespace arisa::provider::horizon {

class Deko3dGpu::Device {
 public:
  using Registry = TextureRegistry<std::uint32_t>;
  using Publisher = Imported::Publisher;
  using Allocation = Deko3dMemory::Allocation;
  using DeviceHandle = Deko3dHandle<DkDevice, dkDeviceDestroy>;
  using QueueHandle = Deko3dHandle<DkQueue, dkQueueDestroy>;
  using CmdBufHandle = Deko3dHandle<DkCmdBuf, dkCmdBufDestroy>;
  using SwapchainHandle = Deko3dHandle<DkSwapchain, dkSwapchainDestroy>;

  static constexpr std::uint32_t command_chunk_size = 0x10000;
  static constexpr std::uint32_t staging_alignment = 0x100;
  static constexpr gpu::Alignment upload_alignment =
      gpu::Alignment::of<staging_alignment>();
  static constexpr std::uint64_t upload_chunk = 256 * 1024;
  static constexpr std::chrono::nanoseconds capture_timeout =
      std::chrono::seconds{1};
  static constexpr std::uint32_t geometry_alignment = 0x100;
  static constexpr std::uint32_t uniform_size = DK_UNIFORM_BUF_ALIGNMENT;
  static constexpr std::uint32_t minimum_vertices = 4096;
  static constexpr std::uint32_t minimum_indices = 8192;
  static constexpr std::size_t max_images = 3;
  static constexpr double vsync_period_ns = 1.0e9 / 60.0;
  static constexpr std::chrono::nanoseconds display_period{16'666'667};
  static constexpr std::int64_t pause_periods = 30;
  static constexpr Colour transparent{
      .red = 0.0F, .green = 0.0F, .blue = 0.0F, .alpha = 0.0F};
  static constexpr std::uint32_t timestamp_stride = 16;
  static constexpr std::uint32_t timestamp_count = 6;
  static constexpr std::uint64_t gpu_tick_numerator = 625;
  static constexpr std::uint64_t gpu_tick_denominator = 384;
  static constexpr std::uint32_t sampler_count = 2;

  struct Transform {
    std::array<float, 2> scale;
    std::array<float, 2> translation;
  };

  struct Shaders {
    Allocation code;
    dk::Shader vertex;
    dk::Shader fragment;
    dk::Shader shape_vertex;
    dk::Shader shape_fragment;
  };

  enum class PipelineKind : std::uint8_t {
    textured,
    shape,
  };

  struct Descriptors {
    Allocation images;
    Allocation samplers;
  };

  struct Retired {
    std::vector<Allocation> allocations;
    std::vector<std::uint32_t> slots;
  };

  struct TextureStorage {
    Allocation memory;
    dk::Image image;
  };

  struct BlockCommand {
    PipelineKind pipeline;
    std::uint32_t block;
    DrawCommand command;
  };

  class StagingChunk {
   public:
    explicit StagingChunk(Allocation memory) noexcept
        : memory_(std::move(memory)) {}

    auto bytes() const noexcept -> std::span<std::byte> {
      return memory_.cpu_bytes();
    }

    auto gpu_address() const noexcept -> DkGpuAddr {
      return memory_.gpu_address();
    }

   private:
    Allocation memory_;
  };

  using Staging = gpu::UploadArena<StagingChunk>;

  struct Upload {
    DkGpuAddr source{DK_GPU_ADDR_INVALID};
    dk::Image image;
    DkImageRect rect{};
    std::optional<SurfaceSize> clear;
  };

  struct Layer {
    dk::Image image;
    SurfaceSize size;
  };

  struct OffscreenPass {
    RenderPass pass;
    std::vector<BlockCommand> commands;
    std::vector<Surfaces::Hole> holes;
  };

  static constexpr std::array<DkVtxAttribState, 3> vertex_attributes{{
      DkVtxAttribState{0, 0, offsetof(Vertex, position), DkVtxAttribSize_2x32,
                       DkVtxAttribType_Float, 0},
      DkVtxAttribState{0, 0, offsetof(Vertex, uv), DkVtxAttribSize_2x32,
                       DkVtxAttribType_Float, 0},
      DkVtxAttribState{0, 0, offsetof(Vertex, colour), DkVtxAttribSize_4x8,
                       DkVtxAttribType_Unorm, 0},
  }};

  static constexpr std::array<DkVtxBufferState, 1> vertex_buffers{{
      DkVtxBufferState{sizeof(Vertex), 0},
  }};

  static constexpr std::array<DkVtxAttribState, 9> shape_attributes{{
      DkVtxAttribState{0, 0, offsetof(ShapeVertex, position),
                       DkVtxAttribSize_2x32, DkVtxAttribType_Float, 0},
      DkVtxAttribState{0, 0, offsetof(ShapeVertex, uv), DkVtxAttribSize_2x32,
                       DkVtxAttribType_Float, 0},
      DkVtxAttribState{0, 0, offsetof(ShapeVertex, local), DkVtxAttribSize_2x32,
                       DkVtxAttribType_Float, 0},
      DkVtxAttribState{0, 0, offsetof(ShapeVertex, half_size),
                       DkVtxAttribSize_2x32, DkVtxAttribType_Float, 0},
      DkVtxAttribState{0, 0, offsetof(ShapeVertex, shape), DkVtxAttribSize_4x32,
                       DkVtxAttribType_Float, 0},
      DkVtxAttribState{0, 0, offsetof(ShapeVertex, colour), DkVtxAttribSize_4x8,
                       DkVtxAttribType_Unorm, 0},
      DkVtxAttribState{0, 0, offsetof(ShapeVertex, kind), DkVtxAttribSize_1x32,
                       DkVtxAttribType_Uint, 0},
      DkVtxAttribState{0, 0, offsetof(ShapeVertex, effect),
                       DkVtxAttribSize_4x32, DkVtxAttribType_Float, 0},
      DkVtxAttribState{0, 0, offsetof(ShapeVertex, detail),
                       DkVtxAttribSize_4x32, DkVtxAttribType_Float, 0},
  }};

  static constexpr std::array<DkVtxBufferState, 1> shape_buffers{{
      DkVtxBufferState{sizeof(ShapeVertex), 0},
  }};

  template <class V>
  class GeometryBlock {
   public:
    using Geometry = provider::Geometry<V>;

    [[nodiscard]] static auto create(Deko3dMemory& memory,
                                     std::uint32_t vertex_capacity,
                                     std::uint32_t index_capacity)
        -> std::optional<GeometryBlock> {
      const std::uint64_t vertex_bytes =
          static_cast<std::uint64_t>(vertex_capacity) * sizeof(V);
      const std::uint64_t index_bytes =
          static_cast<std::uint64_t>(index_capacity) * sizeof(Index);
      if (vertex_bytes > std::numeric_limits<std::uint32_t>::max() ||
          index_bytes > std::numeric_limits<std::uint32_t>::max()) {
        return std::nullopt;
      }
      auto vertices = memory.allocate(Deko3dMemoryKind::data,
                                      static_cast<std::uint32_t>(vertex_bytes),
                                      geometry_alignment);
      auto indices = memory.allocate(Deko3dMemoryKind::data,
                                     static_cast<std::uint32_t>(index_bytes),
                                     geometry_alignment);
      if (!vertices.has_value() || !indices.has_value()) {
        return std::nullopt;
      }
      return GeometryBlock{std::move(*vertices), std::move(*indices),
                           vertex_capacity, index_capacity};
    }

    auto vertex_capacity() const noexcept -> std::uint32_t {
      return vertex_capacity_;
    }

    auto index_capacity() const noexcept -> std::uint32_t {
      return index_capacity_;
    }

    auto fits(std::uint32_t vertices, std::uint32_t indices) const noexcept
        -> bool {
      return vertices <= vertex_capacity_ - vertices_used_ &&
             indices <= index_capacity_ - indices_used_;
    }

    auto take(std::uint32_t vertices, std::uint32_t indices,
              std::uint32_t index) -> Geometry {
      const Geometry geometry{
          .vertices = mapped_vertices().subspan(vertices_used_, vertices),
          .indices = mapped_indices().subspan(indices_used_, indices),
          .block = index,
          .first_vertex = vertices_used_,
          .first_index = indices_used_};
      vertices_used_ += vertices;
      indices_used_ += indices;
      return geometry;
    }

    auto owns(const Geometry& geometry) const noexcept -> bool {
      return geometry.first_vertex <= vertices_used_ &&
             geometry.vertices.size() <=
                 vertices_used_ - geometry.first_vertex &&
             geometry.first_index <= indices_used_ &&
             geometry.indices.size() <= indices_used_ - geometry.first_index &&
             geometry.vertices.data() ==
                 mapped_vertices().subspan(geometry.first_vertex).data() &&
             geometry.indices.data() ==
                 mapped_indices().subspan(geometry.first_index).data();
    }

    void reset() noexcept {
      vertices_used_ = 0;
      indices_used_ = 0;
    }

    auto vertex_address() const noexcept -> DkGpuAddr {
      return vertices_.gpu_address();
    }

    auto vertex_bytes() const noexcept -> std::uint32_t {
      return vertices_.size();
    }

    auto index_address() const noexcept -> DkGpuAddr {
      return indices_.gpu_address();
    }

   private:
    GeometryBlock(Allocation vertices, Allocation indices,
                  std::uint32_t vertex_capacity,
                  std::uint32_t index_capacity) noexcept
        : vertices_(std::move(vertices)),
          indices_(std::move(indices)),
          vertex_capacity_(vertex_capacity),
          index_capacity_(index_capacity) {}

    auto mapped_vertices() const noexcept -> std::span<V> {
      return std::span{
          static_cast<V*>(static_cast<void*>(vertices_.cpu_bytes().data())),
          vertex_capacity_};
    }

    auto mapped_indices() const noexcept -> std::span<Index> {
      return std::span{
          static_cast<Index*>(static_cast<void*>(indices_.cpu_bytes().data())),
          index_capacity_};
    }

    Allocation vertices_;
    Allocation indices_;
    std::uint32_t vertex_capacity_;
    std::uint32_t index_capacity_;
    std::uint32_t vertices_used_{0};
    std::uint32_t indices_used_{0};
  };

  template <class V>
  class GeometryRing {
   public:
    using Block = GeometryBlock<V>;
    using Geometry = provider::Geometry<V>;

    void merge(Deko3dMemory& memory) {
      if (blocks_.size() > 1) {
        std::uint64_t vertices = 0;
        std::uint64_t indices = 0;
        for (const Block& block : blocks_) {
          vertices += block.vertex_capacity();
          indices += block.index_capacity();
        }
        blocks_.clear();
        if (auto merged =
                Block::create(memory, clamp(vertices), clamp(indices))) {
          blocks_.push_back(std::move(*merged));
        }
      }
      reset();
    }

    void reset() noexcept {
      for (Block& block : blocks_) {
        block.reset();
      }
      cursor_ = 0;
    }

    [[nodiscard]] auto allocate(Deko3dMemory& memory, std::uint32_t vertices,
                                std::uint32_t indices)
        -> std::expected<Geometry, ProviderError> {
      for (; cursor_ < blocks_.size(); ++cursor_) {
        if (blocks_[cursor_].fits(vertices, indices)) {
          return blocks_[cursor_].take(vertices, indices,
                                       static_cast<std::uint32_t>(cursor_));
        }
      }
      const std::uint32_t previous_vertices =
          blocks_.empty() ? 0 : blocks_.back().vertex_capacity();
      const std::uint32_t previous_indices =
          blocks_.empty() ? 0 : blocks_.back().index_capacity();
      auto created = Block::create(
          memory,
          std::max(
              {vertices, minimum_vertices, clamp(2ULL * previous_vertices)}),
          std::max({indices, minimum_indices, clamp(2ULL * previous_indices)}));
      if (!created.has_value()) {
        return std::unexpected(
            ProviderError{ProviderErrorCode::resource_exhausted});
      }
      blocks_.push_back(std::move(*created));
      cursor_ = blocks_.size() - 1;
      return blocks_.back().take(vertices, indices,
                                 static_cast<std::uint32_t>(cursor_));
    }

    auto owns(const Geometry& geometry) const noexcept -> bool {
      return geometry.block < blocks_.size() &&
             blocks_[geometry.block].owns(geometry);
    }

    auto block(std::uint32_t index) const noexcept -> const Block* {
      return index < blocks_.size() ? &blocks_[index] : nullptr;
    }

   private:
    static auto clamp(std::uint64_t count) noexcept -> std::uint32_t {
      return static_cast<std::uint32_t>(std::min<std::uint64_t>(
          count, std::numeric_limits<std::uint32_t>::max() / sizeof(V)));
    }

    std::vector<Block> blocks_;
    std::size_t cursor_{0};
  };

  class ResidentBlock {
   public:
    static constexpr std::size_t maximum_ranges = 256;

    [[nodiscard]] auto reserve_indices(Deko3dMemory& memory,
                                       std::uint32_t count) -> bool {
      if (indices_.valid() && index_capacity_ >= count) {
        return true;
      }
      const std::uint64_t grown = std::max<std::uint64_t>(
          {count, minimum_indices, 2ULL * index_capacity_});
      if (grown * sizeof(Index) > std::numeric_limits<std::uint32_t>::max()) {
        return false;
      }
      auto allocated =
          memory.allocate(Deko3dMemoryKind::data,
                          static_cast<std::uint32_t>(grown * sizeof(Index)),
                          geometry_alignment);
      if (!allocated.has_value()) {
        return false;
      }
      indices_ = std::move(*allocated);
      index_capacity_ = static_cast<std::uint32_t>(grown);
      return true;
    }

    auto indices(std::uint32_t count) const noexcept -> std::span<Index> {
      return std::span{
          static_cast<Index*>(static_cast<void*>(indices_.cpu_bytes().data())),
          index_capacity_}
          .first(count);
    }

    void stale(VertexRange range) {
      if (whole_) {
        return;
      }
      if (!ranges_.empty() &&
          ranges_.back().first + ranges_.back().count == range.first) {
        ranges_.back().count += range.count;
        return;
      }
      if (ranges_.size() >= maximum_ranges) {
        whole_ = true;
        ranges_.clear();
        return;
      }
      ranges_.push_back(range);
    }

    [[nodiscard]] auto refresh(Deko3dMemory& memory,
                               std::span<const ShapeVertex> mirror) -> bool {
      const auto size = static_cast<std::uint32_t>(mirror.size());
      if (!vertices_.valid() || vertex_capacity_ < size) {
        const std::uint64_t grown = std::max<std::uint64_t>(
            {size, minimum_vertices, 2ULL * vertex_capacity_});
        if (grown * sizeof(ShapeVertex) >
            std::numeric_limits<std::uint32_t>::max()) {
          return false;
        }
        auto allocated = memory.allocate(
            Deko3dMemoryKind::data,
            static_cast<std::uint32_t>(grown * sizeof(ShapeVertex)),
            geometry_alignment);
        if (!allocated.has_value()) {
          return false;
        }
        vertices_ = std::move(*allocated);
        vertex_capacity_ = static_cast<std::uint32_t>(grown);
        whole_ = true;
      }
      const std::span<std::byte> target = vertices_.cpu_bytes();
      if (whole_) {
        std::ranges::copy(std::as_bytes(mirror), target.begin());
      } else {
        for (const VertexRange& range : ranges_) {
          const std::uint32_t first = std::min(range.first, size);
          const std::uint32_t count = std::min(range.count, size - first);
          std::ranges::copy(
              std::as_bytes(mirror.subspan(first, count)),
              target.subspan(first * sizeof(ShapeVertex)).begin());
        }
      }
      ranges_.clear();
      whole_ = false;
      return true;
    }

    auto vertex_address() const noexcept -> DkGpuAddr {
      return vertices_.gpu_address();
    }

    auto vertex_bytes() const noexcept -> std::uint32_t {
      return vertices_.size();
    }

    auto index_address() const noexcept -> DkGpuAddr {
      return indices_.gpu_address();
    }

   private:
    Allocation vertices_;
    Allocation indices_;
    std::uint32_t vertex_capacity_{0};
    std::uint32_t index_capacity_{0};
    std::vector<VertexRange> ranges_;
    bool whole_{true};
  };

  class FrameSlot {
   public:
    explicit FrameSlot(Deko3dMemory& memory) noexcept : memory_(memory) {}

    FrameSlot(const FrameSlot&) = delete;
    auto operator=(const FrameSlot&) -> FrameSlot& = delete;
    FrameSlot(FrameSlot&&) = delete;
    auto operator=(FrameSlot&&) -> FrameSlot& = delete;
    ~FrameSlot() = default;

    [[nodiscard]] static auto create(DkDevice device, Deko3dMemory& memory)
        -> std::expected<std::unique_ptr<FrameSlot>, ProviderError> {
      auto slot = std::make_unique<FrameSlot>(memory);
      dk::CmdBufMaker maker{device};
      maker.setUserData(slot.get()).setCbAddMem(&FrameSlot::add_memory);
      slot->commands_ = CmdBufHandle{maker.create()};
      if (!slot->commands_.valid()) {
        return std::unexpected(
            ProviderError{ProviderErrorCode::platform_failure});
      }
      auto uniforms = memory.allocate(Deko3dMemoryKind::data, 3 * uniform_size,
                                      DK_UNIFORM_BUF_ALIGNMENT);
      auto stamps =
          memory.allocate(Deko3dMemoryKind::data,
                          timestamp_stride * timestamp_count, timestamp_stride);
      if (!uniforms.has_value() || !stamps.has_value() ||
          !slot->grow_commands(command_chunk_size)) {
        return std::unexpected(
            ProviderError{ProviderErrorCode::resource_exhausted});
      }
      slot->uniforms_ = std::move(*uniforms);
      slot->stamps_ = std::move(*stamps);
      return slot;
    }

    auto stamp_address(std::uint32_t index) const noexcept -> DkGpuAddr {
      return stamps_.gpu_address() +
             (static_cast<DkGpuAddr>(index) * timestamp_stride);
    }

    void stamped(bool effects, bool hooked) noexcept {
      stamped_ = true;
      stamped_effects_ = effects;
      stamped_hooks_ = hooked;
    }

    auto measured() noexcept -> std::optional<GpuTimings> {
      if (!std::exchange(stamped_, false)) {
        return std::nullopt;
      }
      const std::span<const std::byte> bytes = stamps_.cpu_bytes();
      const auto read = [&bytes](std::uint32_t index) {
        std::uint64_t value = 0;
        std::memcpy(
            &value,
            bytes
                .subspan(
                    (static_cast<std::size_t>(index) * timestamp_stride) + 8, 8)
                .data(),
            sizeof(value));
        return value;
      };
      const auto nanoseconds = [](std::uint64_t from, std::uint64_t to) {
        const std::uint64_t ticks = to > from ? to - from : 0;
        return std::chrono::nanoseconds{static_cast<std::int64_t>(
            ticks * gpu_tick_numerator / gpu_tick_denominator)};
      };
      const std::uint64_t first = read(0);
      const std::uint64_t middle = read(1);
      const std::uint64_t cleared = read(2);
      const std::uint64_t below = read(3);
      const std::uint64_t drawn = read(4);
      const std::uint64_t last = read(5);
      return GpuTimings{
          .effects = stamped_effects_
                         ? std::optional{nanoseconds(first, middle)}
                         : std::nullopt,
          .interface = nanoseconds(middle, cleared) + nanoseconds(below, drawn),
          .passes = 0,
          .measured_on_gpu = true,
          .hooks = stamped_hooks_ ? std::optional{nanoseconds(cleared, below) +
                                                  nanoseconds(drawn, last)}
                                  : std::nullopt};
    }

    auto raw() const noexcept -> DkCmdBuf { return commands_.get(); }

    auto fence() noexcept -> dk::Fence& { return fence_; }

    auto uniforms() const noexcept -> const Allocation& { return uniforms_; }

    void wait() noexcept {
      if (pending_) {
        static_cast<void>(fence_.wait());
        pending_ = false;
      }
    }

    [[nodiscard]] auto begin() -> Retired {
      wait();
      dkCmdBufClear(commands_.get());
      if (command_memory_.size() > 1) {
        command_memory_.erase(command_memory_.begin(),
                              command_memory_.end() - 1);
      }
      geometry_.merge(memory_.get());
      shapes_.merge(memory_.get());
      staging_.reset();
      return std::exchange(retired_, Retired{});
    }

    [[nodiscard]] auto stage(std::uint64_t size)
        -> std::expected<Staging::Space, ProviderError> {
      return staging_.take(size, [this](std::uint64_t bytes) -> Staging::Made {
        if (bytes > std::numeric_limits<std::uint32_t>::max()) {
          return std::unexpected(
              ProviderError{ProviderErrorCode::resource_exhausted});
        }
        return memory_.get()
            .allocate(Deko3dMemoryKind::data, static_cast<std::uint32_t>(bytes),
                      staging_alignment)
            .transform([](Allocation memory) {
              return StagingChunk{std::move(memory)};
            });
      });
    }

    void reset_geometry() noexcept {
      geometry_.reset();
      shapes_.reset();
    }

    void submitted(Retired retired, GpuSerial serial) {
      pending_ = true;
      retired_ = std::move(retired);
      serial_ = serial.value();
    }

    auto serial() const noexcept -> std::uint64_t { return serial_; }

    [[nodiscard]] auto allocate_geometry(std::uint32_t vertices,
                                         std::uint32_t indices)
        -> std::expected<FrameGeometry, ProviderError> {
      return geometry_.allocate(memory_.get(), vertices, indices);
    }

    [[nodiscard]] auto allocate_shapes(std::uint32_t vertices,
                                       std::uint32_t indices)
        -> std::expected<ShapeGeometry, ProviderError> {
      return shapes_.allocate(memory_.get(), vertices, indices);
    }

    auto owns(const FrameGeometry& geometry) const noexcept -> bool {
      return geometry_.owns(geometry);
    }

    auto owns(const ShapeGeometry& geometry) const noexcept -> bool {
      return shapes_.owns(geometry);
    }

    auto geometry() const noexcept -> const GeometryRing<Vertex>& {
      return geometry_;
    }

    auto shapes() const noexcept -> const GeometryRing<ShapeVertex>& {
      return shapes_;
    }

    auto resident() noexcept -> ResidentBlock& { return resident_; }

    auto resident() const noexcept -> const ResidentBlock& { return resident_; }

   private:
    auto grow_commands(std::size_t minimum) -> bool {
      const std::uint64_t current =
          command_memory_.empty() ? 0 : command_memory_.back().size();
      const std::uint64_t wanted = Deko3dMemory::align_up(
          std::max<std::uint64_t>({minimum, current * 2, command_chunk_size}),
          DK_CMDMEM_ALIGNMENT);
      if (wanted > std::numeric_limits<std::uint32_t>::max()) {
        return false;
      }
      auto allocated = memory_.get().allocate(
          Deko3dMemoryKind::data, static_cast<std::uint32_t>(wanted),
          DK_CMDMEM_ALIGNMENT);
      if (!allocated.has_value()) {
        return false;
      }
      dkCmdBufAddMemory(commands_.get(), allocated->memblock(),
                        allocated->offset(), allocated->size());
      command_memory_.push_back(std::move(*allocated));
      return true;
    }

    static void add_memory(void* user, DkCmdBuf, std::size_t minimum) {
      static_cast<void>(static_cast<FrameSlot*>(user)->grow_commands(minimum));
    }

    std::reference_wrapper<Deko3dMemory> memory_;
    CmdBufHandle commands_;
    std::vector<Allocation> command_memory_;
    dk::Fence fence_;
    bool pending_{false};
    GeometryRing<Vertex> geometry_;
    GeometryRing<ShapeVertex> shapes_;
    ResidentBlock resident_;
    Staging staging_{upload_alignment, upload_chunk};
    Allocation uniforms_;
    Allocation stamps_;
    bool stamped_{false};
    bool stamped_effects_{false};
    bool stamped_hooks_{false};
    std::uint64_t serial_{0};
    Retired retired_;
  };

  class Framebuffers {
   public:
    Framebuffers(SurfaceSize size, std::uint32_t count) noexcept
        : size_(size), count_(count) {}

    Framebuffers(const Framebuffers&) = delete;
    auto operator=(const Framebuffers&) -> Framebuffers& = delete;
    Framebuffers(Framebuffers&&) = delete;
    auto operator=(Framebuffers&&) -> Framebuffers& = delete;
    ~Framebuffers() = default;

    [[nodiscard]] static auto create(DkDevice device, Deko3dMemory& memory,
                                     NWindow* window, SurfaceSize size,
                                     std::uint32_t count, SwapInterval interval)
        -> std::expected<std::unique_ptr<Framebuffers>, ProviderError> {
      auto framebuffers = std::make_unique<Framebuffers>(size, count);
      dk::ImageLayout layout;
      dk::ImageLayoutMaker{device}
          .setFlags(DkImageFlags_UsageRender | DkImageFlags_UsagePresent |
                    DkImageFlags_Usage2DEngine)
          .setFormat(target_format)
          .setDimensions(size.width.value(), size.height.value())
          .initialize(layout);
      if (layout.getSize() > std::numeric_limits<std::uint32_t>::max()) {
        return std::unexpected(
            ProviderError{ProviderErrorCode::resource_exhausted});
      }
      std::array<const DkImage*, max_images> images{};
      for (std::uint32_t index = 0; index < count; ++index) {
        auto allocated =
            memory.allocate(Deko3dMemoryKind::image,
                            static_cast<std::uint32_t>(layout.getSize()),
                            layout.getAlignment());
        if (!allocated.has_value()) {
          return std::unexpected(allocated.error());
        }
        framebuffers->memory_[index] = std::move(*allocated);
        framebuffers->images_[index].initialize(
            layout, framebuffers->memory_[index].memblock(),
            framebuffers->memory_[index].offset());
        images[index] = &framebuffers->images_[index];
      }
      framebuffers->swapchain_ = SwapchainHandle{
          dk::SwapchainMaker{device, window, images.data(), count}.create()};
      if (!framebuffers->swapchain_.valid()) {
        return std::unexpected(
            ProviderError{ProviderErrorCode::platform_failure});
      }
      framebuffers->set_swap_interval(interval);
      return framebuffers;
    }

    auto size() const noexcept -> SurfaceSize { return size_; }

    auto count() const noexcept -> std::uint32_t { return count_; }

    auto image(std::size_t index) noexcept -> dk::Image& {
      return images_[index];
    }

    auto swapchain() const noexcept -> DkSwapchain { return swapchain_.get(); }

    void set_swap_interval(SwapInterval interval) noexcept {
      dkSwapchainSetSwapInterval(swapchain_.get(), interval.value());
    }

   private:
    SurfaceSize size_;
    std::uint32_t count_;
    std::array<Allocation, max_images> memory_{};
    std::array<dk::Image, max_images> images_{};
    SwapchainHandle swapchain_;
  };

  [[nodiscard]] static auto create(const HorizonSurface& surface,
                                   GpuOptions options)
      -> std::expected<std::shared_ptr<Device>, ProviderError> {
    NWindow* const window = surface.window();
    const SurfaceSize size = surface.size();
    if (window == nullptr || !nwindowIsValid(window) ||
        window->slots_configured != 0 || size.empty()) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::platform_failure});
    }
    DeviceHandle device{dk::DeviceMaker{}.create()};
    if (!device.valid()) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::platform_failure});
    }
    QueueHandle queue{
        dk::QueueMaker{device.get()}
            .setFlags(DkQueueFlags_Graphics | DkQueueFlags_DisableZcull)
            .create()};
    if (!queue.valid()) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::platform_failure});
    }
    auto memory = std::make_unique<Deko3dMemory>(device.get());
    auto shaders = load_shaders(*memory);
    if (!shaders.has_value()) {
      return std::unexpected(shaders.error());
    }
    auto descriptors = create_descriptors(*memory);
    if (!descriptors.has_value()) {
      return std::unexpected(descriptors.error());
    }
    std::vector<std::unique_ptr<FrameSlot>> slots;
    for (std::uint32_t index = 0; index < options.image_count(); ++index) {
      auto slot = FrameSlot::create(device.get(), *memory);
      if (!slot.has_value()) {
        return std::unexpected(slot.error());
      }
      slots.push_back(std::move(*slot));
    }
    auto replay = FrameSlot::create(device.get(), *memory);
    if (!replay.has_value()) {
      return std::unexpected(replay.error());
    }
    auto framebuffers =
        Framebuffers::create(device.get(), *memory, window, size,
                             options.image_count(), options.swap_interval);
    if (!framebuffers.has_value()) {
      return std::unexpected(framebuffers.error());
    }
    return std::make_shared<Device>(
        surface, options, std::move(device), std::move(queue),
        std::move(memory), std::move(*shaders), std::move(*descriptors),
        std::move(slots), std::move(*replay), std::move(*framebuffers));
  }

  Device(const HorizonSurface& surface, GpuOptions options, DeviceHandle device,
         QueueHandle queue, std::unique_ptr<Deko3dMemory> memory,
         Shaders shaders, Descriptors descriptors,
         std::vector<std::unique_ptr<FrameSlot>> slots,
         std::unique_ptr<FrameSlot> replay,
         std::unique_ptr<Framebuffers> framebuffers)
      : surface_(surface),
        options_(options),
        device_(std::move(device)),
        queue_(std::move(queue)),
        memory_(std::move(memory)),
        shaders_(std::move(shaders)),
        descriptors_(std::move(descriptors)),
        slots_(std::move(slots)),
        replay_(std::move(replay)),
        ring_(static_cast<std::uint32_t>(slots_.size())),
        interval_(options.swap_interval.value()),
        framebuffers_(std::move(framebuffers)),
        registry_(std::make_shared<Registry>(
            [this](std::uint32_t& slot) noexcept { retire_texture(slot); })),
        publisher_(
            std::make_shared<Publisher>(hooks_.schedule().weak_from_this())),
        surfaces_(hooks_.schedule().shared_from_this(), publisher_,
                  [this](TextureDescription description) {
                    return create_texture(description);
                  }) {
    free_slots_.reserve(max_textures);
    for (std::uint32_t slot = max_textures; slot > 0; --slot) {
      free_slots_.push_back(slot - 1);
    }
    release(slots_[0]->begin());
  }

  Device(const Device&) = delete;
  auto operator=(const Device&) -> Device& = delete;
  Device(Device&&) = delete;
  auto operator=(Device&&) -> Device& = delete;

  ~Device() {
    dk::Queue{queue_.get()}.waitIdle();
    publisher_->completed(GpuSerial{std::numeric_limits<std::uint64_t>::max()});
    registry_->retire();
  }

  auto handle() const noexcept -> dk::Device {
    return dk::Device{device_.get()};
  }

  auto queue() const noexcept -> dk::Queue { return dk::Queue{queue_.get()}; }

  auto memory() noexcept -> Deko3dMemory& { return *memory_; }

  auto options() const noexcept -> GpuOptions { return options_; }

  auto registry() const noexcept -> const std::shared_ptr<Registry>& {
    return registry_;
  }

  auto frames_presented() const noexcept -> std::uint64_t {
    return presented_.load(std::memory_order_relaxed);
  }

  auto missed_vsyncs() const noexcept -> std::uint64_t {
    return missed_.load(std::memory_order_relaxed);
  }

  auto timings() const -> GpuTimings {
    GpuTimings measured;
    {
      const std::scoped_lock lock{timings_mutex_};
      measured = timings_;
    }
    measured.missed_vsyncs = missed_.load(std::memory_order_relaxed);
    measured.encode_misses = registry_->encode_misses();
    return measured;
  }

  auto resources() const -> GpuResources {
    return GpuResources{
        .textures_created = textures_created_.load(std::memory_order_relaxed),
        .memory_blocks_created = memory_->created()};
  }

  auto hooks() noexcept -> Hooks& { return hooks_; }

  auto surfaces() noexcept -> Surfaces& { return surfaces_; }

  auto publisher() const noexcept -> const std::shared_ptr<Publisher>& {
    return publisher_;
  }

  auto upcoming() noexcept -> GpuSerial {
    return GpuSerial{hooks_.schedule().serial().value() + 1};
  }

  auto completed(GpuSerial serial) const noexcept -> bool {
    return serial.value() <= completed_.load(std::memory_order_acquire);
  }

  void set_swap_interval(SwapInterval interval) noexcept {
    const std::scoped_lock submitting{submit_mutex_};
    if (!lent_.request(interval).has_value()) {
      return;
    }
    options_.swap_interval = interval;
    interval_ = interval.value();
    if (framebuffers_ != nullptr) {
      framebuffers_->set_swap_interval(interval);
    }
  }

  auto swap_interval_changes() const noexcept -> std::uint64_t {
    return interval_changes_.load(std::memory_order_relaxed);
  }

  auto create_texture(TextureDescription description)
      -> std::expected<Texture, ProviderError> {
    if (!Texture::describable(description)) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument});
    }
    std::uint32_t flags = DkImageFlags_UsageRender;
    if (description.usage == TextureUsage::render_target) {
      flags |= DkImageFlags_Usage2DEngine;
    }
    dk::ImageLayout layout;
    dk::ImageLayoutMaker{device_.get()}
        .setFlags(flags)
        .setFormat(image_format(description.format))
        .setDimensions(description.width, description.height)
        .initialize(layout);
    if (layout.getSize() > std::numeric_limits<std::uint32_t>::max()) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::resource_exhausted});
    }
    auto allocated = memory_->allocate(
        Deko3dMemoryKind::image, static_cast<std::uint32_t>(layout.getSize()),
        layout.getAlignment());
    if (!allocated.has_value()) {
      return std::unexpected(allocated.error());
    }
    TextureStorage storage{.memory = std::move(*allocated), .image = {}};
    storage.image.initialize(layout, storage.memory.memblock(),
                             storage.memory.offset());
    const auto slot = install(storage.image, description.format);
    if (!slot.has_value()) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::resource_exhausted});
    }
    Texture texture = Registry::adopt(registry_, description, *slot);
    {
      const std::scoped_lock lock{mutex_};
      textures_.insert_or_assign(*slot, std::move(storage));
      planner_.created(texture.id(), description);
    }
    textures_created_.fetch_add(1, std::memory_order_relaxed);
    return texture;
  }

  auto import_texture(const dk::Image& image, ImportDescription description,
                      support::ScopedCallback released)
      -> std::expected<Imported, ProviderError> {
    const TextureDescription texture = description.texture();
    if (description.planes() != 1 || !Texture::describable(texture)) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument});
    }
    const auto slot = install(image, description.format);
    if (!slot.has_value()) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::resource_exhausted});
    }
    return Imported::adopt(publisher_,
                           Registry::adopt(registry_, texture, *slot),
                           description, std::move(released));
  }

  auto import_texture(std::span<const dk::Image* const> planes,
                      ImportDescription description,
                      support::ScopedCallback released)
      -> std::expected<Imported, ProviderError> {
    if (planes.size() != description.planes() ||
        std::ranges::any_of(
            planes, [](const dk::Image* plane) { return plane == nullptr; })) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument});
    }
    for (std::size_t index = 0; index < planes.size(); ++index) {
      if (!Texture::describable(description.plane(index))) {
        return std::unexpected(
            ProviderError{ProviderErrorCode::invalid_argument});
      }
    }
    std::vector<Texture> textures;
    textures.reserve(planes.size());
    for (std::size_t index = 0; index < planes.size(); ++index) {
      const TextureDescription plane = description.plane(index);
      const auto slot = install(*planes[index], plane.format);
      if (!slot.has_value()) {
        return std::unexpected(
            ProviderError{ProviderErrorCode::resource_exhausted});
      }
      textures.push_back(Registry::adopt(registry_, plane, *slot));
    }
    return Imported::adopt(publisher_, std::move(textures), description,
                           std::move(released));
  }

  auto update_texture(const Texture& texture, TextureRegion region,
                      PixelRows pixels) -> std::expected<void, ProviderError> {
    const auto record = registry_->find(texture.id());
    if (!texture.valid() || !record.has_value() ||
        !Texture::accepts(record->description, region, pixels)) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument});
    }
    const std::size_t row =
        static_cast<std::size_t>(region.width) *
        Texture::bytes_per_pixel(record->description.format);
    const std::size_t size = row * region.height;
    if (size > std::numeric_limits<std::uint32_t>::max()) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::resource_exhausted});
    }
    const std::unique_lock lock = stage();
    const auto found = textures_.find(record->native);
    if (found == textures_.end()) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument});
    }
    Deferred upload{.texture = texture.id(),
                    .native = record->native,
                    .description = record->description,
                    .region = region,
                    .bytes = {}};
    if (!staging_overlay_.has_value() && takeover_.held()) {
      upload.bytes = Deferred::pack(record->description, region, pixels);
      deferred_.push_back(std::move(upload));
      return {};
    }
    FrameSlot& staged = staging_overlay_.has_value()
                            ? staging_overlay_->get()
                            : *slots_[ring_.next().slot];
    stage_deferred(staged);
    return place(staged, upload, pixels);
  }

  struct Deferred {
    TextureId texture;
    std::uint32_t native{0};
    TextureDescription description;
    TextureRegion region;
    std::vector<std::byte> bytes;

    static auto pack(const TextureDescription& description,
                     TextureRegion region, PixelRows pixels)
        -> std::vector<std::byte> {
      const std::size_t row = static_cast<std::size_t>(region.width) *
                              Texture::bytes_per_pixel(description.format);
      std::vector<std::byte> packed(row * region.height);
      for (std::uint32_t line = 0; line < region.height; ++line) {
        std::ranges::copy(pixels.row(line, row),
                          packed.begin() + static_cast<std::ptrdiff_t>(line * row));
      }
      return packed;
    }
  };

  void stage_deferred(FrameSlot& staged) {
    std::vector<Deferred> waiting = std::exchange(deferred_, {});
    for (Deferred& each : waiting) {
      if (!textures_.contains(each.native)) {
        continue;
      }
      if (!place(staged, each, PixelRows{each.bytes}).has_value()) {
        deferred_.push_back(std::move(each));
      }
    }
  }

  auto place(FrameSlot& staged, const Deferred& upload, PixelRows pixels)
      -> std::expected<void, ProviderError> {
    const auto found = textures_.find(upload.native);
    if (found == textures_.end()) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument});
    }
    const TextureRegion region = upload.region;
    const std::size_t row =
        static_cast<std::size_t>(region.width) *
        Texture::bytes_per_pixel(upload.description.format);
    const std::size_t size = row * region.height;
    auto staging = staged.stage(size);
    if (!staging.has_value()) {
      return std::unexpected(staging.error());
    }
    for (std::uint32_t line = 0; line < region.height; ++line) {
      std::ranges::copy(pixels.row(line, row),
                        staging->bytes.subspan(line * row, row).begin());
    }
    std::optional<SurfaceSize> clear;
    if (planner_.upload(upload.texture, region).has_value()) {
      clear = SurfaceSize{.width = SurfaceWidth{upload.description.width},
                          .height = SurfaceHeight{upload.description.height}};
    }
    uploads_.push_back(
        Upload{.source = staging->chunk.get().gpu_address() + staging->offset,
               .image = found->second.image,
               .rect = DkImageRect{.x = region.x,
                                   .y = region.y,
                                   .z = 0,
                                   .width = region.width,
                                   .height = region.height,
                                   .depth = 1},
               .clear = clear});
    return {};
  }

  auto stage() -> std::unique_lock<std::mutex> {
    while (true) {
      {
        std::unique_lock lock{mutex_};
        if (staging_overlay_.has_value() || takeover_.held()) {
          return lock;
        }
      }
      const std::optional<std::uint32_t> next = ring_.await();
      std::unique_lock lock{mutex_};
      if (staging_overlay_.has_value() || takeover_.held() ||
          (next.has_value() && ring_.next().slot == *next)) {
        return lock;
      }
    }
  }

  struct Opened {
    FrameSlots::Claim claim{};
    GpuSerial serial{0};
    RegistryTicket ticket{0};
    bool replay{false};
    bool overlay{false};
  };

  [[nodiscard]] auto open_overlay_frame(FrameSlot& slot, std::uint32_t index)
      -> Opened {
    release(slot.begin());
    settle(slot);
    slot.reset_geometry();
    {
      const std::scoped_lock lock{mutex_};
      staging_overlay_ = std::ref(slot);
      stage_deferred(slot);
    }
    return Opened{.claim = FrameSlots::Claim{.slot = index, .frame = 0},
                  .serial = upcoming(),
                  .ticket = registry_->open(),
                  .replay = false,
                  .overlay = true};
  }

  auto take_overlay_uploads(std::vector<Upload>& into) -> GpuSerial {
    into.clear();
    const GpuSerial serial = hooks_.schedule().next_serial();
    const std::scoped_lock lock{mutex_};
    std::swap(into, uploads_);
    staging_overlay_.reset();
    planner_.seal();
    return serial;
  }

  void close_overlay_frame(const Opened& opened) noexcept {
    {
      const std::scoped_lock lock{mutex_};
      staging_overlay_.reset();
    }
    registry_->close(opened.ticket);
  }

  [[nodiscard]] static auto open_overlay(const std::shared_ptr<Device>& self)
      -> std::expected<std::shared_ptr<OverlayState>, ProviderError>;

  void close_overlay(const std::shared_ptr<OverlayState>& state) noexcept;

  [[nodiscard]] auto record_overlay(OverlayState& state, dk::CmdBuf& buffer,
                                    const FrameTarget& target)
      -> std::expected<bool, ProviderError>;

  void overlay_presented();

  struct Resident {
    DkGpuAddr vertices{0};
    std::uint32_t vertex_bytes{0};
    DkGpuAddr indices{0};
  };

  [[nodiscard]] auto open_frame() -> std::expected<Opened, ProviderError> {
    const std::optional<FrameSlots::Claim> claim = ring_.claim();
    if (!claim.has_value()) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::resource_exhausted});
    }
    building_ = claim->slot;
    slots_[building_]->reset_geometry();
    {
      const std::scoped_lock lock{mutex_};
      stage_deferred(*slots_[building_]);
    }
    return Opened{.claim = *claim,
                  .serial = upcoming(),
                  .ticket = registry_->open(),
                  .replay = false};
  }

  [[nodiscard]] auto open_replay() -> std::expected<Opened, ProviderError> {
    if (!last_ui_slot_.has_value()) {
      return std::unexpected(ProviderError{ProviderErrorCode::not_found});
    }
    const std::optional<GpuSerial> serial = hooks_.schedule().next_serial_after(
        GpuSerial{submitted_.load(std::memory_order_acquire)});
    if (!serial.has_value()) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::resource_exhausted});
    }
    release(replay_->begin());
    settle(*replay_);
    replay_->reset_geometry();
    return Opened{.claim = {},
                  .serial = *serial,
                  .ticket = registry_->open(),
                  .replay = true};
  }

  void close_frame(const Opened& opened) noexcept {
    if (opened.replay) {
      settle_submitted(opened.serial);
    } else {
      ring_.abandon();
    }
    registry_->close(opened.ticket);
  }

  [[nodiscard]] auto replay_indices(std::span<const Index> indices) -> bool {
    ResidentBlock& block = replay_->resident();
    const auto count = static_cast<std::uint32_t>(indices.size());
    if (!block.reserve_indices(*memory_, count)) {
      return false;
    }
    std::ranges::copy(indices, block.indices(count).begin());
    return true;
  }

  void settle_submitted(GpuSerial serial) noexcept {
    std::uint64_t seen = submitted_.load(std::memory_order_relaxed);
    while (serial.value() > seen &&
           !submitted_.compare_exchange_weak(seen, serial.value(),
                                             std::memory_order_acq_rel,
                                             std::memory_order_relaxed)) {
    }
  }

  auto render_queue() noexcept -> RenderQueue& { return render_queue_; }

  static auto take_over(const std::shared_ptr<Device>& self)
      -> std::expected<GpuTakeover, ProviderError> {
    return self->takeover_.claim(
        self->render_queue_, self->hooks_.schedule().weak_from_this(),
        [self]() {
          {
            const std::scoped_lock submitting{self->submit_mutex_};
            self->lent_.lend(self->lent_images());
          }
          self->ring_.interrupt();
          return support::ScopedCallback{
              [self]() noexcept { self->hand_back(); }};
        });
  }

  void hand_back() noexcept {
    {
      const std::scoped_lock submitting{submit_mutex_};
      const LentSwapchain<>::Settlement settled = lent_.settle();
      if (framebuffers_ != nullptr) {
        for (const LentSwapchain<>::Outstanding& each : settled.outstanding) {
          queue().presentImage(framebuffers_->swapchain(),
                               static_cast<int>(each.index));
        }
      }
      queue().waitIdle();
      drop_overlay();
      if (settled.interval.has_value()) {
        options_.swap_interval = *settled.interval;
      }
      if (const std::optional<SwapInterval> restored =
              settled.interval.has_value() ? settled.interval
                                           : SwapInterval::create(interval_);
          restored.has_value() && framebuffers_ != nullptr) {
        interval_ = restored->value();
        framebuffers_->set_swap_interval(*restored);
      }
    }
    const std::scoped_lock lock{mutex_};
    planner_.invalidate();
  }

  auto takeover_counters() const -> GpuTakeover::Counters {
    return takeover_.counters();
  }

  auto lent_images() const noexcept -> std::uint32_t {
    return framebuffers_ != nullptr ? framebuffers_->count() : 0;
  }

  auto lent_target(std::uint32_t index) -> std::optional<FrameTarget> {
    if (framebuffers_ == nullptr || index >= framebuffers_->count()) {
      return std::nullopt;
    }
    dk::Image& image = framebuffers_->image(index);
    const SurfaceSize size = framebuffers_->size();
    return FrameTarget{.image = image,
                       .view = dk::ImageView{image},
                       .format = target_format,
                       .width = size.width.value(),
                       .height = size.height.value(),
                       .generation = generation_};
  }

  auto lent_generation() const noexcept -> std::uint64_t { return generation_; }

  auto lent_acquire() -> std::expected<std::uint32_t, ProviderError> {
    if (lent_.outstanding() == 0) {
      const std::scoped_lock submitting{submit_mutex_};
      auto ready = ensure_framebuffers();
      if (!ready.has_value()) {
        return std::unexpected(ready.error());
      }
      static_cast<void>(lent_.resize(lent_images()));
    }
    if (framebuffers_ == nullptr) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::not_found,
                        ProviderErrorDetail::image_unavailable});
    }
    if (lent_.outstanding() >= framebuffers_->count()) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::resource_exhausted,
                        ProviderErrorDetail::image_unavailable});
    }
    const int index = queue().acquireImage(framebuffers_->swapchain());
    if (index < 0 ||
        static_cast<std::uint32_t>(index) >= framebuffers_->count() ||
        !lent_.acquire(static_cast<std::uint32_t>(index))) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::platform_failure,
                        ProviderErrorDetail::image_unavailable});
    }
    return static_cast<std::uint32_t>(index);
  }

  auto lent_present(std::uint32_t index) -> std::expected<void, ProviderError> {
    if (framebuffers_ == nullptr || !lent_.present(index)) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument});
    }
    overlay_presented();
    queue().presentImage(framebuffers_->swapchain(), static_cast<int>(index));
    lent_presents_.fetch_add(1, std::memory_order_relaxed);
    return {};
  }

  void lent_interval(SwapInterval interval) noexcept {
    if (framebuffers_ != nullptr) {
      framebuffers_->set_swap_interval(interval);
    }
  }

  auto lent_presents() const noexcept -> std::uint64_t {
    return lent_presents_.load(std::memory_order_relaxed);
  }

  void mark(GpuSerial serial, std::span<const BlockCommand> commands) {
    publisher_->sampled(serial, commands, &BlockCommand::command);
  }

  auto take_uploads(std::vector<Upload>& into) -> GpuSerial {
    into.clear();
    const GpuSerial serial = hooks_.schedule().next_serial();
    const std::scoped_lock lock{mutex_};
    std::swap(into, uploads_);
    planner_.seal();
    ring_.seal();
    return serial;
  }

  [[nodiscard]] auto allocate_geometry(std::uint32_t vertices,
                                       std::uint32_t indices)
      -> std::expected<FrameGeometry, ProviderError> {
    return slots_[building_]->allocate_geometry(vertices, indices);
  }

  [[nodiscard]] auto allocate_shapes(std::uint32_t vertices,
                                     std::uint32_t indices)
      -> std::expected<ShapeGeometry, ProviderError> {
    return slots_[building_]->allocate_shapes(vertices, indices);
  }

  [[nodiscard]] auto allocate_resident(std::uint32_t vertices,
                                       std::uint32_t indices)
      -> std::expected<ResidentShapes, ProviderError> {
    ResidentBlock& block = slots_[building_]->resident();
    if (!block.reserve_indices(*memory_, indices)) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::resource_exhausted});
    }
    mirror_.resize(vertices);
    return ResidentShapes{
        .geometry = ShapeGeometry{.vertices = mirror_,
                                  .indices = block.indices(indices),
                                  .block = ResidentShapes::block,
                                  .first_vertex = 0,
                                  .first_index = 0},
        .generation = 1};
  }

  void touch(VertexRange range) {
    for (const std::unique_ptr<FrameSlot>& slot : slots_) {
      slot->resident().stale(range);
    }
  }

  auto owns(const FrameGeometry& geometry) const noexcept -> bool {
    return slots_[building_]->owns(geometry);
  }

  auto owns(const ShapeGeometry& geometry) const noexcept -> bool {
    if (geometry.block == ResidentShapes::block) {
      return geometry.vertices.data() == mirror_.data();
    }
    return slots_[building_]->owns(geometry);
  }

  struct Recording {
    std::optional<Colour> clear;
    std::span<const BlockCommand> commands;
    std::span<const OffscreenPass> offscreen;
    std::span<const Surfaces::Hole> holes;
    std::span<Upload> uploads;
    std::span<const Surfaces::Due> due;
    std::span<dk::Fence> fences;
    std::optional<ClipRect> damage;
    bool resident{false};
    RegistryTicket ticket{0};
    bool replay{false};
  };

  struct Presented {
    std::optional<std::chrono::nanoseconds> displayed;
    std::optional<Image> image;
  };

  struct Execution {
    dk::CmdBuf& buffer;
    const FrameSlot& slot;
    Resident resident;
    const Recording& recording;
    const dk::ImageView& view;
    SurfaceSize size;
    FrameTarget target;
    FrameInfo info;
    FramePlan plan;
    bool hooked{false};
    std::uint32_t drawn{0};
    bool cleared{false};
    bool layered{false};
    bool prelude{true};
    bool screen{true};
  };

  auto plan_surfaces(std::span<const BlockCommand> commands,
                     std::span<OffscreenPass> offscreen, GpuSerial serial)
      -> std::pair<std::vector<Surfaces::Hole>, std::vector<Surfaces::Due>> {
    std::pair<std::vector<Surfaces::Hole>, std::vector<Surfaces::Due>> found;
    if (!surfaces_.installed()) {
      return found;
    }
    found.first = surfaces_.holes(commands, &BlockCommand::command);
    found.second = surfaces_.due(commands, serial, &BlockCommand::command);
    for (OffscreenPass& each : offscreen) {
      each.holes = surfaces_.holes(std::span<const BlockCommand>{each.commands},
                                   &BlockCommand::command);
      for (const Surfaces::Due& due :
           surfaces_.due(std::span<const BlockCommand>{each.commands}, serial,
                         &BlockCommand::command)) {
        found.second.push_back(due);
      }
    }
    return found;
  }

  void drain_fences(std::vector<dk::Fence>& into) {
    into.clear();
    publisher_->drain(
        [&into](const dk::Fence& published) { into.push_back(published); });
  }

  auto submit(const FramePacket& packet, const Recording& recording,
              FramePacket::Clock clock)
      -> std::expected<FrameReceipt, ProviderError> {
    const FrameStopwatch stopwatch{clock};
    const std::chrono::nanoseconds began = stopwatch.now();
    std::chrono::nanoseconds waited{0};
    auto rendered = render(packet, recording, stopwatch, waited);
    registry_->close(recording.ticket);
    settle_submitted(packet.serial());
    if (!rendered.has_value()) {
      return std::unexpected(rendered.error());
    }
    return FrameReceipt{.record = std::max(stopwatch.since(began) - waited,
                                           std::chrono::nanoseconds{0}),
                        .wait = waited,
                        .displayed = rendered->displayed,
                        .image = std::move(rendered->image)};
  }

  void abandon(const FramePacket& packet, const Opened& opened,
               std::span<Upload> uploads) noexcept {
    if (opened.replay) {
      const std::scoped_lock submitting{submit_mutex_};
      close_frame(opened);
      return;
    }
    const std::optional<RenderQueue::Lease> used = render_queue_.submit();
    const std::scoped_lock submitting{submit_mutex_};
    FrameSlot& slot = *slots_[packet.slot()];
    std::chrono::nanoseconds ignored{0};
    advance(packet, FrameStopwatch{std::nullopt}, ignored);
    dk::CmdBuf buffer{slot.raw()};
    const bool uploaded = used.has_value() && record_uploads(buffer, uploads);
    if (used.has_value() && (record_owed(buffer) || uploaded)) {
      dk::Queue queue_handle = queue();
      queue_handle.submitCommands(buffer.finishList());
      queue_handle.signalFence(slot.fence(), false);
      queue_handle.flush();
      slot.submitted(take_pending(), packet.serial());
    }
    settle_submitted(packet.serial());
    registry_->close(opened.ticket);
  }

  auto render(const FramePacket& packet, const Recording& recording,
              const FrameStopwatch& stopwatch, std::chrono::nanoseconds& waited)
      -> std::expected<Presented, ProviderError> {
    const FramePlan plan = packet.plan();
    const bool replaying = recording.replay;
    const std::optional<RenderQueue::Lease> used = render_queue_.submit();
    const std::scoped_lock submitting{submit_mutex_};
    if (!used.has_value()) {
      if (!replaying) {
        advance(packet, stopwatch, waited);
      }
      return std::unexpected(
          ProviderError{ProviderErrorCode::resource_exhausted,
                        ProviderErrorDetail::taken_over});
    }
    FrameSlot& slot = replaying ? *replay_ : *slots_[packet.slot()];
    Resident resident{};
    if (replaying) {
      if (!last_ui_slot_.has_value()) {
        return std::unexpected(ProviderError{ProviderErrorCode::not_found});
      }
      const ResidentBlock& source = slots_[*last_ui_slot_]->resident();
      resident = Resident{.vertices = source.vertex_address(),
                          .vertex_bytes = source.vertex_bytes(),
                          .indices = replay_->resident().index_address()};
      replay_reads_ = last_ui_slot_;
    } else {
      if (recording.resident) {
        if (replay_reads_ == packet.slot()) {
          const std::chrono::nanoseconds fenced = stopwatch.now();
          replay_->wait();
          waited += stopwatch.since(fenced);
          replay_reads_.reset();
        }
        if (!slot.resident().refresh(*memory_, mirror_)) {
          invalidate();
          advance(packet, stopwatch, waited);
          return std::unexpected(
              ProviderError{ProviderErrorCode::resource_exhausted});
        }
      }
      resident = Resident{.vertices = slot.resident().vertex_address(),
                          .vertex_bytes = slot.resident().vertex_bytes(),
                          .indices = slot.resident().index_address()};
      advance(packet, stopwatch, waited);
    }
    dk::CmdBuf buffer{slot.raw()};
    dk::Queue queue_handle = queue();
    const bool uploaded = record_uploads(buffer, recording.uploads);
    const auto retire = [&]() {
      slot.submitted(take_pending(), packet.serial());
    };
    const auto flush_uploads = [&]() {
      if (!record_owed(buffer) && !uploaded) {
        return;
      }
      queue_handle.submitCommands(buffer.finishList());
      queue_handle.signalFence(slot.fence(), false);
      queue_handle.flush();
      retire();
    };
    auto ready = ensure_framebuffers();
    if (!ready.has_value() || !*ready) {
      invalidate();
      flush_uploads();
    }
    if (!ready.has_value()) {
      return std::unexpected(ready.error());
    }
    if (!*ready) {
      return Presented{};
    }
    pace(plan.swap_interval);
    const std::uint64_t began = armGetSystemTick();
    const std::chrono::nanoseconds acquiring = stopwatch.now();
    const int image_index =
        queue_handle.acquireImage(framebuffers_->swapchain());
    const std::chrono::nanoseconds latched = stopwatch.now();
    waited += latched - acquiring;
    if (image_index < 0 ||
        static_cast<std::uint32_t>(image_index) >= framebuffers_->count()) {
      invalidate();
      flush_uploads();
      return std::unexpected(
          ProviderError{ProviderErrorCode::platform_failure});
    }
    dk::Image& image =
        framebuffers_->image(static_cast<std::size_t>(image_index));
    const dk::ImageView view{image};
    const SurfaceSize size = framebuffers_->size();
    const bool hooked = hooks_.installed();
    std::span<const gpu::FramePlanner::Step> steps;
    {
      const std::scoped_lock lock{mutex_};
      steps = planner_.plan(recorded(recording, size,
                                     static_cast<std::size_t>(image_index),
                                     framebuffers_->count(), hooked));
    }
    if (descriptors_dirty_.exchange(false, std::memory_order_acq_rel)) {
      buffer.barrier(DkBarrier_None, DkInvalidateFlags_Descriptors);
    }
    for (dk::Fence& published : recording.fences) {
      queue_handle.waitFence(published);
    }
    if (!recording.fences.empty()) {
      buffer.barrier(DkBarrier_None, DkInvalidateFlags_Image);
    }
    buffer.reportCounter(DkCounter_Timestamp, slot.stamp_address(0));
    const GpuSerial serial = packet.serial();
    Execution run{.buffer = buffer,
                  .slot = slot,
                  .resident = resident,
                  .recording = recording,
                  .view = view,
                  .size = size,
                  .target = FrameTarget{.image = image,
                                        .view = view,
                                        .format = target_format,
                                        .width = size.width.value(),
                                        .height = size.height.value(),
                                        .generation = generation_},
                  .info = FrameInfo{.serial = serial,
                                    .predicted_present = plan.predicted_present,
                                    .extent = size,
                                    .replay = plan.replay,
                                    .damage = recording.damage},
                  .plan = plan,
                  .hooked = hooked,
                  .drawn = 0,
                  .cleared = false,
                  .layered = false,
                  .prelude = true};
    for (const gpu::FramePlanner::Step& step : steps) {
      std::visit([this, &run](const auto& each) { execute(each, run); }, step);
    }
    const std::uint32_t drawn = run.drawn;
    slot.stamped(drawn > 0, hooked);
    queue_handle.submitCommands(buffer.finishList());
    queue_handle.signalFence(slot.fence(), false);
    const std::chrono::nanoseconds presenting = stopwatch.now();
    queue_handle.presentImage(framebuffers_->swapchain(), image_index);
    waited += stopwatch.since(presenting);
    retire();
    last_presented_ = static_cast<std::size_t>(image_index);
    count_vsync(began);
    passes_ = drawn;
    if (!replaying) {
      last_ui_slot_ = packet.slot();
    }
    Presented result;
    if (stopwatch.timed()) {
      result.displayed =
          latched +
          (display_period * std::max<std::uint32_t>(plan.swap_interval, 1));
    }
    if (plan.capture) {
      if (auto captured = read_back(); captured.has_value()) {
        result.image = std::move(*captured);
      }
    }
    return result;
  }

  void pace(std::uint32_t interval) {
    const std::uint32_t wanted =
        std::clamp<std::uint32_t>(interval, 1, SwapInterval::maximum());
    if (wanted == interval_ || framebuffers_ == nullptr) {
      return;
    }
    const std::optional<SwapInterval> chosen = SwapInterval::create(wanted);
    if (!chosen.has_value()) {
      return;
    }
    interval_ = wanted;
    framebuffers_->set_swap_interval(*chosen);
    interval_changes_.fetch_add(1, std::memory_order_relaxed);
  }

  auto record_uploads(dk::CmdBuf& buffer, std::span<Upload> uploads) -> bool {
    if (uploads.empty()) {
      return false;
    }
    bool cleared = false;
    for (const Upload& upload : uploads) {
      if (upload.clear.has_value()) {
        clear_texture(buffer,
                      Layer{.image = upload.image, .size = *upload.clear});
        cleared = true;
      }
    }
    buffer.barrier(cleared ? DkBarrier_Full : DkBarrier_Fragments, 0);
    for (const Upload& upload : uploads) {
      const dk::ImageView view{upload.image};
      buffer.copyBufferToImage(
          DkCopyBuf{.addr = upload.source, .rowLength = 0, .imageHeight = 0},
          view, upload.rect);
    }
    buffer.barrier(DkBarrier_Full, DkInvalidateFlags_Image);
    return true;
  }

  auto capture() -> std::expected<Image, ProviderError> {
    if (render_queue_.parked()) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::resource_exhausted,
                        ProviderErrorDetail::taken_over});
    }
    const std::optional<RenderQueue::Lease> used = render_queue_.submit();
    if (!used.has_value()) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::resource_exhausted,
                        ProviderErrorDetail::taken_over});
    }
    const std::scoped_lock submitting{submit_mutex_};
    return read_back();
  }

  auto read_back() -> std::expected<Image, ProviderError> {
    if (framebuffers_ == nullptr || !last_presented_.has_value()) {
      return std::unexpected(ProviderError{ProviderErrorCode::not_found});
    }
    const SurfaceSize size = framebuffers_->size();
    const std::uint64_t bytes = static_cast<std::uint64_t>(size.width.value()) *
                                size.height.value() * Image::channels;
    if (bytes > std::numeric_limits<std::uint32_t>::max()) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::resource_exhausted});
    }
    if (!readback_.valid() || readback_.size() < bytes) {
      auto allocated = memory_->allocate(Deko3dMemoryKind::data,
                                         static_cast<std::uint32_t>(bytes),
                                         staging_alignment);
      if (!allocated.has_value()) {
        return std::unexpected(allocated.error());
      }
      readback_ = std::move(*allocated);
    }
    const dk::ImageView view{framebuffers_->image(*last_presented_)};
    dk::CmdBuf commands{slots_[prepared_]->raw()};
    commands.copyImageToBuffer(
        view,
        DkImageRect{.x = 0,
                    .y = 0,
                    .z = 0,
                    .width = size.width.value(),
                    .height = size.height.value(),
                    .depth = 1},
        DkCopyBuf{
            .addr = readback_.gpu_address(), .rowLength = 0, .imageHeight = 0});
    dk::Queue queue_handle = queue();
    queue_handle.submitCommands(commands.finishList());
    dk::Fence done;
    queue_handle.signalFence(done, true);
    queue_handle.flush();
    if (done.wait(capture_timeout.count()) != DkResult_Success) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::platform_failure});
    }
    const std::span<const std::byte> mapped =
        readback_.cpu_bytes().first(static_cast<std::size_t>(bytes));
    return Image::create(size.width.value(), size.height.value(),
                         std::vector<std::byte>{mapped.begin(), mapped.end()});
  }

 private:
  [[nodiscard]] static auto load_shaders(Deko3dMemory& memory)
      -> std::expected<Shaders, ProviderError> {
    const std::array<std::span<const std::uint8_t>, 4> sources{
        Deko3dShaders::textured_vertex(), Deko3dShaders::textured_fragment(),
        Deko3dShaders::shape_vertex(), Deko3dShaders::shape_fragment()};
    std::array<std::uint32_t, 4> offsets{};
    std::uint64_t total = 0;
    for (std::size_t index = 0; index < sources.size(); ++index) {
      total = Deko3dMemory::align_up(total, DK_SHADER_CODE_ALIGNMENT);
      offsets[index] = static_cast<std::uint32_t>(total);
      total += sources[index].size();
    }
    auto code = memory.allocate(Deko3dMemoryKind::code,
                                static_cast<std::uint32_t>(total),
                                DK_SHADER_CODE_ALIGNMENT);
    if (!code.has_value()) {
      return std::unexpected(code.error());
    }
    const std::span<std::byte> target = code->cpu_bytes();
    for (std::size_t index = 0; index < sources.size(); ++index) {
      std::ranges::copy(std::as_bytes(sources[index]),
                        target.subspan(offsets[index]).begin());
    }
    Shaders shaders{.code = std::move(*code),
                    .vertex = {},
                    .fragment = {},
                    .shape_vertex = {},
                    .shape_fragment = {}};
    const std::array<std::reference_wrapper<dk::Shader>, 4> outputs{
        shaders.vertex, shaders.fragment, shaders.shape_vertex,
        shaders.shape_fragment};
    for (std::size_t index = 0; index < outputs.size(); ++index) {
      dk::ShaderMaker{shaders.code.memblock(),
                      shaders.code.offset() + offsets[index]}
          .initialize(outputs[index].get());
      if (!outputs[index].get().isValid()) {
        return std::unexpected(
            ProviderError{ProviderErrorCode::platform_failure});
      }
    }
    return shaders;
  }

  [[nodiscard]] static auto create_descriptors(Deko3dMemory& memory)
      -> std::expected<Descriptors, ProviderError> {
    auto images = memory.allocate(Deko3dMemoryKind::data,
                                  max_textures * sizeof(DkImageDescriptor),
                                  DK_IMAGE_DESCRIPTOR_ALIGNMENT);
    auto samplers = memory.allocate(Deko3dMemoryKind::data,
                                    sampler_count * sizeof(DkSamplerDescriptor),
                                    DK_SAMPLER_DESCRIPTOR_ALIGNMENT);
    if (!images.has_value() || !samplers.has_value()) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::resource_exhausted});
    }
    std::array<dk::SamplerDescriptor, sampler_count> described{};
    for (const SamplerFilter filter :
         {SamplerFilter::linear, SamplerFilter::nearest}) {
      const DkFilter picked =
          filter == SamplerFilter::nearest ? DkFilter_Nearest : DkFilter_Linear;
      described[sampler_of(filter)].initialize(
          dk::Sampler{}
              .setFilter(picked, picked)
              .setWrapMode(DkWrapMode_ClampToEdge, DkWrapMode_ClampToEdge,
                           DkWrapMode_ClampToEdge));
    }
    std::ranges::copy(std::as_bytes(std::span{described}),
                      samplers->cpu_bytes().begin());
    return Descriptors{.images = std::move(*images),
                       .samplers = std::move(*samplers)};
  }

  auto ensure_framebuffers() -> std::expected<bool, ProviderError> {
    const SurfaceSize wanted = surface_.get().size();
    if (wanted.empty()) {
      return false;
    }
    if (framebuffers_ != nullptr && framebuffers_->size() == wanted) {
      return true;
    }
    for (const std::unique_ptr<FrameSlot>& slot : slots_) {
      slot->wait();
      settle(*slot);
    }
    replay_->wait();
    settle(*replay_);
    publisher_->completed(
        GpuSerial{completed_.load(std::memory_order_acquire)});
    queue().waitIdle();
    framebuffers_.reset();
    last_presented_.reset();
    invalidate();
    auto created = Framebuffers::create(
        device_.get(), *memory_, surface_.get().window(), wanted,
        options_.image_count(), options_.swap_interval);
    if (!created.has_value()) {
      return std::unexpected(created.error());
    }
    framebuffers_ = std::move(*created);
    ++generation_;
    interval_ = options_.swap_interval.value();
    return true;
  }

  void draw_surfaces(dk::CmdBuf& buffer, std::span<const Surfaces::Due> due,
                     GpuSerial serial, const FramePlan& plan,
                     std::optional<ClipRect> damage) {
    bool drew = false;
    for (const Surfaces::Due& each : due) {
      const auto record = registry_->find(each.texture);
      if (!record.has_value()) {
        continue;
      }
      std::optional<dk::Image> image;
      {
        const std::scoped_lock lock{mutex_};
        const auto found = textures_.find(record->native);
        if (found != textures_.end()) {
          image = found->second.image;
        }
      }
      if (!image.has_value()) {
        continue;
      }
      const dk::ImageView view{*image};
      const std::uint32_t width = record->description.width;
      const std::uint32_t height = record->description.height;
      const FrameTarget target{
          .image = *image,
          .view = view,
          .format = image_format(record->description.format),
          .width = width,
          .height = height};
      drew =
          surfaces_.run(
              each.surface, buffer, target,
              SurfaceFrame{
                  .surface = each.surface,
                  .mode = SurfaceMode::texture,
                  .frame =
                      FrameInfo{
                          .serial = serial,
                          .predicted_present = plan.predicted_present,
                          .extent =
                              SurfaceSize{.width = SurfaceWidth{width},
                                          .height = SurfaceHeight{height}},
                          .replay = plan.replay,
                          .damage = damage},
                  .viewport = PassRegion{.x = 0.0F,
                                         .y = 0.0F,
                                         .width = static_cast<float>(width),
                                         .height = static_cast<float>(height)},
                  .scissor =
                      ClipRect{
                          .x = 0, .y = 0, .width = width, .height = height}}) ||
          drew;
    }
    if (drew) {
      buffer.barrier(DkBarrier_Fragments, DkInvalidateFlags_Image);
    }
  }

  static void bind_target(dk::CmdBuf& buffer, const dk::ImageView& view,
                          SurfaceSize size) {
    buffer.bindRenderTargets(&view);
    buffer.setViewports(
        0, DkViewport{.x = 0.0F,
                      .y = 0.0F,
                      .width = static_cast<float>(size.width.value()),
                      .height = static_cast<float>(size.height.value()),
                      .near = 0.0F,
                      .far = 1.0F});
    buffer.setScissors(0, DkScissor{.x = 0,
                                    .y = 0,
                                    .width = size.width.value(),
                                    .height = size.height.value()});
  }

  void invalidate() noexcept { planner_.invalidate(); }

  auto recorded(const Recording& recording, SurfaceSize size, std::size_t image,
                std::size_t images, bool whole)
      -> gpu::FramePlanner::RecordedFrame {
    recorded_passes_.clear();
    pass_layers_.clear();
    for (const OffscreenPass& each : recording.offscreen) {
      std::optional<Layer> layer;
      if (const auto record =
              registry_->encoded(each.pass.target, EncodeStage::pass_target)) {
        const auto found = textures_.find(record->native);
        if (found != textures_.end()) {
          layer =
              Layer{.image = found->second.image,
                    .size = SurfaceSize{
                        .width = SurfaceWidth{record->description.width},
                        .height = SurfaceHeight{record->description.height}}};
        }
      }
      recorded_passes_.push_back(gpu::FramePlanner::RecordedPass{
          .pass = each.pass,
          .target =
              layer.has_value() ? std::optional{layer->size} : std::nullopt,
          .batch = gpu::FramePlanner::Batch{.commands = each.commands.size(),
                                            .holes = each.holes}});
      pass_layers_.push_back(layer);
    }
    screen_batches_[0] = gpu::FramePlanner::Batch{
        .commands = recording.commands.size(), .holes = recording.holes};
    return gpu::FramePlanner::RecordedFrame{
        .passes = recorded_passes_,
        .screen =
            gpu::FramePlanner::RecordedScreen{.image = image,
                                              .images = images,
                                              .size = size,
                                              .damage = recording.damage,
                                              .whole = whole,
                                              .batches = screen_batches_}};
  }

  auto layer_of(TextureId texture) const -> std::optional<Layer> {
    const auto record = registry_->find(texture);
    if (!record.has_value()) {
      return std::nullopt;
    }
    const auto found = textures_.find(record->native);
    if (found == textures_.end()) {
      return std::nullopt;
    }
    return Layer{.image = found->second.image,
                 .size = SurfaceSize{
                     .width = SurfaceWidth{record->description.width},
                     .height = SurfaceHeight{record->description.height}}};
  }

  static void clear_texture(dk::CmdBuf& buffer, const Layer& layer) {
    const dk::ImageView view{layer.image};
    bind_target(buffer, view, layer.size);
    buffer.clearColor(0, DkColorMask_RGBA, transparent.red, transparent.green,
                      transparent.blue, transparent.alpha);
  }

  auto record_owed(dk::CmdBuf& buffer) -> bool {
    owed_layers_.clear();
    {
      const std::scoped_lock lock{mutex_};
      for (const gpu::FramePlanner::Step& step : planner_.owed()) {
        const auto* clear = std::get_if<gpu::FramePlanner::ClearTexture>(&step);
        if (clear == nullptr) {
          continue;
        }
        if (std::optional<Layer> layer = layer_of(clear->texture)) {
          owed_layers_.push_back(*layer);
        }
      }
    }
    for (const Layer& layer : owed_layers_) {
      clear_texture(buffer, layer);
    }
    if (owed_layers_.empty()) {
      return false;
    }
    buffer.barrier(DkBarrier_Fragments, DkInvalidateFlags_Image);
    return true;
  }

  void execute(const gpu::FramePlanner::ClearTexture& step, Execution& run) {
    if (!run.prelude) {
      return;
    }
    std::optional<Layer> layer;
    {
      const std::scoped_lock lock{mutex_};
      layer = layer_of(step.texture);
    }
    if (layer.has_value()) {
      clear_texture(run.buffer, *layer);
      run.cleared = true;
    }
  }

  void execute(const gpu::FramePlanner::RenderSurfaces&, Execution& run) {
    if (!run.prelude) {
      return;
    }
    if (std::exchange(run.cleared, false)) {
      run.buffer.barrier(DkBarrier_Fragments, DkInvalidateFlags_Image);
    }
    draw_surfaces(run.buffer, run.recording.due, run.info.serial, run.plan,
                  run.recording.damage);
  }

  void execute(const gpu::FramePlanner::SkippedPass&, Execution&) {}

  void execute(const gpu::FramePlanner::OffscreenPass& step, Execution& run) {
    if (!run.prelude) {
      return;
    }
    const Layer& layer = *pass_layers_[step.pass];
    const OffscreenPass& each = run.recording.offscreen[step.pass];
    const std::span<const BlockCommand> commands{each.commands};
    if (!std::exchange(run.layered, true)) {
      run.buffer.barrier(DkBarrier_Fragments, DkInvalidateFlags_Image);
    }
    const dk::ImageView layer_view{layer.image};
    bind_target(run.buffer, layer_view, layer.size);
    if (step.clear.has_value()) {
      run.buffer.clearColor(0, DkColorMask_RGBA, step.clear->red,
                            step.clear->green, step.clear->blue,
                            step.clear->alpha);
    }
    const Colour hole = run.recording.clear.value_or(transparent);
    for (const gpu::FramePlanner::Segment& segment : step.segments) {
      if (const auto* range =
              std::get_if<gpu::FramePlanner::DrawRange>(&segment)) {
        record_ui(run.buffer, run.slot, run.resident, layer.size,
                  commands.subspan(range->first, range->count), step.region,
                  std::nullopt);
        continue;
      }
      const ClipRect area =
          std::get<gpu::FramePlanner::UnderlayFill>(segment).scissor;
      run.buffer.setScissors(0, DkScissor{.x = area.x,
                                          .y = area.y,
                                          .width = area.width,
                                          .height = area.height});
      run.buffer.clearColor(0, DkColorMask_RGBA, hole.red, hole.green,
                            hole.blue, hole.alpha);
    }
    run.buffer.barrier(DkBarrier_Fragments, DkInvalidateFlags_Image);
    ++run.drawn;
  }

  void execute(const gpu::FramePlanner::ScreenPass& step, Execution& run) {
    if (!run.screen) {
      return;
    }
    dk::CmdBuf& buffer = run.buffer;
    const ClipRect region = step.region;
    buffer.reportCounter(DkCounter_Timestamp, run.slot.stamp_address(1));
    bind_target(buffer, run.view, run.size);
    if (step.scissor.has_value() && step.painted()) {
      buffer.setScissors(0, DkScissor{.x = region.x,
                                      .y = region.y,
                                      .width = region.width,
                                      .height = region.height});
    }
    const std::optional<Colour> clear = run.recording.clear;
    if (clear.has_value() && step.painted()) {
      buffer.clearColor(0, DkColorMask_RGBA, clear->red, clear->green,
                        clear->blue, clear->alpha);
    }
    buffer.reportCounter(DkCounter_Timestamp, run.slot.stamp_address(2));
    if (run.hooked &&
        hooks_.run(FrameStage::before_ui, buffer, run.target, run.info)) {
      bind_target(buffer, run.view, run.size);
    }
    buffer.reportCounter(DkCounter_Timestamp, run.slot.stamp_address(3));
    const PassRegion screen_region{
        .x = 0.0F,
        .y = 0.0F,
        .width = static_cast<float>(run.size.width.value()),
        .height = static_cast<float>(run.size.height.value())};
    const Colour hole = clear.value_or(transparent);
    for (const gpu::FramePlanner::Segment& segment : step.segments) {
      if (const auto* range =
              std::get_if<gpu::FramePlanner::DrawRange>(&segment)) {
        record_ui(buffer, run.slot, run.resident, run.size,
                  run.recording.commands.subspan(range->first, range->count),
                  screen_region, step.scissor);
        continue;
      }
      const auto& fill = std::get<gpu::FramePlanner::UnderlayFill>(segment);
      buffer.setScissors(0, DkScissor{.x = fill.scissor.x,
                                      .y = fill.scissor.y,
                                      .width = fill.scissor.width,
                                      .height = fill.scissor.height});
      buffer.clearColor(0, DkColorMask_RGBA, hole.red, hole.green, hole.blue,
                        hole.alpha);
      static_cast<void>(
          surfaces_.run(fill.underlay.surface, buffer, run.target,
                        SurfaceFrame{.surface = fill.underlay.surface,
                                     .mode = SurfaceMode::underlay,
                                     .frame = run.info,
                                     .viewport = fill.underlay.viewport,
                                     .scissor = fill.scissor}));
      bind_target(buffer, run.view, run.size);
    }
    buffer.reportCounter(DkCounter_Timestamp, run.slot.stamp_address(4));
    if (run.hooked) {
      hooks_.run(FrameStage::after_ui, buffer, run.target, run.info);
    }
    buffer.reportCounter(DkCounter_Timestamp, run.slot.stamp_address(5));
  }

  void record_ui(dk::CmdBuf& buffer, const FrameSlot& slot, Resident resident,
                 SurfaceSize size, std::span<const BlockCommand> commands,
                 const PassRegion& region, std::optional<ClipRect> scissor) {
    const std::uint32_t width = size.width.value();
    const std::uint32_t height = size.height.value();
    const Transform transform{
        .scale = {2.0F / region.width, -2.0F / region.height},
        .translation = {-1.0F - (2.0F * region.x / region.width),
                        1.0F + (2.0F * region.y / region.height)}};
    const float across = static_cast<float>(width) / region.width;
    const float down = static_cast<float>(height) / region.height;
    const auto mapped = [](float edge, float origin, float factor,
                           std::uint32_t limit, bool up) {
      const float value = (edge - origin) * factor;
      const float rounded = up ? std::ceil(value) : std::floor(value);
      return static_cast<std::uint32_t>(
          std::clamp(rounded, 0.0F, static_cast<float>(limit)));
    };
    buffer.bindUniformBuffer(DkStage_Vertex, 0, slot.uniforms().gpu_address(),
                             uniform_size);
    buffer.pushConstants(slot.uniforms().gpu_address(), uniform_size, 0,
                         sizeof(Transform), &transform);
    const DkGpuAddr trim_address = slot.uniforms().gpu_address() + uniform_size;
    buffer.bindUniformBuffer(DkStage_Fragment, 1, trim_address, uniform_size);
    std::optional<std::array<float, 2>> bound_trim;
    const DkGpuAddr placement_address =
        slot.uniforms().gpu_address() + (2 * uniform_size);
    buffer.bindUniformBuffer(DkStage_Vertex, 1, placement_address,
                             uniform_size);
    std::optional<std::array<float, 4>> bound_placement;
    buffer.bindRasterizerState(dk::RasterizerState{}.setCullMode(DkFace_None));
    buffer.bindColorState(dk::ColorState{}.setBlendEnable(0, true));
    buffer.bindColorWriteState(dk::ColorWriteState{});
    buffer.bindDepthStencilState(
        dk::DepthStencilState{}.setDepthTestEnable(false).setDepthWriteEnable(
            false));
    buffer.bindBlendStates(
        0, dk::BlendState{}.setFactors(
               DkBlendFactor_SrcAlpha, DkBlendFactor_InvSrcAlpha,
               DkBlendFactor_One, DkBlendFactor_InvSrcAlpha));
    buffer.bindImageDescriptorSet(descriptors_.images.gpu_address(),
                                  max_textures);
    buffer.bindSamplerDescriptorSet(descriptors_.samplers.gpu_address(),
                                    sampler_count);
    std::optional<PipelineKind> bound_pipeline;
    std::optional<std::uint32_t> bound_block;
    std::optional<std::pair<std::uint64_t, std::uint64_t>> bound_texture;
    for (const auto& [kind, block, command] : commands) {
      std::uint32_t left = mapped(static_cast<float>(command.clip.x), region.x,
                                  across, width, false);
      std::uint32_t top = mapped(static_cast<float>(command.clip.y), region.y,
                                 down, height, false);
      std::uint32_t right =
          mapped(static_cast<float>(command.clip.x + command.clip.width),
                 region.x, across, width, true);
      std::uint32_t bottom =
          mapped(static_cast<float>(command.clip.y + command.clip.height),
                 region.y, down, height, true);
      if (scissor.has_value()) {
        left = std::max(left, scissor->x);
        top = std::max(top, scissor->y);
        right = std::min(right, scissor->x + scissor->width);
        bottom = std::min(bottom, scissor->y + scissor->height);
      }
      if (right <= left || bottom <= top || command.index_count == 0) {
        continue;
      }
      DkGpuAddr vertex_address = 0;
      std::uint32_t vertex_bytes = 0;
      DkGpuAddr index_address = 0;
      if (kind == PipelineKind::textured) {
        const auto* found = slot.geometry().block(block);
        if (found == nullptr) {
          continue;
        }
        vertex_address = found->vertex_address();
        vertex_bytes = found->vertex_bytes();
        index_address = found->index_address();
      } else if (block == ResidentShapes::block) {
        vertex_address = resident.vertices;
        vertex_bytes = resident.vertex_bytes;
        index_address = resident.indices;
      } else {
        const auto* found = slot.shapes().block(block);
        if (found == nullptr) {
          continue;
        }
        vertex_address = found->vertex_address();
        vertex_bytes = found->vertex_bytes();
        index_address = found->index_address();
      }
      if (bound_pipeline != kind) {
        bound_pipeline = kind;
        bound_block.reset();
        if (kind == PipelineKind::textured) {
          buffer.bindShaders(DkStageFlag_GraphicsMask,
                             {&shaders_.vertex, &shaders_.fragment});
          buffer.bindVtxAttribState(vertex_attributes);
          buffer.bindVtxBufferState(vertex_buffers);
        } else {
          buffer.bindShaders(
              DkStageFlag_GraphicsMask,
              {&shaders_.shape_vertex, &shaders_.shape_fragment});
          buffer.bindVtxAttribState(shape_attributes);
          buffer.bindVtxBufferState(shape_buffers);
        }
      }
      if (bound_block != block) {
        bound_block = block;
        buffer.bindVtxBuffer(0, vertex_address, vertex_bytes);
        buffer.bindIdxBuffer(DkIdxFormat_Uint32, index_address);
      }
      const TextureId detail =
          command.detail.value() != 0 ? command.detail : command.texture;
      const std::pair<std::uint64_t, std::uint64_t> wanted{
          command.texture.value(), detail.value()};
      if (bound_texture != wanted) {
        const auto record =
            registry_->encoded(command.texture, EncodeStage::sampled_texture);
        const auto second =
            registry_->encoded(detail, EncodeStage::sampled_texture);
        if (!record.has_value() || !second.has_value()) {
          continue;
        }
        bound_texture = wanted;
        const std::array<DkResHandle, 2> handles{
            dkMakeTextureHandle(record->native,
                                sampler_of(record->description.filter)),
            dkMakeTextureHandle(second->native,
                                sampler_of(second->description.filter))};
        buffer.bindTextures(DkStage_Fragment, 0, handles);
      }
      if (kind != PipelineKind::textured && bound_trim != command.trim) {
        bound_trim = command.trim;
        const std::array<float, 4> trim{command.trim[0], command.trim[1], 0.0F,
                                        0.0F};
        buffer.pushConstants(trim_address, uniform_size, 0, sizeof(trim),
                             trim.data());
      }
      if (kind != PipelineKind::textured &&
          bound_placement != command.placement()) {
        bound_placement = command.placement();
        buffer.pushConstants(placement_address, uniform_size, 0,
                             sizeof(*bound_placement), bound_placement->data());
      }
      buffer.setScissors(0, DkScissor{.x = left,
                                      .y = top,
                                      .width = right - left,
                                      .height = bottom - top});
      buffer.drawIndexed(DkPrimitive_Triangles, command.index_count, 1,
                         command.index_offset,
                         static_cast<std::int32_t>(command.vertex_offset), 0);
    }
  }

  void drop_overlay() noexcept {
    if (overlay_ == nullptr) {
      return;
    }
    retire(*overlay_);
    overlay_.reset();
  }

  static void retire(OverlayState& state) noexcept;

  auto embed(const Recording& recording, FrameSlot& slot, dk::CmdBuf& buffer,
             const FrameTarget& target, const FramePlan& plan, GpuSerial serial,
             bool fresh, bool screen) -> bool {
    bool wrote = false;
    if (fresh) {
      wrote = record_uploads(buffer, recording.uploads);
      wrote = record_owed(buffer) || wrote;
      for (dk::Fence& published : recording.fences) {
        queue().waitFence(published);
      }
      if (!recording.fences.empty()) {
        buffer.barrier(DkBarrier_None, DkInvalidateFlags_Image);
        wrote = true;
      }
      slot.submitted(take_pending(), serial);
    }
    const bool drawn = screen && !recording.commands.empty();
    if (!drawn && !(fresh && !recording.offscreen.empty())) {
      return wrote;
    }
    if (descriptors_dirty_.exchange(false, std::memory_order_acq_rel)) {
      buffer.barrier(DkBarrier_None, DkInvalidateFlags_Descriptors);
    }
    const SurfaceSize size{.width = SurfaceWidth{target.width},
                           .height = SurfaceHeight{target.height}};
    std::span<const gpu::FramePlanner::Step> steps;
    {
      const std::scoped_lock lock{mutex_};
      steps = overlay_planner_.plan(recorded(recording, size, 0, 1, true));
    }
    const dk::ImageView view{target.image};
    Execution run{.buffer = buffer,
                  .slot = slot,
                  .resident = Resident{},
                  .recording = recording,
                  .view = view,
                  .size = size,
                  .target = target,
                  .info = FrameInfo{.serial = serial,
                                    .predicted_present = plan.predicted_present,
                                    .extent = size,
                                    .replay = false,
                                    .damage = std::nullopt},
                  .plan = plan,
                  .hooked = false,
                  .drawn = 0,
                  .cleared = false,
                  .layered = false,
                  .prelude = fresh,
                  .screen = screen};
    for (const gpu::FramePlanner::Step& step : steps) {
      std::visit([this, &run](const auto& each) { execute(each, run); }, step);
    }
    return true;
  }

  static auto image_format(TextureFormat format) noexcept -> DkImageFormat {
    switch (format) {
      case TextureFormat::alpha8:
        return DkImageFormat_R8_Unorm;
      case TextureFormat::bgra8:
        return DkImageFormat_BGRA8_Unorm;
      case TextureFormat::rgb565:
        return DkImageFormat_BGR565_Unorm;
      case TextureFormat::r8:
      case TextureFormat::nv12:
        return DkImageFormat_R8_Unorm;
      case TextureFormat::rg8:
        return DkImageFormat_RG8_Unorm;
      case TextureFormat::rgba8:
        return DkImageFormat_RGBA8_Unorm;
    }
    return DkImageFormat_RGBA8_Unorm;
  }

  static auto sampler_of(SamplerFilter filter) noexcept -> std::uint32_t {
    return filter == SamplerFilter::nearest ? 1U : 0U;
  }

  auto install(const dk::Image& image, TextureFormat format)
      -> std::optional<std::uint32_t> {
    dk::ImageView view{image};
    view.setFormat(image_format(format));
    if (format == TextureFormat::alpha8) {
      view.setSwizzle(DkImageSwizzle_One, DkImageSwizzle_One,
                      DkImageSwizzle_One, DkImageSwizzle_Red);
    }
    dk::ImageDescriptor descriptor;
    descriptor.initialize(view);
    std::uint32_t slot = 0;
    {
      const std::scoped_lock lock{mutex_};
      if (free_slots_.empty()) {
        return std::nullopt;
      }
      slot = free_slots_.back();
      free_slots_.pop_back();
    }
    std::ranges::copy(
        std::as_bytes(std::span{&descriptor, 1}),
        descriptors_.images.cpu_bytes()
            .subspan(static_cast<std::size_t>(slot) * sizeof(DkImageDescriptor))
            .begin());
    descriptors_dirty_.store(true, std::memory_order_release);
    return slot;
  }

  void retire_texture(std::uint32_t slot) noexcept {
    const std::scoped_lock lock{mutex_};
    const auto found = textures_.find(slot);
    if (found != textures_.end()) {
      pending_.allocations.push_back(std::move(found->second.memory));
      textures_.erase(found);
    }
    pending_.slots.push_back(slot);
  }

  auto take_pending() -> Retired {
    const std::scoped_lock lock{mutex_};
    return std::exchange(pending_, Retired{});
  }

  void release(Retired retired) {
    const std::scoped_lock lock{mutex_};
    free_slots_.insert(free_slots_.end(), retired.slots.begin(),
                       retired.slots.end());
  }

  void settle(const FrameSlot& slot) noexcept {
    std::uint64_t known = completed_.load(std::memory_order_relaxed);
    while (slot.serial() > known &&
           !completed_.compare_exchange_weak(known, slot.serial(),
                                             std::memory_order_release,
                                             std::memory_order_relaxed)) {
    }
  }

  void advance(const FramePacket& packet, const FrameStopwatch& stopwatch,
               std::chrono::nanoseconds& waited) {
    prepared_ = (packet.slot() + 1) % slots_.size();
    FrameSlot& next = *slots_[prepared_];
    const std::chrono::nanoseconds fenced = stopwatch.now();
    release(next.begin());
    waited += stopwatch.since(fenced);
    settle(next);
    publisher_->completed(
        GpuSerial{completed_.load(std::memory_order_acquire)});
    if (auto measured = next.measured(); measured.has_value()) {
      measured->passes = passes_;
      const std::scoped_lock lock{timings_mutex_};
      timings_ = *measured;
    }
    ring_.release(packet.frame());
  }

  void count_vsync(std::uint64_t began) noexcept {
    ++presented_;
    const std::uint64_t now = armGetSystemTick();
    const std::optional<std::uint64_t> previous =
        std::exchange(last_present_tick_, now);
    const std::uint32_t interval = interval_;
    if (!previous.has_value() || interval == 0) {
      return;
    }
    const auto waited = static_cast<double>(armTicksToNs(began - *previous));
    if (waited > (static_cast<double>(interval) + 0.5) * vsync_period_ns) {
      return;
    }
    const auto elapsed = static_cast<double>(armTicksToNs(now - *previous));
    const std::int64_t periods = std::llround(elapsed / vsync_period_ns);
    if (periods > interval && periods <= pause_periods) {
      missed_ += static_cast<std::uint64_t>(periods - interval);
    }
  }

  std::reference_wrapper<const HorizonSurface> surface_;
  GpuOptions options_;
  DeviceHandle device_;
  QueueHandle queue_;
  std::unique_ptr<Deko3dMemory> memory_;
  Shaders shaders_;
  Descriptors descriptors_;
  std::vector<std::unique_ptr<FrameSlot>> slots_;
  std::unique_ptr<FrameSlot> replay_;
  FrameSlots ring_;
  std::uint32_t interval_{1};
  std::atomic<std::uint64_t> interval_changes_{0};
  std::atomic<std::uint64_t> submitted_{0};
  std::optional<std::uint32_t> last_ui_slot_;
  std::optional<std::uint32_t> replay_reads_;
  RenderQueue render_queue_;
  GpuTakeover::Seat takeover_;
  LentSwapchain<> lent_;
  std::atomic<std::uint64_t> lent_presents_{0};
  std::unique_ptr<Framebuffers> framebuffers_;
  std::uint64_t generation_{0};
  gpu::FramePlanner planner_;
  gpu::FramePlanner overlay_planner_;
  std::shared_ptr<OverlayState> overlay_;
  std::optional<std::reference_wrapper<FrameSlot>> staging_overlay_;
  std::vector<gpu::FramePlanner::RecordedPass> recorded_passes_;
  std::vector<std::optional<Layer>> pass_layers_;
  std::vector<Layer> owed_layers_;
  std::array<gpu::FramePlanner::Batch, 1> screen_batches_{};
  std::vector<ShapeVertex> mirror_;
  std::vector<Upload> uploads_;
  std::vector<Deferred> deferred_;
  std::mutex submit_mutex_;
  mutable std::mutex timings_mutex_;
  std::uint32_t building_{0};
  std::size_t prepared_{0};
  Allocation readback_;
  std::mutex mutex_;
  std::flat_map<std::uint32_t, TextureStorage> textures_;
  std::vector<std::uint32_t> free_slots_;
  Retired pending_;
  std::shared_ptr<Registry> registry_;
  Hooks hooks_;
  std::shared_ptr<Publisher> publisher_;
  Surfaces surfaces_;
  std::atomic<std::uint64_t> completed_{0};
  std::atomic<bool> descriptors_dirty_{false};
  std::optional<std::size_t> last_presented_;
  std::optional<std::uint64_t> last_present_tick_;
  std::atomic<std::uint64_t> presented_{0};
  std::atomic<std::uint64_t> missed_{0};
  std::atomic<std::uint64_t> textures_created_{0};
  std::uint32_t passes_{0};
  GpuTimings timings_;
};

class Deko3dGpu::OverlayState {
 public:
  static constexpr std::uint32_t slot_count = 3;

  explicit OverlayState(
      std::array<std::unique_ptr<Device::FrameSlot>, slot_count> slots) noexcept
      : slots_(std::move(slots)) {}

  OverlayState(const OverlayState&) = delete;
  auto operator=(const OverlayState&) -> OverlayState& = delete;
  OverlayState(OverlayState&&) = delete;
  auto operator=(OverlayState&&) -> OverlayState& = delete;
  ~OverlayState() = default;

  [[nodiscard]] static auto begin(const std::shared_ptr<Device>& device,
                                  const std::shared_ptr<OverlayState>& self)
      -> std::expected<GpuFrame, ProviderError>;

  auto ring() noexcept -> OverlaySlots& { return ring_; }

  auto ring() const noexcept -> const OverlaySlots& { return ring_; }

  auto slot(std::uint32_t index) noexcept -> Device::FrameSlot& {
    return *slots_[index];
  }

  auto frame(std::uint32_t index) noexcept -> std::shared_ptr<Frame>& {
    return frames_[index];
  }

 private:
  OverlaySlots ring_{slot_count, OverlaySlots::Leftovers::carried};
  std::array<std::unique_ptr<Device::FrameSlot>, slot_count> slots_;
  std::array<std::shared_ptr<Frame>, slot_count> frames_{};
};

class Deko3dGpu::Frame final : public GpuFrame::Implementation {
 public:
  explicit Frame(std::weak_ptr<Device> device) noexcept
      : device_(std::move(device)) {}

  Frame(const Frame&) = delete;
  auto operator=(const Frame&) -> Frame& = delete;
  Frame(Frame&&) = delete;
  auto operator=(Frame&&) -> Frame& = delete;

  ~Frame() override { finish(); }

  void open(Device::Opened opened,
            std::shared_ptr<OverlayState> overlay = nullptr) noexcept {
    opened_ = opened;
    if (overlay != nullptr) {
      overlay_ = std::move(overlay);
    }
    clear_.reset();
    damage_.reset();
    plan_ = FramePlan{};
    commands_.clear();
    offscreen_count_ = 0;
    holes_.clear();
    due_.clear();
    uploads_.clear();
    fences_.clear();
    resident_ = false;
    sealed_ = false;
    open_ = true;
  }

  void finish() noexcept override {
    if (!open_ || sealed_) {
      return;
    }
    open_ = false;
    const std::shared_ptr<Device> device = device_.lock();
    if (opened_.overlay) {
      if (device == nullptr) {
        if (overlay_ != nullptr) {
          overlay_->ring().abandon(opened_.claim.slot);
          overlay_.reset();
        }
        return;
      }
      commands_.clear();
      offscreen_count_ = 0;
      device->drain_fences(fences_);
      serial_ = device->take_overlay_uploads(uploads_);
      sealed_ = true;
      static_cast<void>(publish(*device));
      return;
    }
    if (device != nullptr) {
      device->close_frame(opened_);
    }
  }

  auto recording() -> Device::Recording {
    return Device::Recording{
        .clear = std::nullopt,
        .commands = commands_,
        .offscreen = std::span{offscreen_}.first(offscreen_count_),
        .holes = holes_,
        .uploads = uploads_,
        .due = due_,
        .fences = fences_,
        .damage = std::nullopt,
        .resident = false,
        .ticket = opened_.ticket,
        .replay = false};
  }

  auto planned() const noexcept -> const FramePlan& { return plan_; }

  auto serial() const noexcept -> GpuSerial { return serial_; }

  [[nodiscard]] auto allocate_geometry(std::uint32_t vertex_count,
                                       std::uint32_t index_count)
      -> std::expected<FrameGeometry, ProviderError> override {
    const std::shared_ptr<Device> device = device_.lock();
    if (device == nullptr || sealed_) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::platform_failure});
    }
    if (opened_.overlay) {
      return overlay_->slot(opened_.claim.slot)
          .allocate_geometry(vertex_count, index_count);
    }
    return device->allocate_geometry(vertex_count, index_count);
  }

  [[nodiscard]] auto allocate_shapes(std::uint32_t vertex_count,
                                     std::uint32_t index_count)
      -> std::expected<ShapeGeometry, ProviderError> override {
    const std::shared_ptr<Device> device = device_.lock();
    if (device == nullptr || sealed_) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::platform_failure});
    }
    if (opened_.overlay) {
      return overlay_->slot(opened_.claim.slot)
          .allocate_shapes(vertex_count, index_count);
    }
    return device->allocate_shapes(vertex_count, index_count);
  }

  [[nodiscard]] auto allocate_resident(std::uint32_t vertex_count,
                                       std::uint32_t index_count)
      -> std::expected<ResidentShapes, ProviderError> override {
    const std::shared_ptr<Device> device = device_.lock();
    if (device == nullptr || sealed_) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::platform_failure});
    }
    if (opened_.overlay) {
      return allocate_shapes(vertex_count, index_count)
          .transform([](const ShapeGeometry& geometry) {
            return ResidentShapes{.geometry = geometry, .generation = 0};
          });
    }
    auto allocated = device->allocate_resident(vertex_count, index_count);
    if (allocated.has_value()) {
      resident_ = true;
    }
    return allocated;
  }

  [[nodiscard]] auto replay(const ReplayDrawData& data)
      -> std::expected<void, ProviderError> override {
    const std::shared_ptr<Device> device = device_.lock();
    if (device == nullptr || sealed_ || !opened_.replay ||
        data.generation != 1) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument});
    }
    if (const auto refused = device->registry()->refusal(data.commands)) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument, *refused});
    }
    if (!device->replay_indices(data.indices)) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::resource_exhausted});
    }
    resident_ = true;
    for (const DrawCommand& command : data.commands) {
      commands_.push_back(
          Device::BlockCommand{.pipeline = Device::PipelineKind::shape,
                               .block = ResidentShapes::block,
                               .command = command});
    }
    return {};
  }

  void touch(VertexRange range) override {
    if (opened_.overlay) {
      return;
    }
    if (const std::shared_ptr<Device> device = device_.lock()) {
      device->touch(range);
    }
  }

  void damage(ClipRect region) override {
    if (!damage_.has_value() || damage_->width == 0 || damage_->height == 0) {
      damage_ = region;
      return;
    }
    if (region.width == 0 || region.height == 0) {
      return;
    }
    const std::uint32_t left = std::min(damage_->x, region.x);
    const std::uint32_t top = std::min(damage_->y, region.y);
    const std::uint32_t right =
        std::max(damage_->x + damage_->width, region.x + region.width);
    const std::uint32_t bottom =
        std::max(damage_->y + damage_->height, region.y + region.height);
    damage_ = ClipRect{
        .x = left, .y = top, .width = right - left, .height = bottom - top};
  }

  void clear(Colour colour) override { clear_ = colour; }

  void plan(FramePlan plan) override { plan_ = plan; }

  [[nodiscard]] auto draw(const DrawData& data)
      -> std::expected<void, ProviderError> override {
    return record(data, Device::PipelineKind::textured, commands_);
  }

  [[nodiscard]] auto draw(const ShapeDrawData& data)
      -> std::expected<void, ProviderError> override {
    return record(data, Device::PipelineKind::shape, commands_);
  }

  [[nodiscard]] auto draw(const ShapeDrawData& data, const RenderPass& pass)
      -> std::expected<void, ProviderError> override {
    const std::shared_ptr<Device> device = device_.lock();
    if (device == nullptr || sealed_) {
      return std::unexpected(ProviderError{ProviderErrorCode::platform_failure,
                                           ProviderErrorDetail::frame_closed});
    }
    const auto target = device->registry()->find(pass.target);
    if (!target.has_value()) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument,
                        ProviderErrorDetail::unknown_target});
    }
    if (target->description.usage != TextureUsage::render_target) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument,
                        ProviderErrorDetail::not_render_target});
    }
    if (offscreen_count_ == offscreen_.size()) {
      offscreen_.emplace_back();
    }
    Device::OffscreenPass& added = offscreen_[offscreen_count_];
    added.pass = pass;
    added.commands.clear();
    added.holes.clear();
    auto recorded = record(data, Device::PipelineKind::shape, added.commands);
    if (recorded.has_value()) {
      ++offscreen_count_;
    }
    return recorded;
  }

  [[nodiscard]] auto present() -> std::expected<void, ProviderError> override {
    return std::unexpected(ProviderError::unsupported_capability());
  }

  [[nodiscard]] auto seal()
      -> std::expected<GpuFrame::Sealed, ProviderError> override {
    const std::shared_ptr<Device> device = device_.lock();
    if (device == nullptr || !open_ || sealed_) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::platform_failure});
    }
    device->drain_fences(fences_);
    const GpuSerial serial = opened_.overlay
                                 ? device->take_overlay_uploads(uploads_)
                             : opened_.replay ? opened_.serial
                                              : device->take_uploads(uploads_);
    serial_ = serial;
    device->mark(serial, commands_);
    for (const Device::OffscreenPass& each :
         std::span{offscreen_}.first(offscreen_count_)) {
      device->mark(serial, each.commands);
    }
    auto planned = device->plan_surfaces(
        commands_, std::span{offscreen_}.first(offscreen_count_), serial);
    holes_ = std::move(planned.first);
    due_ = std::move(planned.second);
    sealed_ = true;
    return GpuFrame::Sealed{
        .submitter = nullptr,
        .contents = FramePacket::Contents{.plan = plan_,
                                          .serial = serial,
                                          .frame = opened_.claim.frame,
                                          .slot = opened_.claim.slot}};
  }

  [[nodiscard]] auto submit(const FramePacket& packet, FramePacket::Clock clock)
      -> std::expected<FrameReceipt, ProviderError> override {
    open_ = false;
    const std::shared_ptr<Device> device = device_.lock();
    if (device == nullptr) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::platform_failure});
    }
    if (opened_.overlay) {
      return publish(*device);
    }
    return device->submit(
        packet,
        Device::Recording{
            .clear = clear_,
            .commands = commands_,
            .offscreen = std::span{offscreen_}.first(offscreen_count_),
            .holes = holes_,
            .uploads = uploads_,
            .due = due_,
            .fences = fences_,
            .damage = damage_,
            .resident = resident_,
            .ticket = opened_.ticket,
            .replay = opened_.replay},
        clock);
  }

  void abandon(const FramePacket& packet) noexcept override {
    open_ = false;
    if (opened_.overlay) {
      commands_.clear();
      offscreen_count_ = 0;
      if (const std::shared_ptr<Device> device = device_.lock()) {
        static_cast<void>(publish(*device));
      }
      return;
    }
    if (const std::shared_ptr<Device> device = device_.lock()) {
      device->abandon(packet, opened_, uploads_);
    }
  }

 private:
  auto publish(Device& device) -> std::expected<FrameReceipt, ProviderError> {
    device.registry()->close(opened_.ticket);
    const bool empty =
        commands_.empty() && offscreen_count_ == 0 && uploads_.empty();
    const std::shared_ptr<OverlayState> overlay = std::exchange(overlay_, {});
    if (overlay == nullptr ||
        !overlay->ring().publish(opened_.claim.slot, empty, plan_.extent)) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument,
                        ProviderErrorDetail::not_taken_over});
    }
    return FrameReceipt{};
  }

  template <class V>
  auto record(const BasicDrawData<V>& data, Device::PipelineKind kind,
              std::vector<Device::BlockCommand>& into)
      -> std::expected<void, ProviderError> {
    const std::shared_ptr<Device> device = device_.lock();
    if (device == nullptr || sealed_) {
      return std::unexpected(ProviderError{ProviderErrorCode::platform_failure,
                                           ProviderErrorDetail::frame_closed});
    }
    if (opened_.replay) {
      return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument,
                                           ProviderErrorDetail::replay_frame});
    }
    if (const auto refused = device->registry()->refusal(data.commands)) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument, *refused});
    }
    const bool owned =
        opened_.overlay ? overlay_->slot(opened_.claim.slot).owns(data.geometry)
                        : device->owns(data.geometry);
    if (!data.commands.empty() && !owned) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument,
                        ProviderErrorDetail::foreign_geometry});
    }
    for (DrawCommand command : data.commands) {
      command.vertex_offset += data.geometry.first_vertex;
      command.index_offset += data.geometry.first_index;
      into.push_back(Device::BlockCommand{
          .pipeline = kind, .block = data.geometry.block, .command = command});
    }
    return {};
  }

  std::weak_ptr<Device> device_;
  Device::Opened opened_{};
  std::optional<Colour> clear_;
  std::optional<ClipRect> damage_;
  FramePlan plan_{};
  std::vector<Device::BlockCommand> commands_;
  std::vector<Device::OffscreenPass> offscreen_;
  std::size_t offscreen_count_{0};
  std::vector<Surfaces::Hole> holes_;
  std::vector<Surfaces::Due> due_;
  std::vector<Device::Upload> uploads_;
  std::vector<dk::Fence> fences_;
  std::shared_ptr<OverlayState> overlay_;
  GpuSerial serial_{0};
  bool resident_{false};
  bool sealed_{false};
  bool open_{false};
};

auto Deko3dGpu::OverlayState::begin(const std::shared_ptr<Device>& device,
                                    const std::shared_ptr<OverlayState>& self)
    -> std::expected<GpuFrame, ProviderError> {
  if (self->ring_.closed()) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument,
                                         ProviderErrorDetail::not_taken_over});
  }
  const std::optional<std::uint32_t> claimed = self->ring_.claim(
      [&self](std::uint32_t index, OverlaySlots::Standing standing) {
        return standing.last_use == 0 ||
               (standing.presented &&
                self->slot(index).fence().wait(0) == DkResult_Success);
      });
  if (!claimed.has_value()) {
    return std::unexpected(
        ProviderError{ProviderErrorCode::resource_exhausted});
  }
  const Device::Opened opened =
      device->open_overlay_frame(self->slot(*claimed), *claimed);
  std::shared_ptr<Frame>& frame = self->frames_[*claimed];
  if (frame == nullptr || frame.use_count() > 1) {
    frame = std::make_shared<Frame>(device);
  }
  frame->open(opened, self);
  return GpuFrame::reused(frame);
}

auto Deko3dGpu::Device::open_overlay(const std::shared_ptr<Device>& self)
    -> std::expected<std::shared_ptr<OverlayState>, ProviderError> {
  const std::scoped_lock submitting{self->submit_mutex_};
  if (self->overlay_ != nullptr && !self->overlay_->ring().closed()) {
    return std::unexpected(ProviderError{ProviderErrorCode::resource_exhausted,
                                         ProviderErrorDetail::overlay_open});
  }
  std::array<std::unique_ptr<FrameSlot>, OverlayState::slot_count> slots;
  for (std::unique_ptr<FrameSlot>& each : slots) {
    auto created = FrameSlot::create(self->device_.get(), *self->memory_);
    if (!created.has_value()) {
      return std::unexpected(created.error());
    }
    each = std::move(*created);
  }
  self->overlay_ = std::make_shared<OverlayState>(std::move(slots));
  self->overlay_planner_.invalidate();
  return self->overlay_;
}

void Deko3dGpu::Device::retire(OverlayState& state) noexcept {
  state.ring().close();
}

void Deko3dGpu::Device::close_overlay(
    const std::shared_ptr<OverlayState>& state) noexcept {
  const std::scoped_lock submitting{submit_mutex_};
  retire(*state);
  if (overlay_ == state) {
    queue().waitIdle();
    overlay_.reset();
  }
}

auto Deko3dGpu::Device::record_overlay(OverlayState& state, dk::CmdBuf& buffer,
                                       const FrameTarget& target)
    -> std::expected<bool, ProviderError> {
  const std::scoped_lock submitting{submit_mutex_};
  if (overlay_.get() != &state || state.ring().closed()) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument,
                                         ProviderErrorDetail::not_taken_over});
  }
  const auto used =
      state.ring().use(SurfaceSize{.width = SurfaceWidth{target.width},
                                   .height = SurfaceHeight{target.height}});
  if (!used.has_value()) {
    return std::unexpected(used.error());
  }
  if (!used->has_value()) {
    return false;
  }
  const OverlaySlots::Use& use = **used;
  Frame& frame = *state.frame(use.slot);
  const FramePlan& plan = frame.planned();
  bool wrote = false;
  if (use.fresh) {
    for (const std::uint32_t carried :
         std::span{use.carried}.first(use.carrying)) {
      Frame& left = *state.frame(carried);
      wrote = embed(left.recording(), state.slot(carried), buffer, target,
                    left.planned(), left.serial(), true, false) ||
              wrote;
    }
  }
  return embed(frame.recording(), state.slot(use.slot), buffer, target, plan,
               frame.serial(), use.fresh, true) ||
         wrote;
}

void Deko3dGpu::Device::overlay_presented() {
  const std::scoped_lock submitting{submit_mutex_};
  if (overlay_ == nullptr) {
    return;
  }
  OverlayState& state = *overlay_;
  static_cast<void>(state.ring().present([this, &state](std::uint32_t index) {
    queue().signalFence(state.slot(index).fence(), false);
  }));
}

Deko3dGpu::Overlay::Overlay(std::shared_ptr<Device> device,
                            std::shared_ptr<OverlayState> state) noexcept
    : device_(std::move(device)), state_(std::move(state)) {}

Deko3dGpu::Overlay::Overlay(Overlay&& other) noexcept = default;

auto Deko3dGpu::Overlay::operator=(Overlay&& other) noexcept -> Overlay& {
  if (this != &other) {
    release();
    device_ = std::move(other.device_);
    state_ = std::move(other.state_);
  }
  return *this;
}

Deko3dGpu::Overlay::~Overlay() { release(); }

auto Deko3dGpu::Overlay::held() const noexcept -> bool {
  return state_ != nullptr && !state_->ring().closed();
}

auto Deko3dGpu::Overlay::frames() const -> FrameSource {
  std::weak_ptr<const OverlaySlots> ring;
  if (state_ != nullptr) {
    ring = std::shared_ptr<const OverlaySlots>{state_, &state_->ring()};
  }
  return FrameSource{
      [device = std::weak_ptr<Device>{device_},
       state = std::weak_ptr<OverlayState>{state_}]()
          -> std::expected<GpuFrame, ProviderError> {
        const std::shared_ptr<Device> held_device = device.lock();
        const std::shared_ptr<OverlayState> held_state = state.lock();
        if (held_device == nullptr || held_state == nullptr) {
          return std::unexpected(
              ProviderError{ProviderErrorCode::invalid_argument,
                            ProviderErrorDetail::not_taken_over});
        }
        return OverlayState::begin(held_device, held_state);
      },
      std::move(ring)};
}

auto Deko3dGpu::Overlay::record(dk::CmdBuf& buffer, const FrameTarget& target)
    -> std::expected<bool, ProviderError> {
  if (state_ == nullptr) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument,
                                         ProviderErrorDetail::not_taken_over});
  }
  return device_->record_overlay(*state_, buffer, target);
}

void Deko3dGpu::Overlay::release() noexcept {
  if (state_ == nullptr) {
    return;
  }
  device_->close_overlay(state_);
  state_.reset();
  device_.reset();
}

Deko3dGpu::Deko3dGpu(std::shared_ptr<Device> device) noexcept
    : device_(std::move(device)) {}

auto Deko3dGpu::create(const HorizonSurface& surface, GpuOptions options)
    -> std::expected<Deko3dGpu, ProviderError> {
  return Device::create(surface, options)
      .transform([](std::shared_ptr<Device> device) {
        return Deko3dGpu{std::move(device)};
      });
}

auto Deko3dGpu::create(const HorizonSurface& surface, GpuOptions options,
                       DiagnosticsProvider& diagnostics)
    -> std::expected<Deko3dGpu, ProviderError> {
  auto created = create(surface, options);
  if (created.has_value()) {
    diagnostics.report(Diagnostic{.severity = Severity::info,
                                  .category = "arisa.deko3d",
                                  .message = created->startup_report()});
  }
  return created;
}

auto Deko3dGpu::report() const -> std::string {
  return std::format("deko3d: {} frames, {} missed vsyncs", frames_presented(),
                     missed_vsyncs());
}

auto Deko3dGpu::create_texture(TextureDescription description)
    -> std::expected<Texture, ProviderError> {
  return device_->create_texture(description);
}

auto Deko3dGpu::update_texture(Texture& texture, TextureRegion region,
                               PixelRows pixels)
    -> std::expected<void, ProviderError> {
  return device_->update_texture(texture, region, pixels);
}

auto Deko3dGpu::import_texture(const dk::Image& image,
                               ImportDescription description,
                               support::ScopedCallback released)
    -> std::expected<Imported, ProviderError> {
  return device_->import_texture(image, description, std::move(released));
}

auto Deko3dGpu::import_texture(std::span<const dk::Image* const> planes,
                               ImportDescription description,
                               support::ScopedCallback released)
    -> std::expected<Imported, ProviderError> {
  return device_->import_texture(planes, description, std::move(released));
}

auto Deko3dGpu::begin_frame() -> std::expected<GpuFrame, ProviderError> {
  auto opened = device_->open_frame();
  if (!opened.has_value()) {
    return std::unexpected(opened.error());
  }
  std::shared_ptr<Frame>& frame = frames_.at(opened->claim.slot);
  if (frame == nullptr || frame.use_count() > 1) {
    frame = std::make_shared<Frame>(device_);
  }
  frame->open(*opened);
  return GpuFrame::reused(frame);
}

auto Deko3dGpu::begin_replay() -> std::expected<GpuFrame, ProviderError> {
  auto opened = device_->open_replay();
  if (!opened.has_value()) {
    return std::unexpected(opened.error());
  }
  if (replay_ == nullptr || replay_.use_count() > 1) {
    replay_ = std::make_shared<Frame>(device_);
  }
  replay_->open(*opened);
  return GpuFrame::reused(replay_);
}

auto Deko3dGpu::render_queue() noexcept
    -> std::optional<std::reference_wrapper<RenderQueue>> {
  return std::ref(device_->render_queue());
}

auto Deko3dGpu::take_over() -> std::expected<Takeover, ProviderError> {
  return Device::take_over(device_).transform([this](GpuTakeover taken) {
    return Takeover{device_, std::move(taken)};
  });
}

auto Deko3dGpu::takeover_counters() const -> GpuTakeover::Counters {
  return device_->takeover_counters();
}

Deko3dGpu::Takeover::Takeover(std::shared_ptr<Device> device,
                              GpuTakeover taken) noexcept
    : device_(std::move(device)), taken_(std::move(taken)) {}

Deko3dGpu::Takeover::Takeover(Takeover&& other) noexcept = default;

auto Deko3dGpu::Takeover::operator=(Takeover&& other) noexcept
    -> Takeover& = default;

Deko3dGpu::Takeover::~Takeover() = default;

auto Deko3dGpu::Takeover::device() const noexcept -> std::optional<dk::Device> {
  if (!taken_.held()) {
    return std::nullopt;
  }
  return device_->handle();
}

auto Deko3dGpu::Takeover::queue() const noexcept -> std::optional<dk::Queue> {
  if (!taken_.held()) {
    return std::nullopt;
  }
  return device_->queue();
}

auto Deko3dGpu::Takeover::memory() noexcept
    -> std::optional<std::reference_wrapper<Deko3dMemory>> {
  if (!taken_.held()) {
    return std::nullopt;
  }
  return std::ref(device_->memory());
}

auto Deko3dGpu::Takeover::image_count() const noexcept -> std::uint32_t {
  if (!taken_.held()) {
    return 0;
  }
  return device_->lent_images();
}

auto Deko3dGpu::Takeover::target(std::uint32_t index)
    -> std::optional<FrameTarget> {
  if (!taken_.held()) {
    return std::nullopt;
  }
  return device_->lent_target(index);
}

auto Deko3dGpu::Takeover::acquire()
    -> std::expected<std::uint32_t, ProviderError> {
  if (!taken_.held()) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument,
                                         ProviderErrorDetail::not_taken_over});
  }
  return device_->lent_acquire();
}

auto Deko3dGpu::Takeover::present(std::uint32_t index)
    -> std::expected<void, ProviderError> {
  if (!taken_.held()) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument,
                                         ProviderErrorDetail::not_taken_over});
  }
  return device_->lent_present(index);
}

void Deko3dGpu::Takeover::set_swap_interval(SwapInterval interval) {
  if (taken_.held()) {
    device_->lent_interval(interval);
  }
}

auto Deko3dGpu::Takeover::overlay() -> std::expected<Overlay, ProviderError> {
  if (!taken_.held()) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument,
                                         ProviderErrorDetail::not_taken_over});
  }
  return Device::open_overlay(device_).transform(
      [this](std::shared_ptr<OverlayState> state) {
        return Overlay{device_, std::move(state)};
      });
}

auto Deko3dGpu::Takeover::generation() const noexcept -> std::uint64_t {
  if (!taken_.held() || device_ == nullptr) {
    return 0;
  }
  return device_->lent_generation();
}

auto Deko3dGpu::Takeover::presents() const noexcept -> std::uint64_t {
  return device_->lent_presents();
}

auto Deko3dGpu::swap_interval_changes() const noexcept -> std::uint64_t {
  return device_->swap_interval_changes();
}

auto Deko3dGpu::capture() -> std::expected<Image, ProviderError> {
  return device_->capture();
}

auto Deko3dGpu::frame_schedule() noexcept
    -> std::optional<std::reference_wrapper<FrameSchedule>> {
  return std::ref(device_->hooks().schedule());
}

auto Deko3dGpu::publications() const noexcept
    -> std::optional<std::reference_wrapper<const TexturePublications>> {
  return std::cref(
      static_cast<const TexturePublications&>(*device_->publisher()));
}

auto Deko3dGpu::add_frame_hook(FrameStage stage, FrameHookFunction function,
                               FrameHookOptions options) -> FrameHook {
  return device_->hooks().add(stage, std::move(function), options);
}

auto Deko3dGpu::add_surface(SurfaceOptions options, SurfaceFunction function)
    -> std::expected<GpuSurface, ProviderError> {
  return device_->surfaces().add(options, std::move(function));
}

auto Deko3dGpu::surfaces() noexcept
    -> std::optional<std::reference_wrapper<SurfaceDirectory>> {
  return std::ref(device_->surfaces().directory());
}

auto Deko3dGpu::completed(GpuSerial serial) const noexcept -> bool {
  return device_->completed(serial);
}

void Deko3dGpu::set_swap_interval(SwapInterval interval) {
  device_->set_swap_interval(interval);
}

auto Deko3dGpu::options() const noexcept -> GpuOptions {
  return device_->options();
}

auto Deko3dGpu::device() const noexcept -> dk::Device {
  return device_->handle();
}

auto Deko3dGpu::queue() const noexcept -> dk::Queue { return device_->queue(); }

auto Deko3dGpu::memory() noexcept -> Deko3dMemory& { return device_->memory(); }

auto Deko3dGpu::frames_presented() const noexcept -> std::uint64_t {
  return device_->frames_presented();
}

auto Deko3dGpu::missed_vsyncs() const noexcept -> std::uint64_t {
  return device_->missed_vsyncs();
}

auto Deko3dGpu::timings() const -> GpuTimings { return device_->timings(); }

auto Deko3dGpu::resources() const -> GpuResources {
  return device_->resources();
}

auto Deko3dGpu::startup_report() const -> std::string {
  const GpuOptions chosen = device_->options();
  return std::format(
      "deko3d gpu: {} swapchain images, swap interval {}, {} texture slots",
      chosen.image_count(), chosen.swap_interval.value(), max_textures);
}

auto Deko3dGpu::inventory() const -> GpuInventory {
  return GpuInventory{.backend = "deko3d",
                      .report = startup_report(),
                      .textures = device_->registry()->memory(),
                      .frames_presented = device_->frames_presented(),
                      .missed_vsyncs = device_->missed_vsyncs()};
}

auto Deko3dGpu::live_textures() const -> std::size_t {
  return device_->registry()->size();
}

}
