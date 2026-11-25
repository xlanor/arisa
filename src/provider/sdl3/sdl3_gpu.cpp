#include <arisa/provider/gpu/frame_stopwatch.hpp>
#include <arisa/provider/gpu/texture_registry.hpp>
#include <arisa/provider/sdl3/sdl3_context.hpp>
#include <arisa/provider/sdl3/sdl3_gpu.hpp>
#include <arisa/support/unique_handle.hpp>
#include <provider/sdl3/sdl3_shaders.hpp>

#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_timer.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <expected>
#include <format>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <variant>
#include <vector>

namespace arisa::provider::sdl3 {

template <class Resource, auto Release>
struct Sdl3Gpu::DeviceResource {
  struct Handle {
    SDL_GPUDevice* device;
    Resource* resource;
  };

  using handle_type = Handle;

  static auto empty() noexcept -> handle_type {
    return Handle{.device = nullptr, .resource = nullptr};
  }

  static auto is_valid(const handle_type& handle) noexcept -> bool {
    return handle.resource != nullptr;
  }

  static void release(handle_type& handle) noexcept {
    Release(handle.device, handle.resource);
  }
};

class Sdl3Gpu::Device {
 public:
  static constexpr Colour transparent{
      .red = 0.0F, .green = 0.0F, .blue = 0.0F, .alpha = 0.0F};

  struct Native {
    SDL_GPUTexture* texture{nullptr};
    SDL_GPUTextureFormat format{SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM};
    bool owned{true};
  };

  using Registry = TextureRegistry<Native>;
  using Publisher = Imported::Publisher;
  using GpuTexture = support::UniqueHandle<
      DeviceResource<SDL_GPUTexture, &SDL_ReleaseGPUTexture>>;
  using GpuBuffer = support::UniqueHandle<
      DeviceResource<SDL_GPUBuffer, &SDL_ReleaseGPUBuffer>>;
  using GpuTransferBuffer = support::UniqueHandle<
      DeviceResource<SDL_GPUTransferBuffer, &SDL_ReleaseGPUTransferBuffer>>;
  using GpuShader = support::UniqueHandle<
      DeviceResource<SDL_GPUShader, &SDL_ReleaseGPUShader>>;
  using GpuPipeline = support::UniqueHandle<
      DeviceResource<SDL_GPUGraphicsPipeline, &SDL_ReleaseGPUGraphicsPipeline>>;
  using GpuSampler = support::UniqueHandle<
      DeviceResource<SDL_GPUSampler, &SDL_ReleaseGPUSampler>>;
  using GpuFence =
      support::UniqueHandle<DeviceResource<SDL_GPUFence, &SDL_ReleaseGPUFence>>;
  using WindowClaim = support::UniqueHandle<
      DeviceResource<SDL_Window, &SDL_ReleaseWindowFromGPUDevice>>;

  struct DeviceRelease {
    using handle_type = SDL_GPUDevice*;

    static auto empty() noexcept -> handle_type { return nullptr; }

    static auto is_valid(handle_type device) noexcept -> bool {
      return device != nullptr;
    }

    static void release(handle_type& device) noexcept {
      SDL_DestroyGPUDevice(device);
    }
  };

  using DeviceHandle = support::UniqueHandle<DeviceRelease>;

  struct InFlight {
    std::uint64_t serial{0};
    GpuFence fence;
  };

  struct Pipeline {
    GpuShader vertex;
    GpuShader fragment;
    GpuPipeline pipeline;
    GpuSampler sampler;
  };

  class Transfer {
   public:
    auto ensure(SDL_GPUDevice* device, SDL_GPUTransferBufferUsage usage,
                std::uint32_t size) -> bool {
      if (buffer_.valid() && capacity_ >= size) {
        return true;
      }
      const std::uint32_t grown = std::max(size, capacity_ * 2);
      SDL_GPUTransferBufferCreateInfo info{};
      info.usage = usage;
      info.size = grown;
      GpuTransferBuffer created{
          {.device = device,
           .resource = SDL_CreateGPUTransferBuffer(device, &info)}};
      if (!created.valid()) {
        return false;
      }
      buffer_ = std::move(created);
      capacity_ = grown;
      return true;
    }

    auto get() const noexcept -> SDL_GPUTransferBuffer* {
      return buffer_.get().resource;
    }

   private:
    GpuTransferBuffer buffer_;
    std::uint32_t capacity_{0};
  };

  template <class V>
  class GeometryBlock {
   public:
    using Geometry = provider::Geometry<V>;

    static constexpr std::uint32_t minimum_vertices = 4096;
    static constexpr std::uint32_t minimum_indices = 8192;

    [[nodiscard]] static auto create(SDL_GPUDevice* device,
                                     std::uint32_t vertex_capacity,
                                     std::uint32_t index_capacity)
        -> std::optional<GeometryBlock> {
      const std::uint64_t vertex_bytes =
          static_cast<std::uint64_t>(vertex_capacity) * sizeof(V);
      const std::uint64_t index_bytes =
          static_cast<std::uint64_t>(index_capacity) * sizeof(Index);
      if (vertex_bytes + index_bytes > std::numeric_limits<Uint32>::max()) {
        return std::nullopt;
      }
      GeometryBlock block{vertex_capacity, index_capacity};
      SDL_GPUTransferBufferCreateInfo transfer{};
      transfer.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
      transfer.size = static_cast<Uint32>(vertex_bytes + index_bytes);
      block.transfer_ = GpuTransferBuffer{
          {.device = device,
           .resource = SDL_CreateGPUTransferBuffer(device, &transfer)}};
      SDL_GPUBufferCreateInfo vertices{};
      vertices.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
      vertices.size = static_cast<Uint32>(vertex_bytes);
      block.vertex_buffer_ =
          GpuBuffer{{.device = device,
                     .resource = SDL_CreateGPUBuffer(device, &vertices)}};
      SDL_GPUBufferCreateInfo indices{};
      indices.usage = SDL_GPU_BUFFERUSAGE_INDEX;
      indices.size = static_cast<Uint32>(index_bytes);
      block.index_buffer_ =
          GpuBuffer{{.device = device,
                     .resource = SDL_CreateGPUBuffer(device, &indices)}};
      if (!block.transfer_.valid() || !block.vertex_buffer_.valid() ||
          !block.index_buffer_.valid()) {
        return std::nullopt;
      }
      return block;
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

    auto take(SDL_GPUDevice* device, std::uint32_t vertices,
              std::uint32_t indices, std::uint32_t index)
        -> std::optional<Geometry> {
      if (mapped_vertices_.empty()) {
        void* const mapped =
            SDL_MapGPUTransferBuffer(device, transfer_.get().resource, true);
        if (mapped == nullptr) {
          return std::nullopt;
        }
        mapped_vertices_ = std::span{static_cast<V*>(mapped), vertex_capacity_};
        mapped_indices_ = std::span{
            static_cast<Index*>(static_cast<void*>(
                static_cast<std::byte*>(mapped) +
                (static_cast<std::size_t>(vertex_capacity_) * sizeof(V)))),
            index_capacity_};
      }
      const Geometry geometry{
          .vertices = mapped_vertices_.subspan(vertices_used_, vertices),
          .indices = mapped_indices_.subspan(indices_used_, indices),
          .block = index,
          .first_vertex = vertices_used_,
          .first_index = indices_used_};
      vertices_used_ += vertices;
      indices_used_ += indices;
      return geometry;
    }

    auto owns(const Geometry& geometry) const noexcept -> bool {
      return !mapped_vertices_.empty() &&
             geometry.first_vertex <= vertices_used_ &&
             geometry.vertices.size() <=
                 vertices_used_ - geometry.first_vertex &&
             geometry.first_index <= indices_used_ &&
             geometry.indices.size() <= indices_used_ - geometry.first_index &&
             geometry.vertices.data() ==
                 mapped_vertices_.subspan(geometry.first_vertex).data() &&
             geometry.indices.data() ==
                 mapped_indices_.subspan(geometry.first_index).data();
    }

    auto used() const noexcept -> bool { return vertices_used_ > 0; }

    void unmap(SDL_GPUDevice* device) noexcept {
      if (!mapped_vertices_.empty()) {
        SDL_UnmapGPUTransferBuffer(device, transfer_.get().resource);
        mapped_vertices_ = {};
        mapped_indices_ = {};
      }
    }

    void reset() noexcept {
      vertices_used_ = 0;
      indices_used_ = 0;
    }

    void upload(SDL_GPUCopyPass* pass) const {
      SDL_GPUTransferBufferLocation vertex_source{};
      vertex_source.transfer_buffer = transfer_.get().resource;
      SDL_GPUBufferRegion vertex_destination{};
      vertex_destination.buffer = vertex_buffer_.get().resource;
      vertex_destination.size = static_cast<Uint32>(vertices_used_ * sizeof(V));
      SDL_UploadToGPUBuffer(pass, &vertex_source, &vertex_destination, true);
      SDL_GPUTransferBufferLocation index_source{};
      index_source.transfer_buffer = transfer_.get().resource;
      index_source.offset = static_cast<Uint32>(vertex_capacity_ * sizeof(V));
      SDL_GPUBufferRegion index_destination{};
      index_destination.buffer = index_buffer_.get().resource;
      index_destination.size =
          static_cast<Uint32>(indices_used_ * sizeof(Index));
      SDL_UploadToGPUBuffer(pass, &index_source, &index_destination, true);
    }

    auto vertex_buffer() const noexcept -> SDL_GPUBuffer* {
      return vertex_buffer_.get().resource;
    }

    auto index_buffer() const noexcept -> SDL_GPUBuffer* {
      return index_buffer_.get().resource;
    }

   private:
    GeometryBlock(std::uint32_t vertex_capacity,
                  std::uint32_t index_capacity) noexcept
        : vertex_capacity_(vertex_capacity), index_capacity_(index_capacity) {}

    GpuTransferBuffer transfer_;
    GpuBuffer vertex_buffer_;
    GpuBuffer index_buffer_;
    std::uint32_t vertex_capacity_;
    std::uint32_t index_capacity_;
    std::uint32_t vertices_used_{0};
    std::uint32_t indices_used_{0};
    std::span<V> mapped_vertices_;
    std::span<Index> mapped_indices_;
  };

