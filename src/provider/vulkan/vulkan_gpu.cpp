#include <arisa/provider/gpu/frame_planner.hpp>
#include <arisa/provider/gpu/frame_slots.hpp>
#include <arisa/provider/gpu/frame_stopwatch.hpp>
#include <arisa/provider/gpu/lent_swapchain.hpp>
#include <arisa/provider/gpu/overlay_slots.hpp>
#include <arisa/provider/gpu/texture_publisher.hpp>
#include <arisa/provider/gpu/texture_registry.hpp>
#include <arisa/provider/vulkan/offscreen_target.hpp>
#include <arisa/provider/vulkan/vulkan_buffer.hpp>
#include <arisa/provider/vulkan/vulkan_gpu.hpp>
#include <arisa/provider/vulkan/vulkan_image.hpp>
#include <arisa/provider/vulkan/vulkan_instance.hpp>
#include <arisa/provider/vulkan/vulkan_status.hpp>
#include <provider/vulkan/descriptor_pools.hpp>
#include <provider/vulkan/frame_slot.hpp>
#include <provider/vulkan/image_barriers.hpp>
#include <provider/vulkan/shape_pipelines.hpp>
#include <provider/vulkan/timestamp_queries.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <flat_map>
#include <format>
#include <iterator>
#include <limits>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace arisa::provider::vulkan {

class VulkanGpu::Renderer {
 public:
  using Kind = ShapePipelines::Kind;
  using Registry = TextureRegistry<std::uint32_t>;
  using Publisher = TexturePublisher<ReadySemaphore>;

  static constexpr std::size_t minimum_slots = 2;
  static constexpr std::size_t maximum_slots = 3;
  static constexpr std::chrono::nanoseconds display_period{16'666'667};

  using Destination = ShapePipelines::Destination;
  using Access = ImageBarriers::Access;
  using Transition = ImageBarriers::Transition;

  struct BlockCommand {
    Kind pipeline{Kind::shape};
    std::uint32_t block{0};
    DrawCommand command{};
  };

  struct OffscreenPass {
    RenderPass pass;
    std::size_t first{0};
    std::size_t count{0};
    std::size_t holes_first{0};
    std::size_t holes_count{0};
  };

