#include <arisa/provider/gpu/frame_stopwatch.hpp>
#include <arisa/provider/gpu/overlay_slots.hpp>
#include <arisa/provider/headless/null_gpu.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <format>
#include <functional>
#include <limits>
#include <mutex>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace arisa::provider::headless {

class NullGpu::Frame final : public GpuFrame::Implementation {
 public:
  Frame(std::shared_ptr<Registry> registry, std::shared_ptr<Counters> counters,
        std::shared_ptr<Resident> resident, std::shared_ptr<Hooks> hooks,
        std::shared_ptr<Publisher> publisher,
        std::shared_ptr<Surfaces> surfaces, std::shared_ptr<FrameSlots> slots,
        std::shared_ptr<Uploads> uploads,
        std::shared_ptr<RenderQueue> queue) noexcept
      : registry_(std::move(registry)),
        counters_(std::move(counters)),
        resident_(std::move(resident)),
        hooks_(std::move(hooks)),
        publisher_(std::move(publisher)),
        surfaces_(std::move(surfaces)),
        slots_(std::move(slots)),
        uploads_(std::move(uploads)),
        queue_(std::move(queue)) {}

  Frame(const Frame&) = delete;
  auto operator=(const Frame&) -> Frame& = delete;
  Frame(Frame&&) = delete;
  auto operator=(Frame&&) -> Frame& = delete;

  ~Frame() override { finish(); }

  void open(std::optional<FrameSlots::Claim> claim) noexcept {
    {
      const std::scoped_lock lock{counters_->mutex};
      extent_ = counters_->extent;
    }
    claim_ = claim;
    serial_ = GpuSerial{hooks_->schedule().serial().value() + 1};
    ticket_ = registry_->open();
    replaying_ = false;
    textured_.clear();
    replay_indices_.clear();
    replay_vertices_ = 0;
    replay_generation_ = 0;
    clear_.reset();
    effects_ = std::chrono::nanoseconds{0};
    passes_ = 0;
    sealed_ = false;
    presented_ = false;
    vertex_blocks_ = 0;
    shape_blocks_ = 0;
    shape_draws_.clear();
    damage_.reset();
    plan_ = FramePlan{};
    hooks_time_.reset();
    taken_.clear();
    taken_bytes_.clear();
    open_ = true;
  }

  void open_replay(GpuSerial serial) noexcept {
    open(std::nullopt);
    serial_ = serial;
    replaying_ = true;
  }

  void finish() noexcept override {
    if (!open_ || sealed_) {
      return;
    }
    open_ = false;
    discard();
    if (claim_.has_value()) {
      slots_->abandon();
    }
    if (replaying_) {
      settle(serial_);
    }
    close_ticket();
  }

  [[nodiscard]] auto allocate_resident(std::uint32_t vertex_count,
                                       std::uint32_t index_count)
      -> std::expected<ResidentShapes, ProviderError> override {
    counters_->geometry_allocations.fetch_add(1);
    counters_->geometry_bytes.fetch_add((vertex_count * sizeof(ShapeVertex)) +
                                        (index_count * sizeof(Index)));
    resident_->vertices.resize(vertex_count);
    resident_->indices.resize(index_count);
    return ResidentShapes{
        .geometry = ShapeGeometry{.vertices = resident_->vertices,
                                  .indices = resident_->indices,
                                  .block = ResidentShapes::block,
                                  .first_vertex = 0,
                                  .first_index = 0},
        .generation = resident_->generation};
  }

  void touch(VertexRange range) override {
    counters_->vertex_uploads.fetch_add(range.count);
    counters_->vertex_uploaded_bytes.fetch_add(
        static_cast<std::size_t>(range.count) * sizeof(ShapeVertex));
  }

  void damage(ClipRect region) override {
    damage_ = damage_.has_value() ? unite(*damage_, region) : region;
  }

  void plan(FramePlan plan) override { plan_ = plan; }

  [[nodiscard]] auto allocate_geometry(std::uint32_t vertex_count,
                                       std::uint32_t index_count)
      -> std::expected<FrameGeometry, ProviderError> override {
    counters_->geometry_allocations.fetch_add(1);
    counters_->geometry_bytes.fetch_add((vertex_count * sizeof(Vertex)) +
                                        (index_count * sizeof(Index)));
    const std::size_t block = vertex_blocks_++;
    std::vector<Vertex>& vertices = reuse(vertices_, block, vertex_count);
    std::vector<Index>& indices = reuse(indices_, block, index_count);
    return FrameGeometry{.vertices = vertices,
                         .indices = indices,
                         .block = static_cast<std::uint32_t>(block),
                         .first_vertex = 0,
                         .first_index = 0};
  }

  [[nodiscard]] auto allocate_shapes(std::uint32_t vertex_count,
                                     std::uint32_t index_count)
      -> std::expected<ShapeGeometry, ProviderError> override {
    counters_->geometry_allocations.fetch_add(1);
    counters_->geometry_bytes.fetch_add((vertex_count * sizeof(ShapeVertex)) +
                                        (index_count * sizeof(Index)));
    const std::size_t block = shape_blocks_++;
    std::vector<ShapeVertex>& vertices =
        reuse(shape_vertices_, block, vertex_count);
    std::vector<Index>& indices = reuse(shape_indices_, block, index_count);
    return ShapeGeometry{.vertices = vertices,
                         .indices = indices,
                         .block = static_cast<std::uint32_t>(block),
                         .first_vertex = 0,
                         .first_index = 0};
  }

  void clear(Colour colour) override {
    clear_ = colour;
    const std::scoped_lock lock{counters_->mutex};
    counters_->last_clear = colour;
  }