  template <class V>
  class GeometryRing {
   public:
    using Block = GeometryBlock<V>;
    using Geometry = provider::Geometry<V>;

    void open(SDL_GPUDevice* device) {
      if (blocks_.size() > 1) {
        std::uint64_t vertices = 0;
        std::uint64_t indices = 0;
        for (const Block& block : blocks_) {
          vertices += block.vertex_capacity();
          indices += block.index_capacity();
        }
        blocks_.clear();
        if (auto merged =
                Block::create(device, clamp(vertices), clamp(indices))) {
          blocks_.push_back(std::move(*merged));
        }
      }
      for (Block& block : blocks_) {
        block.reset();
      }
      cursor_ = 0;
    }

    void unmap(SDL_GPUDevice* device) noexcept {
      for (Block& block : blocks_) {
        block.unmap(device);
      }
    }

    [[nodiscard]] auto allocate(SDL_GPUDevice* device, std::uint32_t vertices,
                                std::uint32_t indices)
        -> std::expected<Geometry, ProviderError> {
      for (; cursor_ < blocks_.size(); ++cursor_) {
        if (blocks_[cursor_].fits(vertices, indices)) {
          return take(device, vertices, indices);
        }
      }
      const std::uint32_t previous_vertices =
          blocks_.empty() ? 0 : blocks_.back().vertex_capacity();
      const std::uint32_t previous_indices =
          blocks_.empty() ? 0 : blocks_.back().index_capacity();
      auto created = Block::create(
          device,
          std::max({vertices, Block::minimum_vertices, previous_vertices * 2}),
          std::max({indices, Block::minimum_indices, previous_indices * 2}));
      if (!created.has_value()) {
        return std::unexpected(
            ProviderError{ProviderErrorCode::resource_exhausted});
      }
      blocks_.push_back(std::move(*created));
      cursor_ = blocks_.size() - 1;
      return take(device, vertices, indices);
    }

    auto owns(const Geometry& geometry) const noexcept -> bool {
      return geometry.block < blocks_.size() &&
             blocks_[geometry.block].owns(geometry);
    }

    auto used() const noexcept -> bool {
      return std::ranges::any_of(
          blocks_, [](const Block& block) { return block.used(); });
    }

    void upload(SDL_GPUCopyPass* pass) const {
      for (const Block& block : blocks_) {
        if (block.used()) {
          block.upload(pass);
        }
      }
    }

    auto block(std::uint32_t index) const noexcept -> const Block* {
      return index < blocks_.size() ? &blocks_[index] : nullptr;
    }

   private:
    static auto clamp(std::uint64_t count) noexcept -> std::uint32_t {
      return static_cast<std::uint32_t>(std::min<std::uint64_t>(
          count, std::numeric_limits<std::uint32_t>::max()));
    }

    auto take(SDL_GPUDevice* device, std::uint32_t vertices,
              std::uint32_t indices) -> std::expected<Geometry, ProviderError> {
      auto taken = blocks_[cursor_].take(device, vertices, indices,
                                         static_cast<std::uint32_t>(cursor_));
      if (!taken.has_value()) {
        return std::unexpected(
            ProviderError{ProviderErrorCode::platform_failure});
      }
      return *taken;
    }

    std::vector<Block> blocks_;
    std::size_t cursor_{0};
  };

  class ResidentStore {
   public:
    static constexpr std::size_t maximum_ranges = 512;
    static constexpr std::uint32_t minimum_vertices = 4096;
    static constexpr std::uint32_t minimum_indices = 8192;

    auto allocate(std::uint32_t vertices, std::uint32_t indices)
        -> ResidentShapes {
      mirror_.resize(vertices);
      indices_.resize(indices);
      used_ = true;
      return ResidentShapes{
          .geometry = ShapeGeometry{.vertices = mirror_,
                                    .indices = indices_,
                                    .block = ResidentShapes::block,
                                    .first_vertex = 0,
                                    .first_index = 0},
          .generation = generation_};
    }

    void touch(VertexRange range) {
      if (whole_) {
        return;
      }
      if (!dirty_.empty() &&
          dirty_.back().first + dirty_.back().count == range.first) {
        dirty_.back().count += range.count;
        return;
      }
      if (dirty_.size() >= maximum_ranges) {
        whole_ = true;
        dirty_.clear();
        return;
      }
      dirty_.push_back(range);
    }

    auto owns(const ShapeGeometry& geometry) const noexcept -> bool {
      return geometry.block == ResidentShapes::block &&
             geometry.vertices.data() == mirror_.data() &&
             geometry.indices.data() == indices_.data();
    }

    auto used() const noexcept -> bool { return used_; }

    void upload(SDL_GPUDevice* device, SDL_GPUCopyPass* pass) {
      if (!used_ || mirror_.empty() || indices_.empty()) {
        return;
      }
      if (!grow(device, vertex_buffer_, vertex_capacity_,
                static_cast<std::uint32_t>(mirror_.size()), minimum_vertices,
                sizeof(ShapeVertex), SDL_GPU_BUFFERUSAGE_VERTEX) ||
          !grow(device, index_buffer_, index_capacity_,
                static_cast<std::uint32_t>(indices_.size()), minimum_indices,
                sizeof(Index), SDL_GPU_BUFFERUSAGE_INDEX)) {
        return;
      }
      const auto size = static_cast<std::uint32_t>(mirror_.size());
      if (whole_) {
        dirty_.assign(1, VertexRange{.first = 0, .count = size});
      }
      std::size_t vertex_bytes = 0;
      for (VertexRange& range : dirty_) {
        range.first = std::min(range.first, size);
        range.count = std::min(range.count, size - range.first);
        vertex_bytes +=
            static_cast<std::size_t>(range.count) * sizeof(ShapeVertex);
      }
      const std::size_t index_bytes = indices_.size() * sizeof(Index);
      const std::size_t total = vertex_bytes + index_bytes;
      if (total > std::numeric_limits<Uint32>::max() ||
          !staging_.ensure(device, SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                           static_cast<std::uint32_t>(total))) {
        return;
      }
      void* const mapped =
          SDL_MapGPUTransferBuffer(device, staging_.get(), true);
      if (mapped == nullptr) {
        return;
      }
      const std::span<std::byte> bytes{static_cast<std::byte*>(mapped), total};
      std::size_t offset = 0;
      for (const VertexRange& range : dirty_) {
        const std::span<const std::byte> source =
            std::as_bytes(std::span{mirror_}.subspan(range.first, range.count));
        std::ranges::copy(source, bytes.subspan(offset).begin());
        offset += source.size();
      }
      std::ranges::copy(std::as_bytes(std::span{indices_}),
                        bytes.subspan(offset).begin());
      SDL_UnmapGPUTransferBuffer(device, staging_.get());
      offset = 0;
      for (const VertexRange& range : dirty_) {
        if (range.count == 0) {
          continue;
        }
        SDL_GPUTransferBufferLocation source{};
        source.transfer_buffer = staging_.get();
        source.offset = static_cast<Uint32>(offset);
        SDL_GPUBufferRegion destination{};
        destination.buffer = vertex_buffer_.get().resource;
        destination.offset =
            static_cast<Uint32>(range.first * sizeof(ShapeVertex));
        destination.size =
            static_cast<Uint32>(range.count * sizeof(ShapeVertex));
        SDL_UploadToGPUBuffer(pass, &source, &destination, false);
        offset += static_cast<std::size_t>(range.count) * sizeof(ShapeVertex);
      }
      SDL_GPUTransferBufferLocation source{};
      source.transfer_buffer = staging_.get();
      source.offset = static_cast<Uint32>(offset);
      SDL_GPUBufferRegion destination{};
      destination.buffer = index_buffer_.get().resource;
      destination.size = static_cast<Uint32>(index_bytes);
      SDL_UploadToGPUBuffer(pass, &source, &destination, true);
      dirty_.clear();
      whole_ = false;
      used_ = false;
    }