  [[nodiscard]] static auto create(std::shared_ptr<const VulkanDevice> device,
                                   std::unique_ptr<VulkanTarget> target)
      -> std::expected<std::shared_ptr<Renderer>, ProviderError> {
    if (device == nullptr || target == nullptr) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument});
    }
    if (auto prepared = target->prepare({}); !prepared.has_value()) {
      return std::unexpected(prepared.error());
    }
    auto pipelines = ShapePipelines::create(*device, target->format());
    if (!pipelines.has_value()) {
      return std::unexpected(pipelines.error());
    }
    const std::size_t count =
        std::clamp(target->views().size(), minimum_slots, maximum_slots);
    std::vector<FrameSlot> slots;
    slots.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
      auto slot = FrameSlot::create(*device);
      if (!slot.has_value()) {
        return std::unexpected(slot.error());
      }
      slots.push_back(std::move(*slot));
    }
    auto replay = FrameSlot::create(*device);
    if (!replay.has_value()) {
      return std::unexpected(replay.error());
    }
    return std::make_shared<Renderer>(Key{}, std::move(device),
                                      std::move(target), std::move(*pipelines),
                                      std::move(slots), std::move(*replay));
  }

  class Key {
    friend class Renderer;
    Key() = default;
  };

  Renderer(Key, std::shared_ptr<const VulkanDevice> device,
           std::unique_ptr<VulkanTarget> target, ShapePipelines pipelines,
           std::vector<FrameSlot> slots, FrameSlot replay)
      : device_(std::move(device)),
        target_(std::move(target)),
        pipelines_(std::move(pipelines)),
        barriers_(*device_),
        render_barriers_(*device_),
        descriptors_(device_->handle()),
        slots_(std::move(slots)),
        replay_(std::move(replay)),
        ring_(static_cast<std::uint32_t>(slots_.size())),
        registry_(std::make_shared<Registry>(
            [this](std::uint32_t& slot) noexcept { released(slot); })),
        publisher_(
            std::make_shared<Publisher>(hooks_.schedule().weak_from_this())),
        surfaces_(hooks_.schedule().shared_from_this(), publisher_,
                  [this](TextureDescription description) {
                    return create_texture(description);
                  }) {}

  Renderer(const Renderer&) = delete;
  auto operator=(const Renderer&) -> Renderer& = delete;
  Renderer(Renderer&&) = delete;
  auto operator=(Renderer&&) -> Renderer& = delete;

  ~Renderer() {
    device_->wait_idle();
    publisher_->completed(GpuSerial{std::numeric_limits<std::uint64_t>::max()});
    registry_->retire();
    const std::scoped_lock lock{mutex_};
    drain();
    for (Grave& grave : graveyard_) {
      descriptors_.release(grave.storage.descriptor);
    }
    graveyard_.clear();
    for (const TextureStorage& storage : textures_.values()) {
      descriptors_.release(storage.descriptor);
    }
    textures_.clear();
  }

  auto device() const noexcept -> const VulkanDevice& { return *device_; }

  auto registry() const noexcept -> const std::shared_ptr<Registry>& {
    return registry_;
  }

  auto hooks() noexcept -> Hooks& { return hooks_; }

  auto surfaces() noexcept -> Surfaces& { return surfaces_; }

  auto publisher() const noexcept -> const std::shared_ptr<Publisher>& {
    return publisher_;
  }

  auto upcoming() noexcept -> GpuSerial {
    return GpuSerial{hooks_.schedule().serial().value() + 1};
  }

  auto last_damage() const -> std::optional<ClipRect> {
    const std::scoped_lock lock{mutex_};
    return last_damage_;
  }

  auto statistics() const -> Statistics {
    const std::scoped_lock lock{mutex_};
    Statistics current = statistics_;
    current.overlapping_queue_use = overlapped_.load(std::memory_order_relaxed);
    current.allocation_waits = contended_.load(std::memory_order_relaxed);
    return current;
  }

  auto timings() const -> GpuTimings {
    const std::scoped_lock lock{mutex_};
    GpuTimings measured = timings_;
    measured.missed_vsyncs = target_->missed_vsyncs();
    measured.encode_misses = registry_->encode_misses();
    return measured;
  }

  auto resources() const -> GpuResources {
    const std::scoped_lock lock{mutex_};
    return GpuResources{.textures_created = textures_created_,
                        .memory_blocks_created = device_->allocations()};
  }

  auto missed_vsyncs() const -> std::uint64_t {
    const std::scoped_lock lock{mutex_};
    return target_->missed_vsyncs();
  }

  auto completed(GpuSerial serial) -> bool {
    {
      const std::scoped_lock lock{mutex_};
      poll();
    }
    announce();
    return completed_.load(std::memory_order_acquire) >= serial.value();
  }

  void announce() {
    publisher_->completed(
        GpuSerial{completed_.load(std::memory_order_acquire)});
  }

  auto create_texture(TextureDescription description)
      -> std::expected<Texture, ProviderError> {
    if (!Texture::describable(description)) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument});
    }
    const bool renderable = description.usage == TextureUsage::render_target;
    const Placement placement =
        renderable ? Placement{.format = ShapePipelines::offscreen_format,
                               .swizzle = {}}
                   : placement_of(description.format);
    const std::unique_lock lock = stage();
    drain();
    auto image = VulkanImage::create(
        *device_,
        VulkanImage::Description{
            .width = description.width,
            .height = description.height,
            .format = placement.format,
            .usage =
                VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                (renderable
                     ? VkImageUsageFlags{VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT}
                     : VkImageUsageFlags{0}),
            .swizzle = placement.swizzle});
    if (!image.has_value()) {
      return std::unexpected(image.error());
    }
    VulkanFramebufferHandle framebuffer;
    if (renderable) {
      auto made = framebuffer_for(*image);
      if (!made.has_value()) {
        return std::unexpected(made.error());
      }
      framebuffer = std::move(*made);
    }
    auto lease = descriptors_.allocate(pipelines_.set_layout());
    if (!lease.has_value()) {
      return std::unexpected(lease.error());
    }
    describe(lease->set, image->view(), description.filter);
    FrameSlot& slot = staging_slot();
    if (auto began = begin_slot(slot); !began.has_value()) {
      descriptors_.release(*lease);
      return std::unexpected(began.error());
    }
    VkCommandBuffer const commands = slot.commands();
    barriers_.record(
        commands,
        Transition{
            .image = image->image(),
            .from = VK_IMAGE_LAYOUT_UNDEFINED,
            .to = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .before = Access{.stage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                             .access = 0},
            .after = Access{.stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                            .access = VK_ACCESS_2_TRANSFER_WRITE_BIT}});
    const std::uint32_t id = next_texture_++;
    const VkImage raw = image->image();
    const VkImageView view = image->view();
    textures_.insert_or_assign(
        id, TextureStorage{.image = std::move(*image),
                           .borrowed = {},
                           .raw = raw,
                           .view = view,
                           .width = description.width,
                           .height = description.height,
                           .framebuffer = std::move(framebuffer),
                           .descriptor = *lease,
                           .source = description.format,
                           .format = placement.format,
                           .sampled = false});
    written_.push_back(id);
    ++textures_created_;
    Texture texture = Registry::adopt(registry_, description, id);
    planner_.created(texture.id(), description);
    return texture;
  }

  auto update_texture(const Texture& texture, TextureRegion region,
                      PixelRows pixels) -> std::expected<void, ProviderError> {
    const auto record = registry_->find(texture.id());
    if (!texture.valid() || !record.has_value() ||
        !Texture::accepts(record->description, region, pixels)) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument});
    }
    const std::unique_lock lock = stage();
    drain();
    const auto found = textures_.find(record->native);
    if (found == textures_.end()) {
      return std::unexpected(ProviderError{ProviderErrorCode::not_found});
    }
    TextureStorage& storage = found->second;
    FrameSlot& slot = staging_slot();
    if (auto began = begin_slot(slot); !began.has_value()) {
      return std::unexpected(began.error());
    }
    const std::size_t source_row = static_cast<std::size_t>(region.width) *
                                   Texture::bytes_per_pixel(storage.source);
    const std::size_t target_row =
        static_cast<std::size_t>(region.width) * texel_bytes(storage.format);
    const std::size_t size = target_row * region.height;
    auto space = slot.staging().take(size, [this](VkDeviceSize bytes) {
      return VulkanBuffer::create(
          *device_, bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, MemoryUse::upload);
    });
    if (!space.has_value()) {
      return std::unexpected(space.error());
    }
    for (std::uint32_t line = 0; line < region.height; ++line) {
      convert(storage.source, storage.format, pixels.row(line, source_row),
              space->bytes.subspan(line * target_row, target_row));
    }
    VkCommandBuffer const commands = slot.commands();
    const Access writing{.stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                         .access = VK_ACCESS_2_TRANSFER_WRITE_BIT};
    if (storage.sampled) {
      barriers_.record(
          commands, Transition{.image = storage.raw,
                               .from = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                               .to = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               .before = sampling(),
                               .after = writing});
      storage.sampled = false;
      written_.push_back(record->native);
    } else {
      barriers_.record(commands,
                       Transition{.image = storage.raw,
                                  .from = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                  .to = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                  .before = writing,
                                  .after = writing});
    }
    if (planner_.upload(texture.id(), region).has_value()) {
      clear_image(commands, storage.raw);
      barriers_.record(commands,
                       Transition{.image = storage.raw,
                                  .from = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                  .to = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                  .before = writing,
                                  .after = writing});
    }
    const VkBufferImageCopy copy{
        .bufferOffset = space->offset,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource =
            VkImageSubresourceLayers{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                     .mipLevel = 0,
                                     .baseArrayLayer = 0,
                                     .layerCount = 1},
        .imageOffset = VkOffset3D{.x = static_cast<std::int32_t>(region.x),
                                  .y = static_cast<std::int32_t>(region.y),
                                  .z = 0},
        .imageExtent = VkExtent3D{
            .width = region.width, .height = region.height, .depth = 1}};
    vkCmdCopyBufferToImage(commands, space->chunk.get().handle(), storage.raw,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    statistics_.texture_bytes_uploaded += size;
    return {};
  }

  struct Opened {
    FrameSlots::Claim claim{};
    GpuSerial serial{0};
    RegistryTicket ticket{0};
    bool replay{false};
    bool overlay{false};
  };

  struct Resolved {
    VkDescriptorSet descriptor{VK_NULL_HANDLE};
    VkImage image{VK_NULL_HANDLE};
    VkImageView view{VK_NULL_HANDLE};
    VkFramebuffer framebuffer{VK_NULL_HANDLE};
    std::uint32_t width{0};
    std::uint32_t height{0};
    VkFormat format{VK_FORMAT_UNDEFINED};
    bool sampled{false};
  };

  using Resolution = std::flat_map<std::uint64_t, Resolved>;

  [[nodiscard]] auto open_frame() -> std::expected<Opened, ProviderError> {
    if (lost_.load(std::memory_order_acquire)) {
      return std::unexpected(ProviderError{ProviderErrorCode::platform_failure,
                                           ProviderErrorDetail::device_lost});
    }
    const std::optional<FrameSlots::Claim> claim = ring_.claim();
    if (!claim.has_value()) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::resource_exhausted});
    }
    auto opened = open_locked(claim->slot);
    announce();
    if (!opened.has_value()) {
      ring_.abandon();
      return std::unexpected(opened.error());
    }
    return Opened{.claim = *claim,
                  .serial = upcoming(),
                  .ticket = registry_->open(),
                  .replay = false};
  }

  [[nodiscard]] auto open_replay() -> std::expected<Opened, ProviderError> {
    if (lost_.load(std::memory_order_acquire)) {
      return std::unexpected(ProviderError{ProviderErrorCode::platform_failure,
                                           ProviderErrorDetail::device_lost});
    }
    if (!last_ui_slot_.has_value()) {
      return std::unexpected(ProviderError{ProviderErrorCode::not_found});
    }
    const std::optional<GpuSerial> serial = hooks_.schedule().next_serial_after(
        GpuSerial{submitted_.load(std::memory_order_acquire)});
    if (!serial.has_value()) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::resource_exhausted});
    }
    std::optional<VkFence> pending;
    {
      const std::scoped_lock lock{mutex_};
      if (replay_.pending()) {
        pending = replay_.fence();
      }
    }
    if (pending.has_value()) {
      VkFence const fence = *pending;
      static_cast<void>(
          vkWaitForFences(device_->handle(), 1, &fence, VK_TRUE,
                          std::numeric_limits<std::uint64_t>::max()));
    }
    {
      const std::scoped_lock lock{mutex_};
      drain();
      poll();
      if (auto began = begin_slot(replay_); !began.has_value()) {
        settle_submitted(*serial);
        return std::unexpected(began.error());
      }
      replay_.reset_geometry(*device_);
    }
    return Opened{.claim = {},
                  .serial = *serial,
                  .ticket = registry_->open(),
                  .replay = true};
  }

  void close_frame(const Opened& opened) noexcept {
    if (opened.replay) {
      const std::scoped_lock lock{mutex_};
      if (replay_.recording()) {
        static_cast<void>(replay_.submit(*device_, {}, VK_NULL_HANDLE,
                                         opened.serial.value()));
      }
      settle_submitted(opened.serial);
    } else {
      ring_.abandon();
    }
    registry_->close(opened.ticket);
  }

  [[nodiscard]] auto allocate_geometry(std::uint32_t vertices,
                                       std::uint32_t indices)
      -> std::expected<FrameGeometry, ProviderError> {
    return slots_[building_].geometry().allocate(*device_, vertices, indices);
  }

  [[nodiscard]] auto allocate_shapes(std::uint32_t vertices,
                                     std::uint32_t indices)
      -> std::expected<ShapeGeometry, ProviderError> {
    return slots_[building_].shapes().allocate(*device_, vertices, indices);
  }

  [[nodiscard]] auto allocate_resident(std::uint32_t vertices,
                                       std::uint32_t indices)
      -> std::expected<ResidentShapes, ProviderError> {
    ResidentBlock& block = slots_[building_].resident();
    if (auto reserved = block.reserve_indices(*device_, indices);
        !reserved.has_value()) {
      return std::unexpected(reserved.error());
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

  [[nodiscard]] auto replay_indices(std::span<const Index> indices)
      -> std::expected<void, ProviderError> {
    ResidentBlock& block = replay_.resident();
    const auto count = static_cast<std::uint32_t>(indices.size());
    if (auto reserved = block.reserve_indices(*device_, count);
        !reserved.has_value()) {
      return std::unexpected(reserved.error());
    }
    std::ranges::copy(indices, block.indices(count).begin());
    return {};
  }

  void touch(VertexRange range) {
    for (FrameSlot& slot : slots_) {
      slot.resident().stale(range);
    }
  }

  auto owns(const FrameGeometry& geometry) const -> bool {
    return slots_[building_].geometry().owns(geometry);
  }

  auto owns(const ShapeGeometry& geometry) const -> bool {
    if (geometry.block == ResidentShapes::block) {
      return geometry.vertices.data() == mirror_.data();
    }
    return slots_[building_].shapes().owns(geometry);
  }

  struct Recording {
    std::optional<Colour> clear;
    std::span<const BlockCommand> commands;
    std::span<const OffscreenPass> passes;
    std::span<const BlockCommand> pass_commands;
    std::span<const Surfaces::Hole> holes;
    std::span<const Surfaces::Hole> pass_holes;
    std::span<const Surfaces::Due> due;
    std::span<const FrameSlot::Wait> ready;
    std::optional<ClipRect> damage;
    std::optional<std::uint32_t> resident_indices;
    std::reference_wrapper<const Resolution> resolution;
    RegistryTicket ticket{0};
    bool replay{false};
    SurfaceSize extent{};
  };

  struct Sealing {
    GpuSerial serial{0};
    std::vector<Surfaces::Hole> holes;
    std::vector<Surfaces::Due> due;
  };

  struct Presented {
    std::optional<std::chrono::nanoseconds> displayed;
    std::optional<Image> image;
  };

  auto seal(std::span<const BlockCommand> commands,
            std::span<const BlockCommand> pass_commands,
            std::span<const OffscreenPass> passes,
            std::vector<FrameSlot::Wait>& ready, Resolution& resolution)
      -> Sealing {
    const std::unique_lock lock = hold();
    settle(slots_[building_].commands());
    gather_ready(ready);
    const GpuSerial serial = hooks_.schedule().next_serial();
    Sealing sealing = plan(serial, commands, pass_commands);
    resolve(resolution, commands, pass_commands, passes, sealing.due);
    planner_.seal();
    ring_.seal();
    return sealing;
  }

  auto seal_replay(GpuSerial serial, std::span<const BlockCommand> commands,
                   std::vector<FrameSlot::Wait>& ready, Resolution& resolution)
      -> Sealing {
    const std::scoped_lock lock{mutex_};
    gather_ready(ready);
    Sealing sealing = plan(serial, commands, {});
    resolve(resolution, commands, {}, {}, sealing.due);
    return sealing;
  }

  auto pass_holes(std::span<const BlockCommand> pass_commands,
                  std::span<OffscreenPass> passes) const
      -> std::vector<Surfaces::Hole> {
    std::vector<Surfaces::Hole> found;
    for (OffscreenPass& each : passes) {
      each.holes_first = found.size();
      each.holes_count = 0;
      if (!surfaces_.installed() ||
          each.first + each.count > pass_commands.size()) {
        continue;
      }
      const std::vector<Surfaces::Hole> holes =
          surfaces_.holes(pass_commands.subspan(each.first, each.count),
                          &BlockCommand::command);
      found.insert(found.end(), holes.begin(), holes.end());
      each.holes_count = holes.size();
    }
    return found;
  }

  [[nodiscard]] auto open_overlay_frame(FrameSlot& slot, std::uint32_t index)
      -> std::expected<Opened, ProviderError> {
    if (lost_.load(std::memory_order_acquire)) {
      return std::unexpected(ProviderError{ProviderErrorCode::platform_failure,
                                           ProviderErrorDetail::device_lost});
    }
    {
      const std::scoped_lock submitting{submit_mutex_};
      const support::ScopedCallback queue = use_queue();
      const std::unique_lock lock = hold();
      drain();
      poll();
      if (auto flushed = flush_staging(); !flushed.has_value()) {
        return std::unexpected(flushed.error());
      }
      if (auto began = begin_slot(slot); !began.has_value()) {
        return std::unexpected(began.error());
      }
      slot.reset_geometry(*device_);
      staging_overlay_ = std::ref(slot);
    }
    announce();
    return Opened{.claim = FrameSlots::Claim{.slot = index, .frame = 0},
                  .serial = upcoming(),
                  .ticket = registry_->open(),
                  .replay = false,
                  .overlay = true};
  }

  auto seal_overlay(FrameSlot& slot, std::span<const BlockCommand> commands,
                    std::span<const BlockCommand> pass_commands,
                    std::span<const OffscreenPass> passes,
                    std::vector<FrameSlot::Wait>& ready, Resolution& resolution)
      -> Sealing {
    const std::unique_lock lock = hold();
    settle(slot.commands());
    gather_ready(ready);
    const GpuSerial serial = hooks_.schedule().next_serial();
    Sealing sealing = plan(serial, commands, pass_commands);
    resolve(resolution, commands, pass_commands, passes, sealing.due);
    planner_.seal();
    staging_overlay_.reset();
    return sealing;
  }

  [[nodiscard]] auto flush_staging() -> std::expected<void, ProviderError> {
    FrameSlot& staged = slots_[staging_];
    if (!staged.recording()) {
      return {};
    }
    return staged.submit(*device_, {}, VK_NULL_HANDLE,
                         completed_.load(std::memory_order_acquire));
  }

  void flush_overlay_slot(FrameSlot& slot, GpuSerial serial) noexcept {
    const std::scoped_lock submitting{submit_mutex_};
    const support::ScopedCallback queue = use_queue();
    const std::unique_lock lock = hold();
    staging_overlay_.reset();
    if (!slot.recording()) {
      return;
    }
    settle(slot.commands());
    clear_owed(slot.commands());
    static_cast<void>(
        slot.submit(*device_, {}, VK_NULL_HANDLE, serial.value()));
  }

  [[nodiscard]] static auto open_overlay(const std::shared_ptr<Renderer>& self)
      -> std::expected<std::shared_ptr<OverlayState>, ProviderError>;

  void close_overlay(const std::shared_ptr<OverlayState>& state) noexcept;

  [[nodiscard]] auto publish_overlay(OverlayState& state, FrameSlot& slot,
                                     const Recording& recording,
                                     GpuSerial serial)
      -> std::expected<void, ProviderError> {
    const std::scoped_lock submitting{submit_mutex_};
    const SurfaceSize extent = recording.extent;
    const bool live = overlay_.get() == &state && !closed(state);
    std::optional<TimestampQueries> unmeasured;
    {
      const std::unique_lock lock = hold();
      clear_owed(slot.commands());
    }
    if (live && (!recording.passes.empty() || !recording.due.empty())) {
      std::span<const gpu::FramePlanner::Step> steps;
      {
        const std::scoped_lock lock{mutex_};
        steps = overlay_planner_.plan(recorded(recording, 0, 1, extent, true));
      }
      Execution run{
          .buffer = slot.commands(),
          .slot = slot,
          .resident = ResidentBuffers{},
          .recording = recording,
          .clock = unmeasured,
          .image = VulkanTarget::Image{},
          .screen = ClipRect{.x = 0,
                             .y = 0,
                             .width = extent.width.value(),
                             .height = extent.height.value()},
          .target = FrameTarget{},
          .info = FrameInfo{.serial = serial,
                            .predicted_present = std::chrono::nanoseconds{0},
                            .extent = extent,
                            .replay = false,
                            .damage = std::nullopt},
          .plan = FramePlan{},
          .hooked = false,
          .drawn = 0,
          .framebuffer = VK_NULL_HANDLE,
          .prelude = true,
          .screen_pass = false};
      for (const gpu::FramePlanner::Step& step : steps) {
        std::visit([this, &run](const auto& each) { execute(each, run); },
                   step);
      }
      recorded_.offscreen_passes += run.drawn;
    }
    const support::ScopedCallback queue = use_queue();
    const std::scoped_lock lock{mutex_};
    auto submitted =
        slot.submit(*device_, recording.ready, VK_NULL_HANDLE, serial.value());
    if (submitted.has_value() && !live) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument,
                        ProviderErrorDetail::not_taken_over});
    }
    return submitted;
  }

  static auto closed(const OverlayState& state) -> bool;

  [[nodiscard]] auto record_overlay(OverlayState& state, VkCommandBuffer buffer,
                                    const FrameTarget& target)
      -> std::expected<std::optional<OverlayFence>, ProviderError>;

  auto render(const FramePacket& packet, const Recording& recording,
              FramePacket::Clock clock)
      -> std::expected<FrameReceipt, ProviderError> {
    const FrameStopwatch stopwatch{clock};
    const std::chrono::nanoseconds began = stopwatch.now();
    std::chrono::nanoseconds waited{0};
    auto rendered = render_locked(packet, recording, stopwatch, waited);
    if (!rendered.has_value() &&
        rendered.error().detail() == ProviderErrorDetail::device_lost) {
      lost_.store(true, std::memory_order_release);
    }
    announce();
    registry_->close(recording.ticket);
    settle_submitted(packet.serial());
    if (!rendered.has_value()) {
      return std::unexpected(rendered.error());
    }
    const std::chrono::nanoseconds total = stopwatch.since(began);
    return FrameReceipt{
        .record = std::max(total - waited, std::chrono::nanoseconds{0}),
        .wait = waited,
        .displayed = rendered->displayed,
        .image = std::move(rendered->image)};
  }

  void abandon(const FramePacket& packet, const Opened& opened) noexcept {
    const std::scoped_lock submitting{submit_mutex_};
    if (opened.replay) {
      close_frame(opened);
      return;
    }
    {
      const std::scoped_lock lock{mutex_};
      FrameSlot& slot = slots_[packet.slot()];
      if (slot.recording()) {
        clear_owed(slot.commands());
        static_cast<void>(
            slot.submit(*device_, {}, VK_NULL_HANDLE, packet.serial().value()));
      }
    }
    std::chrono::nanoseconds ignored{0};
    advance(packet, FrameStopwatch{std::nullopt}, ignored);
    settle_submitted(packet.serial());
    registry_->close(opened.ticket);
  }

  auto import_view(VkImage image, ImportDescription description,
                   VkImageLayout layout, support::ScopedCallback released)
      -> std::expected<Imported, ProviderError> {
    const TextureDescription texture = description.texture();
    if (image == VK_NULL_HANDLE || description.planes() != 1 ||
        !Texture::describable(texture) || !importable(layout)) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument});
    }
    auto installed = install(Borrowed{.image = image,
                                      .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
                                      .layout = layout},
                             texture);
    if (!installed.has_value()) {
      return std::unexpected(installed.error());
    }
    return Imported::adopt(publisher_,
                           Registry::adopt(registry_, texture, *installed),
                           description, std::move(released));
  }

  auto import_planes(std::span<const VkImage> planes,
                     ImportDescription description, VkImageLayout layout,
                     support::ScopedCallback released)
      -> std::expected<Imported, ProviderError> {
    const bool multi_planar =
        description.format == TextureFormat::nv12 && planes.size() == 1;
    if ((planes.size() != description.planes() && !multi_planar) ||
        !importable(layout) || std::ranges::any_of(planes, [](VkImage plane) {
          return plane == VK_NULL_HANDLE;
        })) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument});
    }
    for (std::size_t index = 0; index < description.planes(); ++index) {
      if (!Texture::describable(description.plane(index))) {
        return std::unexpected(
            ProviderError{ProviderErrorCode::invalid_argument});
      }
    }
    std::vector<Texture> textures;
    textures.reserve(description.planes());
    for (std::size_t index = 0; index < description.planes(); ++index) {
      const TextureDescription plane = description.plane(index);
      const Borrowed borrowed{
          .image = multi_planar ? planes.front() : planes[index],
          .aspect =
              !multi_planar ? VkImageAspectFlags{VK_IMAGE_ASPECT_COLOR_BIT}
              : index == 0  ? VkImageAspectFlags{VK_IMAGE_ASPECT_PLANE_0_BIT}
                            : VkImageAspectFlags{VK_IMAGE_ASPECT_PLANE_1_BIT},
          .layout = layout};
      auto installed = install(borrowed, plane);
      if (!installed.has_value()) {
        return std::unexpected(installed.error());
      }
      textures.push_back(Registry::adopt(registry_, plane, *installed));
    }
    return Imported::adopt(publisher_, std::move(textures), description,
                           std::move(released));
  }

  auto capture() -> std::expected<Image, ProviderError> {
    if (queue_.parked()) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::resource_exhausted,
                        ProviderErrorDetail::taken_over});
    }
    const std::optional<RenderQueue::Lease> used = queue_.submit();
    if (!used.has_value()) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::resource_exhausted,
                        ProviderErrorDetail::taken_over});
    }
    const std::scoped_lock submitting{submit_mutex_};
    return read_back();
  }

  auto queue() noexcept -> RenderQueue& { return queue_; }

  static auto take_over(const std::shared_ptr<Renderer>& self)
      -> std::expected<GpuTakeover, ProviderError> {
    return self->takeover_.claim(
        self->queue_, self->hooks_.schedule().weak_from_this(), [self]() {
          const std::uint32_t restored = self->target_->interval();
          self->lent_.lend(self->lent_images());
          return support::ScopedCallback{
              [self, restored]() noexcept { self->hand_back(restored); }};
        });
  }

  void hand_back(std::uint32_t restored) noexcept {
    const LentSwapchain<VkSemaphore>::Settlement settled = lent_.settle();
    for (const LentSwapchain<VkSemaphore>::Outstanding& each :
         settled.outstanding) {
      settle_lent(each);
    }
    static_cast<void>(device_->with_queue(
        [](VkQueue queue) { return vkQueueWaitIdle(queue); }));
    {
      const std::scoped_lock submitting{submit_mutex_};
      drop_overlay();
    }
    target_->set_interval(restored);
    if (!settled.outstanding.empty()) {
      target_->invalidate();
    }
    const std::scoped_lock lock{mutex_};
    statistics_.present_interval = target_->interval();
    planner_.invalidate();
  }

  void settle_lent(
      const LentSwapchain<VkSemaphore>::Outstanding& outstanding) noexcept {
    VkSemaphore const finished = target_->finished(outstanding.index);
    device_->with_queue([&](VkQueue queue_handle) {
      if (finished != VK_NULL_HANDLE) {
        const VkPipelineStageFlags stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        const bool waits = outstanding.payload != VK_NULL_HANDLE;
        const VkSubmitInfo info{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = nullptr,
            .waitSemaphoreCount = waits ? 1U : 0U,
            .pWaitSemaphores = waits ? &outstanding.payload : nullptr,
            .pWaitDstStageMask = waits ? &stage : nullptr,
            .commandBufferCount = 0,
            .pCommandBuffers = nullptr,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &finished};
        if (vkQueueSubmit(queue_handle, 1, &info, VK_NULL_HANDLE) !=
            VK_SUCCESS) {
          return;
        }
      }
      static_cast<void>(target_->present(queue_handle, outstanding.index));
    });
  }

  auto takeover_counters() const -> GpuTakeover::Counters {
    return takeover_.counters();
  }

  auto lent_images() const noexcept -> std::uint32_t {
    return static_cast<std::uint32_t>(target_->views().size());
  }

  auto lent_target(std::uint32_t index, VkImageLayout layout,
                   VkImage image) const -> FrameTarget {
    const SurfaceSize extent = target_->extent();
    return FrameTarget{.image = image,
                       .view = index < target_->views().size()
                                   ? target_->views()[index]
                                   : VK_NULL_HANDLE,
                       .format = target_->format(),
                       .extent = VkExtent2D{.width = extent.width.value(),
                                            .height = extent.height.value()},
                       .layout = layout,
                       .index = index,
                       .generation = target_->generation()};
  }

  auto lent_acquire(VkSemaphore acquired)
      -> std::expected<LentImage, ProviderError> {
    if (lent_.outstanding() == 0) {
      if (auto ready = target_->prepare({}); !ready.has_value()) {
        return std::unexpected(ready.error());
      }
      static_cast<void>(lent_.resize(lent_images()));
    }
    auto taken = target_->acquire(acquired);
    if (!taken.has_value()) {
      return std::unexpected(taken.error());
    }
    if (!taken->has_value()) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::not_found,
                        ProviderErrorDetail::image_unavailable});
    }
    const VulkanTarget::Image image = **taken;
    if (!lent_.acquire(image.index,
                       image.signals ? acquired : VK_NULL_HANDLE)) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::resource_exhausted,
                        ProviderErrorDetail::image_unavailable});
    }
    return LentImage{
        .target = lent_target(image.index, image.layout, image.image),
        .waits_on_acquire = image.signals,
        .finished = target_->finished(image.index),
        .present_layout = target_->presented_layout(),
        .usage = target_->usage()};
  }

  auto lent_present(std::uint32_t index) -> std::expected<void, ProviderError> {
    if (!lent_.present(index)) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument});
    }
    auto shown = device_->with_queue([this, index](VkQueue queue_handle) {
      return target_->present(queue_handle, index);
    });
    if (shown.has_value()) {
      lent_presents_.fetch_add(1, std::memory_order_relaxed);
    }
    return shown;
  }

  void lent_interval(SwapInterval interval) {
    target_->set_interval(interval.value());
  }

  auto lent_presents() const noexcept -> std::uint64_t {
    return lent_presents_.load(std::memory_order_relaxed);
  }

  void settle_submitted(GpuSerial serial) noexcept {
    std::uint64_t seen = submitted_.load(std::memory_order_relaxed);
    while (serial.value() > seen &&
           !submitted_.compare_exchange_weak(seen, serial.value(),
                                             std::memory_order_acq_rel,
                                             std::memory_order_relaxed)) {
    }
  }

 private:
  auto read_back() -> std::expected<Image, ProviderError> {
    const std::optional<VulkanTarget::Image> readable = target_->readable();
    if (!readable.has_value()) {
      bool presented = false;
      {
        const std::scoped_lock lock{mutex_};
        presented = statistics_.frames_presented != 0;
      }
      return std::unexpected(presented
                                 ? ProviderError::unsupported_capability()
                                 : ProviderError{ProviderErrorCode::not_found});
    }
    const SurfaceSize extent = target_->extent();
    const std::uint32_t width = extent.width.value();
    const std::uint32_t height = extent.height.value();
    const std::size_t size =
        static_cast<std::size_t>(width) * height * Image::channels;
    if (auto ready = ensure_capture(size); !ready.has_value()) {
      return std::unexpected(ready.error());
    }
    Capture& capture = *capture_;
    VkCommandBuffer const commands = capture.commands;
    const VkCommandBufferBeginInfo begin{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr};
    if (const VkResult result = vkBeginCommandBuffer(commands, &begin);
        result != VK_SUCCESS) {
      return std::unexpected(VulkanStatus::error(result));
    }
    const VkBufferImageCopy copy{
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource =
            VkImageSubresourceLayers{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                     .mipLevel = 0,
                                     .baseArrayLayer = 0,
                                     .layerCount = 1},
        .imageOffset = VkOffset3D{.x = 0, .y = 0, .z = 0},
        .imageExtent =
            VkExtent3D{.width = width, .height = height, .depth = 1}};
    const support::ScopedCallback queue = use_queue();
    vkCmdCopyImageToBuffer(commands, readable->image, readable->layout,
                           capture.buffer->handle(), 1, &copy);
    const VkBufferMemoryBarrier visible{
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer = capture.buffer->handle(),
        .offset = 0,
        .size = VK_WHOLE_SIZE};
    vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1, &visible,
                         0, nullptr);
    if (const VkResult result = vkEndCommandBuffer(commands);
        result != VK_SUCCESS) {
      return std::unexpected(VulkanStatus::error(result));
    }
    VkFence const fence = capture.fence.get().object;
    const VkSubmitInfo submit{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                              .pNext = nullptr,
                              .waitSemaphoreCount = 0,
                              .pWaitSemaphores = nullptr,
                              .pWaitDstStageMask = nullptr,
                              .commandBufferCount = 1,
                              .pCommandBuffers = &commands,
                              .signalSemaphoreCount = 0,
                              .pSignalSemaphores = nullptr};
    VkDevice const handle = device_->handle();
    if (auto submitted = device_->submit(std::span{&submit, 1}, fence);
        !submitted.has_value()) {
      return std::unexpected(submitted.error());
    }
    const VkResult waited = vkWaitForFences(
        handle, 1, &fence, VK_TRUE, std::numeric_limits<std::uint64_t>::max());
    static_cast<void>(vkResetFences(handle, 1, &fence));
    static_cast<void>(vkResetCommandPool(handle, capture.pool.get().object, 0));
    if (waited != VK_SUCCESS) {
      return std::unexpected(VulkanStatus::error(waited));
    }
    capture.buffer->invalidate();
    std::vector<std::byte> pixels(size);
    std::ranges::copy(capture.buffer->bytes().first(size), pixels.begin());
    if (target_->format() == VK_FORMAT_B8G8R8A8_UNORM) {
      for (std::size_t at = 0; at + 3 < pixels.size(); at += 4) {
        std::swap(pixels[at], pixels[at + 2]);
      }
    }
    return Image::create(width, height, std::move(pixels));
  }

  struct Placement {
    VkFormat format{VK_FORMAT_R8G8B8A8_UNORM};
    VkComponentMapping swizzle{};
  };

  struct Borrowed {
    VkImage image{VK_NULL_HANDLE};
    VkImageAspectFlags aspect{VK_IMAGE_ASPECT_COLOR_BIT};
    VkImageLayout layout{import_layout};
  };

  struct TextureStorage {
    std::optional<VulkanImage> image;
    VulkanImageViewHandle borrowed;
    VkImage raw{VK_NULL_HANDLE};
    VkImageView view{VK_NULL_HANDLE};
    std::uint32_t width{0};
    std::uint32_t height{0};
    VulkanFramebufferHandle framebuffer;
    DescriptorPools::Lease descriptor;
    TextureFormat source{TextureFormat::rgba8};
    VkFormat format{VK_FORMAT_R8G8B8A8_UNORM};
    bool sampled{false};
  };

  struct Grave {
    std::uint64_t serial{0};
    TextureStorage storage;
  };

  struct Capture {
    VulkanCommandPoolHandle pool;
    VkCommandBuffer commands{VK_NULL_HANDLE};
    VulkanFenceHandle fence;
    std::optional<VulkanBuffer> buffer;
  };

  struct Surface {
    std::uint32_t width{0};
    std::uint32_t height{0};
    PassRegion region{};
    Destination destination{Destination::screen};
  };

  [[nodiscard]] auto open_locked(std::uint32_t claimed)
      -> std::expected<void, ProviderError> {
    const std::unique_lock lock = hold();
    drain();
    poll();
    FrameSlot& slot = slots_[claimed];
    if (auto began = begin_slot(slot); !began.has_value()) {
      return std::unexpected(began.error());
    }
    slot.reset_geometry(*device_);
    building_ = claimed;
    staging_ = claimed;
    return {};
  }

  auto hold() -> std::unique_lock<std::mutex> {
    std::unique_lock lock{mutex_, std::try_to_lock};
    if (!lock.owns_lock()) {
      contended_.fetch_add(1, std::memory_order_relaxed);
      lock.lock();
    }
    return lock;
  }

  auto stage() -> std::unique_lock<std::mutex> {
    while (true) {
      {
        std::unique_lock lock = hold();
        if (staging_overlay_.has_value()) {
          return lock;
        }
      }
      const std::optional<std::uint32_t> next = ring_.await();
      std::unique_lock lock = hold();
      if (staging_overlay_.has_value()) {
        return lock;
      }
      if (next.has_value() && ring_.next().slot == *next) {
        staging_ = *next;
        return lock;
      }
    }
  }

  auto staging_slot() noexcept -> FrameSlot& {
    return staging_overlay_.has_value() ? staging_overlay_->get()
                                        : slots_[staging_];
  }

  void await_fence(FrameSlot& slot, const FrameStopwatch& stopwatch,
                   std::chrono::nanoseconds& waited) {
    std::optional<VkFence> pending;
    {
      const std::scoped_lock lock{mutex_};
      if (slot.pending()) {
        pending = slot.fence();
      }
    }
    if (!pending.has_value()) {
      return;
    }
    VkFence const fence = *pending;
    const std::chrono::nanoseconds fenced = stopwatch.now();
    static_cast<void>(
        vkWaitForFences(device_->handle(), 1, &fence, VK_TRUE,
                        std::numeric_limits<std::uint64_t>::max()));
    waited += stopwatch.since(fenced);
  }

  void advance(const FramePacket& packet, const FrameStopwatch& stopwatch,
               std::chrono::nanoseconds& waited) {
    FrameSlot& next = slots_[(packet.slot() + 1) % slots_.size()];
    await_fence(next, stopwatch, waited);
    {
      const std::scoped_lock lock{mutex_};
      static_cast<void>(begin_slot(next));
    }
    ring_.release(packet.frame());
  }

  void spare_slot(std::uint32_t index, const FrameStopwatch& stopwatch,
                  std::chrono::nanoseconds& waited) {
    if (replay_reads_ != index) {
      return;
    }
    await_fence(replay_, stopwatch, waited);
    replay_reads_.reset();
  }

  void gather_ready(std::vector<FrameSlot::Wait>& ready) {
    ready.clear();
    publisher_->drain([&ready](const ReadySemaphore& fence) {
      if (!fence.waits()) {
        return;
      }
      if (fence.timeline()) {
        const auto found =
            std::ranges::find_if(ready, [&fence](const FrameSlot::Wait& wait) {
              return wait.semaphore == fence.semaphore &&
                     wait.value.has_value();
            });
        if (found != ready.end()) {
          found->value = std::max(*found->value, *fence.value);
          return;
        }
      }
      ready.push_back(
          FrameSlot::Wait{.semaphore = fence.semaphore,
                          .stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                          .value = fence.value});
    });
  }

  auto plan(GpuSerial serial, std::span<const BlockCommand> commands,
            std::span<const BlockCommand> pass_commands) -> Sealing {
    publisher_->sampled(serial, commands, &BlockCommand::command);
    publisher_->sampled(serial, pass_commands, &BlockCommand::command);
    Sealing sealing{.serial = serial, .holes = {}, .due = {}};
    if (surfaces_.installed()) {
      sealing.holes = surfaces_.holes(commands, &BlockCommand::command);
      sealing.due = surfaces_.due(commands, serial, &BlockCommand::command);
      for (const Surfaces::Due& each :
           surfaces_.due(pass_commands, serial, &BlockCommand::command)) {
        sealing.due.push_back(each);
      }
    }
    return sealing;
  }

  void resolve(Resolution& into, std::span<const BlockCommand> commands,
               std::span<const BlockCommand> pass_commands,
               std::span<const OffscreenPass> passes,
               std::span<const Surfaces::Due> due) {
    into.clear();
    for (const BlockCommand& each : commands) {
      resolve(into, each.command.texture, EncodeStage::sampled_texture);
      resolve(into, each.command.detail, EncodeStage::sampled_texture);
    }
    for (const BlockCommand& each : pass_commands) {
      resolve(into, each.command.texture, EncodeStage::sampled_texture);
      resolve(into, each.command.detail, EncodeStage::sampled_texture);
    }
    for (const OffscreenPass& each : passes) {
      resolve(into, each.pass.target, EncodeStage::pass_target);
    }
    for (const Surfaces::Due& each : due) {
      resolve(into, each.texture, EncodeStage::pass_target);
    }
  }

  void resolve(Resolution& into, TextureId id, EncodeStage stage) {
    if (id.value() == 0 || into.contains(id.value())) {
      return;
    }
    const auto record = registry_->encoded(id, stage);
    if (!record.has_value()) {
      return;
    }
    const auto found = textures_.find(record->native);
    if (found == textures_.end()) {
      return;
    }
    const TextureStorage& storage = found->second;
    into.emplace(id.value(),
                 Resolved{.descriptor = storage.descriptor.set,
                          .image = storage.raw,
                          .view = storage.view,
                          .framebuffer = storage.framebuffer.valid()
                                             ? storage.framebuffer.get().object
                                             : VK_NULL_HANDLE,
                          .width = storage.width,
                          .height = storage.height,
                          .format = storage.format,
                          .sampled = storage.sampled});
  }

  static auto lookup(const Resolution& resolution, TextureId id)
      -> std::optional<Resolved> {
    const auto found = resolution.find(id.value());
    if (found == resolution.end() || !found->second.sampled) {
      return std::nullopt;
    }
    return found->second;
  }

  static constexpr auto importable(VkImageLayout layout) noexcept -> bool {
    return layout == import_layout || layout == host_import_layout;
  }

  void describe(VkDescriptorSet set, VkImageView view, SamplerFilter filter,
                VkImageLayout layout = import_layout) const noexcept {
    const VkDescriptorImageInfo described{.sampler = pipelines_.sampler(filter),
                                          .imageView = view,
                                          .imageLayout = layout};
    const VkWriteDescriptorSet write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = set,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &described,
        .pBufferInfo = nullptr,
        .pTexelBufferView = nullptr};
    vkUpdateDescriptorSets(device_->handle(), 1, &write, 0, nullptr);
  }

  auto imported_placement(TextureFormat format) const noexcept
      -> std::optional<Placement> {
    const VulkanCapabilities& capabilities = device_->capabilities();
    switch (format) {
      case TextureFormat::rgba8:
        return Placement{.format = VK_FORMAT_R8G8B8A8_UNORM, .swizzle = {}};
      case TextureFormat::bgra8:
        return Placement{.format = VK_FORMAT_B8G8R8A8_UNORM, .swizzle = {}};
      case TextureFormat::rgb565:
        if (!capabilities.rgb565) {
          return std::nullopt;
        }
        return Placement{.format = VK_FORMAT_R5G6B5_UNORM_PACK16,
                         .swizzle = {}};
      case TextureFormat::r8:
        return Placement{.format = VK_FORMAT_R8_UNORM, .swizzle = {}};
      case TextureFormat::rg8:
        return Placement{.format = VK_FORMAT_R8G8_UNORM, .swizzle = {}};
      case TextureFormat::alpha8:
        if (!capabilities.swizzle) {
          return std::nullopt;
        }
        return placement_of(format);
      case TextureFormat::nv12:
        return std::nullopt;
    }
    return std::nullopt;
  }

  [[nodiscard]] auto install(Borrowed borrowed,
                             const TextureDescription& description)
      -> std::expected<std::uint32_t, ProviderError> {
    const std::optional<Placement> placement =
        imported_placement(description.format);
    if (!placement.has_value()) {
      return std::unexpected(ProviderError::unsupported_capability());
    }
    VkDevice const handle = device_->handle();
    const VkImageViewCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = borrowed.image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = placement->format,
        .components = placement->swizzle,
        .subresourceRange =
            VkImageSubresourceRange{.aspectMask = borrowed.aspect,
                                    .baseMipLevel = 0,
                                    .levelCount = 1,
                                    .baseArrayLayer = 0,
                                    .layerCount = 1}};
    VkImageView raw = VK_NULL_HANDLE;
    if (const VkResult result = vkCreateImageView(handle, &info, nullptr, &raw);
        result != VK_SUCCESS) {
      return std::unexpected(VulkanStatus::error(result));
    }
    VulkanImageViewHandle view{{.device = handle, .object = raw}};
    const std::scoped_lock lock{mutex_};
    drain();
    auto lease = descriptors_.allocate(pipelines_.set_layout());
    if (!lease.has_value()) {
      return std::unexpected(lease.error());
    }
    describe(lease->set, raw, description.filter, borrowed.layout);
    const std::uint32_t id = next_texture_++;
    textures_.insert_or_assign(id, TextureStorage{.image = std::nullopt,
                                                  .borrowed = std::move(view),
                                                  .raw = borrowed.image,
                                                  .view = raw,
                                                  .width = description.width,
                                                  .height = description.height,
                                                  .framebuffer = {},
                                                  .descriptor = *lease,
                                                  .source = description.format,
                                                  .format = placement->format,
                                                  .sampled = true});
    ++statistics_.imports;
    return id;
  }

  void begin_screen(VkCommandBuffer buffer, VkFramebuffer framebuffer,
                    const ClipRect& screen) const noexcept {
    const VkRenderPassBeginInfo pass{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = pipelines_.render_pass(),
        .framebuffer = framebuffer,
        .renderArea = rectangle_of(screen),
        .clearValueCount = 0,
        .pClearValues = nullptr};
    vkCmdBeginRenderPass(buffer, &pass, VK_SUBPASS_CONTENTS_INLINE);
  }

  void lend(VkCommandBuffer buffer, VkImage image) {
    render_barriers_.record(
        buffer,
        Transition{
            .image = image,
            .from = hook_layout,
            .to = hook_layout,
            .before =
                Access{.stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                       .access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT},
            .after = Access{.stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                            .access = VK_ACCESS_2_MEMORY_READ_BIT |
                                      VK_ACCESS_2_MEMORY_WRITE_BIT}});
  }

  void reclaim_target(VkCommandBuffer buffer, VkImage image) {
    render_barriers_.record(
        buffer,
        Transition{
            .image = image,
            .from = hook_layout,
            .to = hook_layout,
            .before = Access{.stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                             .access = VK_ACCESS_2_MEMORY_WRITE_BIT},
            .after = attachment()});
  }

  void gather_waits(const FrameSlot& slot, const VulkanTarget::Image& image,
                    std::span<const FrameSlot::Wait> ready) {
    waits_.clear();
    if (image.signals) {
      waits_.push_back(FrameSlot::Wait{
          .semaphore = slot.acquired(),
          .stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
          .value = std::nullopt});
    }
    waits_.insert(waits_.end(), ready.begin(), ready.end());
    recorded_.semaphore_waits += ready.size();
  }

  struct ResidentBuffers {
    VkBuffer vertices{VK_NULL_HANDLE};
    VkBuffer indices{VK_NULL_HANDLE};
  };

  struct Execution {
    VkCommandBuffer buffer{VK_NULL_HANDLE};
    const FrameSlot& slot;
    ResidentBuffers resident;
    const Recording& recording;
    std::optional<TimestampQueries>& clock;
    VulkanTarget::Image image;
    ClipRect screen;
    FrameTarget target;
    FrameInfo info;
    FramePlan plan;
    bool hooked{false};
    std::uint32_t drawn{0};
    VkFramebuffer framebuffer{VK_NULL_HANDLE};
    bool prelude{true};
    bool screen_pass{true};
  };

  auto render_locked(const FramePacket& packet, const Recording& recording,
                     const FrameStopwatch& stopwatch,
                     std::chrono::nanoseconds& waited)
      -> std::expected<Presented, ProviderError> {
    const std::optional<ClipRect> damage = recording.damage;
    const FramePlan plan = packet.plan();
    const GpuSerial serial = packet.serial();
    const bool replaying = recording.replay;
    const std::optional<RenderQueue::Lease> used = queue_.submit();
    const std::scoped_lock submitting{submit_mutex_};
    if (!used.has_value()) {
      if (!replaying) {
        advance(packet, stopwatch, waited);
      }
      return std::unexpected(
          ProviderError{ProviderErrorCode::resource_exhausted,
                        ProviderErrorDetail::taken_over});
    }
    FrameSlot& slot = replaying ? replay_ : slots_[packet.slot()];
    {
      const std::scoped_lock lock{mutex_};
      drain();
      if (auto began = begin_slot(slot); !began.has_value()) {
        if (!replaying) {
          advance(packet, stopwatch, waited);
        }
        return std::unexpected(began.error());
      }
    }
    ResidentBuffers resident{};
    if (replaying) {
      if (!last_ui_slot_.has_value()) {
        static_cast<void>(submit_slot(slot, {}, VK_NULL_HANDLE, serial));
        return std::unexpected(ProviderError{ProviderErrorCode::not_found});
      }
      replay_.resident().flush_indices(recording.resident_indices.value_or(0));
      resident = ResidentBuffers{
          .vertices = slots_[*last_ui_slot_].resident().vertex_buffer(),
          .indices = replay_.resident().index_buffer()};
      replay_reads_ = last_ui_slot_;
    } else {
      if (recording.resident_indices.has_value()) {
        spare_slot(packet.slot(), stopwatch, waited);
        auto copied = slot.resident().refresh(*device_, mirror_,
                                              *recording.resident_indices);
        if (!copied.has_value()) {
          invalidate();
          advance(packet, stopwatch, waited);
          return std::unexpected(copied.error());
        }
        recorded_.vertex_bytes_copied += *copied;
      }
      resident = ResidentBuffers{.vertices = slot.resident().vertex_buffer(),
                                 .indices = slot.resident().index_buffer()};
      advance(packet, stopwatch, waited);
    }
    const auto skip = [&]() -> std::expected<Presented, ProviderError> {
      {
        const std::scoped_lock lock{mutex_};
        clear_owed(slot.commands());
      }
      return submit_slot(slot, {}, VK_NULL_HANDLE, serial).transform([] {
        return Presented{};
      });
    };
    pace(plan.swap_interval);
    auto ready = target_->prepare(plan.extent);
    if (!ready.has_value()) {
      invalidate();
      static_cast<void>(skip());
      return std::unexpected(ready.error());
    }
    if (!*ready) {
      invalidate();
      return skip();
    }
    {
      const std::scoped_lock lock{mutex_};
      if (auto synced = sync_target(); !synced.has_value()) {
        static_cast<void>(
            slot.submit(*device_, {}, VK_NULL_HANDLE, serial.value()));
        return std::unexpected(synced.error());
      }
    }
    const std::chrono::nanoseconds acquiring = stopwatch.now();
    auto acquired = target_->acquire(slot.acquired());
    const std::chrono::nanoseconds latched = stopwatch.now();
    waited += latched - acquiring;
    if (!acquired.has_value()) {
      invalidate();
      static_cast<void>(skip());
      return std::unexpected(acquired.error());
    }
    if (!acquired->has_value()) {
      invalidate();
      return skip();
    }
    const VulkanTarget::Image image = **acquired;
    const SurfaceSize extent = target_->extent();
    const ClipRect screen{.x = 0,
                          .y = 0,
                          .width = extent.width.value(),
                          .height = extent.height.value()};
    const bool hooked = hooks_.installed();
    std::span<const gpu::FramePlanner::Step> steps;
    {
      const std::scoped_lock lock{mutex_};
      steps =
          planner_.plan(recorded(recording, image.index, framebuffers_.size(),
                                 extent, hooked || !target_->preserves()));
    }
    VkCommandBuffer const buffer = slot.commands();
    std::optional<TimestampQueries>& clock = slot.timestamps();
    if (clock.has_value()) {
      clock->reset(buffer);
    }
    Execution run{
        .buffer = buffer,
        .slot = slot,
        .resident = resident,
        .recording = recording,
        .clock = clock,
        .image = image,
        .screen = screen,
        .target = FrameTarget{.image = image.image,
                              .view = target_->views()[image.index],
                              .format = target_->format(),
                              .extent = VkExtent2D{.width = screen.width,
                                                   .height = screen.height},
                              .layout = hook_layout,
                              .index = image.index,
                              .generation = generation_},
        .info = FrameInfo{.serial = serial,
                          .predicted_present = plan.predicted_present,
                          .extent = extent,
                          .replay = plan.replay,
                          .damage = damage},
        .plan = plan,
        .hooked = hooked,
        .drawn = 0,
        .framebuffer = framebuffers_[image.index].get().object,
        .prelude = true,
        .screen_pass = true};
    mark(run, TimestampQueries::Mark::started);
    for (const gpu::FramePlanner::Step& step : steps) {
      std::visit([this, &run](const auto& each) { execute(each, run); }, step);
    }
    recorded_.offscreen_passes += run.drawn;
    if (clock.has_value()) {
      clock->arm(
          TimestampQueries::Armed{.passes = run.drawn, .hooked = hooked});
    }
    const VkImageLayout presented = target_->presented_layout();
    render_barriers_.record(
        buffer,
        Transition{
            .image = image.image,
            .from = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .to = presented,
            .before =
                Access{.stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                       .access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT},
            .after = acquire_of(presented)});
    gather_waits(slot, image, recording.ready);
    {
      {
        const support::ScopedCallback queue = use_queue();
        const std::scoped_lock lock{mutex_};
        if (auto submitted =
                slot.submit(*device_, waits_, target_->finished(image.index),
                            serial.value());
            !submitted.has_value()) {
          if (clock.has_value()) {
            clock->disarm();
          }
          slot.release_acquired(*device_);
          invalidate();
          return std::unexpected(submitted.error());
        }
        ++recorded_.frames_presented;
        if (replaying) {
          ++recorded_.replays;
        }
        fold();
        last_damage_ = damage;
      }
      const std::chrono::nanoseconds presenting = stopwatch.now();
      auto shown = device_->with_queue([this, &image](VkQueue queue_handle) {
        return target_->present(queue_handle, image.index);
      });
      waited += stopwatch.since(presenting);
      if (!shown.has_value()) {
        invalidate();
        return std::unexpected(shown.error());
      }
    }
    if (!replaying) {
      last_ui_slot_ = packet.slot();
    }
    Presented result;
    if (target_->paced() && stopwatch.timed()) {
      result.displayed =
          latched +
          (display_period * std::max<std::uint32_t>(plan.swap_interval, 1));
    }
    if (plan.capture) {
      if (auto captured = read_back(); captured.has_value()) {
        result.image = std::move(*captured);
        const std::scoped_lock lock{mutex_};
        ++statistics_.captures;
      }
    }
    return result;
  }

  auto submit_slot(FrameSlot& slot, std::span<const FrameSlot::Wait> waits,
                   VkSemaphore signal, GpuSerial serial)
      -> std::expected<void, ProviderError> {
    const std::scoped_lock lock{mutex_};
    return slot.submit(*device_, waits, signal, serial.value());
  }

  void pace(std::uint32_t interval) {
    const std::uint32_t wanted = std::max<std::uint32_t>(interval, 1);
    if (target_->interval() == wanted) {
      return;
    }
    target_->set_interval(wanted);
    ++recorded_.interval_changes;
    recorded_.present_interval = wanted;
  }

  void fold() noexcept {
    statistics_.frames_presented +=
        std::exchange(recorded_.frames_presented, 0);
    statistics_.vertex_bytes_copied +=
        std::exchange(recorded_.vertex_bytes_copied, 0);
    statistics_.offscreen_passes +=
        std::exchange(recorded_.offscreen_passes, 0);
    statistics_.surface_calls += std::exchange(recorded_.surface_calls, 0);
    statistics_.hooked_frames += std::exchange(recorded_.hooked_frames, 0);
    statistics_.semaphore_waits += std::exchange(recorded_.semaphore_waits, 0);
    statistics_.replays += std::exchange(recorded_.replays, 0);
    statistics_.interval_changes +=
        std::exchange(recorded_.interval_changes, 0);
    statistics_.present_interval = target_->interval();
  }

  static constexpr auto sampling() noexcept -> Access {
    return Access{.stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                  .access = VK_ACCESS_2_SHADER_READ_BIT};
  }

  static constexpr auto attachment() noexcept -> Access {
    return Access{.stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                  .access = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT |
                            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT};
  }

  static auto release_of(VkImageLayout layout) noexcept -> Access {
    switch (layout) {
      case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        return Access{.stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                      .access = VK_ACCESS_2_TRANSFER_READ_BIT};
      case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
      case VK_IMAGE_LAYOUT_UNDEFINED:
        return Access{.stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                      .access = 0};
      default:
        return Access{.stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                      .access = VK_ACCESS_2_MEMORY_WRITE_BIT};
    }
  }

  static auto acquire_of(VkImageLayout layout) noexcept -> Access {
    if (layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
      return Access{.stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    .access = VK_ACCESS_2_TRANSFER_READ_BIT};
    }
    return Access{.stage = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, .access = 0};
  }

  static auto rectangle_of(ClipRect region) noexcept -> VkRect2D {
    return VkRect2D{
        .offset = VkOffset2D{.x = static_cast<std::int32_t>(region.x),
                             .y = static_cast<std::int32_t>(region.y)},
        .extent = VkExtent2D{.width = region.width, .height = region.height}};
  }

  auto placement_of(TextureFormat format) const noexcept -> Placement {
    const VulkanCapabilities& capabilities = device_->capabilities();
    switch (format) {
      case TextureFormat::bgra8:
        return Placement{.format = VK_FORMAT_B8G8R8A8_UNORM, .swizzle = {}};
      case TextureFormat::rgb565:
        return Placement{.format = capabilities.rgb565
                                       ? VK_FORMAT_R5G6B5_UNORM_PACK16
                                       : VK_FORMAT_R8G8B8A8_UNORM,
                         .swizzle = {}};
      case TextureFormat::r8:
      case TextureFormat::nv12:
        return Placement{.format = VK_FORMAT_R8_UNORM, .swizzle = {}};
      case TextureFormat::rg8:
        return Placement{.format = VK_FORMAT_R8G8_UNORM, .swizzle = {}};
      case TextureFormat::alpha8:
        if (capabilities.swizzle) {
          return Placement{
              .format = VK_FORMAT_R8_UNORM,
              .swizzle = VkComponentMapping{.r = VK_COMPONENT_SWIZZLE_ONE,
                                            .g = VK_COMPONENT_SWIZZLE_ONE,
                                            .b = VK_COMPONENT_SWIZZLE_ONE,
                                            .a = VK_COMPONENT_SWIZZLE_R}};
        }
        return Placement{.format = VK_FORMAT_R8G8B8A8_UNORM, .swizzle = {}};
      case TextureFormat::rgba8:
        return Placement{.format = VK_FORMAT_R8G8B8A8_UNORM, .swizzle = {}};
    }
    return Placement{};
  }

  static auto texel_bytes(VkFormat format) noexcept -> std::size_t {
    switch (format) {
      case VK_FORMAT_R8_UNORM:
        return 1;
      case VK_FORMAT_R8G8_UNORM:
      case VK_FORMAT_R5G6B5_UNORM_PACK16:
        return 2;
      default:
        return 4;
    }
  }

  static void convert(TextureFormat source, VkFormat target,
                      std::span<const std::byte> from,
                      std::span<std::byte> to) {
    if (source == TextureFormat::alpha8 && target == VK_FORMAT_R8G8B8A8_UNORM) {
      for (std::size_t column = 0; column < from.size(); ++column) {
        to[column * 4] = std::byte{0xff};
        to[(column * 4) + 1] = std::byte{0xff};
        to[(column * 4) + 2] = std::byte{0xff};
        to[(column * 4) + 3] = from[column];
      }
      return;
    }
    if (source == TextureFormat::rgb565 && target == VK_FORMAT_R8G8B8A8_UNORM) {
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

  void released(std::uint32_t slot) noexcept {
    const std::scoped_lock lock{released_mutex_};
    released_.push_back(slot);
  }

  void drain() {
    std::vector<std::uint32_t> taken;
    {
      const std::scoped_lock lock{released_mutex_};
      taken.swap(released_);
    }
    for (const std::uint32_t id : taken) {
      const auto found = textures_.find(id);
      if (found == textures_.end()) {
        continue;
      }
      graveyard_.push_back(
          Grave{.serial = hooks_.schedule().serial().value() + 1,
                .storage = std::move(found->second)});
      textures_.erase(found);
    }
  }

  void settle_serial(std::uint64_t serial) noexcept {
    if (serial > completed_.load(std::memory_order_relaxed)) {
      completed_.store(serial, std::memory_order_release);
    }
  }

  void reclaim() {
    std::erase_if(graveyard_, [this](Grave& grave) {
      if (grave.serial > completed_.load(std::memory_order_relaxed)) {
        return false;
      }
      descriptors_.release(grave.storage.descriptor);
      return true;
    });
  }

  void poll() {
    for (FrameSlot& slot : slots_) {
      if (const std::optional<std::uint64_t> done =
              slot.poll(device_->handle())) {
        settle_serial(*done);
        harvest(slot);
      }
    }
    if (const std::optional<std::uint64_t> done =
            replay_.poll(device_->handle())) {
      settle_serial(*done);
      harvest(replay_);
    }
    reclaim();
  }

  void harvest(FrameSlot& slot) {
    std::optional<TimestampQueries>& clock = slot.timestamps();
    if (!clock.has_value()) {
      return;
    }
    const TimestampQueries::Reading reading = clock->read();
    if (reading.stalled) {
      ++statistics_.timestamp_stalls;
    }
    if (reading.timings.has_value()) {
      ++statistics_.timestamp_reads;
      timings_ = *reading.timings;
    }
  }

  auto use_queue() -> support::ScopedCallback {
    if (queue_users_.fetch_add(1, std::memory_order_acq_rel) != 0) {
      overlapped_.fetch_add(1, std::memory_order_relaxed);
    }
    return support::ScopedCallback{[this]() noexcept {
      queue_users_.fetch_sub(1, std::memory_order_acq_rel);
    }};
  }

  [[nodiscard]] auto begin_slot(FrameSlot& slot)
      -> std::expected<void, ProviderError> {
    const bool pending = slot.pending();
    const std::uint64_t serial = slot.serial();
    auto ready = slot.ready(*device_);
    if (!ready.has_value()) {
      return std::unexpected(ready.error());
    }
    if (*ready) {
      ++statistics_.fence_waits;
    }
    if (pending) {
      settle_serial(serial);
      harvest(slot);
      reclaim();
    }
    return {};
  }

  void settle(VkCommandBuffer commands) {
    settling_.clear();
    for (const std::uint32_t id : written_) {
      const auto found = textures_.find(id);
      if (found == textures_.end() || found->second.sampled) {
        continue;
      }
      found->second.sampled = true;
      settling_.push_back(
          Transition{.image = found->second.raw,
                     .from = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                     .to = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                     .before = Access{.stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                      .access = VK_ACCESS_2_TRANSFER_WRITE_BIT},
                     .after = sampling()});
    }
    written_.clear();
    barriers_.record(commands, settling_);
  }

  [[nodiscard]] auto framebuffer_for(const VulkanImage& image) const
      -> std::expected<VulkanFramebufferHandle, ProviderError> {
    VkDevice const handle = device_->handle();
    VkImageView const view = image.view();
    const VkFramebufferCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .renderPass = pipelines_.render_pass(Destination::offscreen),
        .attachmentCount = 1,
        .pAttachments = &view,
        .width = image.description().width,
        .height = image.description().height,
        .layers = 1};
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    if (const VkResult result =
            vkCreateFramebuffer(handle, &info, nullptr, &framebuffer);
        result != VK_SUCCESS) {
      return std::unexpected(VulkanStatus::error(result));
    }
    return VulkanFramebufferHandle{{.device = handle, .object = framebuffer}};
  }

  static void view(VkCommandBuffer buffer, const Surface& surface) noexcept {
    const VkViewport viewport{.x = 0.0F,
                              .y = 0.0F,
                              .width = static_cast<float>(surface.width),
                              .height = static_cast<float>(surface.height),
                              .minDepth = 0.0F,
                              .maxDepth = 1.0F};
    vkCmdSetViewport(buffer, 0, 1, &viewport);
  }

  static void clear_rectangle(VkCommandBuffer buffer, Colour colour,
                              ClipRect region) noexcept {
    const VkClearAttachment attachment{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .colorAttachment = 0,
        .clearValue =
            VkClearValue{.color = VkClearColorValue{
                             .float32 = {colour.red, colour.green, colour.blue,
                                         colour.alpha}}}};
    const VkClearRect rectangle{
        .rect = rectangle_of(region), .baseArrayLayer = 0, .layerCount = 1};
    vkCmdClearAttachments(buffer, 1, &attachment, 1, &rectangle);
  }

  void draw_surfaces(VkCommandBuffer buffer, std::span<const Surfaces::Due> due,
                     GpuSerial serial, const FramePlan& plan,
                     std::optional<ClipRect> damage,
                     const Resolution& resolution) {
    for (const Surfaces::Due& each : due) {
      const std::optional<Resolved> found = lookup(resolution, each.texture);
      if (!found.has_value() || found->framebuffer == VK_NULL_HANDLE) {
        continue;
      }
      const Resolved& storage = *found;
      const Access lent{
          .stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
          .access = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT};
      render_barriers_.record(
          buffer, Transition{.image = storage.image,
                             .from = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                             .to = hook_layout,
                             .before = sampling(),
                             .after = lent});
      const FrameTarget target{.image = storage.image,
                               .view = storage.view,
                               .format = storage.format,
                               .extent = VkExtent2D{.width = storage.width,
                                                    .height = storage.height},
                               .layout = hook_layout,
                               .index = 0,
                               .generation = each.texture.value()};
      const SurfaceFrame frame{
          .surface = each.surface,
          .mode = SurfaceMode::texture,
          .frame = FrameInfo{.serial = serial,
                             .predicted_present = plan.predicted_present,
                             .extent =
                                 SurfaceSize{
                                     .width = SurfaceWidth{storage.width},
                                     .height = SurfaceHeight{storage.height}},
                             .replay = plan.replay,
                             .damage = damage},
          .viewport = PassRegion{.x = 0.0F,
                                 .y = 0.0F,
                                 .width = static_cast<float>(storage.width),
                                 .height = static_cast<float>(storage.height)},
          .scissor = ClipRect{.x = 0,
                              .y = 0,
                              .width = storage.width,
                              .height = storage.height}};
      if (surfaces_.run(each.surface, buffer, target, frame)) {
        ++recorded_.surface_calls;
      }
      render_barriers_.record(
          buffer,
          Transition{
              .image = storage.image,
              .from = hook_layout,
              .to = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
              .before = Access{.stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                               .access = VK_ACCESS_2_MEMORY_WRITE_BIT},
              .after = sampling()});
    }
  }

  auto recorded(const Recording& recording, std::size_t image,
                std::size_t images, SurfaceSize extent, bool whole)
      -> gpu::FramePlanner::RecordedFrame {
    const Resolution& resolution = recording.resolution.get();
    recorded_passes_.clear();
    pass_storage_.clear();
    for (const OffscreenPass& each : recording.passes) {
      std::optional<Resolved> storage;
      if (each.first + each.count <= recording.pass_commands.size() &&
          each.holes_first + each.holes_count <= recording.pass_holes.size()) {
        storage = lookup(resolution, each.pass.target);
        if (storage.has_value() && storage->framebuffer == VK_NULL_HANDLE) {
          storage.reset();
        }
      }
      std::optional<SurfaceSize> size;
      if (storage.has_value()) {
        size = SurfaceSize{.width = SurfaceWidth{storage->width},
                           .height = SurfaceHeight{storage->height}};
      }
      recorded_passes_.push_back(gpu::FramePlanner::RecordedPass{
          .pass = each.pass,
          .target = size,
          .batch = gpu::FramePlanner::Batch{
              .commands = storage.has_value() ? each.count : 0,
              .holes = storage.has_value()
                           ? recording.pass_holes.subspan(each.holes_first,
                                                          each.holes_count)
                           : std::span<const Surfaces::Hole>{}}});
      pass_storage_.push_back(storage);
    }
    screen_batches_[0] = gpu::FramePlanner::Batch{
        .commands = recording.commands.size(), .holes = recording.holes};
    return gpu::FramePlanner::RecordedFrame{
        .passes = recorded_passes_,
        .screen =
            gpu::FramePlanner::RecordedScreen{.image = image,
                                              .images = images,
                                              .size = extent,
                                              .damage = recording.damage,
                                              .whole = whole,
                                              .batches = screen_batches_}};
  }

  static void mark(Execution& run, TimestampQueries::Mark at) {
    if (run.clock.has_value()) {
      run.clock->mark(run.buffer, at);
    }
  }

  static auto hole_colour(const Recording& recording) noexcept -> Colour {
    return recording.clear.value_or(
        Colour{.red = 0.0F, .green = 0.0F, .blue = 0.0F, .alpha = 0.0F});
  }

  static constexpr auto transfer_writing() noexcept -> Access {
    return Access{.stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                  .access = VK_ACCESS_2_TRANSFER_WRITE_BIT};
  }

  static void clear_image(VkCommandBuffer buffer, VkImage image) noexcept {
    const VkClearColorValue zero{.float32 = {0.0F, 0.0F, 0.0F, 0.0F}};
    const VkImageSubresourceRange range = ImageBarriers::colour_range();
    vkCmdClearColorImage(buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         &zero, 1, &range);
  }

  auto image_of(TextureId texture) const -> std::optional<VkImage> {
    const auto record = registry_->find(texture);
    if (!record.has_value()) {
      return std::nullopt;
    }
    const auto found = textures_.find(record->native);
    if (found == textures_.end() || found->second.raw == VK_NULL_HANDLE) {
      return std::nullopt;
    }
    return found->second.raw;
  }

  void reset_image(VkCommandBuffer buffer, VkImage image) {
    render_barriers_.record(
        buffer, Transition{.image = image,
                           .from = VK_IMAGE_LAYOUT_UNDEFINED,
                           .to = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           .before = sampling(),
                           .after = transfer_writing()});
    clear_image(buffer, image);
    render_barriers_.record(
        buffer,
        Transition{.image = image,
                   .from = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   .to = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                   .before = transfer_writing(),
                   .after = Access{
                       .stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT |
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                       .access = VK_ACCESS_2_SHADER_READ_BIT |
                                 VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT |
                                 VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT}});
  }

  void clear_owed(VkCommandBuffer buffer) {
    for (const gpu::FramePlanner::Step& step : planner_.owed()) {
      const auto* clear = std::get_if<gpu::FramePlanner::ClearTexture>(&step);
      if (clear == nullptr) {
        continue;
      }
      if (const std::optional<VkImage> image = image_of(clear->texture)) {
        reset_image(buffer, *image);
      }
    }
  }

  void execute(const gpu::FramePlanner::ClearTexture& step, Execution& run) {
    if (!run.prelude) {
      return;
    }
    std::optional<VkImage> image;
    {
      const std::scoped_lock lock{mutex_};
      image = image_of(step.texture);
    }
    if (image.has_value()) {
      reset_image(run.buffer, *image);
    }
  }

  void execute(const gpu::FramePlanner::RenderSurfaces&, Execution& run) {
    if (!run.prelude) {
      return;
    }
    draw_surfaces(run.buffer, run.recording.due, run.info.serial, run.plan,
                  run.recording.damage, run.recording.resolution.get());
  }

  void execute(const gpu::FramePlanner::SkippedPass&, Execution&) {}

  void execute(const gpu::FramePlanner::OffscreenPass& step, Execution& run) {
    if (!run.prelude) {
      return;
    }
    const Resolved& storage = *pass_storage_[step.pass];
    const OffscreenPass& each = run.recording.passes[step.pass];
    const std::span<const BlockCommand> commands =
        run.recording.pass_commands.subspan(each.first, each.count);
    const ClipRect whole{
        .x = 0, .y = 0, .width = storage.width, .height = storage.height};
    render_barriers_.record(
        run.buffer,
        Transition{.image = storage.image,
                   .from = step.clear.has_value()
                               ? VK_IMAGE_LAYOUT_UNDEFINED
                               : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                   .to = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                   .before = sampling(),
                   .after = attachment()});
    const VkRenderPassBeginInfo begin{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = pipelines_.render_pass(Destination::offscreen),
        .framebuffer = storage.framebuffer,
        .renderArea = rectangle_of(whole),
        .clearValueCount = 0,
        .pClearValues = nullptr};
    vkCmdBeginRenderPass(run.buffer, &begin, VK_SUBPASS_CONTENTS_INLINE);
    const Surface surface{.width = storage.width,
                          .height = storage.height,
                          .region = step.region,
                          .destination = Destination::offscreen};
    view(run.buffer, surface);
    if (step.clear.has_value()) {
      clear_rectangle(run.buffer, *step.clear, whole);
    }
    const Colour hole = hole_colour(run.recording);
    for (const gpu::FramePlanner::Segment& segment : step.segments) {
      if (const auto* range =
              std::get_if<gpu::FramePlanner::DrawRange>(&segment)) {
        record_ui(run.buffer, run.slot, run.resident, surface,
                  commands.subspan(range->first, range->count), std::nullopt,
                  run.recording.resolution.get());
      } else {
        clear_rectangle(
            run.buffer, hole,
            std::get<gpu::FramePlanner::UnderlayFill>(segment).scissor);
      }
    }
    vkCmdEndRenderPass(run.buffer);
    render_barriers_.record(
        run.buffer,
        Transition{
            .image = storage.image,
            .from = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .to = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .before =
                Access{.stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                       .access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT},
            .after = sampling()});
    ++run.drawn;
  }

  void execute(const gpu::FramePlanner::ScreenPass& step, Execution& run) {
    if (!run.screen_pass) {
      return;
    }
    const VkCommandBuffer buffer = run.buffer;
    const VulkanTarget::Image& image = run.image;
    const ClipRect screen = run.screen;
    mark(run, TimestampQueries::Mark::effects_drawn);
    render_barriers_.record(
        buffer, Transition{.image = image.image,
                           .from = image.layout,
                           .to = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                           .before = release_of(image.layout),
                           .after = attachment()});
    const Surface surface{
        .width = screen.width,
        .height = screen.height,
        .region = PassRegion{.x = 0.0F,
                             .y = 0.0F,
                             .width = static_cast<float>(screen.width),
                             .height = static_cast<float>(screen.height)},
        .destination = Destination::screen};
    const std::optional<Colour> clear = run.recording.clear;
    if (!run.hooked) {
      mark(run, TimestampQueries::Mark::under_started);
      mark(run, TimestampQueries::Mark::under_drawn);
    }
    begin_screen(buffer, run.framebuffer, screen);
    view(buffer, surface);
    if (clear.has_value() && step.painted()) {
      clear_rectangle(buffer, *clear, step.region);
    }
    if (run.hooked) {
      vkCmdEndRenderPass(buffer);
      mark(run, TimestampQueries::Mark::under_started);
      lend(buffer, image.image);
      hooks_.run(FrameStage::before_ui, buffer, run.target, run.info);
      reclaim_target(buffer, image.image);
      mark(run, TimestampQueries::Mark::under_drawn);
      begin_screen(buffer, run.framebuffer, screen);
      view(buffer, surface);
    }
    const Colour hole = hole_colour(run.recording);
    for (const gpu::FramePlanner::Segment& segment : step.segments) {
      if (const auto* range =
              std::get_if<gpu::FramePlanner::DrawRange>(&segment)) {
        record_ui(buffer, run.slot, run.resident, surface,
                  run.recording.commands.subspan(range->first, range->count),
                  step.scissor, run.recording.resolution.get());
        continue;
      }
      const auto& fill = std::get<gpu::FramePlanner::UnderlayFill>(segment);
      clear_rectangle(buffer, hole, fill.scissor);
      vkCmdEndRenderPass(buffer);
      lend(buffer, image.image);
      if (surfaces_.run(fill.underlay.surface, buffer, run.target,
                        SurfaceFrame{.surface = fill.underlay.surface,
                                     .mode = SurfaceMode::underlay,
                                     .frame = run.info,
                                     .viewport = fill.underlay.viewport,
                                     .scissor = fill.scissor})) {
        ++recorded_.surface_calls;
      }
      reclaim_target(buffer, image.image);
      begin_screen(buffer, run.framebuffer, screen);
      view(buffer, surface);
    }
    vkCmdEndRenderPass(buffer);
    mark(run, TimestampQueries::Mark::interface_drawn);
    if (run.hooked) {
      lend(buffer, image.image);
      hooks_.run(FrameStage::after_ui, buffer, run.target, run.info);
      reclaim_target(buffer, image.image);
      ++recorded_.hooked_frames;
    }
    mark(run, TimestampQueries::Mark::over_drawn);
  }

  void drop_overlay() noexcept {
    if (overlay_ == nullptr) {
      return;
    }
    retire(*overlay_);
    device_->wait_idle();
    overlay_.reset();
  }

  static void retire(OverlayState& state) noexcept;

  void invalidate() noexcept { planner_.invalidate(); }

  [[nodiscard]] auto sync_target() -> std::expected<void, ProviderError> {
    if (generation_ == target_->generation() && !framebuffers_.empty()) {
      return {};
    }
    device_->wait_idle();
    framebuffers_.clear();
    if (pipelines_.format() != target_->format()) {
      if (auto retargeted = pipelines_.retarget(*device_, target_->format());
          !retargeted.has_value()) {
        return std::unexpected(retargeted.error());
      }
    }
    const SurfaceSize extent = target_->extent();
    VkDevice const handle = device_->handle();
    for (VkImageView view : target_->views()) {
      const VkFramebufferCreateInfo info{
          .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
          .pNext = nullptr,
          .flags = 0,
          .renderPass = pipelines_.render_pass(),
          .attachmentCount = 1,
          .pAttachments = &view,
          .width = extent.width.value(),
          .height = extent.height.value(),
          .layers = 1};
      VkFramebuffer framebuffer = VK_NULL_HANDLE;
      if (const VkResult result =
              vkCreateFramebuffer(handle, &info, nullptr, &framebuffer);
          result != VK_SUCCESS) {
        framebuffers_.clear();
        return std::unexpected(VulkanStatus::error(result));
      }
      framebuffers_.emplace_back(VulkanFramebufferHandle::handle_type{
          .device = handle, .object = framebuffer});
    }
    planner_.invalidate();
    generation_ = target_->generation();
    return {};
  }

  [[nodiscard]] auto ensure_capture(std::size_t size)
      -> std::expected<void, ProviderError> {
    VkDevice const handle = device_->handle();
    if (!capture_.has_value()) {
      const VkCommandPoolCreateInfo pool_info{
          .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
          .pNext = nullptr,
          .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
          .queueFamilyIndex = device_->queue_family()};
      VkCommandPool pool = VK_NULL_HANDLE;
      if (const VkResult result =
              vkCreateCommandPool(handle, &pool_info, nullptr, &pool);
          result != VK_SUCCESS) {
        return std::unexpected(VulkanStatus::error(result));
      }
      Capture created{
          .pool = VulkanCommandPoolHandle{{.device = handle, .object = pool}},
          .commands = VK_NULL_HANDLE,
          .fence = {},
          .buffer = std::nullopt};
      const VkCommandBufferAllocateInfo buffer_info{
          .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
          .pNext = nullptr,
          .commandPool = pool,
          .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
          .commandBufferCount = 1};
      if (const VkResult result =
              vkAllocateCommandBuffers(handle, &buffer_info, &created.commands);
          result != VK_SUCCESS) {
        return std::unexpected(VulkanStatus::error(result));
      }
      const VkFenceCreateInfo fence_info{
          .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
          .pNext = nullptr,
          .flags = 0};
      VkFence fence = VK_NULL_HANDLE;
      if (const VkResult result =
              vkCreateFence(handle, &fence_info, nullptr, &fence);
          result != VK_SUCCESS) {
        return std::unexpected(VulkanStatus::error(result));
      }
      created.fence = VulkanFenceHandle{{.device = handle, .object = fence}};
      capture_.emplace(std::move(created));
    }
    if (!capture_->buffer.has_value() || capture_->buffer->size() < size) {
      capture_->buffer.reset();
      auto buffer =
          VulkanBuffer::create(*device_, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                               MemoryUse::readback);
      if (!buffer.has_value()) {
        return std::unexpected(buffer.error());
      }
      capture_->buffer.emplace(std::move(*buffer));
    }
    return {};
  }

  void record_ui(VkCommandBuffer buffer, const FrameSlot& slot,
                 ResidentBuffers resident, const Surface& surface,
                 std::span<const BlockCommand> commands,
                 std::optional<ClipRect> scissor,
                 const Resolution& resolution) {
    const std::uint32_t width = surface.width;
    const std::uint32_t height = surface.height;
    const auto fwidth = static_cast<float>(width);
    const auto fheight = static_cast<float>(height);
    const PassRegion& region = surface.region;
    const std::array<float, 2> textured_scale{2.0F / fwidth, 2.0F / fheight};
    const std::array<float, 2> textured_translation{-1.0F, -1.0F};
    const std::array<float, 2> shaped_scale{2.0F / region.width,
                                            2.0F / region.height};
    const std::array<float, 2> shaped_translation{
        -1.0F - (2.0F * region.x / region.width),
        -1.0F - (2.0F * region.y / region.height)};
    const float across = fwidth / region.width;
    const float down = fheight / region.height;
    const auto mapped = [](float edge, float origin, float factor,
                           std::uint32_t limit, bool up) {
      const float value = (edge - origin) * factor;
      const float rounded = up ? std::ceil(value) : std::floor(value);
      return static_cast<std::uint32_t>(
          std::clamp(rounded, 0.0F, static_cast<float>(limit)));
    };
    VkPipelineLayout const layout = pipelines_.layout();
    std::optional<Kind> bound_pipeline;
    std::optional<std::uint32_t> bound_block;
    std::optional<Kind> bound_block_kind;
    std::optional<std::pair<std::uint64_t, std::uint64_t>> bound_texture;
    std::optional<ShapePipelines::VertexConstants> bound_vertex;
    std::optional<ShapePipelines::FragmentConstants> bound_fragment;
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
      VkBuffer vertex_buffer = VK_NULL_HANDLE;
      VkBuffer index_buffer = VK_NULL_HANDLE;
      if (kind == Kind::textured) {
        const auto* found = slot.geometry().block(block);
        if (found == nullptr) {
          continue;
        }
        vertex_buffer = found->vertex_buffer();
        index_buffer = found->index_buffer();
      } else if (block == ResidentShapes::block) {
        vertex_buffer = resident.vertices;
        index_buffer = resident.indices;
      } else {
        const auto* found = slot.shapes().block(block);
        if (found == nullptr) {
          continue;
        }
        vertex_buffer = found->vertex_buffer();
        index_buffer = found->index_buffer();
      }
      if (vertex_buffer == VK_NULL_HANDLE || index_buffer == VK_NULL_HANDLE) {
        continue;
      }
      const TextureId detail =
          command.detail.value() != 0 ? command.detail : command.texture;
      const std::pair<std::uint64_t, std::uint64_t> wanted{
          command.texture.value(), detail.value()};
      if (bound_texture != wanted) {
        const std::optional<Resolved> first =
            lookup(resolution, command.texture);
        const std::optional<Resolved> second = lookup(resolution, detail);
        if (!first.has_value() || !second.has_value()) {
          continue;
        }
        bound_texture = wanted;
        const std::array sets{first->descriptor, second->descriptor};
        vkCmdBindDescriptorSets(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout,
                                0, static_cast<std::uint32_t>(sets.size()),
                                sets.data(), 0, nullptr);
      }
      if (bound_pipeline != kind) {
        bound_pipeline = kind;
        vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipelines_.pipeline(kind, surface.destination));
      }
      if (bound_block != block || bound_block_kind != kind) {
        bound_block = block;
        bound_block_kind = kind;
        const VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(buffer, 0, 1, &vertex_buffer, &offset);
        vkCmdBindIndexBuffer(buffer, index_buffer, 0, VK_INDEX_TYPE_UINT32);
      }
      const bool shaped = kind == Kind::shape;
      const ShapePipelines::VertexConstants vertex{
          .scale = shaped ? shaped_scale : textured_scale,
          .translation = shaped ? shaped_translation : textured_translation,
          .placement = kind == Kind::shape
                           ? command.placement()
                           : std::array<float, 4>{0.0F, 0.0F, 1.0F, 0.0F}};
      if (bound_vertex != vertex) {
        bound_vertex = vertex;
        vkCmdPushConstants(buffer, layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                           sizeof(vertex), &vertex);
      }
      const ShapePipelines::FragmentConstants fragment{
          .trim = {command.trim[0], command.trim[1], 0.0F, 0.0F}};
      if (bound_fragment != fragment) {
        bound_fragment = fragment;
        vkCmdPushConstants(buffer, layout, VK_SHADER_STAGE_FRAGMENT_BIT,
                           ShapePipelines::fragment_offset, sizeof(fragment),
                           &fragment);
      }
      const VkRect2D rectangle{
          .offset = VkOffset2D{.x = static_cast<std::int32_t>(left),
                               .y = static_cast<std::int32_t>(top)},
          .extent = VkExtent2D{.width = right - left, .height = bottom - top}};
      vkCmdSetScissor(buffer, 0, 1, &rectangle);
      vkCmdDrawIndexed(buffer, command.index_count, 1, command.index_offset,
                       static_cast<std::int32_t>(command.vertex_offset), 0);
    }
  }

  std::shared_ptr<const VulkanDevice> device_;
  std::unique_ptr<VulkanTarget> target_;
  GpuTakeover::Seat takeover_;
  LentSwapchain<VkSemaphore> lent_;
  std::atomic<std::uint64_t> lent_presents_{0};
  ShapePipelines pipelines_;
  ImageBarriers barriers_;
  ImageBarriers render_barriers_;
  DescriptorPools descriptors_;
  std::vector<FrameSlot> slots_;
  FrameSlot replay_;
  std::optional<std::uint32_t> last_ui_slot_;
  std::optional<std::uint32_t> replay_reads_;
  std::atomic<std::uint64_t> submitted_{0};
  std::atomic<bool> lost_{false};
  std::atomic<std::uint64_t> contended_{0};
  Statistics recorded_;
  RenderQueue queue_;
  std::vector<VulkanFramebufferHandle> framebuffers_;
  gpu::FramePlanner planner_;
  std::vector<gpu::FramePlanner::RecordedPass> recorded_passes_;
  std::vector<std::optional<Resolved>> pass_storage_;
  std::array<gpu::FramePlanner::Batch, 1> screen_batches_{};
  std::flat_map<std::uint32_t, TextureStorage> textures_;
  std::vector<Grave> graveyard_;
  std::vector<std::uint32_t> written_;
  std::vector<Transition> settling_;
  std::vector<ShapeVertex> mirror_;
  std::optional<Capture> capture_;
  FrameSlots ring_;
  mutable std::mutex mutex_;
  std::mutex released_mutex_;
  std::vector<std::uint32_t> released_;
  std::shared_ptr<Registry> registry_;
  Hooks hooks_;
  std::shared_ptr<Publisher> publisher_;
  Surfaces surfaces_;
  Statistics statistics_;
  GpuTimings timings_;
  std::atomic<std::uint32_t> queue_users_{0};
  std::atomic<std::uint64_t> overlapped_{0};
  std::uint64_t generation_{0};
  std::atomic<std::uint64_t> completed_{0};
  std::optional<ClipRect> last_damage_;
  std::vector<FrameSlot::Wait> waits_;
  std::uint64_t textures_created_{0};
  std::uint32_t next_texture_{1};
  std::uint32_t building_{0};
  std::uint32_t staging_{0};
  std::optional<std::reference_wrapper<FrameSlot>> staging_overlay_;
  std::shared_ptr<OverlayState> overlay_;
  gpu::FramePlanner overlay_planner_;
  std::mutex submit_mutex_;
};