  [[nodiscard]] auto draw(const ShapeDrawData& data)
      -> std::expected<void, ProviderError> override {
    if (const auto refused = refusal(data)) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument, *refused});
    }
    if (rejects(false)) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument,
                        ProviderErrorDetail::unknown_texture});
    }
    counters_->draws.fetch_add(1);
    counters_->commands.fetch_add(data.commands.size());
    keep(data.geometry.block, data.commands, std::nullopt);
    return {};
  }

  [[nodiscard]] auto draw(const ShapeDrawData& data, const RenderPass& pass)
      -> std::expected<void, ProviderError> override {
    const auto target = registry_->find(pass.target);
    if (const auto refused = refusal(data)) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument, *refused});
    }
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
    if (rejects(true)) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument,
                        ProviderErrorDetail::unknown_target});
    }
    counters_->passes.fetch_add(1);
    counters_->draws.fetch_add(1);
    counters_->commands.fetch_add(data.commands.size());
    ++passes_;
    keep(data.geometry.block, data.commands, pass);
    return {};
  }

  [[nodiscard]] auto draw(const DrawData& data)
      -> std::expected<void, ProviderError> override {
    if (!owns(data.geometry)) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument});
    }
    const bool known =
        std::ranges::all_of(data.commands, [this](const DrawCommand& command) {
          return registry_->find(command.texture).has_value();
        });
    if (!known) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument});
    }
    counters_->draws.fetch_add(1);
    counters_->commands.fetch_add(data.commands.size());
    textured_.insert(textured_.end(), data.commands.begin(),
                     data.commands.end());
    return {};
  }

  [[nodiscard]] auto replay(const ReplayDrawData& data)
      -> std::expected<void, ProviderError> override {
    if (!replaying_ || sealed_) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument});
    }
    if (const auto refused = registry_->refusal(data.commands)) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument, *refused});
    }
    replay_indices_.assign(data.indices.begin(), data.indices.end());
    replay_vertices_ = data.vertex_count;
    replay_generation_ = data.generation;
    counters_->draws.fetch_add(1);
    counters_->commands.fetch_add(data.commands.size());
    keep(ResidentShapes::block, data.commands, std::nullopt);
    return {};
  }

  [[nodiscard]] auto present() -> std::expected<void, ProviderError> override {
    return std::unexpected(ProviderError::unsupported_capability());
  }

  [[nodiscard]] auto seal()
      -> std::expected<GpuFrame::Sealed, ProviderError> override {
    if (!open_ || sealed_) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument});
    }
    if (!replaying_) {
      serial_ = hooks_->schedule().next_serial();
      const std::scoped_lock lock{uploads_->mutex};
      std::swap(taken_, uploads_->pending);
      std::swap(taken_bytes_, uploads_->bytes);
      const std::scoped_lock planning{counters_->mutex};
      counters_->planner.seal();
    }
    for (const ShapeDraw& draw : shape_draws_.active()) {
      publisher_->sampled(serial_, draw.commands);
    }
    publisher_->sampled(serial_, textured_);
    sealed_ = true;
    std::uint64_t number = 0;
    std::uint32_t slot = 0;
    if (claim_.has_value()) {
      number = claim_->frame;
      slot = claim_->slot;
      slots_->seal();
    }
    return GpuFrame::Sealed{
        .submitter = nullptr,
        .contents = FramePacket::Contents{
            .plan = plan_, .serial = serial_, .frame = number, .slot = slot}};
  }

  [[nodiscard]] auto submit(const FramePacket& packet, FramePacket::Clock clock)
      -> std::expected<FrameReceipt, ProviderError> override {
    const std::optional<RenderQueue::Lease> used = queue_->submit();
    const FrameStopwatch stopwatch{clock};
    const std::chrono::nanoseconds began = stopwatch.now();
    open_ = false;
    if (!used.has_value()) {
      taken_.clear();
      taken_bytes_.clear();
      if (claim_.has_value()) {
        slots_->release(packet.frame());
      }
      settle(packet.serial());
      close_ticket();
      return std::unexpected(
          ProviderError{ProviderErrorCode::resource_exhausted,
                        ProviderErrorDetail::taken_over});
    }
    for (const Upload& upload : taken_) {
      if (upload.clear) {
        upload.canvas->fill(transparent);
      }
      upload.canvas->upload(
          upload.format, upload.region,
          PixelRows{std::span<const std::byte>{taken_bytes_}.subspan(
              upload.offset, upload.size)});
    }
    taken_.clear();
    taken_bytes_.clear();
    const std::span<const ShapeDraw> draws = shape_draws_.active();
    if (replaying_) {
      const std::scoped_lock lock{counters_->mutex};
      Snapshot& snapshot = counters_->snapshot;
      if (snapshot.generation != replay_generation_ ||
          snapshot.vertices.size() < replay_vertices_) {
        settle(packet.serial());
        close_ticket();
        return std::unexpected(ProviderError{ProviderErrorCode::not_found});
      }
      snapshot.indices.assign(replay_indices_.begin(), replay_indices_.end());
    } else if (std::ranges::any_of(draws, [](const ShapeDraw& draw) {
                 return draw.block == ResidentShapes::block;
               })) {
      const std::scoped_lock lock{counters_->mutex};
      Snapshot& snapshot = counters_->snapshot;
      snapshot.vertices.reserve(resident_->vertices.capacity());
      snapshot.indices.reserve(resident_->indices.capacity());
      snapshot.vertices.assign(resident_->vertices.begin(),
                               resident_->vertices.end());
      snapshot.indices.assign(resident_->indices.begin(),
                              resident_->indices.end());
      snapshot.generation = resident_->generation;
    }
    if (claim_.has_value()) {
      slots_->release(packet.frame());
    }
    presented_ = true;
    counters_->presented.fetch_add(1);
    const FrameInfo info{.serial = packet.serial(),
                         .predicted_present = plan_.predicted_present,
                         .extent = extent_.value_or(SurfaceSize{}),
                         .replay = plan_.replay,
                         .damage = damage_};
    interface_.reset();
    for (const gpu::FramePlanner::Step& step : planned(draws)) {
      std::visit([this, draws,
                  &info](const auto& each) { execute(each, draws, info); },
                 step);
    }
    const std::optional<std::chrono::nanoseconds> interface = interface_;
    std::unique_lock lock{counters_->mutex};
    if (!interface.has_value()) {
      run_hooks(FrameStage::before_ui, std::nullopt, info);
      for (const ShapeDraw& draw : draws) {
        if (!draw.pass.has_value()) {
          Surfaces::interleave(
              std::span<const DrawCommand>{draw.commands},
              std::span<const Surfaces::Hole>{draw.holes},
              [](std::span<const DrawCommand>) {},
              [&](const Surfaces::Underlay& underlay) {
                place_underlay(std::nullopt, underlay,
                               clear_.value_or(opaque_black), info);
              });
        }
      }
      run_hooks(FrameStage::after_ui, std::nullopt, info);
    }
    ShapeRecord& record = counters_->last_shapes;
    record.vertices.clear();
    record.indices.clear();
    record.commands.clear();
    Snapshot& snapshot = counters_->snapshot;
    snapshot.draws.clear();
    for (const ShapeDraw& draw : draws) {
      const bool offscreen = draw.pass.has_value();
      if (draw.block == ResidentShapes::block) {
        ResidentDraw& kept = snapshot.draws.next(draw.commands.size());
        kept.offscreen = offscreen;
        kept.commands.assign(draw.commands.begin(), draw.commands.end());
        continue;
      }
      if (offscreen) {
        continue;
      }
      const auto base = static_cast<std::uint32_t>(record.vertices.size());
      const auto first = static_cast<std::uint32_t>(record.indices.size());
      record.vertices.insert(record.vertices.end(),
                             shape_vertices_[draw.block].begin(),
                             shape_vertices_[draw.block].end());
      record.indices.insert(record.indices.end(),
                            shape_indices_[draw.block].begin(),
                            shape_indices_[draw.block].end());
      for (DrawCommand command : draw.commands) {
        command.vertex_offset += base;
        command.index_offset += first;
        record.commands.push_back(command);
      }
    }
    counters_->timings = GpuTimings{
        .effects = passes_ > 0 ? std::optional{effects_} : std::nullopt,
        .interface = interface,
        .passes = passes_,
        .measured_on_gpu = false,
        .hooks = hooks_time_};
    FrameReceipt receipt{.record = std::chrono::nanoseconds{0},
                         .wait = std::chrono::nanoseconds{0},
                         .displayed = display(stopwatch),
                         .image = std::nullopt};
    if (plan_.capture && counters_->last_image.has_value()) {
      receipt.image = *counters_->last_image;
      counters_->captures.fetch_add(1);
    }
    lock.unlock();
    pace(plan_.swap_interval);
    settle(packet.serial());
    const std::uint64_t held = counters_->held.load();
    if (info.serial.value() > held) {
      publisher_->completed(GpuSerial{info.serial.value() - held});
    }
    close_ticket();
    receipt.record = stopwatch.since(began);
    return receipt;
  }

  void abandon(const FramePacket& packet) noexcept override {
    open_ = false;
    for (const Upload& upload : taken_) {
      if (upload.clear) {
        upload.canvas->fill(transparent);
      }
    }
    taken_.clear();
    taken_bytes_.clear();
    discard();
    std::span<const gpu::FramePlanner::Step> owed;
    {
      const std::scoped_lock lock{counters_->mutex};
      owed = counters_->planner.owed();
    }
    for (const gpu::FramePlanner::Step& step : owed) {
      if (const auto* clear =
              std::get_if<gpu::FramePlanner::ClearTexture>(&step)) {
        execute(*clear, {}, FrameInfo{});
      }
    }
    if (claim_.has_value()) {
      slots_->release(packet.frame());
    }
    settle(packet.serial());
    close_ticket();
  }

 private:
  static constexpr Colour opaque_black{
      .red = 0.0F, .green = 0.0F, .blue = 0.0F, .alpha = 1.0F};
  static constexpr Colour transparent{
      .red = 0.0F, .green = 0.0F, .blue = 0.0F, .alpha = 0.0F};

  struct ShapeDraw {
    std::uint32_t block{0};
    std::optional<RenderPass> pass;
    Colour hole{};
    std::vector<DrawCommand> commands;
    std::vector<Surfaces::Hole> holes;
    std::vector<Surfaces::Due> due;
  };

  void discard() noexcept {
    counters_->discarded.fetch_add(1);
    const std::scoped_lock lock{counters_->mutex};
    counters_->planner.invalidate();
  }

  void keep(std::uint32_t block, std::span<const DrawCommand> commands,
            std::optional<RenderPass> pass) {
    ShapeDraw& recorded = shape_draws_.next(commands.size());
    recorded.block = block;
    recorded.pass = pass;
    recorded.hole = clear_.value_or(opaque_black);
    recorded.commands.assign(commands.begin(), commands.end());
    recorded.holes.clear();
    recorded.due.clear();
    if (surfaces_->installed()) {
      recorded.holes = surfaces_->holes(commands, std::identity{});
      recorded.due = surfaces_->due(commands, serial_, std::identity{});
    }
  }

  void close_ticket() noexcept {
    if (ticket_.has_value()) {
      registry_->close(*ticket_);
      ticket_.reset();
    }
  }

  void settle(GpuSerial serial) noexcept {
    std::uint64_t seen = counters_->submitted.load();
    while (serial.value() > seen &&
           !counters_->submitted.compare_exchange_weak(seen, serial.value())) {
    }
  }

  auto display(const FrameStopwatch& stopwatch)
      -> std::optional<std::chrono::nanoseconds> {
    const std::chrono::nanoseconds period{counters_->display_period.load()};
    if (period <= std::chrono::nanoseconds{0} || !stopwatch.timed()) {
      return std::nullopt;
    }
    const std::chrono::nanoseconds now = stopwatch.now();
    std::chrono::nanoseconds at = ((now / period) + 1) * period;
    if (counters_->displayed.has_value()) {
      at = std::max(
          at, *counters_->displayed +
                  (period * std::max<std::uint32_t>(plan_.swap_interval, 1)));
    }
    counters_->displayed = at;
    return at;
  }

  void pace(std::uint32_t interval) noexcept {
    if (counters_->swap_interval.exchange(interval) != interval) {
      counters_->swap_interval_changes.fetch_add(1);
    }
  }

  auto planned(std::span<const ShapeDraw> draws)
      -> std::span<const gpu::FramePlanner::Step> {
    recorded_passes_.clear();
    pass_draws_.clear();
    pass_targets_.clear();
    batches_.clear();
    batch_draws_.clear();
    for (const auto& [index, draw] : std::views::enumerate(draws)) {
      const auto at = static_cast<std::size_t>(index);
      const gpu::FramePlanner::Batch batch{.commands = draw.commands.size(),
                                           .holes = draw.holes};
      if (!draw.pass.has_value()) {
        batches_.push_back(batch);
        batch_draws_.push_back(at);
        continue;
      }
      const auto target = registry_->find(draw.pass->target);
      std::shared_ptr<Canvas> canvas =
          target.has_value() ? target->native : nullptr;
      std::optional<SurfaceSize> size;
      if (canvas != nullptr) {
        size = SurfaceSize{.width = SurfaceWidth{canvas->width()},
                           .height = SurfaceHeight{canvas->height()}};
      }
      recorded_passes_.push_back(gpu::FramePlanner::RecordedPass{
          .pass = *draw.pass, .target = size, .batch = batch});
      pass_draws_.push_back(at);
      pass_targets_.push_back(std::move(canvas));
    }
    const std::scoped_lock lock{counters_->mutex};
    return counters_->planner.plan(gpu::FramePlanner::RecordedFrame{
        .passes = recorded_passes_, .screen = screen()});
  }

  auto screen() -> std::optional<gpu::FramePlanner::RecordedScreen> {
    if (!extent_.has_value() || extent_->empty()) {
      return std::nullopt;
    }
    const std::uint32_t width = extent_->width.value();
    const std::uint32_t height = extent_->height.value();
    std::vector<Buffer>& buffers = counters_->buffers;
    buffer_ = counters_->next_buffer % buffers.size();
    counters_->next_buffer = (counters_->next_buffer + 1) % buffers.size();
    Buffer& buffer = buffers[buffer_];
    bool recreated = false;
    if (!buffer.canvas.has_value() || buffer.canvas->width() != width ||
        buffer.canvas->height() != height) {
      buffer.canvas.emplace(width, height);
      recreated = true;
    }
    return gpu::FramePlanner::RecordedScreen{
        .image = buffer_,
        .images = buffers.size(),
        .size = *extent_,
        .damage = damage_,
        .whole = recreated || !counters_->partial || hooks_->installed(),
        .batches = batches_};
  }

  void execute(const gpu::FramePlanner::ClearTexture& step,
               std::span<const ShapeDraw>, const FrameInfo&) {
    const auto target = registry_->find(step.texture);
    if (target.has_value() && target->native != nullptr) {
      target->native->fill(transparent);
    }
  }

  void execute(const gpu::FramePlanner::RenderSurfaces&,
               std::span<const ShapeDraw> draws, const FrameInfo&) {
    for (const ShapeDraw& draw : draws) {
      render_surfaces(draw.due);
    }
  }

  void execute(const gpu::FramePlanner::SkippedPass&,
               std::span<const ShapeDraw>, const FrameInfo&) {}

  void execute(const gpu::FramePlanner::OffscreenPass& step,
               std::span<const ShapeDraw> draws, const FrameInfo&) {
    const ShapeDraw& draw = draws[pass_draws_[step.pass]];
    Canvas& layer = *pass_targets_[step.pass];
    const auto started = std::chrono::steady_clock::now();
    if (step.clear.has_value()) {
      layer.fill(*step.clear);
    }
    for (const gpu::FramePlanner::Segment& segment : step.segments) {
      std::visit(
          [&](const auto& part) {
            using Part = std::remove_cvref_t<decltype(part)>;
            if constexpr (std::is_same_v<Part, gpu::FramePlanner::DrawRange>) {
              SoftwareRaster::draw(
                  layer, step.region, vertices_of(draw.block),
                  indices_of(draw.block),
                  std::span<const DrawCommand>{draw.commands}.subspan(
                      part.first, part.count),
                  lookup());
            } else {
              layer.fill(part.scissor, draw.hole);
            }
          },
          segment);
    }
    effects_ += std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - started);
  }

  void execute(const gpu::FramePlanner::ScreenPass& step,
               std::span<const ShapeDraw> draws, const FrameInfo& info) {
    const std::scoped_lock lock{counters_->mutex};
    if (buffer_ >= counters_->buffers.size() ||
        !counters_->buffers[buffer_].canvas.has_value()) {
      return;
    }
    const auto started = std::chrono::steady_clock::now();
    SoftwareRaster::Canvas& canvas = *counters_->buffers[buffer_].canvas;
    const ClipRect region = step.region;
    counters_->last_damage =
        step.whole ? std::nullopt : std::optional<ClipRect>{region};
    counters_->damaged_pixels =
        static_cast<std::size_t>(region.width) * region.height;
    const Colour clear = clear_.value_or(opaque_black);
    if (step.whole) {
      canvas.fill(clear);
    } else if (step.painted()) {
      canvas.fill(region, clear);
    }
    run_hooks(FrameStage::before_ui, std::ref(canvas), info);
    const PassRegion pass{.x = 0.0F,
                          .y = 0.0F,
                          .width = static_cast<float>(canvas.width()),
                          .height = static_cast<float>(canvas.height())};
    for (const gpu::FramePlanner::Segment& segment : step.segments) {
      std::visit(
          [&](const auto& part) {
            using Part = std::remove_cvref_t<decltype(part)>;
            if constexpr (std::is_same_v<Part, gpu::FramePlanner::DrawRange>) {
              const ShapeDraw& draw = draws[batch_draws_[part.batch]];
              clip(std::span<const DrawCommand>{draw.commands}.subspan(
                       part.first, part.count),
                   region);
              SoftwareRaster::draw(canvas, pass, vertices_of(draw.block),
                                   indices_of(draw.block), clipped_, lookup());
            } else {
              Surfaces::Underlay underlay = part.underlay;
              underlay.scissor = part.scissor;
              place_underlay(std::ref(canvas), underlay, clear, info);
            }
          },
          segment);
    }
    run_hooks(FrameStage::after_ui, std::ref(canvas), info);
    if (auto made = canvas.image(); made.has_value()) {
      counters_->last_image = std::move(*made);
    }
    interface_ = std::chrono::duration_cast<std::chrono::nanoseconds>(
                     std::chrono::steady_clock::now() - started) -
                 hooks_time_.value_or(std::chrono::nanoseconds{0});
  }

  static auto unite(ClipRect a, ClipRect b) noexcept -> ClipRect {
    if (a.width == 0 || a.height == 0) {
      return b;
    }
    if (b.width == 0 || b.height == 0) {
      return a;
    }
    const std::uint32_t left = std::min(a.x, b.x);
    const std::uint32_t top = std::min(a.y, b.y);
    const std::uint32_t right = std::max(a.x + a.width, b.x + b.width);
    const std::uint32_t bottom = std::max(a.y + a.height, b.y + b.height);
    return ClipRect{
        .x = left, .y = top, .width = right - left, .height = bottom - top};
  }

  void run_hooks(FrameStage stage, FrameCanvas canvas, const FrameInfo& info) {
    if (!hooks_->installed()) {
      return;
    }
    const auto started = std::chrono::steady_clock::now();
    if (hooks_->run(stage, canvas, info)) {
      hooks_time_ = hooks_time_.value_or(std::chrono::nanoseconds{0}) +
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() - started);
    }
  }

  void render_surfaces(std::span<const Surfaces::Due> due) {
    if (due.empty()) {
      return;
    }
    const auto started = std::chrono::steady_clock::now();
    bool ran = false;
    for (const Surfaces::Due& each : due) {
      const auto target = registry_->find(each.texture);
      FrameCanvas canvas;
      if (target.has_value() && target->native != nullptr) {
        canvas = std::ref(*target->native);
      }
      const SurfaceFrame frame{
          .surface = each.surface,
          .mode = SurfaceMode::texture,
          .frame =
              FrameInfo{
                  .serial = serial_,
                  .predicted_present = plan_.predicted_present,
                  .extent = SurfaceSize{.width = SurfaceWidth{each.width},
                                        .height = SurfaceHeight{each.height}},
                  .replay = plan_.replay,
                  .damage = damage_},
          .viewport = PassRegion{.x = 0.0F,
                                 .y = 0.0F,
                                 .width = static_cast<float>(each.width),
                                 .height = static_cast<float>(each.height)},
          .scissor = ClipRect{
              .x = 0, .y = 0, .width = each.width, .height = each.height}};
      ran = surfaces_->run(each.surface, canvas, frame) || ran;
    }
    if (ran) {
      hooks_time_ = hooks_time_.value_or(std::chrono::nanoseconds{0}) +
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() - started);
    }
  }

  void place_underlay(FrameCanvas canvas, const Surfaces::Underlay& underlay,
                      Colour clear, const FrameInfo& info) {
    if (canvas.has_value() && underlay.scissor.width > 0 &&
        underlay.scissor.height > 0) {
      canvas->get().fill(underlay.scissor, clear);
    }
    static_cast<void>(
        surfaces_->run(underlay.surface, canvas,
                       SurfaceFrame{.surface = underlay.surface,
                                    .mode = SurfaceMode::underlay,
                                    .frame = info,
                                    .viewport = underlay.viewport,
                                    .scissor = underlay.scissor}));
  }

  void clip(std::span<const DrawCommand> commands, ClipRect region) {
    clipped_.clear();
    for (DrawCommand command : commands) {
      command.clip = Surfaces::intersect(command.clip, region);
      if (command.clip.width > 0 && command.clip.height > 0) {
        clipped_.push_back(command);
      }
    }
  }

  auto vertices_of(std::uint32_t block) const -> std::span<const ShapeVertex> {
    if (block == ResidentShapes::block) {
      return counters_->snapshot.vertices;
    }
    return shape_vertices_[block];
  }

  auto indices_of(std::uint32_t block) const -> std::span<const Index> {
    if (block == ResidentShapes::block) {
      return counters_->snapshot.indices;
    }
    return shape_indices_[block];
  }

  template <class T>
  static auto reuse(std::deque<std::vector<T>>& blocks, std::size_t block,
                    std::uint32_t count) -> std::vector<T>& {
    if (block == blocks.size()) {
      blocks.emplace_back();
    }
    std::vector<T>& reused = blocks[block];
    reused.clear();
    reused.resize(count);
    return reused;
  }

  auto lookup() const -> SoftwareRaster::Lookup {
    return [registry = registry_](TextureId id) -> SoftwareRaster::Source {
      const auto found = registry->find(id);
      if (!found.has_value()) {
        return {};
      }
      return SoftwareRaster::Source{.canvas = found->native.get(),
                                    .filter = found->description.filter};
    };
  }

  auto rejects(bool pass) -> bool {
    if (counters_->reject_count.load() == 0 ||
        counters_->reject_passes.load() != pass) {
      return false;
    }
    const std::size_t seen = counters_->reject_seen.fetch_add(1);
    const std::size_t skipped = counters_->reject_skipped.load();
    return seen >= skipped && seen - skipped < counters_->reject_count.load();
  }

  auto refusal(const ShapeDrawData& data) const
      -> std::optional<ProviderErrorDetail> {
    if (!owned(data)) {
      return ProviderErrorDetail::foreign_geometry;
    }
    return registry_->refusal(data.commands);
  }

  auto owned(const ShapeDrawData& data) const -> bool {
    const bool resident =
        data.geometry.block == ResidentShapes::block &&
        data.geometry.vertices.data() == resident_->vertices.data() &&
        data.geometry.indices.data() == resident_->indices.data();
    const bool owned =
        resident || (data.geometry.block < shape_blocks_ &&
                     data.geometry.vertices.data() ==
                         shape_vertices_[data.geometry.block].data() &&
                     data.geometry.indices.data() ==
                         shape_indices_[data.geometry.block].data());
    return owned;
  }

  auto owns(const FrameGeometry& geometry) const noexcept -> bool {
    return geometry.block < vertex_blocks_ &&
           geometry.vertices.data() == vertices_[geometry.block].data() &&
           geometry.vertices.size() == vertices_[geometry.block].size() &&
           geometry.indices.data() == indices_[geometry.block].data() &&
           geometry.indices.size() == indices_[geometry.block].size();
  }

  std::shared_ptr<Registry> registry_;
  std::shared_ptr<Counters> counters_;
  std::shared_ptr<Resident> resident_;
  std::shared_ptr<Hooks> hooks_;
  std::shared_ptr<Publisher> publisher_;
  std::shared_ptr<Surfaces> surfaces_;
  std::shared_ptr<FrameSlots> slots_;
  std::shared_ptr<Uploads> uploads_;
  std::shared_ptr<RenderQueue> queue_;
  std::optional<FrameSlots::Claim> claim_;
  std::optional<RegistryTicket> ticket_;
  GpuSerial serial_{0};
  std::vector<DrawCommand> textured_;
  std::vector<Index> replay_indices_;
  std::uint32_t replay_vertices_{0};
  std::uint64_t replay_generation_{0};
  bool replaying_{false};
  std::vector<DrawCommand> clipped_;
  std::vector<gpu::FramePlanner::RecordedPass> recorded_passes_;
  std::vector<std::size_t> pass_draws_;
  std::vector<std::shared_ptr<Canvas>> pass_targets_;
  std::vector<gpu::FramePlanner::Batch> batches_;
  std::vector<std::size_t> batch_draws_;
  std::size_t buffer_{0};
  std::optional<std::chrono::nanoseconds> interface_;
  std::vector<Upload> taken_;
  std::vector<std::byte> taken_bytes_;
  std::optional<ClipRect> damage_;
  std::deque<std::vector<Vertex>> vertices_;
  std::deque<std::vector<Index>> indices_;
  std::deque<std::vector<ShapeVertex>> shape_vertices_;
  std::deque<std::vector<Index>> shape_indices_;
  DrawSlots<ShapeDraw> shape_draws_;
  std::optional<SurfaceSize> extent_;
  std::optional<Colour> clear_;
  FramePlan plan_{};
  std::optional<std::chrono::nanoseconds> hooks_time_;
  std::chrono::nanoseconds effects_{0};
  std::uint32_t passes_{0};
  std::size_t vertex_blocks_{0};
  std::size_t shape_blocks_{0};
  bool sealed_{false};
  bool presented_{false};
  bool open_{false};
};