    auto vertex_buffer() const noexcept -> SDL_GPUBuffer* {
      return vertex_buffer_.get().resource;
    }

    auto index_buffer() const noexcept -> SDL_GPUBuffer* {
      return index_buffer_.get().resource;
    }

   private:
    auto grow(SDL_GPUDevice* device, GpuBuffer& buffer, std::uint32_t& capacity,
              std::uint32_t wanted, std::uint32_t minimum, std::size_t stride,
              SDL_GPUBufferUsageFlags usage) -> bool {
      if (buffer.valid() && capacity >= wanted) {
        return true;
      }
      const std::uint64_t grown =
          std::max<std::uint64_t>({wanted, minimum, 2ULL * capacity});
      if (grown * stride > std::numeric_limits<Uint32>::max()) {
        return false;
      }
      SDL_GPUBufferCreateInfo info{};
      info.usage = usage;
      info.size = static_cast<Uint32>(grown * stride);
      GpuBuffer created{
          {.device = device, .resource = SDL_CreateGPUBuffer(device, &info)}};
      if (!created.valid()) {
        return false;
      }
      buffer = std::move(created);
      capacity = static_cast<std::uint32_t>(grown);
      whole_ = true;
      return true;
    }

    std::vector<ShapeVertex> mirror_;
    std::vector<Index> indices_;
    std::vector<VertexRange> dirty_;
    GpuBuffer vertex_buffer_;
    GpuBuffer index_buffer_;
    std::uint32_t vertex_capacity_{0};
    std::uint32_t index_capacity_{0};
    Transfer staging_;
    std::uint64_t generation_{1};
    bool whole_{true};
    bool used_{false};
  };

  enum class PipelineKind : std::uint8_t {
    textured,
    shape,
  };

  struct BlockCommand {
    PipelineKind pipeline;
    std::uint32_t block;
    DrawCommand command;
  };

  struct OffscreenPass {
    RenderPass pass;
    std::vector<BlockCommand> commands;
  };

  struct Target {
    SDL_GPUTexture* texture;
    std::uint32_t width;
    std::uint32_t height;
  };

  using Destination = std::variant<SDL_Window*, SurfaceSize>;