class VulkanGpu::OverlayState {
 public:
  static constexpr std::uint32_t slot_count = 3;

  OverlayState(std::vector<FrameSlot> slots, VulkanSemaphoreHandle timeline,
               PFN_vkGetSemaphoreCounterValue counter) noexcept
      : slots_(std::move(slots)),
        timeline_(std::move(timeline)),
        counter_(counter) {}

  static constexpr std::size_t maximum_framebuffers = 8;

  OverlayState(const OverlayState&) = delete;
  auto operator=(const OverlayState&) -> OverlayState& = delete;
  OverlayState(OverlayState&&) = delete;
  auto operator=(OverlayState&&) -> OverlayState& = delete;

  ~OverlayState() {
    VkDevice const device = timeline_.get().device;
    for (const FrameSlot& each : slots_) {
      if (!each.pending()) {
        continue;
      }
      VkFence const fence = each.fence();
      static_cast<void>(
          vkWaitForFences(device, 1, &fence, VK_TRUE,
                          std::numeric_limits<std::uint64_t>::max()));
    }
  }

  [[nodiscard]] static auto begin(const std::shared_ptr<Renderer>& renderer,
                                  const std::shared_ptr<OverlayState>& self)
      -> std::expected<GpuFrame, ProviderError>;

  auto ring() noexcept -> OverlaySlots& { return ring_; }

