#include <runtime/frame_pacer.hpp>
#include <runtime/painter_renderer.hpp>

#include <arisa/layout/viewport.hpp>

#include <algorithm>
#include <format>
#include <utility>

namespace arisa::runtime {

PainterRenderer::PainterRenderer(provider::ProviderFactory& providers,
                                 std::unique_ptr<text::FontCollection> fonts,
                                 std::optional<text::Typesetter> typesetter,
                                 layout::LayoutContext layout,
                                 rendering::Painter&& painter) noexcept
    : surface_(providers.surface()),
      gpu_(providers.gpu()),
      clock_(providers.clock()),
      fonts_(std::move(fonts)),
      typesetter_(std::move(typesetter)),
      layout_(std::move(layout)),
      painter_(std::move(painter)) {}

auto PainterRenderer::create(provider::ProviderFactory& providers,
                             const ApplicationOptions& options)
    -> std::expected<PainterRenderer, ApplicationError> {
  auto layout = layout::LayoutContext::create();
  if (!layout.has_value()) {
    return std::unexpected(
        ApplicationError{ApplicationErrorCode::layout_failed});
  }
  auto painter = rendering::Painter::create(providers.gpu(), options.atlas,
                                            providers.diagnostics());
  if (!painter.has_value()) {
    return std::unexpected(
        ApplicationError{ApplicationErrorCode::paint_failed, painter.error()});
  }
  painter->debug_glass_coverage(options.debug_glass_coverage);
  painter->debug_full_walk(options.debug_full_walk);
  painter->set_picture_budget(
      rendering::Painter::PictureBudget{options.images.textures});
  painter->debug_separate_backdrops(options.debug_separate_backdrops);
  return assemble(providers, options, std::move(*layout), std::move(*painter));
}

auto PainterRenderer::embedded(provider::ProviderFactory& providers,
                               const ApplicationOptions& options,
                               provider::FrameSource frames,
                               provider::SurfaceSize extent)
    -> std::expected<PainterRenderer, ApplicationError> {
  auto created = create(providers, options);
  if (created.has_value()) {
    created->frames_.emplace(std::move(frames));
    created->painter_.see_through_backdrops(true);
    created->extent_ = extent;
    created->launch_.reset();
  }
  return created;
}

auto PainterRenderer::assemble(provider::ProviderFactory& providers,
                               const ApplicationOptions& options,
                               layout::LayoutContext layout,
                               rendering::Painter&& painter)
    -> std::expected<PainterRenderer, ApplicationError> {
  std::unique_ptr<text::FontCollection> fonts;
  std::optional<text::Typesetter> typesetter;
  auto loaded = text::FontCollection::load(providers.assets(),
                                           providers.fonts(), options.fonts);
  if (loaded.has_value()) {
    fonts = std::make_unique<text::FontCollection>(std::move(*loaded));
    if (auto created = text::Typesetter::create(*fonts); created.has_value()) {
      typesetter.emplace(std::move(*created));
    }
  }
  if (!typesetter.has_value()) {
    providers.diagnostics().report(provider::Diagnostic{
        .severity = provider::Severity::warning,
        .category = "text",
        .message =
            std::format("fonts could not be loaded ({}); text is not drawn",
                        loaded.has_value() ? "typesetter failed"
                                           : loaded.error().description())});
  }
  PainterRenderer renderer{providers, std::move(fonts), std::move(typesetter),
                           std::move(layout), std::move(painter)};
  renderer.launch_ = options.launch_colour;
  return renderer;
}

auto PainterRenderer::typesetter() noexcept
    -> std::optional<std::reference_wrapper<text::Typesetter>> {
  if (!typesetter_.has_value()) {
    return std::nullopt;
  }
  return std::ref(*typesetter_);
}

auto PainterRenderer::shaped() const noexcept -> std::size_t {
  if (!typesetter_.has_value()) {
    return 0;
  }
  return typesetter_->statistics().shaped;
}

auto PainterRenderer::counters() const noexcept -> Counters {
  Counters current{.shaping_runs = 0,
                   .text_layouts = 0,
                   .rasterised = 0,
                   .atlas_bytes = 0,
                   .textures_created = 0,
                   .memory_blocks_created = 0};
  if (typesetter_.has_value()) {
    const text::CacheStatistics cached = typesetter_->statistics();
    current.shaping_runs = cached.shaping_misses;
    current.text_layouts = cached.layout_misses + cached.measure_misses;
  }
  const text::AtlasStatistics atlas = painter_.atlas().statistics();
  current.rasterised = atlas.rasterised;
  current.atlas_bytes = atlas.uploaded_bytes;
  const provider::GpuResources resources = gpu_.get().resources();
  current.textures_created = resources.textures_created;
  current.memory_blocks_created = resources.memory_blocks_created;
  return current;
}

auto PainterRenderer::controller_glyphs() const noexcept -> bool {
  if (fonts_ == nullptr) {
    return false;
  }
  return std::ranges::any_of(
      fonts_->entries(), [](const text::FontEntry& entry) {
        return entry.role == text::FontRole::symbols && entry.face != nullptr &&
               entry.face->covers(controller_probe);
      });
}

auto PainterRenderer::prepare(Root& root)
    -> std::expected<Preparation, ApplicationError> {
  Preparation preparation;
  provider::SurfaceProvider& surface = surface_;
  while (!extent_.has_value() && surface.poll().has_value()) {
    preparation.changed = true;
    preparation.resized = true;
  }
  viewport_ = layout::Viewport{.pixels = extent_.value_or(surface.size()),
                               .scale = provider::SurfaceScale::create(
                                            surface.scale().value() * ui_scale_)
                                            .value_or(surface.scale())};
  if (shown_ != viewport_) {
    preparation.changed = true;
    preparation.resized = true;
  }
  provider::ClockProvider& clock = clock_;
  const auto layout_start = clock.now();
  const std::uint64_t computed = layout_.computations();
  auto laid = layout_.update(root.tree(), viewport_, typesetter());
  if (!laid.has_value()) {
    return std::unexpected(
        ApplicationError{ApplicationErrorCode::layout_failed});
  }
  bool relaid = *laid;
  for (std::size_t round = 0;
       round < geometry_rounds && root.tree().geometry_dirty(); ++round) {
    root.settle_geometry();
    auto again = layout_.update(root.tree(), viewport_, typesetter());
    if (!again.has_value()) {
      return std::unexpected(
          ApplicationError{ApplicationErrorCode::layout_failed});
    }
    relaid = relaid || *again;
  }
  preparation.layout = clock.now() - layout_start;
  preparation.laid_out = relaid;
  preparation.computations = layout_.computations() - computed;
  if (preparation.laid_out) {
    preparation.changed = true;
    root.tree().relaid();
  }
  return preparation;
}

auto PainterRenderer::render(
    Root& root, rendering::Painter::Highlights highlights,
    std::optional<std::reference_wrapper<rendering::ReplayScene>> replay,
    Request request) -> std::expected<Painted, ApplicationError> {
  Painted result;
  FrameTimings& timings = result.timings;
  shown_ = viewport_;
  if (viewport_.pixels.empty()) {
    return result;
  }
  provider::ClockProvider& clock = clock_;
  const auto acquire_start = clock.now();
  auto opened =
      frames_.has_value() ? frames_->open() : gpu_.get().begin_frame();
  if (!opened.has_value()) {
    return std::unexpected(ApplicationError{
        ApplicationErrorCode::presentation_failed, opened.error()});
  }
  const auto paint_start = clock.now();
  provider::FramePlan plan{
      .predicted_present = request.predicted_present.value_or(
          paint_start.since_origin() + display_period),
      .replay = false,
      .exclusive = request.exclusive,
      .extent = viewport_.pixels,
      .swap_interval = 1,
      .capture = request.capture};
  opened->plan(plan);
  auto painted = painter_.paint(
      root.tree(), *opened,
      rendering::Painter::Target{.pixels = viewport_.pixels,
                                 .scale = viewport_.scale,
                                 .time = paint_start.since_origin(),
                                 .zoom = zoom(paint_start.since_origin()),
                                 .clear = std::exchange(launch_, std::nullopt),
                                 .replay = replay,
                                 .clock = std::cref(clock)},
      typesetter(), highlights);
  if (!painted.has_value()) {
    return std::unexpected(
        ApplicationError{ApplicationErrorCode::paint_failed, painted.error()});
  }
  const auto seal_start = clock.now();
  timings.paint = seal_start - paint_start;
  timings.walk = painted->walk;
  timings.upload = painted->upload;
  timings.emit = painted->emit;
  timings.quads = painted->quads;
  timings.commands = painted->commands;
  timings.animating = painted->animating || zooming(paint_start.since_origin());
  timings.frame_rate = painted->frame_rate;
  timings.unhinted = painted->unhinted;
  timings.culled = painted->culled;
  timings.visited = painted->visited;
  timings.recorded = painted->recorded;
  timings.spliced = painted->spliced;
  timings.occluded = painted->occluded;
  timings.shadow_hits = painted->shadow_hits;
  timings.shadow_misses = painted->shadow_misses;
  timings.tessellations = painted->tessellations;
  timings.mesh_builds = painted->mesh_builds;
  timings.pictures = painted->pictures;
  timings.picture_bytes = painted->picture_bytes;
  timings.backdrops = painted->backdrops;
  timings.blurs = painted->blurs;
  timings.blur_hits = painted->blur_hits;
  timings.shared_backdrops = painted->shared_backdrops;
  timings.effect_passes = painted->effect_passes;
  timings.layer_renders = painted->layer_renders;
  timings.layer_hits = painted->layer_hits;
  timings.targets_created = painted->targets_created;
  timings.targets_reused = painted->targets_reused;
  timings.targets_released = painted->targets_released;
  timings.targets_evicted = painted->targets_evicted;
  timings.targets_refused = painted->targets_refused;
  timings.uploaded_vertices = painted->uploaded_vertices;
  timings.uploaded_bytes = painted->uploaded_bytes;
  timings.damaged_pixels = painted->damaged_pixels;
  plan.swap_interval = FramePacer::swap_interval(
      FramePacer::rate(painted->frame_rate, timings.animating, request.demand),
      request.driven);
  if (replay.has_value()) {
    replay->get().driven = request.driven;
  }
  timings.swap_interval = plan.swap_interval;
  opened->plan(plan);
  auto sealed = opened->seal();
  if (!sealed.has_value()) {
    return std::unexpected(ApplicationError{
        ApplicationErrorCode::presentation_failed, sealed.error()});
  }
  result.packet.emplace(std::move(*sealed));
  timings.stall = paint_start - acquire_start;
  timings.present = (clock.now() - seal_start) + (paint_start - acquire_start);
  return result;
}

void PainterRenderer::measure(FrameTimings& timings) {
  const provider::GpuTimings measured = gpu_.get().timings();
  timings.effects_gpu = measured.effects;
  timings.interface_gpu = measured.interface;
  timings.hooks_gpu = measured.hooks;
  timings.gpu_measured = measured.measured_on_gpu;
  timings.missed_vsyncs =
      measured.missed_vsyncs - std::exchange(missed_, measured.missed_vsyncs);
  timings.encode_misses =
      measured.encode_misses.count -
      std::exchange(encode_missed_, measured.encode_misses.count);
  timings.encode_missed_texture = measured.encode_misses.texture;
  timings.encode_missed_target =
      measured.encode_misses.stage == provider::EncodeStage::pass_target;
}

void PainterRenderer::set_ui_scale(float scale, std::chrono::nanoseconds now,
                                   bool reduce_motion) {
  if (scale == ui_scale_ && scaled_) {
    return;
  }
  const float shown = ui_scale_ * zoom(now);
  ui_scale_ = scale;
  const bool first = !scaled_;
  scaled_ = true;
  if (reduce_motion || first) {
    zoom_.reset();
    return;
  }
  zoom_.emplace(
      motion::Animation::smooth(zoom_duration),
      motion::Animation::Span{.from = motion::AnimatableVector{shown / scale},
                              .to = motion::AnimatableVector{1.0},
                              .velocity = motion::AnimatableVector{0.0},
                              .precision = 1.0e-4},
      now, nullptr);
}

auto PainterRenderer::display_mode() const noexcept -> provider::DisplayMode {
  return surface_.get().display_mode();
}

auto PainterRenderer::zoom(std::chrono::nanoseconds now) const noexcept
    -> float {
  if (!zoom_.has_value()) {
    return 1.0F;
  }
  const motion::Animation::Sample sample = zoom_->sample(now);
  if (sample.finished) {
    return 1.0F;
  }
  return static_cast<float>(sample.value[0]);
}

auto PainterRenderer::zooming(std::chrono::nanoseconds now) const noexcept
    -> bool {
  return zoom_.has_value() && !zoom_->sample(now).finished;
}

}
