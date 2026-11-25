#include <arisa/appearance/appearance_model.hpp>
#include <arisa/appearance/theme_library.hpp>
#include <arisa/async/thread_pool.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/image_library.hpp>
#include <arisa/components/open_url_inbox.hpp>
#include <arisa/composition/background_task_scheduler.hpp>
#include <arisa/composition/exclusive_scopes.hpp>
#include <arisa/composition/layer.hpp>
#include <arisa/composition/persistent_system_overlays.hpp>
#include <arisa/composition/preferred_color_scheme.hpp>
#include <arisa/composition/system_gesture_deferral.hpp>
#include <arisa/composition/undo_manager.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/provider/gpu/frame_schedule.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/support/scoped_callback.hpp>
#include <arisa/text/string_catalog.hpp>

#include <algorithm>
#include <chrono>
#include <format>
#include <optional>
#include <string>

#include <arisa/runtime/screen_reader.hpp>
#include <arisa/theme/dynamic_type.hpp>
#include <interaction/input_router.hpp>
#include <runtime/drag_preview.hpp>
#include <runtime/feedback_player.hpp>
#include <runtime/frame_loop.hpp>
#include <runtime/frame_pacer.hpp>
#include <runtime/input_gate.hpp>
#include <runtime/painter_renderer.hpp>
#include <runtime/pointer_cursor.hpp>
#include <runtime/render_thread.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <utility>

namespace arisa::runtime {

class Application::Implementation {
 public:
  Implementation(
      provider::ProviderFactory& providers, PainterRenderer&& renderer,
      std::optional<async::ThreadPool> background,
      std::optional<async::ThreadPool> blocking,
      std::optional<async::Executors> executors, composition::Element content,
      interaction::InteractionOptions routing,
      std::optional<AppearanceOptions> appearance,
      std::optional<RenderThread> render_thread, bool pacing,
      ImageOptions images, StorageOptions storage,
      const LocalizationOptions& localization,
      std::optional<std::reference_wrapper<provider::StorageProvider>> files)
      : providers_(providers),
        lifecycle_(providers.lifecycle()),
        gpu_(providers.gpu()),
        clock_(providers.clock()),
        input_(providers.input()),
        audio_(providers.audio()),
        feedback_(providers.haptics()),
        router_(routing),
        reader_(providers.speech(),
                providers.supplies(provider::Capability::speech)),
        text_input_(providers.text_input()),
        images_(providers.assets()),
        cache_(providers.assets(), images.decoded),
        status_(providers.system_info(), providers.clock()),
        diagnostics_(providers.diagnostics()),
        open_url_(components::OpenURLAction::system(providers.browser())),
        pasteboard_(pasteboard_for(providers)),
        app_storage_(defaults(storage.provider, storage.app_file,
                              providers.diagnostics())),
        scene_storage_(defaults(storage.provider, storage.scene_file,
                                providers.diagnostics())),
        time_zone_(zone(providers.clock())),
        locale_(locale_for(localization, providers.system_info())),
        catalog_(catalog_for(localization, providers.assets(),
                             providers.diagnostics())),
        applets_(
            providers.supplies(provider::Capability::applets)
                ? std::optional<components::SystemApplets>{providers.applets()}
                : std::nullopt),
        files_(files.has_value()
                   ? std::optional<components::FileStorage>{files->get()}
                   : std::nullopt),
        renderer_(std::move(renderer)),
        background_(std::move(background)),
        blocking_(std::move(blocking)),
        root_(std::move(content), providers,
              select(executors, background_, blocking_)),
        glyphs_(renderer_.controller_glyphs()),
        render_thread_(std::move(render_thread)),
        pacer_(pacing),
        schedule_(providers.gpu().frame_schedule()),
        introspection_(root_, router_, renderer_, providers.gpu()),
        reset_focus_(root_.tree().focus_resets(),
                     [dispatcher = root_.dispatcher()] {
                       static_cast<void>(dispatcher.post([] {}));
                     }) {
    if (schedule_.has_value()) {
      waker_ = schedule_->get().wake_with([dispatcher = root_.dispatcher()] {
        static_cast<void>(dispatcher.post([] {}));
      });
    }
    if (appearance.has_value()) {
      std::vector<std::string> packs = appearance->policy.packs;
      model_.emplace(
          appearance->policy,
          appearance::ThemeLibrary{providers.assets(), appearance->storage,
                                   std::move(packs), appearance->themes},
          appearance->storage, providers.diagnostics());
      if (appearance->hot_reload.has_value()) {
        watcher_.emplace(
            *model_, root_.scheduler(), root_.executors().background.get(),
            [dispatcher =
                 root_.dispatcher()](std::move_only_function<void()> task) {
              static_cast<void>(dispatcher.post(std::move(task)));
            },
            *appearance->hot_reload);
      }
    }
  }

  Implementation(const Implementation&) = delete;
  auto operator=(const Implementation&) -> Implementation& = delete;
  Implementation(Implementation&&) = delete;
  auto operator=(Implementation&&) -> Implementation& = delete;

  ~Implementation() {
    persist(true);
    root_.tree().cancel_tasks();
    blocking_.reset();
    background_.reset();
    if (render_thread_.has_value()) {
      const auto queue = gpu_.get().render_queue();
      const support::ScopedCallback refusing = queue.has_value()
                                                   ? queue->get().withdraw()
                                                   : support::ScopedCallback{};
      render_thread_.reset();
    }
  }

  auto theme_watcher() const noexcept
      -> std::optional<std::reference_wrapper<const appearance::ThemeWatcher>> {
    if (!watcher_.has_value()) {
      return std::nullopt;
    }
    return std::cref(*watcher_);
  }

  auto appearance() const noexcept
      -> const std::optional<appearance::AppearanceModel>& {
    return model_;
  }