class NullGpu::OverlayState {
 public:
  static constexpr std::uint32_t slot_count = 3;

  struct Draw {
    std::uint32_t block{0};
    std::optional<RenderPass> pass;
    std::vector<DrawCommand> commands;
  };

  struct Slot {
    std::deque<std::vector<ShapeVertex>> vertices;
    std::deque<std::vector<Index>> indices;
    std::deque<std::vector<Vertex>> textured_vertices;
    std::deque<std::vector<Index>> textured_indices;
    std::size_t blocks{0};
    std::size_t textured_blocks{0};
    std::vector<Draw> draws;
    std::vector<Upload> uploads;
    std::vector<std::byte> bytes;
    std::optional<SurfaceSize> extent;
    GpuSerial serial{0};
  };

  OverlayState(std::shared_ptr<Registry> registry,
               std::shared_ptr<Counters> counters, std::shared_ptr<Hooks> hooks,
               std::shared_ptr<Publisher> publisher,
               std::shared_ptr<Uploads> uploads) noexcept
      : registry_(std::move(registry)),
        counters_(std::move(counters)),
        hooks_(std::move(hooks)),
        publisher_(std::move(publisher)),
        uploads_(std::move(uploads)) {}

  OverlayState(const OverlayState&) = delete;
  auto operator=(const OverlayState&) -> OverlayState& = delete;
  OverlayState(OverlayState&&) = delete;
  auto operator=(OverlayState&&) -> OverlayState& = delete;
  ~OverlayState() = default;