  auto ring() const noexcept -> const OverlaySlots& { return ring_; }

  auto slot(std::uint32_t index) noexcept -> FrameSlot& {
    return slots_[index];
  }

  auto statistics() const -> Overlay::Statistics {
    const std::scoped_lock lock{attachments_mutex_};
    return Overlay::Statistics{.framebuffers = attachments_.size(),
                               .retired_framebuffers = retired_.size()};
  }

  auto frame(std::uint32_t index) noexcept -> std::shared_ptr<Frame>& {
    return frames_[index];
  }

  auto timeline() const noexcept -> VkSemaphore {
    return timeline_.get().object;
  }

  auto reached() const noexcept -> std::uint64_t {
    std::uint64_t value = 0;
    if (counter_(timeline_.get().device, timeline_.get().object, &value) !=
        VK_SUCCESS) {
      return 0;
    }
    return value;
  }

  auto idle(std::uint32_t index, OverlaySlots::Standing standing) const
      -> bool {
    const FrameSlot& chosen = slots_[index];
    return (!chosen.pending() ||
            vkGetFenceStatus(timeline_.get().device, chosen.fence()) ==
                VK_SUCCESS) &&
           reached() >= standing.last_use;
  }

  [[nodiscard]] auto attachment(VkRenderPass pass, const FrameTarget& target,
                                std::uint64_t use)
      -> std::expected<VkFramebuffer, ProviderError> {
    const std::scoped_lock lock{attachments_mutex_};
    const std::uint64_t done = reached();
    std::erase_if(retired_,
                  [done](const Attachment& each) { return each.use <= done; });
    const VkImageView view = target.view;
    const VkExtent2D extent = target.extent;
    const std::uint64_t generation = target.generation;
    const auto stale = std::ranges::remove_if(
        attachments_, [generation](const Attachment& each) {
          return each.generation < generation;
        });
    std::ranges::move(stale, std::back_inserter(retired_));
    attachments_.erase(stale.begin(), stale.end());
    const auto found = std::ranges::find_if(
        attachments_, [view, extent, generation](const Attachment& each) {
          return each.view == view && each.generation == generation &&
                 each.extent.width == extent.width &&
                 each.extent.height == extent.height;
        });
    if (found != attachments_.end()) {
      found->use = use;
      return found->framebuffer.get().object;
    }
    if (attachments_.size() >= maximum_framebuffers) {
      const auto oldest =
          std::ranges::min_element(attachments_, {}, &Attachment::use);
      retired_.push_back(std::move(*oldest));
      attachments_.erase(oldest);
    }
    VkDevice const device = timeline_.get().device;
    const VkFramebufferCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .renderPass = pass,
        .attachmentCount = 1,
        .pAttachments = &view,
        .width = extent.width,
        .height = extent.height,
        .layers = 1};
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    if (const VkResult result =
            vkCreateFramebuffer(device, &info, nullptr, &framebuffer);
        result != VK_SUCCESS) {
      return std::unexpected(VulkanStatus::error(result));
    }
    attachments_.push_back(
        Attachment{.view = view,
                   .extent = extent,
                   .generation = generation,
                   .use = use,
                   .framebuffer = VulkanFramebufferHandle{
                       {.device = device, .object = framebuffer}}});
    return framebuffer;
  }

 private:
  struct Attachment {
    VkImageView view{VK_NULL_HANDLE};
    VkExtent2D extent{};
    std::uint64_t generation{0};
    std::uint64_t use{0};
    VulkanFramebufferHandle framebuffer;
  };

  OverlaySlots ring_{slot_count, OverlaySlots::Leftovers::dropped};
  std::vector<FrameSlot> slots_;
  VulkanSemaphoreHandle timeline_;
  PFN_vkGetSemaphoreCounterValue counter_;
  std::array<std::shared_ptr<Frame>, slot_count> frames_{};
  mutable std::mutex attachments_mutex_;
  std::vector<Attachment> attachments_;
  std::vector<Attachment> retired_;
};