  [[nodiscard]] static auto create(Destination destination)
      -> std::expected<std::shared_ptr<Device>, ProviderError> {
    auto video = Sdl3Context::create(SDL_INIT_VIDEO);
    if (!video.has_value()) {
      return std::unexpected(video.error());
    }
    DeviceHandle device{SDL_CreateGPUDevice(shader_formats, false, nullptr)};
    if (!device.valid()) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::platform_failure});
    }
    SDL_GPUDevice* const raw = device.get();
    WindowClaim claim;
    GpuTexture offscreen;
    SurfaceSize offscreen_size{};
    SDL_GPUTextureFormat format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    if (SDL_Window* const* window = std::get_if<SDL_Window*>(&destination)) {
      if (!SDL_ClaimWindowForGPUDevice(raw, *window)) {
        return std::unexpected(
            ProviderError{ProviderErrorCode::platform_failure});
      }
      claim = WindowClaim{{.device = raw, .resource = *window}};
      format = SDL_GetGPUSwapchainTextureFormat(raw, *window);
    } else {
      offscreen_size = std::get<SurfaceSize>(destination);
      SDL_GPUTextureCreateInfo info{};
      info.type = SDL_GPU_TEXTURETYPE_2D;
      info.format = format;
      info.usage =
          SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
      info.width = offscreen_size.width.value();
      info.height = offscreen_size.height.value();
      info.layer_count_or_depth = 1;
      info.num_levels = 1;
      info.sample_count = SDL_GPU_SAMPLECOUNT_1;
      offscreen = GpuTexture{
          {.device = raw, .resource = SDL_CreateGPUTexture(raw, &info)}};
      if (!offscreen.valid()) {
        return std::unexpected(
            ProviderError{ProviderErrorCode::resource_exhausted});
      }
    }
    auto pipeline = create_pipeline(
        raw, format,
        PipelineShaders{.vertex = Sdl3Shaders::textured_vertex(),
                        .fragment = Sdl3Shaders::textured_fragment(),
                        .vertex_uniforms = 1,
                        .fragment_uniforms = 0,
                        .samplers = 1},
        textured_layout());
    if (!pipeline.has_value()) {
      return std::unexpected(pipeline.error());
    }
    auto shape_pipeline = create_pipeline(
        raw, format,
        PipelineShaders{.vertex = Sdl3Shaders::shape_vertex(),
                        .fragment = Sdl3Shaders::shape_fragment(),
                        .vertex_uniforms = 2,
                        .fragment_uniforms = 1,
                        .samplers = 2},
        shape_layout());
    if (!shape_pipeline.has_value()) {
      return std::unexpected(shape_pipeline.error());
    }
    GpuSampler nearest = create_sampler(raw, SDL_GPU_FILTER_NEAREST);
    if (!nearest.valid()) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::platform_failure});
    }
    return std::make_shared<Device>(
        std::move(*video), std::move(device), std::move(claim),
        std::move(offscreen), offscreen_size, format, std::move(*pipeline),
        std::move(*shape_pipeline), std::move(nearest));
  }

  Device(Sdl3Context video, DeviceHandle device, WindowClaim claim,
         GpuTexture offscreen, SurfaceSize offscreen_size,
         SDL_GPUTextureFormat format, Pipeline pipeline,
         Pipeline shape_pipeline, GpuSampler nearest)
      : video_(std::move(video)),
        device_(std::move(device)),
        claim_(std::move(claim)),
        offscreen_(std::move(offscreen)),
        offscreen_size_(offscreen_size),
        format_(format),
        pipeline_(std::move(pipeline)),
        shape_pipeline_(std::move(shape_pipeline)),
        nearest_(std::move(nearest)),
        registry_(std::make_shared<Registry>(
            [raw = device_.get()](Native& native) noexcept {
              if (native.owned) {
                SDL_ReleaseGPUTexture(raw, native.texture);
              }
            })),
        publisher_(
            std::make_shared<Publisher>(hooks_.schedule().weak_from_this())),
        surfaces_(hooks_.schedule().shared_from_this(), publisher_,
                  [this](TextureDescription description) {
                    return create_texture(description);
                  }) {}

  Device(const Device&) = delete;
  auto operator=(const Device&) -> Device& = delete;
  Device(Device&&) = delete;
  auto operator=(Device&&) -> Device& = delete;

  ~Device() {
    hole_.reset();
    static_cast<void>(SDL_WaitForGPUIdle(device_.get()));
    in_flight_.clear();
    publisher_->completed(GpuSerial{std::numeric_limits<std::uint64_t>::max()});
    registry_->retire();
  }

  auto handle() const noexcept -> SDL_GPUDevice* { return device_.get(); }

  auto format() const noexcept -> SDL_GPUTextureFormat { return format_; }

  auto registry() const noexcept -> const std::shared_ptr<Registry>& {
    return registry_;
  }

  auto timings() const noexcept -> GpuTimings { return timings_; }

  auto hooks() noexcept -> Hooks& { return hooks_; }

  auto surfaces() noexcept -> Surfaces& { return surfaces_; }

  auto queue() noexcept -> RenderQueue& { return queue_; }

  auto publisher() const noexcept -> const std::shared_ptr<Publisher>& {
    return publisher_;
  }

  auto upcoming() noexcept -> GpuSerial {
    return GpuSerial{hooks_.schedule().serial().value() + 1};
  }

  auto create_texture(TextureDescription description)
      -> std::expected<Texture, ProviderError> {
    if (!Texture::describable(description)) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument});
    }
    const bool renderable = description.usage == TextureUsage::render_target;
    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = renderable ? format_ : texture_format(description.format);
    info.usage =
        renderable
            ? (SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER)
            : SDL_GPU_TEXTUREUSAGE_SAMPLER;
    info.width = description.width;
    info.height = description.height;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    SDL_GPUTexture* const texture = SDL_CreateGPUTexture(device_.get(), &info);
    if (texture == nullptr) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::resource_exhausted});
    }
    if (auto blanked = blank(texture, info); !blanked.has_value()) {
      SDL_ReleaseGPUTexture(device_.get(), texture);
      return std::unexpected(blanked.error());
    }
    return Registry::adopt(
        registry_, description,
        Native{.texture = texture, .format = info.format, .owned = true});
  }

  [[nodiscard]] auto blank(SDL_GPUTexture* texture,
                           const SDL_GPUTextureCreateInfo& info)
      -> std::expected<void, ProviderError> {
    SDL_GPUDevice* const raw = device_.get();
    SDL_GPUCommandBuffer* const commands = SDL_AcquireGPUCommandBuffer(raw);
    if (commands == nullptr) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::platform_failure});
    }
    if ((info.usage & SDL_GPU_TEXTUREUSAGE_COLOR_TARGET) != 0) {
      run_pass(
          commands,
          Target{
              .texture = texture, .width = info.width, .height = info.height},
          transparent, [](SDL_GPURenderPass*) {});
      return submit(commands);
    }
    const std::size_t size = static_cast<std::size_t>(info.width) *
                             info.height * texel_bytes(info.format);
    if (size > std::numeric_limits<Uint32>::max() ||
        !texture_upload_.ensure(raw, SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                                static_cast<std::uint32_t>(size))) {
      static_cast<void>(SDL_CancelGPUCommandBuffer(commands));
      return std::unexpected(
          ProviderError{ProviderErrorCode::resource_exhausted});
    }
    void* const mapped =
        SDL_MapGPUTransferBuffer(raw, texture_upload_.get(), true);
    if (mapped == nullptr) {
      static_cast<void>(SDL_CancelGPUCommandBuffer(commands));
      return std::unexpected(
          ProviderError{ProviderErrorCode::platform_failure});
    }
    std::ranges::fill(std::span{static_cast<std::byte*>(mapped), size},
                      std::byte{0});
    SDL_UnmapGPUTransferBuffer(raw, texture_upload_.get());
    SDL_GPUCopyPass* const pass = SDL_BeginGPUCopyPass(commands);
    SDL_GPUTextureTransferInfo source{};
    source.transfer_buffer = texture_upload_.get();
    source.pixels_per_row = info.width;
    source.rows_per_layer = info.height;
    SDL_GPUTextureRegion destination{};
    destination.texture = texture;
    destination.w = info.width;
    destination.h = info.height;
    destination.d = 1;
    SDL_UploadToGPUTexture(pass, &source, &destination, false);
    SDL_EndGPUCopyPass(pass);
    return submit(commands);
  }

  auto import_texture(SDL_GPUTexture* texture, ImportDescription description,
                      support::ScopedCallback released)
      -> std::expected<Imported, ProviderError> {
    const TextureDescription imported = description.texture();
    if (texture == nullptr || description.planes() != 1 ||
        !Texture::describable(imported)) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument});
    }
    return Imported::adopt(
        publisher_,
        Registry::adopt(registry_, imported,
                        Native{.texture = texture,
                               .format = texture_format(description.format),
                               .owned = false}),
        description, std::move(released));
  }

  auto import_texture(std::span<SDL_GPUTexture* const> planes,
                      ImportDescription description,
                      support::ScopedCallback released)
      -> std::expected<Imported, ProviderError> {
    if (planes.size() != description.planes() ||
        std::ranges::any_of(
            planes, [](SDL_GPUTexture* plane) { return plane == nullptr; })) {
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
      textures.push_back(
          Registry::adopt(registry_, plane,
                          Native{.texture = planes[index],
                                 .format = texture_format(plane.format),
                                 .owned = false}));
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
    const TextureFormat source_format = record->description.format;
    const std::size_t source_row = static_cast<std::size_t>(region.width) *
                                   Texture::bytes_per_pixel(source_format);
    const std::size_t target_row = static_cast<std::size_t>(region.width) *
                                   texel_bytes(record->native.format);
    const std::size_t size = target_row * region.height;
    SDL_GPUDevice* const raw = device_.get();
    if (size > std::numeric_limits<Uint32>::max() ||
        !texture_upload_.ensure(raw, SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                                static_cast<std::uint32_t>(size))) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::resource_exhausted});
    }
    void* const mapped =
        SDL_MapGPUTransferBuffer(raw, texture_upload_.get(), true);
    if (mapped == nullptr) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::platform_failure});
    }
    const std::span target{static_cast<std::byte*>(mapped), size};
    for (std::uint32_t line = 0; line < region.height; ++line) {
      const std::span<const std::byte> source = pixels.row(line, source_row);
      const std::span<std::byte> destination =
          target.subspan(line * target_row, target_row);
      convert(source_format, record->native.format, source, destination);
    }
    SDL_UnmapGPUTransferBuffer(raw, texture_upload_.get());
    SDL_GPUCommandBuffer* const commands = SDL_AcquireGPUCommandBuffer(raw);
    if (commands == nullptr) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::platform_failure});
    }
    SDL_GPUCopyPass* const pass = SDL_BeginGPUCopyPass(commands);
    SDL_GPUTextureTransferInfo source{};
    source.transfer_buffer = texture_upload_.get();
    source.pixels_per_row = region.width;
    source.rows_per_layer = region.height;
    SDL_GPUTextureRegion destination{};
    destination.texture = record->native.texture;
    destination.x = region.x;
    destination.y = region.y;
    destination.w = region.width;
    destination.h = region.height;
    destination.d = 1;
    SDL_UploadToGPUTexture(pass, &source, &destination, false);
    SDL_EndGPUCopyPass(pass);
    return submit(commands);
  }

  [[nodiscard]] auto open_frame() -> std::expected<void, ProviderError> {
    if (frame_open_) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::resource_exhausted});
    }
    geometry_.open(device_.get());
    shapes_.open(device_.get());
    frame_open_ = true;
    return {};
  }

  void close_frame() noexcept {
    geometry_.unmap(device_.get());
    shapes_.unmap(device_.get());
    frame_open_ = false;
  }

  [[nodiscard]] auto allocate_geometry(std::uint32_t vertices,
                                       std::uint32_t indices)
      -> std::expected<FrameGeometry, ProviderError> {
    return geometry_.allocate(device_.get(), vertices, indices);
  }

  [[nodiscard]] auto allocate_shapes(std::uint32_t vertices,
                                     std::uint32_t indices)
      -> std::expected<ShapeGeometry, ProviderError> {
    return shapes_.allocate(device_.get(), vertices, indices);
  }

  auto allocate_resident(std::uint32_t vertices, std::uint32_t indices)
      -> ResidentShapes {
    return resident_.allocate(vertices, indices);
  }

  void touch(VertexRange range) { resident_.touch(range); }

  auto owns(const FrameGeometry& geometry) const noexcept -> bool {
    return geometry_.owns(geometry);
  }

  auto owns(const ShapeGeometry& geometry) const noexcept -> bool {
    return resident_.owns(geometry) || shapes_.owns(geometry);
  }

  auto render(std::optional<Colour> clear,
              std::span<const BlockCommand> commands,
              std::span<const OffscreenPass> offscreen, FramePlan plan,
              const FrameStopwatch& stopwatch, std::chrono::nanoseconds& waited)
      -> std::expected<void, ProviderError> {
    SDL_GPUTexture* const hole = surfaces_.installed()
                                     ? hole_texture(clear.value_or(transparent))
                                     : nullptr;
    retire_fences();
    geometry_.unmap(device_.get());
    shapes_.unmap(device_.get());
    SDL_GPUCommandBuffer* const buffer =
        SDL_AcquireGPUCommandBuffer(device_.get());
    if (buffer == nullptr) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::platform_failure});
    }
    const std::chrono::nanoseconds acquiring = stopwatch.now();
    const auto target = acquire_target(buffer);
    waited += stopwatch.since(acquiring);
    if (!target.has_value()) {
      static_cast<void>(SDL_CancelGPUCommandBuffer(buffer));
      return std::unexpected(target.error());
    }
    if (!target->has_value()) {
      return submit(buffer);
    }
    const Target& destination = **target;
    const bool geometry =
        geometry_.used() || shapes_.used() || resident_.used();
    if (geometry) {
      SDL_GPUCopyPass* const pass = SDL_BeginGPUCopyPass(buffer);
      geometry_.upload(pass);
      shapes_.upload(pass);
      resident_.upload(device_.get(), pass);
      SDL_EndGPUCopyPass(pass);
    }
    const GpuSerial serial = hooks_.schedule().next_serial();
    draw_surfaces(buffer, commands, offscreen, serial, plan);
    if (geometry && !offscreen.empty()) {
      const std::uint64_t started = SDL_GetTicksNS();
      for (const OffscreenPass& each : offscreen) {
        const auto record = registry_->find(each.pass.target);
        if (!record.has_value()) {
          continue;
        }
        const Target layer{.texture = record->native.texture,
                           .width = record->description.width,
                           .height = record->description.height};
        std::optional<Colour> opening = each.pass.clear;
        surfaces_.interleave(
            std::span<const BlockCommand>{each.commands},
            &BlockCommand::command,
            [&](std::span<const BlockCommand> part) {
              run_pass(buffer, layer, std::exchange(opening, std::nullopt),
                       [&](SDL_GPURenderPass* pass) {
                         record_ui(pass, buffer, layer, part, each.pass.region);
                       });
            },
            [&](const Surfaces::Underlay& underlay) {
              if (opening.has_value()) {
                run_pass(buffer, layer, std::exchange(opening, std::nullopt),
                         [](SDL_GPURenderPass*) {});
              }
              blit_hole(buffer, hole, layer,
                        Surfaces::mapped(underlay.scissor, each.pass.region,
                                         layer.width, layer.height));
            });
        if (opening.has_value()) {
          run_pass(buffer, layer, opening, [](SDL_GPURenderPass*) {});
        }
      }
      timings_ = GpuTimings{
          .effects = std::chrono::nanoseconds{static_cast<std::int64_t>(
              SDL_GetTicksNS() - started)},
          .interface = std::nullopt,
          .passes = static_cast<std::uint32_t>(offscreen.size()),
          .measured_on_gpu = false};
    } else {
      timings_ = GpuTimings{};
    }
    const FrameTarget hook_target{.commands = buffer,
                                  .texture = destination.texture,
                                  .format = format_,
                                  .width = destination.width,
                                  .height = destination.height};
    const FrameInfo info{
        .serial = serial,
        .predicted_present = plan.predicted_present,
        .extent = SurfaceSize{.width = SurfaceWidth{destination.width},
                              .height = SurfaceHeight{destination.height}},
        .replay = plan.replay,
        .damage = std::nullopt};
    const bool hooked = hooks_.installed();
    bool load = !clear.has_value();
    if (hooked) {
      if (clear.has_value()) {
        run_pass(buffer, destination, clear, [](SDL_GPURenderPass*) {});
      }
      hooks_.run(FrameStage::before_ui, hook_target, info);
      load = true;
    }
    const PassRegion whole{.x = 0.0F,
                           .y = 0.0F,
                           .width = static_cast<float>(destination.width),
                           .height = static_cast<float>(destination.height)};
    const ClipRect screen{.x = 0,
                          .y = 0,
                          .width = destination.width,
                          .height = destination.height};
    std::optional<Colour> opening = load ? std::nullopt : clear;
    if (geometry && !commands.empty()) {
      surfaces_.interleave(
          commands, &BlockCommand::command,
          [&](std::span<const BlockCommand> part) {
            run_pass(buffer, destination, std::exchange(opening, std::nullopt),
                     [&](SDL_GPURenderPass* pass) {
                       record_ui(pass, buffer, destination, part, whole);
                     });
          },
          [&](const Surfaces::Underlay& underlay) {
            if (opening.has_value()) {
              run_pass(buffer, destination,
                       std::exchange(opening, std::nullopt),
                       [](SDL_GPURenderPass*) {});
            }
            const ClipRect scissor =
                Surfaces::intersect(underlay.scissor, screen);
            if (scissor.width == 0 || scissor.height == 0) {
              return;
            }
            blit_hole(buffer, hole, destination, scissor);
            static_cast<void>(surfaces_.run(
                underlay.surface, hook_target,
                SurfaceFrame{.surface = underlay.surface,
                             .mode = provider::SurfaceMode::underlay,
                             .frame = info,
                             .viewport = underlay.viewport,
                             .scissor = scissor}));
          });
    }
    if (opening.has_value()) {
      run_pass(buffer, destination, opening, [](SDL_GPURenderPass*) {});
    }
    if (hooked) {
      hooks_.run(FrameStage::after_ui, hook_target, info);
    }
    return submit(buffer, info.serial);
  }

  void draw_surfaces(SDL_GPUCommandBuffer* buffer,
                     std::span<const BlockCommand> commands,
                     std::span<const OffscreenPass> offscreen, GpuSerial serial,
                     const FramePlan& plan) {
    if (!surfaces_.installed()) {
      return;
    }
    std::vector<Surfaces::Due> due =
        surfaces_.due(commands, serial, &BlockCommand::command);
    for (const OffscreenPass& each : offscreen) {
      for (const Surfaces::Due& more :
           surfaces_.due(std::span<const BlockCommand>{each.commands}, serial,
                         &BlockCommand::command)) {
        due.push_back(more);
      }
    }
    for (const Surfaces::Due& each : due) {
      const auto record = registry_->find(each.texture);
      if (!record.has_value()) {
        continue;
      }
      const std::uint32_t width = record->description.width;
      const std::uint32_t height = record->description.height;
      const FrameTarget target{.commands = buffer,
                               .texture = record->native.texture,
                               .format = record->native.format,
                               .width = width,
                               .height = height};
      static_cast<void>(surfaces_.run(
          each.surface, target,
          SurfaceFrame{
              .surface = each.surface,
              .mode = provider::SurfaceMode::texture,
              .frame =
                  FrameInfo{
                      .serial = serial,
                      .predicted_present = plan.predicted_present,
                      .extent = SurfaceSize{.width = SurfaceWidth{width},
                                            .height = SurfaceHeight{height}},
                      .replay = plan.replay,
                      .damage = std::nullopt},
              .viewport = PassRegion{.x = 0.0F,
                                     .y = 0.0F,
                                     .width = static_cast<float>(width),
                                     .height = static_cast<float>(height)},
              .scissor =
                  ClipRect{.x = 0, .y = 0, .width = width, .height = height}}));
    }
  }

  auto hole_texture(Colour colour) -> SDL_GPUTexture* {
    if (!hole_.has_value()) {
      auto made =
          create_texture(TextureDescription{.width = 1,
                                            .height = 1,
                                            .format = TextureFormat::rgba8,
                                            .usage = TextureUsage::sampled,
                                            .filter = SamplerFilter::nearest});
      if (!made.has_value()) {
        return nullptr;
      }
      hole_.emplace(std::move(*made));
      hole_colour_.reset();
    }
    if (hole_colour_ != colour) {
      const auto level = [](float channel) {
        return static_cast<std::byte>(
            std::lround(std::clamp(channel, 0.0F, 1.0F) * 255.0F));
      };
      const std::array<std::byte, 4> pixel{
          level(colour.red), level(colour.green), level(colour.blue),
          level(colour.alpha)};
      if (update_texture(*hole_,
                         TextureRegion{.x = 0, .y = 0, .width = 1, .height = 1},
                         PixelRows{pixel})
              .has_value()) {
        hole_colour_ = colour;
      }
    }
    const auto record = registry_->find(hole_->id());
    return record.has_value() ? record->native.texture : nullptr;
  }

  static void blit_hole(SDL_GPUCommandBuffer* buffer, SDL_GPUTexture* hole,
                        const Target& target, ClipRect area) {
    if (hole == nullptr || area.width == 0 || area.height == 0) {
      return;
    }
    SDL_GPUBlitInfo info{};
    info.source.texture = hole;
    info.source.w = 1;
    info.source.h = 1;
    info.destination.texture = target.texture;
    info.destination.x = area.x;
    info.destination.y = area.y;
    info.destination.w = area.width;
    info.destination.h = area.height;
    info.load_op = SDL_GPU_LOADOP_LOAD;
    info.filter = SDL_GPU_FILTER_NEAREST;
    SDL_BlitGPUTexture(buffer, &info);
  }

  auto capture() -> std::expected<Image, ProviderError> {
    if (!offscreen_.valid()) {
      return std::unexpected(ProviderError::unsupported_capability());
    }
    SDL_GPUDevice* const raw = device_.get();
    const std::uint32_t width = offscreen_size_.width.value();
    const std::uint32_t height = offscreen_size_.height.value();
    const std::size_t size =
        static_cast<std::size_t>(width) * height * Image::channels;
    if (!download_.ensure(raw, SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
                          static_cast<std::uint32_t>(size))) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::resource_exhausted});
    }
    SDL_GPUCommandBuffer* const commands = SDL_AcquireGPUCommandBuffer(raw);
    if (commands == nullptr) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::platform_failure});
    }
    SDL_GPUCopyPass* const pass = SDL_BeginGPUCopyPass(commands);
    SDL_GPUTextureRegion source{};
    source.texture = offscreen_.get().resource;
    source.w = width;
    source.h = height;
    source.d = 1;
    SDL_GPUTextureTransferInfo destination{};
    destination.transfer_buffer = download_.get();
    destination.pixels_per_row = width;
    destination.rows_per_layer = height;
    SDL_DownloadFromGPUTexture(pass, &source, &destination);
    SDL_EndGPUCopyPass(pass);
    const GpuFence fence{
        {.device = raw,
         .resource = SDL_SubmitGPUCommandBufferAndAcquireFence(commands)}};
    if (!fence.valid()) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::platform_failure});
    }
    SDL_GPUFence* const waited = fence.get().resource;
    if (!SDL_WaitForGPUFences(raw, true, &waited, 1)) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::platform_failure});
    }
    const void* const mapped =
        SDL_MapGPUTransferBuffer(raw, download_.get(), false);
    if (mapped == nullptr) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::platform_failure});
    }
    std::vector<std::byte> pixels(size);
    std::memcpy(pixels.data(), mapped, size);
    SDL_UnmapGPUTransferBuffer(raw, download_.get());
    return Image::create(width, height, std::move(pixels));
  }

 private:
  struct PipelineShaders {
    std::span<const std::uint8_t> vertex;
    std::span<const std::uint8_t> fragment;
    std::uint32_t vertex_uniforms;
    std::uint32_t fragment_uniforms;
    std::uint32_t samplers;
  };

  struct VertexLayout {
    std::uint32_t pitch;
    std::vector<SDL_GPUVertexAttribute> attributes;
  };

  static auto attribute(std::uint32_t location,
                        SDL_GPUVertexElementFormat format, std::size_t offset)
      -> SDL_GPUVertexAttribute {
    SDL_GPUVertexAttribute described{};
    described.location = location;
    described.format = format;
    described.offset = static_cast<Uint32>(offset);
    return described;
  }

  static auto textured_layout() -> VertexLayout {
    return VertexLayout{
        .pitch = sizeof(Vertex),
        .attributes = {attribute(0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
                                 offsetof(Vertex, position)),
                       attribute(1, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
                                 offsetof(Vertex, uv)),
                       attribute(2, SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM,
                                 offsetof(Vertex, colour))}};
  }

  static auto shape_layout() -> VertexLayout {
    return VertexLayout{
        .pitch = sizeof(ShapeVertex),
        .attributes = {attribute(0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
                                 offsetof(ShapeVertex, position)),
                       attribute(1, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
                                 offsetof(ShapeVertex, uv)),
                       attribute(2, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
                                 offsetof(ShapeVertex, local)),
                       attribute(3, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
                                 offsetof(ShapeVertex, half_size)),
                       attribute(4, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
                                 offsetof(ShapeVertex, shape)),
                       attribute(5, SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM,
                                 offsetof(ShapeVertex, colour)),
                       attribute(6, SDL_GPU_VERTEXELEMENTFORMAT_UINT,
                                 offsetof(ShapeVertex, kind)),
                       attribute(7, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
                                 offsetof(ShapeVertex, effect)),
                       attribute(8, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
                                 offsetof(ShapeVertex, detail))}};
  }

  static auto create_pipeline(SDL_GPUDevice* device,
                              SDL_GPUTextureFormat format,
                              const PipelineShaders& shaders,
                              const VertexLayout& layout)
      -> std::expected<Pipeline, ProviderError> {
    Pipeline pipeline{
        .vertex = create_shader(device, SDL_GPU_SHADERSTAGE_VERTEX,
                                shaders.vertex, shaders.vertex_uniforms, 0),
        .fragment = create_shader(device, SDL_GPU_SHADERSTAGE_FRAGMENT,
                                  shaders.fragment, shaders.fragment_uniforms,
                                  shaders.samplers),
        .pipeline = {},
        .sampler = {}};
    if (!pipeline.vertex.valid() || !pipeline.fragment.valid()) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::platform_failure});
    }
    SDL_GPUVertexBufferDescription buffer{};
    buffer.slot = 0;
    buffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    buffer.pitch = layout.pitch;
    SDL_GPUColorTargetDescription target{};
    target.format = format;
    target.blend_state.enable_blend = true;
    target.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    target.blend_state.dst_color_blendfactor =
        SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    target.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    target.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    target.blend_state.dst_alpha_blendfactor =
        SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    target.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    target.blend_state.color_write_mask =
        SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G |
        SDL_GPU_COLORCOMPONENT_B | SDL_GPU_COLORCOMPONENT_A;
    SDL_GPUGraphicsPipelineCreateInfo info{};
    info.vertex_shader = pipeline.vertex.get().resource;
    info.fragment_shader = pipeline.fragment.get().resource;
    info.vertex_input_state.vertex_buffer_descriptions = &buffer;
    info.vertex_input_state.num_vertex_buffers = 1;
    info.vertex_input_state.vertex_attributes = layout.attributes.data();
    info.vertex_input_state.num_vertex_attributes =
        static_cast<Uint32>(layout.attributes.size());
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
    info.target_info.color_target_descriptions = &target;
    info.target_info.num_color_targets = 1;
    pipeline.pipeline =
        GpuPipeline{{.device = device,
                     .resource = SDL_CreateGPUGraphicsPipeline(device, &info)}};
    SDL_GPUSamplerCreateInfo sampler{};
    sampler.min_filter = SDL_GPU_FILTER_LINEAR;
    sampler.mag_filter = SDL_GPU_FILTER_LINEAR;
    sampler.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
    sampler.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler.min_lod = -1000.0F;
    sampler.max_lod = 1000.0F;
    pipeline.sampler = GpuSampler{
        {.device = device, .resource = SDL_CreateGPUSampler(device, &sampler)}};
    if (!pipeline.pipeline.valid() || !pipeline.sampler.valid()) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::platform_failure});
    }
    return pipeline;
  }

  static auto create_shader(SDL_GPUDevice* device, SDL_GPUShaderStage stage,
                            std::span<const std::uint8_t> code,
                            std::uint32_t uniforms, std::uint32_t samplers)
      -> GpuShader {
    if ((SDL_GetGPUShaderFormats(device) & SDL_GPU_SHADERFORMAT_SPIRV) == 0 ||
        code.empty()) {
      return GpuShader{};
    }
    SDL_GPUShaderCreateInfo info{};
    info.entrypoint = "main";
    info.stage = stage;
    info.format = SDL_GPU_SHADERFORMAT_SPIRV;
    info.num_uniform_buffers = uniforms;
    info.num_samplers = samplers;
    info.code = code.data();
    info.code_size = code.size();
    return GpuShader{
        {.device = device, .resource = SDL_CreateGPUShader(device, &info)}};
  }

  auto acquire_target(SDL_GPUCommandBuffer* buffer)
      -> std::expected<std::optional<Target>, ProviderError> {
    if (offscreen_.valid()) {
      return Target{.texture = offscreen_.get().resource,
                    .width = offscreen_size_.width.value(),
                    .height = offscreen_size_.height.value()};
    }
    SDL_GPUTexture* texture = nullptr;
    Uint32 width = 0;
    Uint32 height = 0;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(buffer, claim_.get().resource,
                                               &texture, &width, &height)) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::platform_failure});
    }
    if (texture == nullptr || width == 0 || height == 0) {
      return std::optional<Target>{};
    }
    return Target{.texture = texture, .width = width, .height = height};
  }

  template <class Record>
  static void run_pass(SDL_GPUCommandBuffer* buffer, const Target& target,
                       std::optional<Colour> clear, Record&& record) {
    SDL_GPUColorTargetInfo colour{};
    colour.texture = target.texture;
    colour.load_op =
        clear.has_value() ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
    colour.store_op = SDL_GPU_STOREOP_STORE;
    if (clear.has_value()) {
      colour.clear_color = SDL_FColor{.r = clear->red,
                                      .g = clear->green,
                                      .b = clear->blue,
                                      .a = clear->alpha};
    }
    SDL_GPURenderPass* const pass =
        SDL_BeginGPURenderPass(buffer, &colour, 1, nullptr);
    if (pass == nullptr) {
      return;
    }
    std::forward<Record>(record)(pass);
    SDL_EndGPURenderPass(pass);
  }

  void record_ui(SDL_GPURenderPass* pass, SDL_GPUCommandBuffer* buffer,
                 const Target& target, std::span<const BlockCommand> commands,
                 const PassRegion& region) {
    struct Transform {
      std::array<float, 2> scale;
      std::array<float, 2> translation;
    };
    const auto width = static_cast<float>(target.width);
    const auto height = static_cast<float>(target.height);
    const SDL_GPUViewport viewport{.x = 0.0F,
                                   .y = 0.0F,
                                   .w = width,
                                   .h = height,
                                   .min_depth = 0.0F,
                                   .max_depth = 1.0F};
    SDL_SetGPUViewport(pass, &viewport);
    const Transform textured{.scale = {2.0F / width, 2.0F / height},
                             .translation = {-1.0F, -1.0F}};
    const Transform shaped{
        .scale = {2.0F / region.width, -2.0F / region.height},
        .translation = {-1.0F - (2.0F * region.x / region.width),
                        1.0F + (2.0F * region.y / region.height)}};
    const float across = width / region.width;
    const float down = height / region.height;
    std::optional<PipelineKind> bound_pipeline;
    std::optional<std::pair<std::uint64_t, std::uint64_t>> bound;
    std::optional<std::uint32_t> bound_block;
    std::optional<std::array<float, 2>> bound_trim;
    std::optional<std::array<float, 4>> bound_placement;
    const auto mapped = [](float edge, float origin, float factor,
                           std::uint32_t limit, bool up) {
      const float value = (edge - origin) * factor;
      const float rounded = up ? std::ceil(value) : std::floor(value);
      return static_cast<std::uint32_t>(
          std::clamp(rounded, 0.0F, static_cast<float>(limit)));
    };
    for (const auto& [kind, block, command] : commands) {
      const std::uint32_t left = mapped(static_cast<float>(command.clip.x),
                                        region.x, across, target.width, false);
      const std::uint32_t top = mapped(static_cast<float>(command.clip.y),
                                       region.y, down, target.height, false);
      const std::uint32_t right =
          mapped(static_cast<float>(command.clip.x + command.clip.width),
                 region.x, across, target.width, true);
      const std::uint32_t bottom =
          mapped(static_cast<float>(command.clip.y + command.clip.height),
                 region.y, down, target.height, true);
      if (right <= left || bottom <= top || command.index_count == 0) {
        continue;
      }
      SDL_GPUBuffer* vertex_buffer = nullptr;
      SDL_GPUBuffer* index_buffer = nullptr;
      if (kind == PipelineKind::textured) {
        const auto* found = geometry_.block(block);
        if (found == nullptr) {
          continue;
        }
        vertex_buffer = found->vertex_buffer();
        index_buffer = found->index_buffer();
      } else if (block == ResidentShapes::block) {
        vertex_buffer = resident_.vertex_buffer();
        index_buffer = resident_.index_buffer();
        if (vertex_buffer == nullptr || index_buffer == nullptr) {
          continue;
        }
      } else {
        const auto* found = shapes_.block(block);
        if (found == nullptr) {
          continue;
        }
        vertex_buffer = found->vertex_buffer();
        index_buffer = found->index_buffer();
      }
      if (bound_pipeline != kind) {
        bound_pipeline = kind;
        bound.reset();
        bound_block.reset();
        const bool shape = kind == PipelineKind::shape;
        SDL_BindGPUGraphicsPipeline(
            pass, shape ? shape_pipeline_.pipeline.get().resource
                        : pipeline_.pipeline.get().resource);
        const Transform& transform = shape ? shaped : textured;
        SDL_PushGPUVertexUniformData(buffer, 0, &transform, sizeof(transform));
        bound_trim.reset();
        bound_placement.reset();
      }
      if (kind == PipelineKind::shape &&
          bound_placement != command.placement()) {
        bound_placement = command.placement();
        SDL_PushGPUVertexUniformData(buffer, 1, bound_placement->data(),
                                     sizeof(*bound_placement));
      }
      if (kind == PipelineKind::shape && bound_trim != command.trim) {
        bound_trim = command.trim;
        const std::array<float, 4> trim{command.trim[0], command.trim[1], 0.0F,
                                        0.0F};
        SDL_PushGPUFragmentUniformData(buffer, 0, trim.data(), sizeof(trim));
      }
      if (bound_block != block) {
        bound_block = block;
        const SDL_GPUBufferBinding vertex_binding{.buffer = vertex_buffer,
                                                  .offset = 0};
        SDL_BindGPUVertexBuffers(pass, 0, &vertex_binding, 1);
        const SDL_GPUBufferBinding index_binding{.buffer = index_buffer,
                                                 .offset = 0};
        SDL_BindGPUIndexBuffer(pass, &index_binding,
                               SDL_GPU_INDEXELEMENTSIZE_32BIT);
      }
      const TextureId detail =
          command.detail.value() != 0 ? command.detail : command.texture;
      if (!bound.has_value() || bound->first != command.texture.value() ||
          bound->second != detail.value()) {
        const auto record = registry_->find(command.texture);
        const auto second = registry_->find(detail);
        if (!record.has_value() || !second.has_value()) {
          continue;
        }
        bound.emplace(command.texture.value(), detail.value());
        const std::array<SDL_GPUTextureSamplerBinding, 2> bindings{
            SDL_GPUTextureSamplerBinding{
                .texture = record->native.texture,
                .sampler = sampler_of(record->description.filter)},
            SDL_GPUTextureSamplerBinding{
                .texture = second->native.texture,
                .sampler = sampler_of(second->description.filter)}};
        SDL_BindGPUFragmentSamplers(pass, 0, bindings.data(),
                                    kind == PipelineKind::shape ? 2U : 1U);
      }
      const SDL_Rect scissor{.x = static_cast<int>(left),
                             .y = static_cast<int>(top),
                             .w = static_cast<int>(right - left),
                             .h = static_cast<int>(bottom - top)};
      SDL_SetGPUScissor(pass, &scissor);
      SDL_DrawGPUIndexedPrimitives(
          pass, command.index_count, 1, command.index_offset,
          static_cast<Sint32>(command.vertex_offset), 0);
    }
  }

  static auto submit(SDL_GPUCommandBuffer* buffer)
      -> std::expected<void, ProviderError> {
    if (!SDL_SubmitGPUCommandBuffer(buffer)) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::platform_failure});
    }
    return {};
  }

  auto submit(SDL_GPUCommandBuffer* buffer, GpuSerial serial)
      -> std::expected<void, ProviderError> {
    SDL_GPUFence* const fence =
        SDL_SubmitGPUCommandBufferAndAcquireFence(buffer);
    if (fence == nullptr) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::platform_failure});
    }
    in_flight_.push_back(InFlight{
        .serial = serial.value(),
        .fence = GpuFence{{.device = device_.get(), .resource = fence}}});
    return {};
  }

  void retire_fences() {
    std::optional<std::uint64_t> done;
    while (!in_flight_.empty() &&
           SDL_QueryGPUFence(device_.get(),
                             in_flight_.front().fence.get().resource)) {
      done = in_flight_.front().serial;
      in_flight_.erase(in_flight_.begin());
    }
    if (done.has_value()) {
      publisher_->completed(GpuSerial{*done});
    }
  }

  auto sampler_of(SamplerFilter filter) const noexcept -> SDL_GPUSampler* {
    return filter == SamplerFilter::nearest ? nearest_.get().resource
                                            : pipeline_.sampler.get().resource;
  }

  static auto create_sampler(SDL_GPUDevice* device, SDL_GPUFilter filter)
      -> GpuSampler {
    SDL_GPUSamplerCreateInfo sampler{};
    sampler.min_filter = filter;
    sampler.mag_filter = filter;
    sampler.mipmap_mode = filter == SDL_GPU_FILTER_NEAREST
                              ? SDL_GPU_SAMPLERMIPMAPMODE_NEAREST
                              : SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
    sampler.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler.min_lod = -1000.0F;
    sampler.max_lod = 1000.0F;
    return GpuSampler{
        {.device = device, .resource = SDL_CreateGPUSampler(device, &sampler)}};
  }

  auto texture_format(TextureFormat format) const noexcept
      -> SDL_GPUTextureFormat {
    switch (format) {
      case TextureFormat::bgra8:
        return SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
      case TextureFormat::rgb565:
        return SDL_GPUTextureSupportsFormat(
                   device_.get(), SDL_GPU_TEXTUREFORMAT_B5G6R5_UNORM,
                   SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREUSAGE_SAMPLER)
                   ? SDL_GPU_TEXTUREFORMAT_B5G6R5_UNORM
                   : SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
      case TextureFormat::r8:
      case TextureFormat::nv12:
        return SDL_GPU_TEXTUREFORMAT_R8_UNORM;
      case TextureFormat::rg8:
        return SDL_GPU_TEXTUREFORMAT_R8G8_UNORM;
      case TextureFormat::rgba8:
      case TextureFormat::alpha8:
        return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    }
    return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
  }

  static auto texel_bytes(SDL_GPUTextureFormat format) noexcept -> std::size_t {
    switch (format) {
      case SDL_GPU_TEXTUREFORMAT_R8_UNORM:
        return 1;
      case SDL_GPU_TEXTUREFORMAT_R8G8_UNORM:
      case SDL_GPU_TEXTUREFORMAT_B5G6R5_UNORM:
        return 2;
      default:
        return 4;
    }
  }

  static void convert(TextureFormat source, SDL_GPUTextureFormat target,
                      std::span<const std::byte> from,
                      std::span<std::byte> to) {
    if (source == TextureFormat::alpha8) {
      for (std::size_t column = 0; column < from.size(); ++column) {
        to[column * 4] = std::byte{0xff};
        to[(column * 4) + 1] = std::byte{0xff};
        to[(column * 4) + 2] = std::byte{0xff};
        to[(column * 4) + 3] = from[column];
      }
      return;
    }
    if (source == TextureFormat::rgb565 &&
        target == SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM) {
      for (std::size_t column = 0; column * 2 < from.size(); ++column) {
        const std::uint32_t packed =
            std::to_integer<std::uint32_t>(from[column * 2]) |
            (std::to_integer<std::uint32_t>(from[(column * 2) + 1]) << 8U);
        const std::uint32_t red = (packed >> 11U) & 0x1FU;
        const std::uint32_t green = (packed >> 5U) & 0x3FU;
        const std::uint32_t blue = packed & 0x1FU;
        to[column * 4] = static_cast<std::byte>((red << 3U) | (red >> 2U));
        to[(column * 4) + 1] =
            static_cast<std::byte>((green << 2U) | (green >> 4U));
        to[(column * 4) + 2] =
            static_cast<std::byte>((blue << 3U) | (blue >> 2U));
        to[(column * 4) + 3] = std::byte{0xff};
      }
      return;
    }
    std::ranges::copy(from, to.begin());
  }

  Sdl3Context video_;
  DeviceHandle device_;
  WindowClaim claim_;
  GpuTexture offscreen_;
  SurfaceSize offscreen_size_;
  SDL_GPUTextureFormat format_;
  Pipeline pipeline_;
  Pipeline shape_pipeline_;
  GpuSampler nearest_;
  GeometryRing<Vertex> geometry_;
  GeometryRing<ShapeVertex> shapes_;
  ResidentStore resident_;
  bool frame_open_{false};
  Transfer texture_upload_;
  Transfer download_;
  GpuTimings timings_;
  std::shared_ptr<Registry> registry_;
  Hooks hooks_;
  std::shared_ptr<Publisher> publisher_;
  RenderQueue queue_;
  Surfaces surfaces_;
  std::optional<Texture> hole_;
  std::optional<Colour> hole_colour_;
  std::vector<InFlight> in_flight_;
};