  auto frame() -> std::expected<void, ApplicationError> {
    provider::ClockProvider& clock = clock_;
    const auto began = clock.now();
    const std::uint64_t created = root_.tree().created();
    const PainterRenderer::Counters counted = renderer_.counters();
    const std::uint64_t reads = images_.reads();
    const std::uint64_t decodes = images_.decodes();
    bool lifecycle = false;
    while (const auto event = lifecycle_.get().poll()) {
      lifecycle = true;
      if (*event == provider::LifecycleEvent::exit_requested) {
        exit_requested_ = true;
      }
    }
    if (lifecycle) {
      time_zone_ = zone(clock);
      persist(true);
      battery_checked_.reset();
    }
    FrameTimings timings;
    const std::uint64_t evaluated = root_.tree().evaluations();
    const std::uint64_t debugged = root_.tree().debug_evaluations();
    const std::size_t shaped = renderer_.shaped();
    provider::InputProvider& input = input_;
    provider::InputFrame sampled = input.sample();
    reader_.filter(sampled, router_.options().layout.confirm(),
                   router_.options().layout.back());
    timings.sampled = clock.now().since_origin();
    if (observer_->has_value()) {
      (*observer_)->get().filter(sampled);
    }
    const bool embedding = embedded_->embedded.has_value();
    if (embedding) {
      const std::array<provider::PadState, provider::max_pads> held =
          sampled.pads;
      embedded_->framing = true;
      embed_frame(*embedded_->embedded, sampled);
      embedded_->framing = false;
      if (embedded_->detaching) {
        detach(held);
      }
    } else if (root_.input_gate().filter(sampled,
                                         router_.options().layout.confirm(),
                                         router_.options().layout.back())) {
      router_.interrupt(root_.tree());
    }
    if (blanking_.has_value()) {
      blank();
    }
    const bool touched = !sampled_.has_value() || *sampled_ != sampled;
    const auto started = clock.now();
    appear(root_.tree());
    root_.frame();
    timings.reconcile = clock.now() - started;
    auto prepared = renderer_.prepare(root_);
    if (!prepared.has_value()) {
      return std::unexpected(prepared.error());
    }
    const auto routing = clock.now();
    const std::uint64_t collected = router_.engine().collections();
    const std::uint64_t refreshed = router_.engine().refreshes();
    const std::uint64_t uncovered = router_.engine().uncovered();
    interaction::InputRouter::Outcome routed =
        embedding ? interaction::InputRouter::Outcome{}
                  : router_.update(sampled, root_.tree(), renderer_.scale(),
                                   routing.since_origin(),
                                   input.tuning().navigation());
    timings.input = clock.now() - routing;
    if (router_.take_keyboard_dismissal()) {
      text_input_.dismiss();
    }
    timings.focus_collections = router_.engine().collections() - collected;
    timings.focus_refreshes = router_.engine().refreshes() - refreshed;
    timings.focus_uncovered = router_.engine().uncovered() - uncovered;
    sampled_ = std::move(sampled);
    for (const provider::SoundRole sound : routed.sounds) {
      static_cast<void>(audio_.get().play(sound));
    }
    const auto acting = clock.now();
    for (interaction::Invocation& invocation : routed.invocations) {
      invocation.perform(root_.tree());
    }
    const bool narrated = reader_.follow(root_.tree(), router_);
    timings.actions = clock.now() - acting;
    if (!routed.invocations.empty() || narrated) {
      const auto settling = clock.now();
      root_.settle();
      timings.reconcile += clock.now() - settling;
      auto settled = renderer_.prepare(root_);
      if (!settled.has_value()) {
        return std::unexpected(settled.error());
      }
      prepared->changed = prepared->changed || settled->changed;
      prepared->resized = prepared->resized || settled->resized;
      prepared->laid_out = prepared->laid_out || settled->laid_out;
      prepared->layout += settled->layout;
      prepared->computations += settled->computations;
    }
    feedback_.play(root_.tree().take_feedback(), clock.now().since_origin(),
                   haptic_strength());
    timings.layout = prepared->layout;
    timings.laid_out = prepared->laid_out;
    timings.layout_computations = prepared->computations;
    publish();
    const std::uint64_t revision = root_.tree().revision();
    const provider::FrameSchedule::Demand demand =
        schedule_.has_value() ? schedule_->get().demand()
                              : provider::FrameSchedule::Demand{};
    const bool requested =
        schedule_.has_value() && schedule_->get().take_request();
    const composition::ExclusiveScopes& scopes = root_.tree().exclusive();
    const bool exclusive = scopes.active();
    const bool unleased = lease(scopes.leased());
    const bool released = handed_back() || unleased;
    const bool parked = taken_over_ || scopes.leased();
    const bool work = exclusive
                          ? !rendered_.has_value() || prepared->resized ||
                                lifecycle || routed.changed || requested ||
                                released || scopes.revision() != scoped_ ||
                                kept_signature(root_.tree()) != kept_
                          : !rendered_.has_value() || *rendered_ != revision ||
                                prepared->changed || lifecycle || touched ||
                                routed.changed || requested || released;
    const bool driven =
        touched || routed.changed || !routed.invocations.empty();
    continuous_ = animating_ || demand.continuous;
    if (parked) {
      timings.kind = loop_.hold(exclusive);
      timings.leased = scopes.leased();
      timings.taken_over = taken_over_;
    } else {
      timings.kind = loop_.plan(FrameLoop::Signals{
          .work = work, .animating = continuous_, .exclusive = exclusive});
      if (timings.kind == FrameKind::animation && paced_.has_value() &&
          clock.now().since_origin() < *paced_) {
        timings.kind = FrameKind::idle;
        timings.paced = true;
      }
    }
    timings.exclusive = loop_.exclusive();
    if (timings.kind != FrameKind::idle) {
      std::optional<std::reference_wrapper<rendering::ReplayScene>> replay;
      if (render_thread_.has_value()) {
        if (render_thread_->replayed_since()) {
          renderer_.repaint_whole();
        }
        replay = std::ref(render_thread_->scene());
      }
      const std::chrono::nanoseconds painting = clock.now().since_origin();
      auto painted = renderer_.render(
          root_, highlights(router_), replay,
          PainterRenderer::Request{
              .exclusive = exclusive,
              .capture = capture_requested_,
              .predicted_present =
                  pacer_.active(painting, continuous_)
                      ? std::optional{pacer_.predicted_present(painting)}
                      : std::nullopt,
              .demand = demand,
              .driven = driven});
      if (!painted.has_value()) {
        if (render_thread_.has_value()) {
          render_thread_->abandon();
        }
        return std::unexpected(painted.error());
      }
      FrameTimings& drawn = painted->timings;
      if (painted->packet.has_value()) {
        auto handed = hand_off(std::move(*painted->packet), drawn);
        if (!handed.has_value()) {
          return std::unexpected(handed.error());
        }
        capture_requested_ = false;
        renderer_.measure(drawn);
        pacer_.presented(drawn.displayed, drawn.swap_interval);
        pacer_.built((clock.now() - began) - drawn.blocked() + drawn.record);
      } else if (render_thread_.has_value()) {
        render_thread_->abandon();
      }
      timings.paint = drawn.paint;
      timings.walk = drawn.walk;
      timings.upload = drawn.upload;
      timings.emit = drawn.emit;
      timings.present = drawn.present;
      timings.stall = drawn.stall;
      timings.record = drawn.record;
      timings.wait = drawn.wait;
      timings.threaded = drawn.threaded;
      timings.presented = drawn.presented;
      timings.animating = drawn.animating;
      timings.missed_vsyncs = drawn.missed_vsyncs;
      timings.encode_misses = drawn.encode_misses;
      timings.encode_missed_texture = drawn.encode_missed_texture;
      timings.encode_missed_target = drawn.encode_missed_target;
      timings.swap_interval = drawn.swap_interval;
      timings.displayed = drawn.displayed;
      timings.quads = drawn.quads;
      timings.commands = drawn.commands;
      animating_ = drawn.animating;
      rendered_ = revision;
      scoped_ = scopes.revision();
      kept_ = kept_signature(root_.tree());
      timings.frame_rate =
          FramePacer::rate(drawn.frame_rate, drawn.animating, demand);
      timings.unhinted = drawn.unhinted;
      timings.culled = drawn.culled;
      timings.visited = drawn.visited;
      timings.recorded = drawn.recorded;
      timings.spliced = drawn.spliced;
      timings.occluded = drawn.occluded;
      timings.shadow_hits = drawn.shadow_hits;
      timings.shadow_misses = drawn.shadow_misses;
      timings.tessellations = drawn.tessellations;
      timings.mesh_builds = drawn.mesh_builds;
      timings.pictures = drawn.pictures;
      timings.picture_bytes = drawn.picture_bytes;
      timings.backdrops = drawn.backdrops;
      timings.blurs = drawn.blurs;
      timings.blur_hits = drawn.blur_hits;
      timings.shared_backdrops = drawn.shared_backdrops;
      timings.effect_passes = drawn.effect_passes;
      timings.layer_renders = drawn.layer_renders;
      timings.layer_hits = drawn.layer_hits;
      timings.targets_created = drawn.targets_created;
      timings.targets_reused = drawn.targets_reused;
      timings.targets_released = drawn.targets_released;
      timings.targets_evicted = drawn.targets_evicted;
      timings.targets_refused = drawn.targets_refused;
      timings.uploaded_vertices = drawn.uploaded_vertices;
      timings.uploaded_bytes = drawn.uploaded_bytes;
      timings.damaged_pixels = drawn.damaged_pixels;
      timings.effects_gpu = drawn.effects_gpu;
      timings.interface_gpu = drawn.interface_gpu;
      timings.hooks_gpu = drawn.hooks_gpu;
      timings.gpu_measured = drawn.gpu_measured;
      paced_ = driven || pacer_.active(clock.now().since_origin(), continuous_)
                   ? std::nullopt
                   : FramePacer::skip_until(timings.frame_rate,
                                            started.since_origin());
    }
    timings.due = paced_;
    timings.evaluations = root_.tree().evaluations() - evaluated;
    timings.debug_evaluations = root_.tree().debug_evaluations() - debugged;
    timings.debug =
        root_.tree().debug_layer().has_value() && timings.evaluations == 0 &&
        (timings.debug_evaluations > 0 || touched || routed.changed);
    timings.shaped = renderer_.shaped() - shaped;
    root_.tree().close_transactions();
    persist(false);
    const auto finished = clock.now();
    timings.wall = finished - began;
    count(timings, created, counted, reads, decodes);
    last_frame_ = timings;
    if (observer_->has_value()) {
      (*observer_)->get().observe(last_frame_);
    }
    return {};
  }