class VulkanGpu::Frame final : public GpuFrame::Implementation {
 public:
  explicit Frame(std::weak_ptr<Renderer> renderer) noexcept
      : renderer_(std::move(renderer)) {}

  Frame(const Frame&) = delete;
  auto operator=(const Frame&) -> Frame& = delete;
  Frame(Frame&&) = delete;
  auto operator=(Frame&&) -> Frame& = delete;

  ~Frame() override { finish(); }

  void open(Renderer::Opened opened,
            std::shared_ptr<OverlayState> overlay = nullptr) noexcept {
    opened_ = opened;
    if (overlay != nullptr) {
      overlay_ = std::move(overlay);
    }
    clear_.reset();
    damage_.reset();
    plan_ = FramePlan{};
    commands_.clear();
    passes_.clear();
    pass_commands_.clear();
    holes_.clear();
    pass_holes_.clear();
    due_.clear();
    resident_indices_.reset();
    sealed_ = false;
    open_ = true;
  }

  void finish() noexcept override {
    if (!open_ || sealed_) {
      return;
    }
    open_ = false;
    const std::shared_ptr<Renderer> renderer = renderer_.lock();
    if (opened_.overlay) {
      const std::shared_ptr<OverlayState> overlay = std::exchange(overlay_, {});
      if (renderer != nullptr && overlay != nullptr) {
        renderer->flush_overlay_slot(overlay->slot(opened_.claim.slot),
                                     opened_.serial);
        renderer->registry()->close(opened_.ticket);
        overlay->ring().abandon(opened_.claim.slot);
      }
      return;
    }
    if (renderer != nullptr) {
      renderer->close_frame(opened_);
    }
  }