class Sdl3Gpu::Frame final : public GpuFrame::Implementation {
 public:
  explicit Frame(std::weak_ptr<Device> device) noexcept
      : device_(std::move(device)) {
    open();
  }

  Frame(const Frame&) = delete;
  auto operator=(const Frame&) -> Frame& = delete;
  Frame(Frame&&) = delete;
  auto operator=(Frame&&) -> Frame& = delete;

  ~Frame() override { finish(); }

  void open() noexcept {
    clear_.reset();
    plan_ = FramePlan{};
    commands_.clear();
    offscreen_.clear();
    submitted_ = false;
    open_ = true;
  }

  void finish() noexcept override {
    if (!open_) {
      return;
    }
    open_ = false;
    if (const std::shared_ptr<Device> device = device_.lock()) {
      device->close_frame();
    }
  }

  [[nodiscard]] auto allocate_geometry(std::uint32_t vertex_count,
                                       std::uint32_t index_count)
      -> std::expected<FrameGeometry, ProviderError> override {
    const std::shared_ptr<Device> device = device_.lock();
    if (device == nullptr || submitted_) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::platform_failure});
    }
    return device->allocate_geometry(vertex_count, index_count);
  }

  [[nodiscard]] auto allocate_shapes(std::uint32_t vertex_count,
                                     std::uint32_t index_count)
      -> std::expected<ShapeGeometry, ProviderError> override {
    const std::shared_ptr<Device> device = device_.lock();
    if (device == nullptr || submitted_) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::platform_failure});
    }
    return device->allocate_shapes(vertex_count, index_count);
  }

  [[nodiscard]] auto allocate_resident(std::uint32_t vertex_count,
                                       std::uint32_t index_count)
      -> std::expected<ResidentShapes, ProviderError> override {
    const std::shared_ptr<Device> device = device_.lock();
    if (device == nullptr || submitted_) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::platform_failure});
    }
    return device->allocate_resident(vertex_count, index_count);
  }

  void touch(VertexRange range) override {
    if (const std::shared_ptr<Device> device = device_.lock()) {
      device->touch(range);
    }
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
    if (device == nullptr) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::platform_failure});
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
    Device::OffscreenPass& added = offscreen_.emplace_back(
        Device::OffscreenPass{.pass = pass, .commands = {}});
    auto recorded = record(data, Device::PipelineKind::shape, added.commands);
    if (!recorded.has_value()) {
      offscreen_.pop_back();
    }
    return recorded;
  }

  [[nodiscard]] auto present() -> std::expected<void, ProviderError> override {
    std::chrono::nanoseconds waited{0};
    return render(FrameStopwatch{std::nullopt}, waited);
  }

  [[nodiscard]] auto submit(const FramePacket&, FramePacket::Clock clock)
      -> std::expected<FrameReceipt, ProviderError> override {
    const FrameStopwatch stopwatch{clock};
    const std::chrono::nanoseconds began = stopwatch.now();
    std::chrono::nanoseconds waited{0};
    auto rendered = render(stopwatch, waited);
    std::optional<Image> image;
    if (rendered.has_value() && plan_.capture) {
      if (const std::shared_ptr<Device> device = device_.lock()) {
        if (auto captured = device->capture(); captured.has_value()) {
          image = std::move(*captured);
        }
      }
    }
    finish();
    if (!rendered.has_value()) {
      return std::unexpected(rendered.error());
    }
    return FrameReceipt{.record = std::max(stopwatch.since(began) - waited,
                                           std::chrono::nanoseconds{0}),
                        .wait = waited,
                        .displayed = std::nullopt,
                        .image = std::move(image)};
  }

 private:
  auto render(const FrameStopwatch& stopwatch, std::chrono::nanoseconds& waited)
      -> std::expected<void, ProviderError> {
    const std::shared_ptr<Device> device = device_.lock();
    if (device == nullptr) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::platform_failure});
    }
    submitted_ = true;
    const std::optional<RenderQueue::Lease> used = device->queue().submit();
    if (!used.has_value()) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::resource_exhausted,
                        ProviderErrorDetail::taken_over});
    }
    return device->render(clear_, commands_, offscreen_, plan_, stopwatch,
                          waited);
  }

  template <class V>
  auto record(const BasicDrawData<V>& data, Device::PipelineKind kind,
              std::vector<Device::BlockCommand>& into)
      -> std::expected<void, ProviderError> {
    const std::shared_ptr<Device> device = device_.lock();
    if (device == nullptr) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::platform_failure});
    }
    if (const auto refused = device->registry()->refusal(data.commands)) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument, *refused});
    }
    if (!data.commands.empty() && !device->owns(data.geometry)) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument,
                        ProviderErrorDetail::foreign_geometry});
    }
    device->publisher()->sampled(device->upcoming(), data.commands);
    for (DrawCommand command : data.commands) {
      command.vertex_offset += data.geometry.first_vertex;
      command.index_offset += data.geometry.first_index;
      into.push_back(Device::BlockCommand{
          .pipeline = kind, .block = data.geometry.block, .command = command});
    }
    return {};
  }

  std::weak_ptr<Device> device_;
  std::optional<Colour> clear_;
  FramePlan plan_{};
  std::vector<Device::BlockCommand> commands_;
  std::vector<Device::OffscreenPass> offscreen_;
  bool open_{false};
  bool submitted_{false};
};