  [[nodiscard]] static auto begin(const std::shared_ptr<OverlayState>& self)
      -> std::expected<GpuFrame, ProviderError>;

  [[nodiscard]] auto record(FrameCanvas target)
      -> std::expected<bool, ProviderError> {
    if (slots_.closed()) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument,
                        ProviderErrorDetail::not_taken_over});
    }
    const std::optional<SurfaceSize> size =
        target.has_value()
            ? std::optional{SurfaceSize{
                  .width = SurfaceWidth{target->get().width()},
                  .height = SurfaceHeight{target->get().height()}}}
            : std::nullopt;
    const auto used = slots_.use(size);
    if (!used.has_value()) {
      return std::unexpected(used.error());
    }
    if (!used->has_value()) {
      idle_.fetch_add(1);
      return false;
    }
    const OverlaySlots::Use& use = **used;
    Slot& slot = slots_storage_[use.slot];
    bool prepared = false;
    if (use.fresh) {
      fresh_.fetch_add(1);
      for (const std::uint32_t carried :
           std::span{use.carried}.first(use.carrying)) {
        prepared = prepare(slots_storage_[carried]) || prepared;
      }
      prepared = prepare(slot) || prepared;
    }
    const bool drawn = std::ranges::any_of(
        slot.draws, [](const Draw& draw) { return !draw.pass.has_value(); });
    if (!drawn) {
      (prepared ? recorded_ : idle_).fetch_add(1);
      return prepared;
    }
    if (target.has_value()) {
      SoftwareRaster::Canvas& canvas = target->get();
      const PassRegion whole{.x = 0.0F,
                             .y = 0.0F,
                             .width = static_cast<float>(canvas.width()),
                             .height = static_cast<float>(canvas.height())};
      for (const Draw& draw : slot.draws) {
        if (!draw.pass.has_value()) {
          SoftwareRaster::draw(canvas, whole, slot.vertices[draw.block],
                               slot.indices[draw.block], draw.commands,
                               lookup());
        }
      }
    }
    recorded_.fetch_add(1);
    return true;
  }

  void present() {
    std::uint64_t done = 0;
    static_cast<void>(slots_.present([this, &done](std::uint32_t index) {
      done = std::max(done, serials_[index].load());
    }));
    if (done != 0) {
      publisher_->completed(GpuSerial{done});
    }
  }

  void close() noexcept { slots_.close(); }

  auto closed() const -> bool { return slots_.closed(); }

  auto ring() const noexcept -> const OverlaySlots& { return slots_; }

  auto current() const -> std::optional<std::uint32_t> {
    return slots_.current();
  }

  auto statistics() const -> Overlay::Statistics {
    return Overlay::Statistics{.published = published_.load(),
                               .empty = empty_.load(),
                               .refused = refused_.load(),
                               .recorded = recorded_.load(),
                               .idle = idle_.load(),
                               .fresh = fresh_.load()};
  }

  auto slot(std::uint32_t index) -> Slot& { return slots_storage_[index]; }

  auto registry() const noexcept -> const std::shared_ptr<Registry>& {
    return registry_;
  }

  void seal(std::uint32_t index, const FramePlan& plan) {
    Slot& slot = slots_storage_[index];
    slot.serial = hooks_->schedule().next_serial();
    serials_[index].store(slot.serial.value());
    slot.extent = plan.extent;
    {
      const std::scoped_lock lock{uploads_->mutex};
      std::swap(slot.uploads, uploads_->pending);
      std::swap(slot.bytes, uploads_->bytes);
    }
    {
      const std::scoped_lock lock{counters_->mutex};
      counters_->planner.seal();
    }
    for (const Draw& draw : slot.draws) {
      publisher_->sampled(slot.serial, draw.commands);
    }
  }

  [[nodiscard]] auto publish(std::uint32_t index) -> bool {
    const Slot& slot = slots_storage_[index];
    const bool drawn = !slot.draws.empty();
    if (!slots_.publish(index, !drawn && slot.uploads.empty(),
                        slot.extent.value_or(SurfaceSize{}))) {
      return false;
    }
    published_.fetch_add(1);
    if (!drawn) {
      empty_.fetch_add(1);
    }
    return true;
  }

  void abandon(std::uint32_t index, bool sealed) {
    Slot& slot = slots_storage_[index];
    if (sealed && !slot.uploads.empty()) {
      slot.draws.clear();
      static_cast<void>(publish(index));
      return;
    }
    slots_.abandon(index);
  }

 private:
  static constexpr Colour transparent{
      .red = 0.0F, .green = 0.0F, .blue = 0.0F, .alpha = 0.0F};

  auto prepare(Slot& slot) -> bool {
    bool recorded = !slot.uploads.empty();
    for (const Upload& upload : slot.uploads) {
      if (upload.clear) {
        upload.canvas->fill(transparent);
      }
      upload.canvas->upload(
          upload.format, upload.region,
          PixelRows{std::span<const std::byte>{slot.bytes}.subspan(
              upload.offset, upload.size)});
    }
    slot.uploads.clear();
    slot.bytes.clear();
    std::vector<TextureId> owed;
    {
      const std::scoped_lock lock{counters_->mutex};
      for (const gpu::FramePlanner::Step& step : counters_->planner.owed()) {
        if (const auto* clear =
                std::get_if<gpu::FramePlanner::ClearTexture>(&step)) {
          owed.push_back(clear->texture);
        }
      }
    }
    for (const TextureId texture : owed) {
      const auto found = registry_->find(texture);
      if (found.has_value() && found->native != nullptr) {
        found->native->fill(transparent);
        recorded = true;
      }
    }
    for (const Draw& draw : slot.draws) {
      if (!draw.pass.has_value()) {
        continue;
      }
      const auto found = registry_->find(draw.pass->target);
      if (!found.has_value() || found->native == nullptr) {
        continue;
      }
      if (draw.pass->clear.has_value()) {
        found->native->fill(*draw.pass->clear);
      }
      SoftwareRaster::draw(*found->native, draw.pass->region,
                           slot.vertices[draw.block], slot.indices[draw.block],
                           draw.commands, lookup());
      recorded = true;
    }
    return recorded;
  }

  auto lookup() const -> SoftwareRaster::Lookup {
    return [registry = registry_](TextureId id) -> SoftwareRaster::Source {
      const auto found = registry->find(id);
      if (!found.has_value()) {
        return {};
      }
      return SoftwareRaster::Source{.canvas = found->native.get(),
                                    .filter = found->description.filter};
    };
  }

  std::shared_ptr<Registry> registry_;
  std::shared_ptr<Counters> counters_;
  std::shared_ptr<Hooks> hooks_;
  std::shared_ptr<Publisher> publisher_;
  std::shared_ptr<Uploads> uploads_;
  OverlaySlots slots_{slot_count, OverlaySlots::Leftovers::carried};
  std::array<Slot, slot_count> slots_storage_{};
  std::array<std::atomic<std::uint64_t>, slot_count> serials_{};
  std::atomic<std::uint64_t> published_{0};
  std::atomic<std::uint64_t> empty_{0};
  std::atomic<std::uint64_t> refused_{0};
  std::atomic<std::uint64_t> recorded_{0};
  std::atomic<std::uint64_t> idle_{0};
  std::atomic<std::uint64_t> fresh_{0};

  auto claim() -> std::optional<std::uint32_t> {
    const std::optional<std::uint32_t> claimed =
        slots_.claim([](std::uint32_t, OverlaySlots::Standing standing) {
          return standing.last_use == 0 || standing.presented;
        });
    if (!claimed.has_value()) {
      refused_.fetch_add(1);
      return std::nullopt;
    }
    Slot& slot = slots_storage_[*claimed];
    slot.blocks = 0;
    slot.textured_blocks = 0;
    slot.draws.clear();
    return claimed;
  }
};