  auto recording() const -> Renderer::Recording {
    return Renderer::Recording{.clear = std::nullopt,
                               .commands = commands_,
                               .passes = passes_,
                               .pass_commands = pass_commands_,
                               .holes = holes_,
                               .pass_holes = pass_holes_,
                               .due = due_,
                               .ready = ready_,
                               .damage = std::nullopt,
                               .resident_indices = std::nullopt,
                               .resolution = std::cref(resolution_),
                               .ticket = opened_.ticket,
                               .replay = false,
                               .extent = plan_.extent};
  }

  auto planned() const noexcept -> const FramePlan& { return plan_; }

  [[nodiscard]] auto allocate_geometry(std::uint32_t vertex_count,
                                       std::uint32_t index_count)
      -> std::expected<FrameGeometry, ProviderError> override {
    const std::shared_ptr<Renderer> renderer = renderer_.lock();
    if (renderer == nullptr || sealed_ || opened_.replay) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::platform_failure});
    }
    if (opened_.overlay) {
      return overlay_->slot(opened_.claim.slot)
          .geometry()
          .allocate(renderer->device(), vertex_count, index_count);
    }
    return renderer->allocate_geometry(vertex_count, index_count);
  }

  [[nodiscard]] auto allocate_shapes(std::uint32_t vertex_count,
                                     std::uint32_t index_count)
      -> std::expected<ShapeGeometry, ProviderError> override {
    const std::shared_ptr<Renderer> renderer = renderer_.lock();
    if (renderer == nullptr || sealed_ || opened_.replay) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::platform_failure});
    }
    if (opened_.overlay) {
      return overlay_->slot(opened_.claim.slot)
          .shapes()
          .allocate(renderer->device(), vertex_count, index_count);
    }
    return renderer->allocate_shapes(vertex_count, index_count);
  }

  [[nodiscard]] auto allocate_resident(std::uint32_t vertex_count,
                                       std::uint32_t index_count)
      -> std::expected<ResidentShapes, ProviderError> override {
    const std::shared_ptr<Renderer> renderer = renderer_.lock();
    if (renderer == nullptr || sealed_ || opened_.replay) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::platform_failure});
    }
    if (opened_.overlay) {
      return allocate_shapes(vertex_count, index_count)
          .transform([](const ShapeGeometry& geometry) {
            return ResidentShapes{.geometry = geometry, .generation = 0};
          });
    }
    auto allocated = renderer->allocate_resident(vertex_count, index_count);
    if (allocated.has_value()) {
      resident_indices_ = index_count;
    }
    return allocated;
  }

  [[nodiscard]] auto replay(const ReplayDrawData& data)
      -> std::expected<void, ProviderError> override {
    const std::shared_ptr<Renderer> renderer = renderer_.lock();
    if (renderer == nullptr || sealed_ || !opened_.replay ||
        data.generation != 1) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument});
    }
    if (const auto refused = renderer->registry()->refusal(data.commands)) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument, *refused});
    }
    if (auto copied = renderer->replay_indices(data.indices);
        !copied.has_value()) {
      return copied;
    }
    resident_indices_ = static_cast<std::uint32_t>(data.indices.size());
    for (const DrawCommand& command : data.commands) {
      commands_.push_back(
          Renderer::BlockCommand{.pipeline = Renderer::Kind::shape,
                                 .block = ResidentShapes::block,
                                 .command = command});
    }
    return {};
  }

  void touch(VertexRange range) override {
    if (opened_.overlay) {
      return;
    }
    if (const std::shared_ptr<Renderer> renderer = renderer_.lock()) {
      renderer->touch(range);
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
    return record(data, Renderer::Kind::textured, commands_);
  }

  [[nodiscard]] auto draw(const ShapeDrawData& data)
      -> std::expected<void, ProviderError> override {
    return record(data, Renderer::Kind::shape, commands_);
  }

  [[nodiscard]] auto draw(const ShapeDrawData& data, const RenderPass& pass)
      -> std::expected<void, ProviderError> override {
    const std::shared_ptr<Renderer> renderer = renderer_.lock();
    if (renderer == nullptr || sealed_) {
      return std::unexpected(ProviderError{ProviderErrorCode::platform_failure,
                                           ProviderErrorDetail::frame_closed});
    }
    const auto target = renderer->registry()->find(pass.target);
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
    const std::size_t first = pass_commands_.size();
    if (auto recorded = record(data, Renderer::Kind::shape, pass_commands_);
        !recorded.has_value()) {
      pass_commands_.resize(first);
      return recorded;
    }
    passes_.push_back(Renderer::OffscreenPass{
        .pass = pass, .first = first, .count = pass_commands_.size() - first});
    return {};
  }

  [[nodiscard]] auto present() -> std::expected<void, ProviderError> override {
    return std::unexpected(ProviderError::unsupported_capability());
  }

  [[nodiscard]] auto seal()
      -> std::expected<GpuFrame::Sealed, ProviderError> override {
    const std::shared_ptr<Renderer> renderer = renderer_.lock();
    if (renderer == nullptr || !open_ || sealed_) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::platform_failure});
    }
    Renderer::Sealing sealing =
        opened_.overlay  ? renderer->seal_overlay(
                               overlay_->slot(opened_.claim.slot), commands_,
                               pass_commands_, passes_, ready_, resolution_)
        : opened_.replay ? renderer->seal_replay(opened_.serial, commands_,
                                                 ready_, resolution_)
                         : renderer->seal(commands_, pass_commands_, passes_,
                                          ready_, resolution_);
    serial_ = sealing.serial;
    pass_holes_ = renderer->pass_holes(pass_commands_, passes_);
    holes_ = std::move(sealing.holes);
    due_ = std::move(sealing.due);
    sealed_ = true;
    return GpuFrame::Sealed{
        .submitter = nullptr,
        .contents = FramePacket::Contents{.plan = plan_,
                                          .serial = sealing.serial,
                                          .frame = opened_.claim.frame,
                                          .slot = opened_.claim.slot}};
  }

  [[nodiscard]] auto submit(const FramePacket& packet, FramePacket::Clock clock)
      -> std::expected<FrameReceipt, ProviderError> override {
    open_ = false;
    const std::shared_ptr<Renderer> renderer = renderer_.lock();
    if (renderer == nullptr) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::platform_failure});
    }
    if (opened_.overlay) {
      return publish(*renderer);
    }
    return renderer->render(
        packet,
        Renderer::Recording{.clear = clear_,
                            .commands = commands_,
                            .passes = passes_,
                            .pass_commands = pass_commands_,
                            .holes = holes_,
                            .pass_holes = pass_holes_,
                            .due = due_,
                            .ready = ready_,
                            .damage = damage_,
                            .resident_indices = resident_indices_,
                            .resolution = std::cref(resolution_),
                            .ticket = opened_.ticket,
                            .replay = opened_.replay},
        clock);
  }

  void abandon(const FramePacket& packet) noexcept override {
    open_ = false;
    if (opened_.overlay) {
      commands_.clear();
      if (const std::shared_ptr<Renderer> renderer = renderer_.lock()) {
        static_cast<void>(publish(*renderer));
      }
      return;
    }
    if (const std::shared_ptr<Renderer> renderer = renderer_.lock()) {
      renderer->abandon(packet, opened_);
    }
  }

 private:
  auto publish(Renderer& renderer)
      -> std::expected<FrameReceipt, ProviderError> {
    const std::shared_ptr<OverlayState> overlay = std::exchange(overlay_, {});
    renderer.registry()->close(opened_.ticket);
    if (overlay == nullptr) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument,
                        ProviderErrorDetail::not_taken_over});
    }
    const std::uint32_t slot = opened_.claim.slot;
    auto submitted = renderer.publish_overlay(*overlay, overlay->slot(slot),
                                              recording(), serial_);
    if (!submitted.has_value()) {
      overlay->ring().abandon(slot);
      return std::unexpected(submitted.error());
    }
    if (!overlay->ring().publish(slot, commands_.empty(), plan_.extent)) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument,
                        ProviderErrorDetail::not_taken_over});
    }
    return FrameReceipt{};
  }

  template <class V>
  auto record(const BasicDrawData<V>& data, Renderer::Kind kind,
              std::vector<Renderer::BlockCommand>& into)
      -> std::expected<void, ProviderError> {
    const std::shared_ptr<Renderer> renderer = renderer_.lock();
    if (renderer == nullptr || sealed_) {
      return std::unexpected(ProviderError{ProviderErrorCode::platform_failure,
                                           ProviderErrorDetail::frame_closed});
    }
    if (opened_.replay) {
      return std::unexpected(ProviderError{ProviderErrorCode::platform_failure,
                                           ProviderErrorDetail::replay_frame});
    }
    if (const auto refused = renderer->registry()->refusal(data.commands)) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument, *refused});
    }
    const bool owned =
        opened_.overlay
            ? owned_by(overlay_->slot(opened_.claim.slot), data.geometry)
            : renderer->owns(data.geometry);
    if (!data.commands.empty() && !owned) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument,
                        ProviderErrorDetail::foreign_geometry});
    }
    for (DrawCommand command : data.commands) {
      command.vertex_offset += data.geometry.first_vertex;
      command.index_offset += data.geometry.first_index;
      into.push_back(Renderer::BlockCommand{
          .pipeline = kind, .block = data.geometry.block, .command = command});
    }
    return {};
  }

  std::weak_ptr<Renderer> renderer_;
  Renderer::Opened opened_{};
  std::optional<Colour> clear_;
  std::optional<ClipRect> damage_;
  FramePlan plan_{};
  std::vector<Renderer::BlockCommand> commands_;
  std::vector<Renderer::OffscreenPass> passes_;
  std::vector<Renderer::BlockCommand> pass_commands_;
  std::vector<Surfaces::Hole> holes_;
  std::vector<Surfaces::Hole> pass_holes_;
  std::vector<Surfaces::Due> due_;
  std::vector<FrameSlot::Wait> ready_;
  Renderer::Resolution resolution_;
  std::optional<std::uint32_t> resident_indices_;
  std::shared_ptr<OverlayState> overlay_;
  GpuSerial serial_{0};
  bool open_{false};
  bool sealed_{false};

  static auto owned_by(const FrameSlot& slot, const FrameGeometry& geometry)
      -> bool {
    return slot.geometry().owns(geometry);
  }

  static auto owned_by(const FrameSlot& slot, const ShapeGeometry& geometry)
      -> bool {
    return slot.shapes().owns(geometry);
  }
};