Sdl3Gpu::Sdl3Gpu(std::shared_ptr<Device> device) noexcept
    : device_(std::move(device)) {}

auto Sdl3Gpu::create(Sdl3WindowSurface& surface)
    -> std::expected<Sdl3Gpu, ProviderError> {
  return Device::create(surface.window())
      .transform([](std::shared_ptr<Device> device) {
        return Sdl3Gpu{std::move(device)};
      });
}

auto Sdl3Gpu::create(const Sdl3OffscreenSurface& surface)
    -> std::expected<Sdl3Gpu, ProviderError> {
  return Device::create(surface.size())
      .transform([](std::shared_ptr<Device> device) {
        return Sdl3Gpu{std::move(device)};
      });
}

auto Sdl3Gpu::create_texture(TextureDescription description)
    -> std::expected<Texture, ProviderError> {
  return device_->create_texture(description);
}

auto Sdl3Gpu::update_texture(Texture& texture, TextureRegion region,
                             PixelRows pixels)
    -> std::expected<void, ProviderError> {
  return device_->update_texture(texture, region, pixels);
}

auto Sdl3Gpu::import_texture(SDL_GPUTexture* texture,
                             ImportDescription description,
                             support::ScopedCallback released)
    -> std::expected<Imported, ProviderError> {
  return device_->import_texture(texture, description, std::move(released));
}