class NullGpu::OverlayFrame final : public GpuFrame::Implementation {
 public:
  OverlayFrame(std::shared_ptr<OverlayState> state, std::uint32_t slot) noexcept
      : state_(std::move(state)), slot_(slot) {}

  OverlayFrame(const OverlayFrame&) = delete;
  auto operator=(const OverlayFrame&) -> OverlayFrame& = delete;
  OverlayFrame(OverlayFrame&&) = delete;
  auto operator=(OverlayFrame&&) -> OverlayFrame& = delete;

  ~OverlayFrame() override { finish(); }

  void finish() noexcept override {
    if (!open_ || sealed_) {
      return;
    }
    open_ = false;
    state_->abandon(slot_, false);
  }

  [[nodiscard]] auto allocate_geometry(std::uint32_t vertex_count,
                                       std::uint32_t index_count)
      -> std::expected<FrameGeometry, ProviderError> override {
    if (sealed_) {
      return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument,
                                           ProviderErrorDetail::frame_closed});
    }
    OverlayState::Slot& slot = state_->slot(slot_);
    const std::size_t block = slot.textured_blocks++;
    std::vector<Vertex>& vertices =
        reuse(slot.textured_vertices, block, vertex_count);
    std::vector<Index>& indices =
        reuse(slot.textured_indices, block, index_count);
    return FrameGeometry{.vertices = vertices,
                         .indices = indices,
                         .block = static_cast<std::uint32_t>(block),
                         .first_vertex = 0,
                         .first_index = 0};
  }

  [[nodiscard]] auto allocate_shapes(std::uint32_t vertex_count,
                                     std::uint32_t index_count)
      -> std::expected<ShapeGeometry, ProviderError> override {
    if (sealed_) {
      return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument,
                                           ProviderErrorDetail::frame_closed});
    }
    OverlayState::Slot& slot = state_->slot(slot_);
    const std::size_t block = slot.blocks++;
    std::vector<ShapeVertex>& vertices =
        reuse(slot.vertices, block, vertex_count);
    std::vector<Index>& indices = reuse(slot.indices, block, index_count);
    return ShapeGeometry{.vertices = vertices,
                         .indices = indices,
                         .block = static_cast<std::uint32_t>(block),
                         .first_vertex = 0,
                         .first_index = 0};
  }

  void clear(Colour) override {}

  void plan(FramePlan plan) override { plan_ = plan; }

  [[nodiscard]] auto draw(const DrawData& data)
      -> std::expected<void, ProviderError> override {
    const OverlayState::Slot& slot = state_->slot(slot_);
    if (sealed_ || data.geometry.block >= slot.textured_blocks ||
        data.geometry.vertices.data() !=
            slot.textured_vertices[data.geometry.block].data()) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument,
                        ProviderErrorDetail::foreign_geometry});
    }
    if (const auto refused = state_->registry()->refusal(data.commands)) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument, *refused});
    }
    return {};
  }

  [[nodiscard]] auto draw(const ShapeDrawData& data)
      -> std::expected<void, ProviderError> override {
    return keep(data, std::nullopt);
  }

  [[nodiscard]] auto draw(const ShapeDrawData& data, const RenderPass& pass)
      -> std::expected<void, ProviderError> override {
    const auto target = state_->registry()->find(pass.target);
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
    return keep(data, pass);
  }

  [[nodiscard]] auto present() -> std::expected<void, ProviderError> override {
    return std::unexpected(ProviderError::unsupported_capability());
  }

  [[nodiscard]] auto seal()
      -> std::expected<GpuFrame::Sealed, ProviderError> override {
    if (!open_ || sealed_) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument});
    }
    state_->seal(slot_, plan_);
    sealed_ = true;
    return GpuFrame::Sealed{
        .submitter = nullptr,
        .contents = FramePacket::Contents{.plan = plan_,
                                          .serial = state_->slot(slot_).serial,
                                          .frame = 0,
                                          .slot = slot_}};
  }

  [[nodiscard]] auto submit(const FramePacket&, FramePacket::Clock)
      -> std::expected<FrameReceipt, ProviderError> override {
    open_ = false;
    if (!state_->publish(slot_)) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument,
                        ProviderErrorDetail::not_taken_over});
    }
    return FrameReceipt{};
  }

  void abandon(const FramePacket&) noexcept override {
    open_ = false;
    state_->abandon(slot_, true);
  }

 private:
  [[nodiscard]] auto keep(const ShapeDrawData& data,
                          std::optional<RenderPass> pass)
      -> std::expected<void, ProviderError> {
    OverlayState::Slot& slot = state_->slot(slot_);
    if (sealed_) {
      return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument,
                                           ProviderErrorDetail::frame_closed});
    }
    if (data.geometry.block >= slot.blocks ||
        data.geometry.vertices.data() !=
            slot.vertices[data.geometry.block].data() ||
        data.geometry.indices.data() !=
            slot.indices[data.geometry.block].data()) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument,
                        ProviderErrorDetail::foreign_geometry});
    }
    if (const auto refused = state_->registry()->refusal(data.commands)) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument, *refused});
    }
    if (data.commands.empty()) {
      return {};
    }
    slot.draws.push_back(OverlayState::Draw{
        .block = data.geometry.block,
        .pass = pass,
        .commands = {data.commands.begin(), data.commands.end()}});
    return {};
  }

  template <class T>
  static auto reuse(std::deque<std::vector<T>>& blocks, std::size_t block,
                    std::uint32_t count) -> std::vector<T>& {
    if (block == blocks.size()) {
      blocks.emplace_back();
    }
    std::vector<T>& reused = blocks[block];
    reused.clear();
    reused.resize(count);
    return reused;
  }

  std::shared_ptr<OverlayState> state_;
  std::uint32_t slot_;
  FramePlan plan_{};
  bool sealed_{false};
  bool open_{true};
};