auto VulkanGpu::OverlayState::begin(const std::shared_ptr<Renderer>& renderer,
                                    const std::shared_ptr<OverlayState>& self)
    -> std::expected<GpuFrame, ProviderError> {
  if (self->ring_.closed()) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument,
                                         ProviderErrorDetail::not_taken_over});
  }
  const std::optional<std::uint32_t> claimed = self->ring_.claim(
      [&self](std::uint32_t index, OverlaySlots::Standing standing) {
        return self->idle(index, standing);
      });
  if (!claimed.has_value()) {
    return std::unexpected(
        ProviderError{ProviderErrorCode::resource_exhausted});
  }
  auto opened = renderer->open_overlay_frame(self->slot(*claimed), *claimed);
  if (!opened.has_value()) {
    self->ring_.abandon(*claimed);
    return std::unexpected(opened.error());
  }
  std::shared_ptr<Frame>& frame = self->frames_[*claimed];
  if (frame == nullptr || frame.use_count() > 1) {
    frame = std::make_shared<Frame>(renderer);
  }
  frame->open(*opened, self);
  return GpuFrame::reused(frame);
}

auto VulkanGpu::Renderer::open_overlay(const std::shared_ptr<Renderer>& self)
    -> std::expected<std::shared_ptr<OverlayState>, ProviderError> {
  const std::scoped_lock submitting{self->submit_mutex_};
  if (self->overlay_ != nullptr && !self->overlay_->ring().closed()) {
    return std::unexpected(ProviderError{ProviderErrorCode::resource_exhausted,
                                         ProviderErrorDetail::overlay_open});
  }
  const VulkanDevice& device = *self->device_;
  if (!device.capabilities().timeline_semaphores) {
    return std::unexpected(ProviderError::unsupported_capability());
  }
  VkDevice const handle = device.handle();
  auto counter = reinterpret_cast<PFN_vkGetSemaphoreCounterValue>(
      vkGetDeviceProcAddr(handle, "vkGetSemaphoreCounterValue"));
  if (counter == nullptr) {
    counter = reinterpret_cast<PFN_vkGetSemaphoreCounterValue>(
        vkGetDeviceProcAddr(handle, "vkGetSemaphoreCounterValueKHR"));
  }
  if (counter == nullptr) {
    return std::unexpected(ProviderError::unsupported_capability());
  }
  const VkSemaphoreTypeCreateInfo typed{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
      .pNext = nullptr,
      .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
      .initialValue = 0};
  const VkSemaphoreCreateInfo info{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = &typed,
      .flags = 0};
  VkSemaphore semaphore = VK_NULL_HANDLE;
  if (const VkResult result =
          vkCreateSemaphore(handle, &info, nullptr, &semaphore);
      result != VK_SUCCESS) {
    return std::unexpected(VulkanStatus::error(result));
  }
  VulkanSemaphoreHandle timeline{{.device = handle, .object = semaphore}};
  std::vector<FrameSlot> slots;
  slots.reserve(OverlayState::slot_count);
  for (std::uint32_t index = 0; index < OverlayState::slot_count; ++index) {
    auto created = FrameSlot::create(device);
    if (!created.has_value()) {
      return std::unexpected(created.error());
    }
    slots.push_back(std::move(*created));
  }
  self->overlay_ = std::make_shared<OverlayState>(std::move(slots),
                                                  std::move(timeline), counter);
  self->overlay_planner_.invalidate();
  return self->overlay_;
}