  auto introspection() const noexcept -> const Introspection& {
    return introspection_;
  }

  auto screen_reader() const noexcept -> const ScreenReader& { return reader_; }

  auto observe(FrameObserver& observer) -> support::ScopedCallback {
    *observer_ = std::ref(observer);
    return support::ScopedCallback{
        [slot = std::weak_ptr<ObserverSlot>{observer_},
         watched = &observer]() noexcept {
          if (const std::shared_ptr<ObserverSlot> held = slot.lock();
              held != nullptr && held->has_value() &&
              &held->value().get() == watched) {
            held->reset();
          }
        }};
  }

  void count(FrameTimings& timings, std::uint64_t created,
             const PainterRenderer::Counters& counted, std::uint64_t reads,
             std::uint64_t decodes) const noexcept {
    const PainterRenderer::Counters now = renderer_.counters();
    timings.nodes_created = root_.tree().created() - created;
    timings.shaping_runs = now.shaping_runs - counted.shaping_runs;
    timings.text_layouts = now.text_layouts - counted.text_layouts;
    timings.rasterised = now.rasterised - counted.rasterised;
    timings.atlas_bytes = now.atlas_bytes - counted.atlas_bytes;
    timings.textures_created = now.textures_created - counted.textures_created;
    timings.memory_blocks_created =
        now.memory_blocks_created - counted.memory_blocks_created;
    timings.image_reads = images_.reads() - reads;
    timings.image_decodes = images_.decodes() - decodes;
  }

  auto interaction() const noexcept -> const interaction::InputRouter& {
    return router_;
  }

  auto waiting() const noexcept -> bool {
    return (loop_.waiting() || last_frame_.paced || early()) && !embedded_due();
  }

  auto embedded_due() const noexcept -> bool {
    if (!embedded_->embedded.has_value()) {
      return false;
    }
    const Embedded& embedded = *embedded_->embedded;
    const auto& frames = embedded.renderer.frames();
    return frames.has_value() &&
           (frames->closed() || (embedded.animating && !embedded.owed &&
                                 frames->uses() > embedded.seen));
  }

  auto embed(Root& overlay, provider::FrameSource frames,
             provider::SurfaceSize extent)
      -> std::expected<EmbeddedRoot, ApplicationError> {
    if (embedded_->embedded.has_value()) {
      return std::unexpected(
          ApplicationError{ApplicationErrorCode::already_embedded});
    }
    const std::uint64_t seen = frames.uses();
    auto renderer = PainterRenderer::embedded(providers_, painting_,
                                              std::move(frames), extent);
    if (!renderer.has_value()) {
      return std::unexpected(renderer.error());
    }
    blanking_.reset();
    Embedded& embedded = embedded_->embedded.emplace(
        overlay, std::move(*renderer), router_.options());
    embedded.seen = seen;
    router_.interrupt(root_.tree());
    if (sampled_.has_value()) {
      overlay.input_gate().latch(sampled_->pads);
    }
    return EmbeddedRoot{
        support::ScopedCallback{
            [this, seat = std::weak_ptr<EmbeddedSeat>{embedded_}]() noexcept {
              const std::shared_ptr<EmbeddedSeat> held = seat.lock();
              if (held == nullptr || !held->embedded.has_value()) {
                return;
              }
              if (held->framing) {
                held->detaching = true;
                return;
              }
              detach(
                  sampled_.has_value()
                      ? sampled_->pads
                      : std::array<provider::PadState, provider::max_pads>{});
            }},
        std::weak_ptr<const void>{embedded.token},
        [seat = std::weak_ptr<EmbeddedSeat>{embedded_}](
            provider::SurfaceSize size) {
          const std::shared_ptr<EmbeddedSeat> held = seat.lock();
          if (held == nullptr || !held->embedded.has_value() || size.empty()) {
            return false;
          }
          held->embedded->renderer.set_extent(size);
          held->embedded->owed = true;
          return true;
        }};
  }

  void detach(
      const std::array<provider::PadState, provider::max_pads>& held) noexcept {
    embedded_->detaching = false;
    if (!embedded_->embedded.has_value()) {
      return;
    }
    std::optional<provider::FrameSource> frames =
        embedded_->embedded->renderer.take_frames();
    embedded_->embedded.reset();
    if (frames.has_value() && !frames->closed()) {
      blanking_ = std::move(frames);
    }
    router_.interrupt(root_.tree());
    root_.input_gate().latch(held);
    if (blanking_.has_value()) {
      blank();
    }
  }

  void blank() {
    provider::FrameSource& source = *blanking_;
    if (source.closed()) {
      blanking_.reset();
      return;
    }
    auto opened = source.open();
    if (!opened.has_value()) {
      if (opened.error().code() !=
          provider::ProviderErrorCode::resource_exhausted) {
        blanking_.reset();
      }
      return;
    }
    auto sealed = opened->seal();
    if (sealed.has_value()) {
      static_cast<void>(sealed->submit());
    }
    blanking_.reset();
  }

  void keep_painting(const ApplicationOptions& options) {
    painting_.fonts = options.fonts;
    painting_.atlas = options.atlas;
    painting_.debug_glass_coverage = options.debug_glass_coverage;
    painting_.debug_full_walk = options.debug_full_walk;
    painting_.debug_separate_backdrops = options.debug_separate_backdrops;
    painting_.images = options.images;
  }

  void wait() {
    std::chrono::nanoseconds limit = idle_poll;
    const std::chrono::nanoseconds now = clock_.get().now().since_origin();
    if (paced_.has_value()) {
      limit = std::clamp(*paced_ - now, std::chrono::nanoseconds{0}, limit);
    }
    if (const auto start = next_build(); start.has_value()) {
      limit = std::clamp(*start - now, std::chrono::nanoseconds{0}, limit);
    }
    if (embedded_->embedded.has_value()) {
      const Embedded& embedded = *embedded_->embedded;
      const auto& frames = embedded.renderer.frames();
      if (frames.has_value() && (embedded.animating || embedded.owed)) {
        static_cast<void>(frames->await(embedded.seen, limit));
        return;
      }
    } else if (blanking_.has_value()) {
      static_cast<void>(blanking_->await(blanking_->uses(), limit));
      return;
    }
    root_.wait(limit);
  }