auto NullGpu::OverlayState::begin(const std::shared_ptr<OverlayState>& self)
    -> std::expected<GpuFrame, ProviderError> {
  if (self->slots_.closed()) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument,
                                         ProviderErrorDetail::not_taken_over});
  }
  const std::optional<std::uint32_t> claimed = self->claim();
  if (!claimed.has_value()) {
    return std::unexpected(
        ProviderError{ProviderErrorCode::resource_exhausted});
  }
  return GpuFrame{std::make_unique<OverlayFrame>(self, *claimed)};
}

class NullGpu::OverlaySeat {
 public:
  OverlaySeat(std::shared_ptr<Registry> registry,
              std::shared_ptr<Counters> counters, std::shared_ptr<Hooks> hooks,
              std::shared_ptr<Publisher> publisher,
              std::shared_ptr<Uploads> uploads) noexcept
      : registry_(std::move(registry)),
        counters_(std::move(counters)),
        hooks_(std::move(hooks)),
        publisher_(std::move(publisher)),
        uploads_(std::move(uploads)) {}

  [[nodiscard]] auto open()
      -> std::expected<std::shared_ptr<OverlayState>, ProviderError> {
    const std::scoped_lock lock{mutex_};
    if (open_ != nullptr && !open_->closed()) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::resource_exhausted,
                        ProviderErrorDetail::overlay_open});
    }
    open_ = std::make_shared<OverlayState>(registry_, counters_, hooks_,
                                           publisher_, uploads_);
    return open_;
  }

  void close(const std::shared_ptr<OverlayState>& state) noexcept {
    state->close();
    const std::scoped_lock lock{mutex_};
    if (open_ == state) {
      open_.reset();
    }
  }

  void hand_back() noexcept {
    const std::scoped_lock lock{mutex_};
    if (open_ != nullptr) {
      open_->close();
      open_.reset();
    }
  }

  void presented() {
    std::shared_ptr<OverlayState> state;
    {
      const std::scoped_lock lock{mutex_};
      state = open_;
    }
    if (state != nullptr) {
      state->present();
    }
  }

 private:
  std::shared_ptr<Registry> registry_;
  std::shared_ptr<Counters> counters_;
  std::shared_ptr<Hooks> hooks_;
  std::shared_ptr<Publisher> publisher_;
  std::shared_ptr<Uploads> uploads_;
  std::mutex mutex_;
  std::shared_ptr<OverlayState> open_;
};

NullGpu::Overlay::Overlay(std::shared_ptr<OverlaySeat> seat,
                          std::shared_ptr<OverlayState> state) noexcept
    : seat_(std::move(seat)), state_(std::move(state)) {}

auto NullGpu::Overlay::operator=(Overlay&& other) noexcept -> Overlay& {
  if (this != &other) {
    release();
    seat_ = std::move(other.seat_);
    state_ = std::move(other.state_);
  }
  return *this;
}

NullGpu::Overlay::~Overlay() { release(); }

auto NullGpu::Overlay::held() const noexcept -> bool {
  return state_ != nullptr && !state_->closed();
}

auto NullGpu::Overlay::frames() const -> FrameSource {
  std::weak_ptr<const OverlaySlots> ring;
  if (state_ != nullptr) {
    ring = std::shared_ptr<const OverlaySlots>{state_, &state_->ring()};
  }
  return FrameSource{[state = std::weak_ptr<OverlayState>{state_}]()
                         -> std::expected<GpuFrame, ProviderError> {
                       const std::shared_ptr<OverlayState> held = state.lock();
                       if (held == nullptr) {
                         return std::unexpected(ProviderError{
                             ProviderErrorCode::invalid_argument,
                             ProviderErrorDetail::not_taken_over});
                       }
                       return OverlayState::begin(held);
                     },
                     std::move(ring)};
}

auto NullGpu::Overlay::record(FrameCanvas target)
    -> std::expected<bool, ProviderError> {
  if (state_ == nullptr) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument,
                                         ProviderErrorDetail::not_taken_over});
  }
  return state_->record(target);
}

auto NullGpu::Overlay::current_slot() const -> std::optional<std::uint32_t> {
  if (state_ == nullptr) {
    return std::nullopt;
  }
  return state_->current();
}

auto NullGpu::Overlay::statistics() const -> Statistics {
  if (state_ == nullptr) {
    return {};
  }
  return state_->statistics();
}

void NullGpu::Overlay::release() noexcept {
  if (state_ == nullptr) {
    return;
  }
  if (seat_ != nullptr) {
    seat_->close(state_);
  } else {
    state_->close();
  }
  state_.reset();
  seat_.reset();
}

NullGpu::NullGpu()
    : registry_(std::make_shared<Registry>(
          [](std::shared_ptr<Canvas>& canvas) noexcept { canvas.reset(); })),
      counters_(std::make_shared<Counters>()),
      resident_(std::make_shared<Resident>()),
      hooks_(std::make_shared<Hooks>()),
      publisher_(
          std::make_shared<Publisher>(hooks_->schedule().weak_from_this())),
      surfaces_(std::make_shared<Surfaces>(
          hooks_->schedule().shared_from_this(), publisher_,
          [registry = registry_,
           counters = counters_](TextureDescription description) {
            return make_texture(registry, *counters, description);
          })),
      slots_(std::make_shared<FrameSlots>(slot_count)),
      uploads_(std::make_shared<Uploads>()),
      queue_(std::make_shared<RenderQueue>()),
      overlay_(std::make_shared<OverlaySeat>(registry_, counters_, hooks_,
                                             publisher_, uploads_)) {}

NullGpu::~NullGpu() {
  if (publisher_ != nullptr) {
    publisher_->completed(GpuSerial{std::numeric_limits<std::uint64_t>::max()});
  }
  if (registry_ != nullptr) {
    registry_->retire();
  }
}

auto NullGpu::create_texture(TextureDescription description)
    -> std::expected<Texture, ProviderError> {
  return make_texture(registry_, *counters_, description);
}

auto NullGpu::make_texture(const std::shared_ptr<Registry>& registry,
                           Counters& counters, TextureDescription description)
    -> std::expected<Texture, ProviderError> {
  if (!Texture::describable(description)) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument});
  }
  if (description.usage == TextureUsage::render_target) {
    const std::size_t bytes = static_cast<std::size_t>(description.width) *
                              description.height *
                              Texture::bytes_per_pixel(description.format);
    const std::uint32_t extent = counters.target_extent.load();
    if (description.width > extent || description.height > extent ||
        bytes > counters.target_bytes.load() ||
        registry->memory().targets >= counters.target_count.load()) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::resource_exhausted});
    }
    counters.render_targets.fetch_add(1);
  }
  counters.textures_created.fetch_add(1);
  bool rasterizing = false;
  {
    const std::scoped_lock lock{counters.mutex};
    rasterizing = counters.extent.has_value();
  }
  std::shared_ptr<Canvas> canvas;
  if (rasterizing) {
    canvas = std::make_shared<Canvas>(description.width, description.height);
  }
  Texture texture = Registry::adopt(registry, description, std::move(canvas));
  const std::scoped_lock lock{counters.mutex};
  counters.planner.created(texture.id(), description);
  return texture;
}

auto NullGpu::import_texture(std::shared_ptr<SoftwareRaster::Canvas> image,
                             ImportDescription description,
                             support::ScopedCallback released)
    -> std::expected<Imported, ProviderError> {
  const TextureDescription texture = description.texture();
  if (description.planes() != 1 || !Texture::describable(texture) ||
      image == nullptr || image->width() != description.width ||
      image->height() != description.height) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument});
  }
  counters_->imports.fetch_add(1);
  return Imported::adopt(publisher_,
                         Registry::adopt(registry_, texture, std::move(image)),
                         description, std::move(released));
}