auto Sdl3Gpu::import_texture(std::span<SDL_GPUTexture* const> planes,
                             ImportDescription description,
                             support::ScopedCallback released)
    -> std::expected<Imported, ProviderError> {
  return device_->import_texture(planes, description, std::move(released));
}

auto Sdl3Gpu::begin_frame() -> std::expected<GpuFrame, ProviderError> {
  if (auto opened = device_->open_frame(); !opened.has_value()) {
    return std::unexpected(opened.error());
  }
  if (spare_ == nullptr || spare_.use_count() > 1) {
    spare_ = std::make_shared<Frame>(device_);
  } else {
    spare_->open();
  }
  return GpuFrame::reused(spare_);
}

auto Sdl3Gpu::render_queue() noexcept
    -> std::optional<std::reference_wrapper<RenderQueue>> {
  return std::ref(device_->queue());
}

auto Sdl3Gpu::capture() -> std::expected<Image, ProviderError> {
  return device_->capture();
}

auto Sdl3Gpu::frame_schedule() noexcept
    -> std::optional<std::reference_wrapper<FrameSchedule>> {
  return std::ref(device_->hooks().schedule());
}

auto Sdl3Gpu::publications() const noexcept
    -> std::optional<std::reference_wrapper<const TexturePublications>> {
  return std::cref(
      static_cast<const TexturePublications&>(*device_->publisher()));
}