  auto next_build() const noexcept -> std::optional<std::chrono::nanoseconds> {
    const std::chrono::nanoseconds now = clock_.get().now().since_origin();
    if (!pacer_.active(now, continuous_)) {
      return std::nullopt;
    }
    return pacer_.next_build(now);
  }

  auto early() const noexcept -> bool {
    const auto start = next_build();
    return start.has_value() && *start > clock_.get().now().since_origin();
  }

  void request_capture() noexcept { capture_requested_ = true; }

  auto take_capture() -> std::optional<CapturedFrame> {
    if (render_thread_.has_value()) {
      return render_thread_->take_capture();
    }
    return std::exchange(captured_, std::nullopt);
  }

  auto lease(bool wanted) -> bool {
    if (wanted && !lease_.held()) {
      if (const auto queue = gpu_.get().render_queue(); queue.has_value()) {
        lease_ = queue->get().park();
        ++own_parks_;
      }
      return false;
    }
    if (!wanted && lease_.held()) {
      lease_.release();
      renderer_.repaint_whole();
      return true;
    }
    return false;
  }

  auto handed_back() -> bool {
    const auto queue = gpu_.get().render_queue();
    if (!queue.has_value()) {
      taken_over_ = false;
      return false;
    }
    const std::uint64_t external = queue->get().counters().parks - own_parks_;
    const bool taken = !lease_.held() && queue->get().parked();
    const bool missed = seen_parks_.has_value() && *seen_parks_ != external;
    seen_parks_ = external;
    const bool was = std::exchange(taken_over_, taken);
    const bool returned = !taken && (was || missed);
    if (returned) {
      renderer_.repaint_whole();
    }
    return returned;
  }

  auto last_frame() const noexcept -> const FrameTimings& {
    return last_frame_;
  }

  auto root() noexcept -> Root& { return root_; }

  auto executors() const noexcept -> const async::Executors& {
    return root_.executors();
  }

  auto text_input() const noexcept -> TextInput { return text_input_; }

  auto exit_requested() const noexcept -> bool { return exit_requested_; }

  void step_animations() {
    if (render_thread_.has_value()) {
      render_thread_->step();
    }
  }

  void drain() {
    if (render_thread_.has_value()) {
      render_thread_->drain();
    }
  }

  auto replayed_frames() const noexcept -> std::uint64_t {
    return render_thread_.has_value() ? render_thread_->replayed() : 0;
  }

  auto animation_wakes() const noexcept -> std::uint64_t {
    return render_thread_.has_value() ? render_thread_->wakes() : 0;
  }

  auto render_thread() const noexcept -> bool {
    return render_thread_.has_value();
  }

 private:
  auto hand_off(provider::FramePacket packet, FrameTimings& drawn)
      -> std::expected<void, ApplicationError> {
    provider::ClockProvider& clock = clock_;
    const auto handing = clock.now();
    if (render_thread_.has_value()) {
      auto handed =
          render_thread_->submit(std::move(packet), handing.since_origin());
      if (!handed.has_value()) {
        return std::unexpected(handed.error());
      }
      drawn.stall += handed->stall;
      if (handed->receipt.has_value()) {
        drawn.record = handed->receipt->record;
        drawn.wait = handed->receipt->wait;
        drawn.displayed = handed->receipt->displayed;
      }
      drawn.threaded = true;
    } else {
      const std::uint64_t frame = packet.frame();
      auto receipt = packet.submit(std::cref(clock));
      if (!receipt.has_value() &&
          receipt.error().detail() == provider::ProviderErrorDetail::taken_over) {
        return {};
      }
      if (!receipt.has_value()) {
        return std::unexpected(ApplicationError{
            ApplicationErrorCode::presentation_failed, receipt.error()});
      }
      drawn.record = receipt->record;
      drawn.wait = receipt->wait;
      drawn.displayed = receipt->displayed;
      if (receipt->image.has_value()) {
        captured_.emplace(
            CapturedFrame{.frame = frame, .image = std::move(*receipt->image)});
      }
    }
    drawn.present += clock.now() - handing;
    drawn.presented = true;
    return {};
  }

  static auto select(std::optional<async::Executors> executors,
                     std::optional<async::ThreadPool>& background,
                     std::optional<async::ThreadPool>& blocking)
      -> async::Executors {
    if (executors.has_value()) {
      return *executors;
    }
    return async::Executors{.background = *background, .blocking = *blocking};
  }

  static auto kept_signature(const composition::Tree& tree) -> std::uint64_t {
    constexpr std::uint64_t prime = 1'099'511'628'211ULL;
    const composition::ExclusiveScopes& scopes = tree.exclusive();
    std::uint64_t signature = scopes.revision();
    for (const composition::NodeId id : tree.layers()) {
      const auto node = tree.node(id);
      if (!node.has_value()) {
        continue;
      }
      const auto layer = node->get().content_as<composition::Layer::Content>();
      if (!layer.has_value() || !scopes.keeps(layer->get().layer())) {
        continue;
      }
      signature = (signature ^ id.value()) * prime;
      signature = (signature ^ node->get().paint_stamp()) * prime;
    }
    return signature;
  }

  struct Embedded {
    Embedded(Root& overlay, PainterRenderer&& painter,
             interaction::InteractionOptions routing)
        : root(overlay), renderer(std::move(painter)), router(routing) {}

    std::reference_wrapper<Root> root;
    PainterRenderer renderer;
    interaction::InputRouter router;
    std::shared_ptr<const bool> token{std::make_shared<const bool>(true)};
    std::optional<std::uint64_t> rendered;
    std::optional<composition::ObservableVersion> announced;
    std::uint64_t seen{0};
    bool animating{false};
    bool owed{false};
  };

  struct EmbeddedSeat {
    std::optional<Embedded> embedded;
    bool framing{false};
    bool detaching{false};
  };

  static auto handed_back(const provider::ProviderError& error) noexcept
      -> bool {
    return error.detail() == provider::ProviderErrorDetail::not_taken_over;
  }