auto NullGpu::import_texture(
    std::span<const std::shared_ptr<SoftwareRaster::Canvas>> planes,
    ImportDescription description, support::ScopedCallback released)
    -> std::expected<Imported, ProviderError> {
  if (planes.size() != description.planes()) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument});
  }
  for (std::size_t index = 0; index < planes.size(); ++index) {
    const TextureDescription plane = description.plane(index);
    if (!Texture::describable(plane) || planes[index] == nullptr ||
        planes[index]->width() != plane.width ||
        planes[index]->height() != plane.height) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument});
    }
  }
  std::vector<Texture> textures;
  textures.reserve(planes.size());
  for (std::size_t index = 0; index < planes.size(); ++index) {
    textures.push_back(
        Registry::adopt(registry_, description.plane(index), planes[index]));
  }
  counters_->imports.fetch_add(1);
  return Imported::adopt(publisher_, std::move(textures), description,
                         std::move(released));
}

void NullGpu::reject_draws(DrawRejection rejection) noexcept {
  counters_->reject_passes.store(rejection.passes);
  counters_->reject_skipped.store(rejection.skipped);
  counters_->reject_seen.store(0);
  counters_->reject_count.store(rejection.count);
}

void NullGpu::limit_render_targets(RenderTargetLimit limit) noexcept {
  counters_->target_extent.store(limit.extent);
  counters_->target_bytes.store(limit.bytes);
  counters_->target_count.store(limit.count);
}

auto NullGpu::update_texture(Texture& texture, TextureRegion region,
                             PixelRows pixels)
    -> std::expected<void, ProviderError> {
  const auto record = registry_->find(texture.id());
  if (!texture.valid() || !record.has_value()) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument});
  }
  if (!Texture::accepts(record->description, region, pixels)) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument});
  }
  counters_->uploads.fetch_add(1);
  counters_->uploaded_bytes.fetch_add(
      Texture::upload_size(record->description, region).value_or(0));
  bool clear = false;
  {
    const std::scoped_lock lock{counters_->mutex};
    clear = counters_->planner.upload(texture.id(), region).has_value();
  }
  if (record->native != nullptr) {
    const std::size_t row =
        static_cast<std::size_t>(region.width) *
        Texture::bytes_per_pixel(record->description.format);
    const std::scoped_lock lock{uploads_->mutex};
    const std::size_t offset = uploads_->bytes.size();
    for (std::uint32_t line = 0; line < region.height; ++line) {
      const std::span<const std::byte> source = pixels.row(line, row);
      uploads_->bytes.insert(uploads_->bytes.end(), source.begin(),
                             source.end());
    }
    uploads_->pending.push_back(Upload{.canvas = record->native,
                                       .format = record->description.format,
                                       .region = region,
                                       .offset = offset,
                                       .size = uploads_->bytes.size() - offset,
                                       .clear = clear});
  }
  return {};
}

auto NullGpu::begin_frame() -> std::expected<GpuFrame, ProviderError> {
  const std::optional<FrameSlots::Claim> claim = slots_->claim();
  if (!claim.has_value()) {
    auto loose = std::make_unique<Frame>(registry_, counters_, resident_,
                                         hooks_, publisher_, surfaces_, slots_,
                                         uploads_, queue_);
    loose->open(std::nullopt);
    return GpuFrame{std::move(loose)};
  }
  std::shared_ptr<Frame>& frame = frames_.at(claim->slot);
  if (frame == nullptr || frame.use_count() > 1) {
    frame = std::make_shared<Frame>(registry_, counters_, resident_, hooks_,
                                    publisher_, surfaces_, slots_, uploads_,
                                    queue_);
  }
  frame->open(claim);
  return GpuFrame::reused(frame);
}

auto NullGpu::begin_replay() -> std::expected<GpuFrame, ProviderError> {
  const std::optional<GpuSerial> serial = hooks_->schedule().next_serial_after(
      GpuSerial{counters_->submitted.load()});
  if (!serial.has_value()) {
    return std::unexpected(
        ProviderError{ProviderErrorCode::resource_exhausted});
  }
  if (replay_ == nullptr || replay_.use_count() > 1) {
    replay_ = std::make_shared<Frame>(registry_, counters_, resident_, hooks_,
                                      publisher_, surfaces_, slots_, uploads_,
                                      queue_);
  }
  replay_->open_replay(*serial);
  return GpuFrame::reused(replay_);
}

auto NullGpu::render_queue() noexcept
    -> std::optional<std::reference_wrapper<RenderQueue>> {
  if (queue_ == nullptr) {
    return std::nullopt;
  }
  return std::ref(*queue_);
}

auto NullGpu::take_over() -> std::expected<Takeover, ProviderError> {
  if (queue_ == nullptr) {
    return std::unexpected(ProviderError::unsupported_capability());
  }
  auto taken = takeover_.claim(
      *queue_, hooks_->schedule().weak_from_this(),
      [counters = counters_, overlay = overlay_]() {
        const std::uint32_t restored = counters->swap_interval.load();
        {
          const std::scoped_lock lock{counters->mutex};
          counters->lent.lend(
              static_cast<std::uint32_t>(counters->buffers.size()));
        }
        return support::ScopedCallback{[counters, overlay,
                                        restored]() noexcept {
          overlay->hand_back();
          const LentSwapchain<>::Settlement settled = counters->lent.settle();
          counters->swap_interval.store(restored);
          const std::scoped_lock lock{counters->mutex};
          for (const LentSwapchain<>::Outstanding& each : settled.outstanding) {
            Takeover::show(*counters, each.index);
            counters->takeover_settled.fetch_add(1);
          }
          counters->planner.invalidate();
        }};
      });
  if (!taken.has_value()) {
    return std::unexpected(taken.error());
  }
  return Takeover{std::move(*taken), counters_, overlay_};
}

auto NullGpu::takeover_counters() const -> GpuTakeover::Counters {
  return takeover_.counters();
}

auto NullGpu::takeover_presents() const noexcept -> std::size_t {
  return counters_->takeover_presents.load();
}

auto NullGpu::takeover_settled() const noexcept -> std::size_t {
  return counters_->takeover_settled.load();
}

NullGpu::Takeover::Takeover(GpuTakeover taken,
                            std::shared_ptr<Counters> counters,
                            std::shared_ptr<OverlaySeat> seat) noexcept
    : taken_(std::move(taken)),
      counters_(std::move(counters)),
      seat_(std::move(seat)) {}

auto NullGpu::Takeover::overlay() -> std::expected<Overlay, ProviderError> {
  if (!taken_.held() || seat_ == nullptr) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument,
                                         ProviderErrorDetail::not_taken_over});
  }
  return seat_->open().transform([this](std::shared_ptr<OverlayState> state) {
    return Overlay{seat_, std::move(state)};
  });
}

auto NullGpu::Takeover::image_count() const -> std::uint32_t {
  if (!taken_.held()) {
    return 0;
  }
  const std::scoped_lock lock{counters_->mutex};
  return static_cast<std::uint32_t>(counters_->buffers.size());
}

auto NullGpu::Takeover::acquire()
    -> std::expected<std::uint32_t, ProviderError> {
  if (!taken_.held()) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument,
                                         ProviderErrorDetail::not_taken_over});
  }
  const std::scoped_lock lock{counters_->mutex};
  std::vector<Buffer>& buffers = counters_->buffers;
  const std::size_t count = buffers.size();
  std::optional<std::uint32_t> found;
  for (std::size_t step = 0; step < count && !found.has_value(); ++step) {
    const auto candidate =
        static_cast<std::uint32_t>((counters_->next_buffer + step) % count);
    if (counters_->lent.acquire(candidate)) {
      found = candidate;
    }
  }
  if (!found.has_value()) {
    return std::unexpected(
        ProviderError{ProviderErrorCode::resource_exhausted,
                      ProviderErrorDetail::image_unavailable});
  }
  const std::uint32_t index = *found;
  counters_->next_buffer = (index + 1) % count;
  if (counters_->extent.has_value() && !counters_->extent->empty()) {
    Buffer& buffer = buffers[index];
    const std::uint32_t width = counters_->extent->width.value();
    const std::uint32_t height = counters_->extent->height.value();
    if (!buffer.canvas.has_value() || buffer.canvas->width() != width ||
        buffer.canvas->height() != height) {
      buffer.canvas.emplace(width, height);
    }
  }
  return index;
}

auto NullGpu::Takeover::target(std::uint32_t index) -> FrameCanvas {
  if (!taken_.held()) {
    return std::nullopt;
  }
  const std::scoped_lock lock{counters_->mutex};
  if (index >= counters_->buffers.size() ||
      !counters_->buffers[index].canvas.has_value()) {
    return std::nullopt;
  }
  return std::ref(*counters_->buffers[index].canvas);
}

auto NullGpu::Takeover::present(std::uint32_t index)
    -> std::expected<void, ProviderError> {
  if (!taken_.held()) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument,
                                         ProviderErrorDetail::not_taken_over});
  }
  if (!counters_->lent.present(index)) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument});
  }
  counters_->takeover_presents.fetch_add(1);
  seat_->presented();
  const std::scoped_lock lock{counters_->mutex};
  show(*counters_, index);
  return {};
}