auto Sdl3Gpu::add_frame_hook(FrameStage stage, FrameHookFunction function,
                             FrameHookOptions options) -> FrameHook {
  return device_->hooks().add(stage, std::move(function), options);
}

auto Sdl3Gpu::add_surface(SurfaceOptions options, SurfaceFunction function)
    -> std::expected<GpuSurface, ProviderError> {
  return device_->surfaces().add(options, std::move(function));
}

auto Sdl3Gpu::surfaces() noexcept
    -> std::optional<std::reference_wrapper<SurfaceDirectory>> {
  return std::ref(device_->surfaces().directory());
}

auto Sdl3Gpu::device() const noexcept -> SDL_GPUDevice* {
  return device_->handle();
}

auto Sdl3Gpu::driver() const noexcept -> std::string_view {
  return SDL_GetGPUDeviceDriver(device_->handle());
}

auto Sdl3Gpu::target_format() const noexcept -> SDL_GPUTextureFormat {
  return device_->format();
}

auto Sdl3Gpu::timings() const -> GpuTimings { return device_->timings(); }

auto Sdl3Gpu::inventory() const -> GpuInventory {
  return GpuInventory{.backend = "sdl_gpu",
                      .report = std::format("sdl_gpu: {} driver", driver()),
                      .textures = device_->registry()->memory(),
                      .frames_presented = std::nullopt,
                      .missed_vsyncs = std::nullopt};
}

auto Sdl3Gpu::live_textures() const -> std::size_t {
  return device_->registry()->size();
}

}