  void embed_frame(Embedded& embedded, provider::InputFrame& sampled) {
    const auto& frames = embedded.renderer.frames();
    if (!frames.has_value() || frames->closed()) {
      embedded_->detaching = true;
      return;
    }
    Root& overlay = embedded.root;
    composition::Tree& tree = overlay.tree();
    provider::ClockProvider& clock = clock_;
    const std::chrono::nanoseconds now = clock.now().since_origin();
    if (overlay.input_gate().filter(sampled,
                                    embedded.router.options().layout.confirm(),
                                    embedded.router.options().layout.back())) {
      embedded.router.interrupt(tree);
    }
    embedded.renderer.set_ui_scale(renderer_.ui_scale(), now, true);
    present_embedded(embedded);
    overlay.frame();
    auto prepared = embedded.renderer.prepare(overlay);
    if (!prepared.has_value()) {
      embedded.owed = true;
      return;
    }
    interaction::InputRouter::Outcome routed = embedded.router.update(
        sampled, tree, embedded.renderer.scale(), clock.now().since_origin(),
        input_.get().tuning().navigation());
    for (const provider::SoundRole sound : routed.sounds) {
      static_cast<void>(audio_.get().play(sound));
    }
    for (interaction::Invocation& invocation : routed.invocations) {
      invocation.perform(tree);
    }
    bool changed = prepared->changed;
    if (!routed.invocations.empty()) {
      overlay.settle();
      auto settled = embedded.renderer.prepare(overlay);
      if (!settled.has_value()) {
        embedded.owed = true;
        return;
      }
      changed = changed || settled->changed;
    }
    feedback_.play(tree.take_feedback(), clock.now().since_origin(),
                   haptic_strength());
    tree.highlight(embedded.router.focused(), embedded.router.pressed());
    const std::uint64_t revision = tree.revision();
    const std::uint64_t uses = frames->uses();
    const bool paced = embedded.animating && uses > embedded.seen;
    const bool work = !embedded.rendered.has_value() ||
                      *embedded.rendered != revision || changed ||
                      routed.changed || !routed.invocations.empty() || paced ||
                      embedded.owed;
    if (work) {
      embedded.owed = true;
      embedded.seen = uses;
      auto painted = embedded.renderer.render(
          overlay, highlights(embedded.router), std::nullopt,
          PainterRenderer::Request{.exclusive = false,
                                   .capture = false,
                                   .predicted_present = std::nullopt,
                                   .demand = {},
                                   .driven = true});
      if (!painted.has_value()) {
        const auto& cause = painted.error().cause();
        if (cause.has_value() && handed_back(*cause)) {
          embedded_->detaching = true;
        }
      } else if (painted->packet.has_value()) {
        auto submitted = painted->packet->submit(std::cref(clock));
        if (submitted.has_value()) {
          embedded.rendered = revision;
          embedded.animating = painted->timings.animating;
          embedded.owed = false;
        } else if (handed_back(submitted.error())) {
          embedded_->detaching = true;
        }
      }
    }
    tree.close_transactions();
  }

  void present_embedded(Embedded& embedded) {
    composition::Tree& tree = embedded.root.get().tree();
    const interaction::InputRouter& router = embedded.router;
    tree.set_environment(components::Environment::input_modality,
                         router.modality());
    tree.set_environment(components::Environment::command_hints,
                         router.hints());
    tree.set_environment(components::Environment::controller_glyphs, glyphs_);
    tree.set_environment(components::Environment::scene_phase,
                         lifecycle_.get().phase());
    const provider::DisplayMode mode = renderer_.display_mode();
    tree.set_environment(components::Environment::display_mode, mode);
    const float scale = embedded.renderer.scale().value();
    tree.set_environment(components::Environment::display_scale, scale);
    tree.set_environment(components::Environment::pixel_length, 1.0F / scale);
    const components::UserInterfaceSizeClass size_class =
        mode == provider::DisplayMode::handheld
            ? components::UserInterfaceSizeClass::compact
            : components::UserInterfaceSizeClass::regular;
    tree.set_environment(components::Environment::horizontal_size_class,
                         size_class);
    tree.set_environment(components::Environment::vertical_size_class,
                         size_class);
    tree.set_environment(components::Environment::image_library,
                         std::optional<components::ImageLibrary>{images_});
    tree.set_environment(components::Environment::image_cache,
                         std::optional<components::ImageCache>{cache_});
    tree.set_environment(components::Environment::system_status, status_);
    tree.set_environment(components::Environment::open_url, open_url_);
    tree.set_environment(
        Overlays::environment,
        std::optional<Overlays>{embedded.root.get().overlays()});
    tree.set_environment(components::Environment::time_zone, time_zone_);
    tree.set_environment(components::Environment::locale, locale_);
    tree.set_environment(components::Environment::layout_direction,
                         locale_.right_to_left()
                             ? components::LayoutDirection::right_to_left
                             : components::LayoutDirection::left_to_right);
    tree.set_environment(components::Environment::calendar,
                         components::Calendar::for_locale(locale_));
    tree.set_environment(components::Environment::string_catalog, catalog_);
    if (model_.has_value() &&
        embedded.announced != model_->settings().version()) {
      embedded.announced = model_->settings().version();
      announce(tree, model_->settings().get());
    }
  }

  auto highlights(const interaction::InputRouter& router) const
      -> rendering::Painter::Highlights {
    rendering::Painter::Highlights highlights{
        .focused = router.focused(),
        .pressed = router.pressed(),
        .tilt = router.tilt(),
        .focus_visible = router.focus_visible(),
        .shake = std::nullopt,
        .contact = router.contact(),
        .touched = router.touched(),
        .hovering = router.hover_path()};
    if (const auto& shake = router.shake()) {
      highlights.shake =
          rendering::Painter::Shake{.node = shake->node,
                                    .direction = shake->direction,
                                    .started = shake->started};
    }
    return highlights;
  }

  void publish() {
    composition::Tree& tree = root_.tree();
    tree.set_environment(components::Environment::input_modality,
                         router_.modality());
    tree.set_environment(components::Environment::command_hints,
                         router_.hints());
    tree.set_environment(components::Environment::controller_glyphs, glyphs_);
    const provider::ScenePhase phase = lifecycle_.get().phase();
    tree.set_environment(components::Environment::scene_phase, phase);
    if (phase == provider::ScenePhase::background &&
        phase_ != provider::ScenePhase::background) {
      background_tasks_.run_pending();
    }
    phase_ = phase;
    tree.set_environment(components::Environment::text_input,
                         std::optional<TextInput>{text_input_});
    tree.set_environment(components::Environment::image_library,
                         std::optional<components::ImageLibrary>{images_});
    tree.set_environment(components::Environment::image_cache,
                         std::optional<components::ImageCache>{cache_});
    tree.set_environment(components::Environment::system_status, status_);
    const provider::DisplayMode mode = renderer_.display_mode();
    tree.set_environment(components::Environment::display_mode, mode);
    const float scale = renderer_.scale().value();
    tree.set_environment(components::Environment::display_scale, scale);
    tree.set_environment(components::Environment::pixel_length, 1.0F / scale);
    const bool handheld = mode == provider::DisplayMode::handheld;
    const components::UserInterfaceSizeClass size_class =
        handheld ? components::UserInterfaceSizeClass::compact
                 : components::UserInterfaceSizeClass::regular;
    tree.set_environment(components::Environment::horizontal_size_class,
                         size_class);
    router_.set_horizontally_compact(handheld);
    tree.set_environment(components::Environment::vertical_size_class,
                         size_class);
    tree.set_environment(components::Environment::open_url, open_url_);
    tree.set_environment(Overlays::environment,
                         std::optional<Overlays>{root_.overlays()});
    tree.set_environment(components::Environment::time_zone, time_zone_);
    tree.set_environment(components::Environment::locale, locale_);
    tree.set_environment(components::Environment::layout_direction,
                         locale_.right_to_left()
                             ? components::LayoutDirection::right_to_left
                             : components::LayoutDirection::left_to_right);
    tree.set_environment(components::Environment::calendar,
                         components::Calendar::for_locale(locale_));
    tree.set_environment(components::Environment::string_catalog, catalog_);
    tree.set_environment(components::Environment::file_storage, files_);
    tree.set_environment(components::Environment::system_applets, applets_);
    tree.set_environment(
        composition::UserDefaults::app_storage,
        std::optional<composition::UserDefaults>{app_storage_});
    tree.set_environment(
        composition::UserDefaults::scene_storage,
        std::optional<composition::UserDefaults>{scene_storage_});
    tree.set_environment(composition::Pasteboard::general,
                         std::optional<composition::Pasteboard>{pasteboard_});
    tree.set_environment(components::Environment::reset_focus, reset_focus_);
    tree.set_environment(components::Environment::open_url_inbox,
                         std::optional<components::OpenURLInbox>{inbox_});
    tree.set_environment(
        composition::BackgroundTaskScheduler::key,
        std::optional<composition::BackgroundTaskScheduler>{background_tasks_});
    tree.set_environment(components::Environment::undo_manager,
                         std::optional<composition::UndoManager>{undo_});
    conserve(tree);
    tree.set_environment(
        components::Environment::persistent_system_overlays,
        tree.root()
            .preference(composition::PersistentSystemOverlays::key)
            .value_or(composition::Visibility::automatic));
    if (launch_url_.has_value() && ++published_frames_ > 1) {
      inbox_.deliver(*std::exchange(launch_url_, std::nullopt));
    }
    prefer_scheme(tree);
    defer_system_gestures(tree);
    tree.highlight(router_.focused(), router_.pressed());
    drag_preview_.present(tree, router_.drag());
    pointer_cursor_.present(tree, router_.pointer(), input_.get());
  }