void NullGpu::Takeover::show(Counters& counters, std::uint32_t index) {
  if (index >= counters.buffers.size()) {
    return;
  }
  if (const std::optional<Canvas>& canvas = counters.buffers[index].canvas;
      canvas.has_value()) {
    if (auto made = canvas->image(); made.has_value()) {
      counters.last_image = std::move(*made);
    }
  }
}

auto NullGpu::Takeover::generation() const -> std::uint64_t {
  if (!taken_.held()) {
    return 0;
  }
  const std::scoped_lock lock{counters_->mutex};
  return counters_->generation;
}

void NullGpu::Takeover::set_swap_interval(SwapInterval interval) {
  if (taken_.held()) {
    counters_->swap_interval.store(interval.value());
  }
}

void NullGpu::display(std::optional<std::chrono::nanoseconds> period) noexcept {
  counters_->display_period.store(
      period.value_or(std::chrono::nanoseconds{0}).count());
}

auto NullGpu::swap_interval() const noexcept -> std::uint32_t {
  return counters_->swap_interval.load();
}

auto NullGpu::swap_interval_changes() const noexcept -> std::size_t {
  return counters_->swap_interval_changes.load();
}

auto NullGpu::captures() const noexcept -> std::size_t {
  return counters_->captures.load();
}

auto NullGpu::capture() -> std::expected<Image, ProviderError> {
  const std::scoped_lock lock{counters_->mutex};
  if (!counters_->last_image.has_value()) {
    return std::unexpected(ProviderError::unsupported_capability());
  }
  return *counters_->last_image;
}

auto NullGpu::frame_schedule() noexcept
    -> std::optional<std::reference_wrapper<FrameSchedule>> {
  return std::ref(hooks_->schedule());
}

auto NullGpu::publications() const noexcept
    -> std::optional<std::reference_wrapper<const TexturePublications>> {
  if (publisher_ == nullptr) {
    return std::nullopt;
  }
  return std::cref(static_cast<const TexturePublications&>(*publisher_));
}

void NullGpu::hold_completion(std::uint64_t frames) {
  counters_->held.store(frames);
  const std::uint64_t presented = hooks_->schedule().serial().value();
  if (presented > frames) {
    publisher_->completed(GpuSerial{presented - frames});
  }
}

auto NullGpu::add_frame_hook(FrameStage stage, FrameHookFunction function,
                             FrameHookOptions options) -> FrameHook {
  return hooks_->add(stage, std::move(function), options);
}

auto NullGpu::add_surface(SurfaceOptions options, SurfaceFunction function)
    -> std::expected<GpuSurface, ProviderError> {
  return surfaces_->add(options, std::move(function));
}

auto NullGpu::surfaces() noexcept
    -> std::optional<std::reference_wrapper<SurfaceDirectory>> {
  if (surfaces_ == nullptr) {
    return std::nullopt;
  }
  return std::ref(surfaces_->directory());
}

auto NullGpu::surface_calls() const noexcept -> std::size_t {
  return surfaces_ != nullptr ? surfaces_->calls() : 0;
}

auto NullGpu::timings() const -> GpuTimings {
  const std::scoped_lock lock{counters_->mutex};
  return counters_->timings;
}

auto NullGpu::resources() const -> GpuResources {
  return GpuResources{.textures_created = counters_->textures_created.load(),
                      .memory_blocks_created = 0};
}

auto NullGpu::inventory() const -> GpuInventory {
  return GpuInventory{
      .backend = "null",
      .report = std::format("null gpu: {}",
                            rasterizing() ? "software raster" : "no raster"),
      .textures = registry_->memory(),
      .frames_presented = counters_->presented.load(),
      .missed_vsyncs = std::nullopt};
}

void NullGpu::rasterize(SurfaceSize extent) {
  const std::scoped_lock lock{counters_->mutex};
  if (counters_->extent != extent) {
    ++counters_->generation;
  }
  counters_->extent = extent;
}

void NullGpu::swapchain(std::uint32_t images, bool partial) {
  const std::scoped_lock lock{counters_->mutex};
  counters_->buffers.clear();
  counters_->buffers.resize(std::max<std::uint32_t>(images, 1));
  counters_->next_buffer = 0;
  counters_->partial = partial;
  counters_->planner.invalidate();
}

auto NullGpu::rasterizing() const -> bool {
  const std::scoped_lock lock{counters_->mutex};
  return counters_->extent.has_value();
}

auto NullGpu::live_textures() const -> std::size_t { return registry_->size(); }

auto NullGpu::passes() const noexcept -> std::size_t {
  return counters_->passes.load();
}

auto NullGpu::render_targets() const noexcept -> std::size_t {
  return counters_->render_targets.load();
}

auto NullGpu::uploads() const noexcept -> std::size_t {
  return counters_->uploads.load();
}

auto NullGpu::imports() const noexcept -> std::size_t {
  return counters_->imports.load();
}

auto NullGpu::uploaded_bytes() const noexcept -> std::size_t {
  return counters_->uploaded_bytes.load();
}

auto NullGpu::frames_presented() const noexcept -> std::size_t {
  return counters_->presented.load();
}

auto NullGpu::frames_discarded() const noexcept -> std::size_t {
  return counters_->discarded.load();
}

auto NullGpu::draws() const noexcept -> std::size_t {
  return counters_->draws.load();
}

auto NullGpu::commands() const noexcept -> std::size_t {
  return counters_->commands.load();
}

auto NullGpu::geometry_allocations() const noexcept -> std::size_t {
  return counters_->geometry_allocations.load();
}

auto NullGpu::geometry_bytes() const noexcept -> std::size_t {
  return counters_->geometry_bytes.load();
}

auto NullGpu::vertex_uploads() const noexcept -> std::size_t {
  return counters_->vertex_uploads.load();
}

auto NullGpu::vertex_uploaded_bytes() const noexcept -> std::size_t {
  return counters_->vertex_uploaded_bytes.load();
}

auto NullGpu::last_damage() const -> std::optional<ClipRect> {
  const std::scoped_lock lock{counters_->mutex};
  return counters_->last_damage;
}

auto NullGpu::damaged_pixels() const -> std::size_t {
  const std::scoped_lock lock{counters_->mutex};
  return counters_->damaged_pixels;
}

auto NullGpu::last_clear() const -> std::optional<Colour> {
  const std::scoped_lock lock{counters_->mutex};
  return counters_->last_clear;
}

auto NullGpu::last_shapes() const -> ShapeRecord {
  const std::scoped_lock lock{counters_->mutex};
  ShapeRecord record = counters_->last_shapes;
  append(record, counters_->snapshot);
  return record;
}

void NullGpu::append(ShapeRecord& record, const Snapshot& snapshot) {
  constexpr std::uint32_t unmapped = 0xFFFF'FFFFU;
  constexpr std::size_t corners = 4;
  const std::span<const ShapeVertex> vertices = snapshot.vertices;
  const std::span<const Index> indices = snapshot.indices;
  std::vector<std::uint32_t> remap((vertices.size() + corners - 1) / corners,
                                   unmapped);
  for (const ResidentDraw& draw : snapshot.draws.active()) {
    for (const DrawCommand& command : draw.commands) {
      const auto first = static_cast<std::uint32_t>(record.indices.size());
      const std::size_t end = std::min<std::size_t>(
          indices.size(),
          static_cast<std::size_t>(command.index_offset) + command.index_count);
      for (std::size_t at = command.index_offset; at < end; ++at) {
        const std::size_t vertex =
            static_cast<std::size_t>(indices[at]) + command.vertex_offset;
        const std::size_t block = vertex / corners;
        if (block >= remap.size()) {
          continue;
        }
        if (remap[block] == unmapped) {
          remap[block] = static_cast<std::uint32_t>(record.vertices.size());
          const std::size_t from = block * corners;
          const std::size_t to = std::min(vertices.size(), from + corners);
          for (const ShapeVertex& source : vertices.subspan(from, to - from)) {
            ShapeVertex& placed = record.vertices.emplace_back(source);
            const std::array<float, 2> shift =
                command.translation_for(source.kind);
            placed.position = {source.position[0] + shift[0],
                               source.position[1] + shift[1]};
            if (command.opacity != 1.0F) {
              const auto alpha = static_cast<float>(placed.colour >> 24U);
              placed.colour =
                  (placed.colour & 0x00FF'FFFFU) |
                  (static_cast<std::uint32_t>(std::lround(
                       std::clamp(alpha * command.opacity, 0.0F, 255.0F)))
                   << 24U);
            }
          }
        }
        record.indices.push_back(remap[block] +
                                 static_cast<std::uint32_t>(vertex % corners));
      }
      if (!draw.offscreen) {
        record.commands.push_back(DrawCommand{
            .texture = command.texture,
            .clip = command.clip,
            .index_offset = first,
            .index_count =
                static_cast<std::uint32_t>(record.indices.size()) - first,
            .vertex_offset = 0,
            .detail = command.detail,
            .trim = command.trim});
      }
    }
  }
}

}