void VulkanGpu::Renderer::retire(OverlayState& state) noexcept {
  state.ring().close();
}

auto VulkanGpu::Renderer::closed(const OverlayState& state) -> bool {
  return state.ring().closed();
}

void VulkanGpu::Renderer::close_overlay(
    const std::shared_ptr<OverlayState>& state) noexcept {
  const std::scoped_lock submitting{submit_mutex_};
  retire(*state);
  if (overlay_ == state) {
    device_->wait_idle();
    overlay_.reset();
  }
}

auto VulkanGpu::Renderer::record_overlay(OverlayState& state,
                                         VkCommandBuffer buffer,
                                         const FrameTarget& target)
    -> std::expected<std::optional<OverlayFence>, ProviderError> {
  const std::scoped_lock submitting{submit_mutex_};
  if (overlay_.get() != &state || state.ring().closed()) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument,
                                         ProviderErrorDetail::not_taken_over});
  }
  if (target.format != pipelines_.format() || target.view == VK_NULL_HANDLE) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument});
  }
  const auto used = state.ring().use(
      SurfaceSize{.width = SurfaceWidth{target.extent.width},
                  .height = SurfaceHeight{target.extent.height}});
  if (!used.has_value()) {
    return std::unexpected(used.error());
  }
  if (!used->has_value()) {
    return std::nullopt;
  }
  const OverlaySlots::Use& use = **used;
  const OverlayFence fence{.semaphore = state.timeline(), .value = use.value};
  const Frame& frame = *state.frame(use.slot);
  const Recording recording = frame.recording();
  if (recording.commands.empty()) {
    return fence;
  }
  auto framebuffer =
      state.attachment(pipelines_.render_pass(), target, use.value);
  if (!framebuffer.has_value()) {
    return std::unexpected(framebuffer.error());
  }
  const SurfaceSize extent{.width = SurfaceWidth{target.extent.width},
                           .height = SurfaceHeight{target.extent.height}};
  std::span<const gpu::FramePlanner::Step> steps;
  {
    const std::scoped_lock lock{mutex_};
    steps = overlay_planner_.plan(recorded(recording, 0, 1, extent, true));
  }
  std::optional<TimestampQueries> unmeasured;
  Execution run{
      .buffer = buffer,
      .slot = state.slot(use.slot),
      .resident = ResidentBuffers{},
      .recording = recording,
      .clock = unmeasured,
      .image = VulkanTarget::Image{.image = target.image,
                                   .index = 0,
                                   .layout = target.layout,
                                   .signals = false},
      .screen = ClipRect{.x = 0,
                         .y = 0,
                         .width = target.extent.width,
                         .height = target.extent.height},
      .target = target,
      .info = FrameInfo{.serial = GpuSerial{use.value},
                        .predicted_present = frame.planned().predicted_present,
                        .extent = extent,
                        .replay = false,
                        .damage = std::nullopt},
      .plan = frame.planned(),
      .hooked = false,
      .drawn = 0,
      .framebuffer = *framebuffer,
      .prelude = false,
      .screen_pass = true};
  for (const gpu::FramePlanner::Step& step : steps) {
    std::visit([this, &run](const auto& each) { execute(each, run); }, step);
  }
  return fence;
}

VulkanGpu::Overlay::Overlay(std::shared_ptr<Renderer> renderer,
                            std::shared_ptr<OverlayState> state) noexcept
    : renderer_(std::move(renderer)), state_(std::move(state)) {}

VulkanGpu::Overlay::Overlay(Overlay&& other) noexcept = default;

auto VulkanGpu::Overlay::operator=(Overlay&& other) noexcept -> Overlay& {
  if (this != &other) {
    release();
    renderer_ = std::move(other.renderer_);
    state_ = std::move(other.state_);
  }
  return *this;
}

VulkanGpu::Overlay::~Overlay() { release(); }

auto VulkanGpu::Overlay::held() const noexcept -> bool {
  return state_ != nullptr && !state_->ring().closed();
}

auto VulkanGpu::Overlay::frames() const -> FrameSource {
  std::weak_ptr<const OverlaySlots> ring;
  if (state_ != nullptr) {
    ring = std::shared_ptr<const OverlaySlots>{state_, &state_->ring()};
  }
  return FrameSource{
      [renderer = std::weak_ptr<Renderer>{renderer_},
       state = std::weak_ptr<OverlayState>{state_}]()
          -> std::expected<GpuFrame, ProviderError> {
        const std::shared_ptr<Renderer> held_renderer = renderer.lock();
        const std::shared_ptr<OverlayState> held_state = state.lock();
        if (held_renderer == nullptr || held_state == nullptr) {
          return std::unexpected(
              ProviderError{ProviderErrorCode::invalid_argument,
                            ProviderErrorDetail::not_taken_over});
        }
        return OverlayState::begin(held_renderer, held_state);
      },
      std::move(ring)};
}

auto VulkanGpu::Overlay::statistics() const -> Statistics {
  if (state_ == nullptr) {
    return {};
  }
  return state_->statistics();
}

auto VulkanGpu::Overlay::record(VkCommandBuffer buffer,
                                const FrameTarget& target)
    -> std::expected<std::optional<OverlayFence>, ProviderError> {
  if (state_ == nullptr) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument,
                                         ProviderErrorDetail::not_taken_over});
  }
  return renderer_->record_overlay(*state_, buffer, target);
}

void VulkanGpu::Overlay::release() noexcept {
  if (state_ == nullptr) {
    return;
  }
  renderer_->close_overlay(state_);
  state_.reset();
  renderer_.reset();
}

VulkanGpu::VulkanGpu(std::shared_ptr<Renderer> renderer) noexcept
    : renderer_(std::move(renderer)) {}

auto VulkanGpu::create(std::shared_ptr<const VulkanDevice> device,
                       std::unique_ptr<VulkanTarget> target,
                       Diagnostics diagnostics)
    -> std::expected<VulkanGpu, ProviderError> {
  auto created = Renderer::create(std::move(device), std::move(target))
                     .transform([](std::shared_ptr<Renderer> renderer) {
                       return VulkanGpu{std::move(renderer)};
                     });
  if (created.has_value() && diagnostics.has_value()) {
    diagnostics->get().report(Diagnostic{.severity = Severity::info,
                                         .category = "arisa.vulkan",
                                         .message = created->startup_report()});
  }
  return created;
}

auto VulkanGpu::startup_report() const -> std::string {
  return std::format("vulkan gpu: {}", renderer_->device().report());
}

auto VulkanGpu::create(OffscreenTarget::Options offscreen,
                       Diagnostics diagnostics)
    -> std::expected<VulkanGpu, ProviderError> {
  auto instance = VulkanInstance::create();
  if (!instance.has_value()) {
    return std::unexpected(instance.error());
  }
  auto device = VulkanDevice::create(
      std::make_shared<const VulkanInstance>(std::move(*instance)));
  if (!device.has_value()) {
    return std::unexpected(device.error());
  }
  auto target = OffscreenTarget::create(*device, offscreen);
  if (!target.has_value()) {
    return std::unexpected(target.error());
  }
  return create(std::move(*device),
                std::make_unique<OffscreenTarget>(std::move(*target)),
                diagnostics);
}

auto VulkanGpu::create_texture(TextureDescription description)
    -> std::expected<Texture, ProviderError> {
  return renderer_->create_texture(description);
}

auto VulkanGpu::update_texture(Texture& texture, TextureRegion region,
                               PixelRows pixels)
    -> std::expected<void, ProviderError> {
  return renderer_->update_texture(texture, region, pixels);
}

auto VulkanGpu::import_texture(VkImage image, ImportDescription description,
                               support::ScopedCallback released)
    -> std::expected<Imported, ProviderError> {
  return renderer_->import_view(image, description, import_layout,
                                std::move(released));
}

auto VulkanGpu::import_texture(std::span<const VkImage> planes,
                               ImportDescription description,
                               support::ScopedCallback released)
    -> std::expected<Imported, ProviderError> {
  return renderer_->import_planes(planes, description, import_layout,
                                  std::move(released));
}

auto VulkanGpu::import_texture(VkImage image, ImportDescription description,
                               VkImageLayout layout,
                               support::ScopedCallback released)
    -> std::expected<Imported, ProviderError> {
  return renderer_->import_view(image, description, layout,
                                std::move(released));
}

auto VulkanGpu::import_texture(std::span<const VkImage> planes,
                               ImportDescription description,
                               VkImageLayout layout,
                               support::ScopedCallback released)
    -> std::expected<Imported, ProviderError> {
  return renderer_->import_planes(planes, description, layout,
                                  std::move(released));
}

auto VulkanGpu::begin_frame() -> std::expected<GpuFrame, ProviderError> {
  auto opened = renderer_->open_frame();
  if (!opened.has_value()) {
    return std::unexpected(opened.error());
  }
  std::shared_ptr<Frame>& frame = frames_.at(opened->claim.slot);
  if (frame == nullptr || frame.use_count() > 1) {
    frame = std::make_shared<Frame>(renderer_);
  }
  frame->open(*opened);
  return GpuFrame::reused(frame);
}

auto VulkanGpu::begin_replay() -> std::expected<GpuFrame, ProviderError> {
  auto opened = renderer_->open_replay();
  if (!opened.has_value()) {
    return std::unexpected(opened.error());
  }
  if (replay_ == nullptr || replay_.use_count() > 1) {
    replay_ = std::make_shared<Frame>(renderer_);
  }
  replay_->open(*opened);
  return GpuFrame::reused(replay_);
}

auto VulkanGpu::render_queue() noexcept
    -> std::optional<std::reference_wrapper<RenderQueue>> {
  return std::ref(renderer_->queue());
}

auto VulkanGpu::take_over() -> std::expected<Takeover, ProviderError> {
  return Renderer::take_over(renderer_).transform([this](GpuTakeover taken) {
    return Takeover{renderer_, std::move(taken)};
  });
}

auto VulkanGpu::takeover_counters() const -> GpuTakeover::Counters {
  return renderer_->takeover_counters();
}

VulkanGpu::Takeover::Takeover(std::shared_ptr<Renderer> renderer,
                              GpuTakeover taken) noexcept
    : renderer_(std::move(renderer)), taken_(std::move(taken)) {}

VulkanGpu::Takeover::Takeover(Takeover&& other) noexcept = default;

auto VulkanGpu::Takeover::operator=(Takeover&& other) noexcept
    -> Takeover& = default;

VulkanGpu::Takeover::~Takeover() = default;

auto VulkanGpu::Takeover::device() const noexcept
    -> std::optional<std::reference_wrapper<const VulkanDevice>> {
  if (!taken_.held()) {
    return std::nullopt;
  }
  return std::cref(renderer_->device());
}

auto VulkanGpu::Takeover::image_count() const noexcept -> std::uint32_t {
  if (!taken_.held()) {
    return 0;
  }
  return renderer_->lent_images();
}

auto VulkanGpu::Takeover::acquire(VkSemaphore acquired)
    -> std::expected<LentImage, ProviderError> {
  if (!taken_.held()) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument,
                                         ProviderErrorDetail::not_taken_over});
  }
  return renderer_->lent_acquire(acquired);
}

auto VulkanGpu::Takeover::present(std::uint32_t index)
    -> std::expected<void, ProviderError> {
  if (!taken_.held()) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument,
                                         ProviderErrorDetail::not_taken_over});
  }
  return renderer_->lent_present(index);
}

void VulkanGpu::Takeover::set_swap_interval(SwapInterval interval) {
  if (taken_.held()) {
    renderer_->lent_interval(interval);
  }
}

auto VulkanGpu::Takeover::overlay() -> std::expected<Overlay, ProviderError> {
  if (!taken_.held()) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument,
                                         ProviderErrorDetail::not_taken_over});
  }
  return Renderer::open_overlay(renderer_).transform(
      [this](std::shared_ptr<OverlayState> state) {
        return Overlay{renderer_, std::move(state)};
      });
}

auto VulkanGpu::Takeover::presents() const noexcept -> std::uint64_t {
  return renderer_->lent_presents();
}

auto VulkanGpu::capture() -> std::expected<Image, ProviderError> {
  return renderer_->capture();
}

auto VulkanGpu::resources() const -> GpuResources {
  return renderer_->resources();
}

auto VulkanGpu::timings() const -> GpuTimings { return renderer_->timings(); }

auto VulkanGpu::frame_schedule() noexcept
    -> std::optional<std::reference_wrapper<FrameSchedule>> {
  return std::ref(renderer_->hooks().schedule());
}

auto VulkanGpu::publications() const noexcept
    -> std::optional<std::reference_wrapper<const TexturePublications>> {
  return std::cref(
      static_cast<const TexturePublications&>(*renderer_->publisher()));
}

auto VulkanGpu::add_frame_hook(FrameStage stage, FrameHookFunction function,
                               FrameHookOptions options) -> FrameHook {
  return renderer_->hooks().add(stage, std::move(function), options);
}

auto VulkanGpu::add_surface(SurfaceOptions options, SurfaceFunction function)
    -> std::expected<GpuSurface, ProviderError> {
  return renderer_->surfaces().add(options, std::move(function));
}

auto VulkanGpu::surfaces() noexcept
    -> std::optional<std::reference_wrapper<SurfaceDirectory>> {
  return std::ref(renderer_->surfaces().directory());
}

auto VulkanGpu::completed(GpuSerial serial) const -> bool {
  return renderer_->completed(serial);
}

auto VulkanGpu::last_damage() const -> std::optional<ClipRect> {
  return renderer_->last_damage();
}

auto VulkanGpu::statistics() const -> Statistics {
  return renderer_->statistics();
}

auto VulkanGpu::device() const noexcept -> const VulkanDevice& {
  return renderer_->device();
}

auto VulkanGpu::frames_presented() const -> std::uint64_t {
  return renderer_->statistics().frames_presented;
}

auto VulkanGpu::missed_vsyncs() const -> std::uint64_t {
  return renderer_->missed_vsyncs();
}

auto VulkanGpu::inventory() const -> GpuInventory {
  return GpuInventory{.backend = "vulkan",
                      .report = startup_report(),
                      .textures = renderer_->registry()->memory(),
                      .frames_presented = frames_presented(),
                      .missed_vsyncs = missed_vsyncs()};
}

auto VulkanGpu::live_textures() const -> std::size_t {
  return renderer_->registry()->size();
}

}