  void defer_system_gestures(const composition::Tree& tree) {
    const bool wanted = tree.root()
                            .preference(composition::SystemGestureDeferral::key)
                            .value_or(0U) != 0U;
    if (!wanted) {
      home_block_.reset();
      home_refused_ = false;
      return;
    }
    if (home_block_.has_value() || home_refused_) {
      return;
    }
    std::expected<provider::HomeButtonBlock, provider::ProviderError> block =
        lifecycle_.get().block_home_button();
    if (block.has_value()) {
      home_block_.emplace(std::move(*block));
    } else {
      home_refused_ = true;
    }
  }

  void conserve(composition::Tree& tree) {
    const std::chrono::nanoseconds now = clock_.get().now().since_origin();
    if (battery_checked_.has_value() &&
        now - *battery_checked_ < battery_interval) {
      return;
    }
    battery_checked_ = now;
    const auto battery = status_.battery();
    const bool reduced = battery.has_value() && !battery->charging &&
                         battery->percent.value() <= low_battery_percent;
    tree.set_environment(
        components::Environment::system_prefers_reduced_resource_usage,
        reduced);
  }

 public:
  void route_links(LinkOptions links) {
    inbox_ = components::OpenURLInbox{std::move(links.claimed)};
    if (links.launch_url.has_value()) {
      if (auto parsed = provider::Url::create(*links.launch_url);
          parsed.has_value()) {
        launch_url_ = std::move(*parsed);
      }
    }
    open_url_ = components::OpenURLAction{
        [system = open_url_, inbox = inbox_](
            const provider::Url& url) -> components::OpenURLAction::Result {
          if (inbox.claims(url)) {
            inbox.deliver(url);
            return components::OpenURLAction::Result::handled;
          }
          return system(url) ? components::OpenURLAction::Result::handled
                             : components::OpenURLAction::Result::discarded;
        }};
  }

  void open_url(provider::Url url) const { inbox_.deliver(std::move(url)); }

 private:
  void prefer_scheme(composition::Tree& tree) {
    const std::optional<std::optional<theme::ColorScheme>> found =
        tree.root().preference(composition::PreferredColorScheme::key);
    const std::optional<theme::ColorScheme> preferred =
        found.has_value() ? *found : std::nullopt;
    if (preferred == preferred_scheme_) {
      return;
    }
    preferred_scheme_ = preferred;
    if (preferred.has_value()) {
      tree.set_environment(components::Environment::colour_scheme, preferred);
    } else if (model_.has_value()) {
      tree.set_environment(components::Environment::colour_scheme,
                           std::optional<theme::ColorScheme>{
                               model_->settings().get().resolved_scheme()});
    } else {
      tree.set_environment(components::Environment::colour_scheme,
                           std::optional<theme::ColorScheme>{});
    }
  }

  static auto pasteboard_for(provider::ProviderFactory& providers)
      -> composition::Pasteboard {
    if (providers.supplies(provider::Capability::clipboard)) {
      return composition::Pasteboard{providers.clipboard()};
    }
    return composition::Pasteboard{};
  }

  static auto defaults(
      std::optional<std::reference_wrapper<provider::StorageProvider>> storage,
      const std::string& file, provider::DiagnosticsProvider& diagnostics)
      -> composition::UserDefaults {
    if (!storage.has_value()) {
      return composition::UserDefaults{};
    }
    const auto path = provider::AssetPath::create(file);
    if (!path.has_value()) {
      return composition::UserDefaults{};
    }
    auto opened = composition::UserDefaults::open(storage->get(), *path);
    if (!opened.has_value()) {
      diagnostics.report(provider::Diagnostic{
          .severity = provider::Severity::warning,
          .category = "storage",
          .message = std::format("{} could not be read; it is kept and the "
                                 "values start empty",
                                 file)});
      return composition::UserDefaults{};
    }
    return std::move(*opened);
  }

  static auto zone(const provider::ClockProvider& clock)
      -> components::TimeZone {
    const auto offset = clock.local_offset();
    if (!offset.has_value()) {
      return components::TimeZone::gmt();
    }
    if (auto name = clock.zone_name(); name.has_value()) {
      return components::TimeZone::named(std::move(*name), *offset);
    }
    return components::TimeZone::seconds_from_gmt(*offset);
  }

  static auto locale_for(const LocalizationOptions& localization,
                         const provider::SystemInfoProvider& system)
      -> text::Locale {
    if (localization.locale.has_value()) {
      return *localization.locale;
    }
    const auto languages = system.preferred_languages();
    if (!languages.has_value()) {
      return text::Locale::undetermined();
    }
    for (const std::string& tag : *languages) {
      if (auto parsed = text::Locale::create(tag)) {
        return std::move(*parsed);
      }
    }
    return text::Locale::undetermined();
  }

  static auto catalog_for(const LocalizationOptions& localization,
                          const provider::AssetProvider& assets,
                          provider::DiagnosticsProvider& diagnostics)
      -> std::optional<text::StringCatalog> {
    text::StringCatalog::Loaded framework =
        text::StringCatalog::load(assets, framework_strings);
    text::StringCatalog::Loaded application =
        text::StringCatalog::load(assets, localization.strings);
    for (const text::StringCatalog::Loaded* loaded :
         {&framework, &application}) {
      for (const text::StringCatalog::Failure& failure : loaded->failures) {
        diagnostics.report(provider::Diagnostic{
            .severity = provider::Severity::warning,
            .category = "strings",
            .message =
                std::format("{}:{}: {}; the table is skipped", failure.file,
                            failure.line, failure.reason)});
      }
    }
    text::StringCatalog merged = framework.catalog.merged(application.catalog);
    if (merged.empty()) {
      return std::nullopt;
    }
    return merged;
  }

  void persist(bool now) {
    if (!app_storage_.pending() && !scene_storage_.pending()) {
      return;
    }
    const std::chrono::nanoseconds at = clock_.get().now().since_origin();
    if (!now && persisted_.has_value() && at - *persisted_ < persist_interval) {
      return;
    }
    persisted_ = at;
    for (const composition::UserDefaults& store :
         {app_storage_, scene_storage_}) {
      if (auto flushed = store.flush(); !flushed.has_value()) {
        diagnostics_.get().report(provider::Diagnostic{
            .severity = provider::Severity::warning,
            .category = "storage",
            .message = std::format("stored values could not be saved: {}",
                                   flushed.error().description())});
      }
    }
  }

  void appear(composition::Tree& tree) {
    const std::chrono::nanoseconds now = clock_.get().now().since_origin();
    if (!model_.has_value()) {
      renderer_.set_ui_scale(
          appearance::UiScales{}.resolve(renderer_.display_mode()), now, false);
      return;
    }
    const appearance::AppearanceSettings& settings = model_->settings().get();
    const composition::ObservableVersion version = model_->settings().version();
    if (!published_.has_value() || *published_ != version) {
      const bool first = !published_.has_value();
      published_ = version;
      motion::with_animation(
          first || !model_->animated()
              ? std::nullopt
              : std::optional<motion::Animation>{motion::Animation::smooth(
                    appearance_duration)},
          [this, &tree, &settings] { announce(tree, settings); });
    }
    renderer_.set_ui_scale(settings.ui_scale.resolve(renderer_.display_mode()),
                           now, settings.reduce_motion);
  }

  auto haptic_strength() const -> provider::HapticStrength {
    if (!model_.has_value()) {
      return provider::HapticStrength{};
    }
    return model_->settings().get().haptic_strength;
  }

  void announce(composition::Tree& tree,
                const appearance::AppearanceSettings& settings) {
    tree.set_environment(components::Environment::theme, model_->theme());
    tree.set_environment(
        components::Environment::colour_scheme,
        preferred_scheme_.has_value()
            ? preferred_scheme_
            : std::optional<theme::ColorScheme>{settings.resolved_scheme()});
    tree.set_environment(components::Environment::dynamic_type_size,
                         settings.dynamic_type_size);
    tree.set_environment(components::Environment::reduce_motion,
                         settings.reduce_motion);
    tree.set_environment(
        components::Environment::accessibility_reduce_transparency,
        settings.reduce_transparency);
    tree.set_environment(components::Environment::color_scheme_contrast,
                         settings.increase_contrast
                             ? theme::ColorSchemeContrast::increased
                             : theme::ColorSchemeContrast::standard);
    tree.set_environment(components::Environment::sidebar_visibility,
                         settings.sidebar);
    tree.set_environment(components::Environment::legibility_weight,
                         settings.bold_text
                             ? composition::LegibilityWeight::bold
                             : composition::LegibilityWeight::regular);
    tree.set_environment(
        components::Environment::accessibility_show_button_shapes,
        settings.button_shapes);
    tree.set_environment(
        components::Environment::accessibility_differentiate_without_color,
        settings.differentiate_without_color);
    tree.set_environment(
        components::Environment::accessibility_dim_flashing_lights,
        settings.dim_flashing_lights);
    announce_assistive(tree, settings);
    tree.set_environment(components::Environment::is_system_feedback_enabled,
                         settings.sensory_feedback);
    tree.set_environment(components::Environment::haptic_strength,
                         settings.haptic_strength);
    tree.set_environment(components::Environment::appearance,
                         std::optional<appearance::AppearanceModel>{*model_});
  }

  void announce_assistive(composition::Tree& tree,
                          const appearance::AppearanceSettings& settings) {
    composition::AccessibilityTechnologies running;
    if (settings.voice_over) {
      running.form_union(composition::AccessibilityTechnologies::voice_over);
    }
    if (settings.switch_control) {
      running.form_union(
          composition::AccessibilityTechnologies::switch_control);
    }
    tree.set_environment(components::Environment::accessibility_technologies,
                         running);
    tree.set_environment(components::Environment::accessibility_enabled,
                         !running.empty());
    tree.set_environment(
        components::Environment::accessibility_voice_over_enabled,
        settings.voice_over);
    tree.set_environment(
        components::Environment::accessibility_switch_control_enabled,
        settings.switch_control);
    tree.set_environment(components::Environment::accessibility_invert_colors,
                         settings.invert_colors);
    tree.set_environment(
        components::Environment::accessibility_play_animated_images,
        settings.play_animated_images);
    tree.set_environment(
        components::Environment::accessibility_prefers_cross_fade_transitions,
        settings.prefers_cross_fade_transitions);
    tree.set_environment(
        components::Environment::accessibility_reduce_highlighting_effects,
        settings.reduce_highlighting_effects);
    tree.set_environment(components::Environment::accessibility_show_borders,
                         settings.show_borders);
    tree.set_environment(
        components::Environment::accessibility_assistive_access_enabled,
        settings.assistive_access);
    const bool large =
        theme::DynamicType::is_accessibility_size(settings.dynamic_type_size);
    tree.set_environment(
        components::Environment::accessibility_large_content_viewer_enabled,
        large);
    reader_.configure(tree, running, large);
  }

  std::reference_wrapper<provider::ProviderFactory> providers_;
  std::reference_wrapper<provider::LifecycleProvider> lifecycle_;
  std::optional<provider::HomeButtonBlock> home_block_;
  bool home_refused_{false};
  std::reference_wrapper<provider::GpuProvider> gpu_;
  std::reference_wrapper<provider::ClockProvider> clock_;
  std::reference_wrapper<provider::InputProvider> input_;
  std::reference_wrapper<provider::AudioProvider> audio_;
  FeedbackPlayer feedback_;
  interaction::InputRouter router_;
  ScreenReader reader_;
  std::optional<provider::InputFrame> sampled_;
  TextInput text_input_;
  components::ImageLibrary images_;
  components::ImageCache cache_;
  components::SystemStatus status_;
  std::reference_wrapper<provider::DiagnosticsProvider> diagnostics_;
  components::OpenURLAction open_url_;
  components::OpenURLInbox inbox_;
  composition::BackgroundTaskScheduler background_tasks_;
  composition::UndoManager undo_;
  std::optional<provider::Url> launch_url_;
  std::uint64_t published_frames_{0};
  std::optional<std::chrono::nanoseconds> battery_checked_;
  static constexpr std::chrono::seconds battery_interval{10};
  static constexpr std::uint8_t low_battery_percent = 20;
  provider::ScenePhase phase_{provider::ScenePhase::active};
  composition::Pasteboard pasteboard_;
  composition::UserDefaults app_storage_;
  composition::UserDefaults scene_storage_;
  components::TimeZone time_zone_;
  text::Locale locale_;
  std::optional<text::StringCatalog> catalog_;
  std::optional<components::SystemApplets> applets_;
  std::optional<components::FileStorage> files_;
  static constexpr std::string_view framework_strings = "arisa/strings";
  std::optional<std::chrono::nanoseconds> persisted_;
  static constexpr std::chrono::seconds persist_interval{1};
  PainterRenderer renderer_;
  std::optional<async::ThreadPool> background_;
  std::optional<async::ThreadPool> blocking_;
  Root root_;
  FrameTimings last_frame_;
  FrameLoop loop_;
  std::optional<std::uint64_t> rendered_;
  std::uint64_t scoped_{0};
  std::uint64_t kept_{0};
  static constexpr double appearance_duration = 0.35;

  bool glyphs_{false};
  bool animating_{false};
  std::optional<std::chrono::nanoseconds> paced_;
  bool exit_requested_{false};
  std::optional<appearance::AppearanceModel> model_;
  std::optional<composition::ObservableVersion> published_;
  std::optional<theme::ColorScheme> preferred_scheme_;
  std::optional<appearance::ThemeWatcher> watcher_;
  std::optional<RenderThread> render_thread_;
  provider::RenderQueue::Lease lease_;
  std::uint64_t own_parks_{0};
  std::optional<std::uint64_t> seen_parks_;
  bool taken_over_{false};
  FramePacer pacer_;
  bool continuous_{false};
  bool capture_requested_{false};
  std::optional<CapturedFrame> captured_;
  std::optional<std::reference_wrapper<provider::FrameSchedule>> schedule_;
  support::ScopedCallback waker_;
  using ObserverSlot = std::optional<std::reference_wrapper<FrameObserver>>;
  std::shared_ptr<ObserverSlot> observer_{std::make_shared<ObserverSlot>()};
  Introspection introspection_;
  composition::ResetFocusAction reset_focus_;
  DragPreview drag_preview_;
  PointerCursor pointer_cursor_;
  ApplicationOptions painting_;
  std::shared_ptr<EmbeddedSeat> embedded_{std::make_shared<EmbeddedSeat>()};
  std::optional<provider::FrameSource> blanking_;
};

Application::Application(
    std::unique_ptr<Implementation> implementation) noexcept
    : implementation_(std::move(implementation)) {}

Application::Application(Application&& other) noexcept = default;

auto Application::operator=(Application&& other) noexcept
    -> Application& = default;

Application::~Application() = default;

auto Application::create(provider::ProviderFactory& providers,
                         composition::Element content,
                         std::optional<async::Executors> executors)
    -> std::expected<Application, ApplicationError> {
  return create(
      providers, std::move(content),
      ApplicationOptions{
          .executors = executors, .fonts = {}, .atlas = {}, .interaction = {}});
}

auto Application::create(provider::ProviderFactory& providers,
                         composition::Element content,
                         ApplicationOptions options)
    -> std::expected<Application, ApplicationError> {
  const std::optional<async::Executors> executors = options.executors;
  if (!providers.supplies(provider::Capability::surface)) {
    return std::unexpected(
        ApplicationError{ApplicationErrorCode::missing_surface});
  }
  if (!providers.supplies(provider::Capability::input)) {
    return std::unexpected(
        ApplicationError{ApplicationErrorCode::missing_input});
  }
  if (!providers.supplies(provider::Capability::gpu)) {
    return std::unexpected(ApplicationError{ApplicationErrorCode::missing_gpu});
  }
  providers.threading().adopt_current_thread(provider::ThreadRole::ui);
  auto renderer = PainterRenderer::create(providers, options);
  if (!renderer.has_value()) {
    return std::unexpected(renderer.error());
  }
  std::optional<async::ThreadPool> background;
  std::optional<async::ThreadPool> blocking;
  if (!executors.has_value()) {
    auto background_pool = async::ThreadPool::create(
        providers.threading(),
        async::ThreadPool::Config{
            .workers = async::WorkerCount::of<background_workers>(),
            .role = provider::ThreadRole::background,
            .name = "arisa-background"});
    if (!background_pool.has_value()) {
      return std::unexpected(
          ApplicationError{ApplicationErrorCode::executors_unavailable,
                           background_pool.error()});
    }
    auto blocking_pool = async::ThreadPool::create(
        providers.threading(),
        async::ThreadPool::Config{.workers = async::WorkerCount::of<1>(),
                                  .role = provider::ThreadRole::blocking,
                                  .name = "arisa-blocking"});
    if (!blocking_pool.has_value()) {
      return std::unexpected(ApplicationError{
          ApplicationErrorCode::executors_unavailable, blocking_pool.error()});
    }
    background.emplace(std::move(*background_pool));
    blocking.emplace(std::move(*blocking_pool));
  }
  std::optional<RenderThread> render_thread;
  if (options.render_thread && providers.gpu().presents_off_thread()) {
    auto started = RenderThread::create(providers.threading(), providers.gpu(),
                                        providers.clock());
    if (!started.has_value()) {
      return std::unexpected(started.error());
    }
    render_thread.emplace(std::move(*started));
  }
  auto implementation = std::make_unique<Implementation>(
      providers, std::move(*renderer), std::move(background),
      std::move(blocking), executors, std::move(content), options.interaction,
      std::move(options.appearance), std::move(render_thread), options.pacing,
      options.images, std::move(options.storage), options.localization,
      options.files);
  implementation->keep_painting(options);
  implementation->route_links(std::move(options.links));
  return Application{std::move(implementation)};
}

void Application::open_url(provider::Url url) const {
  implementation_->open_url(std::move(url));
}

auto Application::frame() -> std::expected<void, ApplicationError> {
  return implementation_->frame();
}

auto Application::run() -> std::expected<void, ApplicationError> {
  return run_until([](Root&) { return false; });
}

auto Application::run_until(StopCondition stop)
    -> std::expected<void, ApplicationError> {
  while (!implementation_->exit_requested()) {
    if (auto framed = implementation_->frame(); !framed.has_value()) {
      return framed;
    }
    if (stop(implementation_->root())) {
      break;
    }
    if (implementation_->waiting()) {
      implementation_->wait();
    }
  }
  return {};
}

auto Application::root() noexcept -> Root& { return implementation_->root(); }

auto Application::embed(Root& overlay, provider::FrameSource frames,
                        provider::SurfaceSize extent)
    -> std::expected<EmbeddedRoot, ApplicationError> {
  return implementation_->embed(overlay, std::move(frames), extent);
}

auto Application::block_input() -> InputBlock {
  return implementation_->root().block_input();
}

auto Application::pass_input(provider::Buttons buttons, PassedInput passed)
    -> InputPassthrough {
  return implementation_->root().pass_input(buttons, passed);
}

auto Application::capture_buttons() -> ButtonCapture {
  return implementation_->root().capture_buttons();
}

auto Application::exclusive_frame(composition::ExclusiveFrame::Options options)
    -> composition::ExclusiveFrame {
  const bool leasing = options.lease;
  composition::ExclusiveFrame scope =
      implementation_->root().exclusive_frame(std::move(options));
  if (leasing) {
    static_cast<void>(implementation_->lease(true));
  }
  return scope;
}

auto Application::next_build() const noexcept
    -> std::optional<std::chrono::nanoseconds> {
  return implementation_->next_build();
}

void Application::request_capture() noexcept {
  implementation_->request_capture();
}

auto Application::take_capture() -> std::optional<CapturedFrame> {
  return implementation_->take_capture();
}

auto Application::executors() const noexcept -> const async::Executors& {
  return implementation_->executors();
}

auto Application::text_input() const noexcept -> TextInput {
  return implementation_->text_input();
}

auto Application::exit_requested() const noexcept -> bool {
  return implementation_->exit_requested();
}

auto Application::last_frame() const noexcept -> const FrameTimings& {
  return implementation_->last_frame();
}

auto Application::waiting() const noexcept -> bool {
  return implementation_->waiting();
}

void Application::wait() { implementation_->wait(); }

void Application::step_animations() { implementation_->step_animations(); }

void Application::drain() { implementation_->drain(); }

auto Application::render_thread() const noexcept -> bool {
  return implementation_->render_thread();
}

auto Application::replayed_frames() const noexcept -> std::uint64_t {
  return implementation_->replayed_frames();
}

auto Application::animation_wakes() const noexcept -> std::uint64_t {
  return implementation_->animation_wakes();
}

auto Application::introspection() const noexcept -> const Introspection& {
  return implementation_->introspection();
}

auto Application::screen_reader() const noexcept -> const ScreenReader& {
  return implementation_->screen_reader();
}

auto Application::observe(FrameObserver& observer) -> support::ScopedCallback {
  return implementation_->observe(observer);
}

auto Application::appearance() const noexcept
    -> const std::optional<appearance::AppearanceModel>& {
  return implementation_->appearance();
}

auto Application::theme_watcher() const noexcept
    -> std::optional<std::reference_wrapper<const appearance::ThemeWatcher>> {
  return implementation_->theme_watcher();
}

auto Application::interaction() const noexcept
    -> const interaction::InputRouter& {
  return implementation_->interaction();
}

auto Application::focused() const noexcept
    -> std::optional<composition::NodeId> {
  return implementation_->interaction().focused();
}

}
