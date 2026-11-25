#include <arisa/provider/horizon/horizon_audio.hpp>

#include <arisa/provider/audio/pcm_sound.hpp>
#include <arisa/provider/audio/voice_slots.hpp>
#include <arisa/provider/horizon/service_session.hpp>
#include <arisa/support/unique_handle.hpp>

#include <switch.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <optional>
#include <span>
#include <stop_token>
#include <utility>
#include <vector>

namespace arisa::provider::horizon {

class HorizonAudio::Renderer {
 public:
  static constexpr std::size_t voice_count = 4;
  static constexpr int app_voice = static_cast<int>(voice_count);
  static constexpr std::size_t page_size = AUDREN_MEMPOOL_ALIGNMENT;
  static constexpr std::size_t clip_alignment = AUDREN_BUFFER_ALIGNMENT;
  static constexpr int output_channels = 2;

  struct AudrenService {
    static constexpr AudioRendererConfig config{
        .output_rate = AudioRendererOutputRate_48kHz,
        .num_voices = static_cast<int>(voice_count) + 1,
        .num_effects = 0,
        .num_sinks = 1,
        .num_mix_objs = 1,
        .num_mix_buffers = output_channels};

    static auto open() noexcept -> Result { return audrenInitialize(&config); }

    static void close() noexcept { audrenExit(); }
  };

  using Sounds = SoundBank::Sounds;

  explicit Renderer(ServiceSession<AudrenService> session) noexcept
      : session_(std::move(session)) {}

  Renderer(const Renderer&) = delete;
  auto operator=(const Renderer&) -> Renderer& = delete;
  Renderer(Renderer&&) = delete;
  auto operator=(Renderer&&) -> Renderer& = delete;
  ~Renderer() = default;

  [[nodiscard]] static auto create(const Sounds& sounds)
      -> std::expected<std::unique_ptr<Renderer>, ProviderError> {
    auto session = ServiceSession<AudrenService>::create();
    if (!session.has_value()) {
      return std::unexpected(session.error());
    }
    auto renderer = std::make_unique<Renderer>(std::move(*session));
    const std::size_t pool_bytes = renderer->load(sounds);
    if (const Result created =
            audrvCreate(&renderer->driver_storage_, &AudrenService::config,
                        output_channels);
        R_FAILED(created)) {
      return std::unexpected(failure(created));
    }
    renderer->driver_ =
        support::UniqueHandle<DriverRelease>{&renderer->driver_storage_};
    AudioDriver* driver = &renderer->driver_storage_;
    if (pool_bytes > 0) {
      const int pool = audrvMemPoolAdd(driver, renderer->pool(), pool_bytes);
      if (pool < 0 || !audrvMemPoolAttach(driver, pool)) {
        return std::unexpected(
            ProviderError{ProviderErrorCode::platform_failure});
      }
    }
    static constexpr std::array<u8, output_channels> sink_channels{0, 1};
    if (audrvDeviceSinkAdd(driver, AUDREN_DEFAULT_DEVICE_NAME, output_channels,
                           sink_channels.data()) < 0) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::platform_failure});
    }
    if (const Result updated = audrvUpdate(driver); R_FAILED(updated)) {
      return std::unexpected(failure(updated));
    }
    if (const Result started = audrenStartAudioRenderer(); R_FAILED(started)) {
      return std::unexpected(failure(started));
    }
    return renderer;
  }

  [[nodiscard]] auto play(SoundRole role)
      -> std::expected<void, ProviderError> {
    const std::optional<Clip>& clip = clips_[std::to_underlying(role)];
    const float gain = ui_gain_.load(std::memory_order_acquire);
    if (!clip.has_value() || gain <= 0.0F) {
      return {};
    }
    const std::scoped_lock lock{mutex_};
    AudioDriver* driver = driver_.get();
    static_cast<void>(audrvUpdate(driver));
    sweep(driver);
    const std::size_t voice = free_voice();
    const int id = static_cast<int>(voice);
    audrvVoiceStop(driver, id);
    if (!audrvVoiceInit(driver, id, clip->channels, PcmFormat_Int16,
                        static_cast<int>(clip->sample_rate))) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::platform_failure});
    }
    audrvVoiceSetDestinationMix(driver, id, AUDREN_FINAL_MIX_ID);
    if (clip->channels == 1) {
      audrvVoiceSetMixFactor(driver, id, 1.0F, 0, 0);
      audrvVoiceSetMixFactor(driver, id, 1.0F, 0, 1);
    } else {
      audrvVoiceSetMixFactor(driver, id, 1.0F, 0, 0);
      audrvVoiceSetMixFactor(driver, id, 0.0F, 0, 1);
      audrvVoiceSetMixFactor(driver, id, 0.0F, 1, 0);
      audrvVoiceSetMixFactor(driver, id, 1.0F, 1, 1);
    }
    audrvVoiceSetVolume(driver, id, gain);
    AudioDriverWaveBuf& wave = waves_[voice];
    wave = AudioDriverWaveBuf{};
    wave.data_raw = pool() + clip->offset;
    wave.size = clip->bytes;
    wave.start_sample_offset = 0;
    wave.end_sample_offset = static_cast<s32>(clip->frames);
    if (!audrvVoiceAddWaveBuf(driver, id, &wave)) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::platform_failure});
    }
    audrvVoiceStart(driver, id);
    if (const Result updated = audrvUpdate(driver); R_FAILED(updated)) {
      return std::unexpected(failure(updated));
    }
    return {};
  }

  struct alignas(page_size) Page {
    std::array<std::byte, page_size> bytes;
  };

  template <class Use>
  auto locked(Use&& use) -> decltype(auto) {
    const std::scoped_lock lock{mutex_};
    sweep(driver_.get());
    return std::forward<Use>(use)(driver_.get());
  }

  void retire(int pool, std::vector<Page> pages) {
    const std::scoped_lock lock{mutex_};
    retired_.push_back(Retired{.pool = pool, .pages = std::move(pages)});
    sweep(driver_.get());
  }

  auto claim_voice() noexcept -> bool {
    return !voice_open_.exchange(true, std::memory_order_acq_rel);
  }

  void release_voice() noexcept {
    ui_gain_.store(1.0F, std::memory_order_release);
    voice_open_.store(false, std::memory_order_release);
  }

  void set_ui_gain(float gain) noexcept {
    ui_gain_.store(gain, std::memory_order_release);
  }

  static auto failure(Result result) noexcept -> ProviderError {
    return ProviderError{ProviderErrorCode::platform_failure, result};
  }

  static auto aligned(std::size_t size, std::size_t alignment) noexcept
      -> std::size_t {
    return (size + alignment - 1) / alignment * alignment;
  }

 private:
  struct Clip {
    std::size_t offset;
    std::size_t bytes;
    std::size_t frames;
    std::uint16_t channels;
    std::uint32_t sample_rate;
  };

  struct Retired {
    int pool;
    std::vector<Page> pages;
  };

  void sweep(AudioDriver* driver) noexcept {
    std::erase_if(retired_, [driver](const Retired& each) {
      return audrvMemPoolRemove(driver, each.pool);
    });
  }

  struct DriverRelease {
    using handle_type = AudioDriver*;

    static auto empty() noexcept -> handle_type { return nullptr; }

    static auto is_valid(handle_type driver) noexcept -> bool {
      return driver != nullptr;
    }

    static void release(handle_type& driver) noexcept { audrvClose(driver); }
  };

  auto load(const Sounds& sounds) -> std::size_t {
    std::size_t total = 0;
    for (std::size_t index = 0; index < sounds.size(); ++index) {
      if (const std::optional<PcmSound>& sound = sounds[index]) {
        const std::size_t bytes = sound->samples().size_bytes();
        clips_[index] = Clip{.offset = total,
                             .bytes = bytes,
                             .frames = sound->frames(),
                             .channels = sound->channels(),
                             .sample_rate = sound->sample_rate()};
        total = aligned(total + bytes, clip_alignment);
      }
    }
    const std::size_t pool_bytes = aligned(total, page_size);
    pages_.resize(pool_bytes / page_size);
    for (std::size_t index = 0; index < sounds.size(); ++index) {
      if (sounds[index].has_value() && clips_[index].has_value()) {
        std::memcpy(pool() + clips_[index]->offset,
                    sounds[index]->samples().data(), clips_[index]->bytes);
      }
    }
    if (pool_bytes > 0) {
      armDCacheFlush(pool(), pool_bytes);
    }
    return pool_bytes;
  }

  auto pool() noexcept -> std::byte* {
    return pages_.empty() ? nullptr : pages_.front().bytes.data();
  }

  auto free_voice() noexcept -> std::size_t {
    for (std::size_t voice = 0; voice < waves_.size(); ++voice) {
      const AudioDriverWaveBufState state = waves_[voice].state;
      if (state == AudioDriverWaveBufState_Free ||
          state == AudioDriverWaveBufState_Done) {
        return voice;
      }
    }
    const std::size_t stolen = next_steal_;
    next_steal_ = (next_steal_ + 1) % waves_.size();
    return stolen;
  }

  std::vector<Page> pages_;
  std::vector<Retired> retired_;
  ServiceSession<AudrenService> session_;
  AudioDriver driver_storage_{};
  support::UniqueHandle<DriverRelease> driver_;
  std::array<std::optional<Clip>, all_sound_roles.size()> clips_{};
  std::array<AudioDriverWaveBuf, voice_count> waves_{};
  std::size_t next_steal_{0};
  std::mutex mutex_;
  std::atomic<float> ui_gain_{1.0F};
  std::atomic<bool> voice_open_{false};
};

class HorizonAudio::Voice final : public AppVoice::Implementation {
 public:
  static constexpr std::size_t frame_bytes =
      AppVoice::channels * sizeof(std::int16_t);

  Voice(std::shared_ptr<Renderer> renderer, AppVoiceOptions options)
      : renderer_(std::move(renderer)),
        frames_(options.buffer_frames.value()),
        stride_(
            Renderer::aligned(frames_ * frame_bytes, Renderer::clip_alignment)),
        pages_(Renderer::aligned(stride_ * options.buffer_count.value(),
                                 Renderer::page_size) /
               Renderer::page_size),
        waves_(options.buffer_count.value()),
        slots_(options.buffer_count.value()) {
    set_ui_sounds(options.ui_sounds);
  }

  Voice(const Voice&) = delete;
  auto operator=(const Voice&) -> Voice& = delete;
  Voice(Voice&&) = delete;
  auto operator=(Voice&&) -> Voice& = delete;

  ~Voice() override {
    thread_.reset();
    slots_.close();
    if (pool_.has_value()) {
      const bool removed = renderer_->locked([this](AudioDriver* driver) {
        audrvVoiceStop(driver, Renderer::app_voice);
        static_cast<void>(audrvUpdate(driver));
        static_cast<void>(audrvMemPoolDetach(driver, *pool_));
        static_cast<void>(audrvUpdate(driver));
        return audrvMemPoolRemove(driver, *pool_);
      });
      if (!removed) {
        renderer_->retire(*pool_, std::move(pages_));
      }
    }
    renderer_->release_voice();
  }

  [[nodiscard]] auto start(ThreadingProvider& threading)
      -> std::expected<void, ProviderError> {
    auto opened = renderer_->locked(
        [this](AudioDriver* driver) -> std::expected<void, ProviderError> {
          const int pool = audrvMemPoolAdd(driver, base(),
                                           pages_.size() * Renderer::page_size);
          if (pool < 0) {
            return std::unexpected(
                ProviderError{ProviderErrorCode::resource_exhausted});
          }
          pool_ = pool;
          if (!audrvMemPoolAttach(driver, pool) ||
              !audrvVoiceInit(driver, Renderer::app_voice,
                              static_cast<int>(AppVoice::channels),
                              PcmFormat_Int16,
                              static_cast<int>(AppVoice::sample_rate))) {
            return std::unexpected(
                ProviderError{ProviderErrorCode::platform_failure});
          }
          audrvVoiceSetDestinationMix(driver, Renderer::app_voice,
                                      AUDREN_FINAL_MIX_ID);
          audrvVoiceSetMixFactor(driver, Renderer::app_voice, 1.0F, 0, 0);
          audrvVoiceSetMixFactor(driver, Renderer::app_voice, 0.0F, 0, 1);
          audrvVoiceSetMixFactor(driver, Renderer::app_voice, 0.0F, 1, 0);
          audrvVoiceSetMixFactor(driver, Renderer::app_voice, 1.0F, 1, 1);
          audrvVoiceSetVolume(driver, Renderer::app_voice, 1.0F);
          if (const Result updated = audrvUpdate(driver); R_FAILED(updated)) {
            return std::unexpected(Renderer::failure(updated));
          }
          return {};
        });
    if (!opened.has_value()) {
      return opened;
    }
    auto started = threading.start(
        ThreadSpec{.name = "arisa-app-voice", .role = ThreadRole::app},
        [this](const std::stop_token& stop) { service(stop); });
    if (!started.has_value()) {
      return std::unexpected(started.error());
    }
    thread_.emplace(std::move(*started));
    return {};
  }

  [[nodiscard]] auto take(std::optional<std::chrono::nanoseconds> timeout)
      -> std::optional<std::uint32_t> override {
    return slots_.take(timeout);
  }

  auto samples(std::uint32_t slot) noexcept
      -> std::span<std::int16_t> override {
    return std::span<std::int16_t>{
        reinterpret_cast<std::int16_t*>(base() + (slot * stride_)),
        static_cast<std::size_t>(frames_) * AppVoice::channels};
  }

  [[nodiscard]] auto submit(std::uint32_t slot, std::uint32_t frames)
      -> std::expected<void, ProviderError> override {
    if (slot >= waves_.size()) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument});
    }
    const std::size_t bytes = frames * frame_bytes;
    std::byte* const data = base() + (slot * stride_);
    armDCacheFlush(data, bytes);
    return renderer_->locked(
        [&](AudioDriver* driver) -> std::expected<void, ProviderError> {
          auto queued = slots_.queue(slot, frames, [&]() {
            AudioDriverWaveBuf& wave = waves_[slot];
            wave = AudioDriverWaveBuf{};
            wave.data_raw = data;
            wave.size = bytes;
            wave.start_sample_offset = 0;
            wave.end_sample_offset = static_cast<s32>(frames);
            return audrvVoiceAddWaveBuf(driver, Renderer::app_voice, &wave);
          });
          if (!queued.has_value()) {
            return queued;
          }
          if (!audrvVoiceIsPlaying(driver, Renderer::app_voice)) {
            audrvVoiceStart(driver, Renderer::app_voice);
          }
          if (const Result updated = audrvUpdate(driver); R_FAILED(updated)) {
            return std::unexpected(Renderer::failure(updated));
          }
          return {};
        });
  }

  void give_back(std::uint32_t slot) noexcept override {
    slots_.give_back(slot);
  }

  auto statistics() const -> AppVoiceStatistics override {
    return slots_.statistics();
  }

  void set_ui_sounds(UiSounds sounds) override {
    switch (sounds) {
      case UiSounds::play:
        renderer_->set_ui_gain(1.0F);
        return;
      case UiSounds::duck:
        renderer_->set_ui_gain(AppVoice::ducked_gain);
        return;
      case UiSounds::mute:
        renderer_->set_ui_gain(0.0F);
        return;
    }
  }

 private:
  auto base() noexcept -> std::byte* { return pages_.front().bytes.data(); }

  void service(const std::stop_token& stop) {
    while (!stop.stop_requested()) {
      audrenWaitFrame();
      renderer_->locked([&](AudioDriver* driver) {
        static_cast<void>(audrvUpdate(driver));
        slots_.reap([this](std::uint32_t slot) {
          return waves_[slot].state == AudioDriverWaveBufState_Done;
        });
      });
    }
  }

  std::shared_ptr<Renderer> renderer_;
  std::uint32_t frames_;
  std::size_t stride_;
  std::vector<Renderer::Page> pages_;
  std::vector<AudioDriverWaveBuf> waves_;
  VoiceSlots slots_;
  std::optional<int> pool_;
  std::optional<Thread> thread_;
};

HorizonAudio::HorizonAudio(std::shared_ptr<Renderer> renderer,
                           Threading threading) noexcept
    : renderer_(std::move(renderer)), threading_(threading) {}

HorizonAudio::HorizonAudio(HorizonAudio&& other) noexcept = default;

auto HorizonAudio::operator=(HorizonAudio&& other) noexcept
    -> HorizonAudio& = default;

HorizonAudio::~HorizonAudio() = default;

auto HorizonAudio::create(const AssetProvider& assets,
                          DiagnosticsProvider& diagnostics,
                          const SoundBank& sounds, Threading threading)
    -> std::expected<HorizonAudio, ProviderError> {
  const SoundBank::Sounds loaded =
      sounds.load(assets, diagnostics, "arisa.horizon.audio");
  return Renderer::create(loaded).transform(
      [threading](std::unique_ptr<Renderer> renderer) {
        return HorizonAudio{std::shared_ptr<Renderer>{std::move(renderer)},
                            threading};
      });
}

auto HorizonAudio::open_voice(AppVoiceOptions options)
    -> std::expected<AppVoice, ProviderError> {
  if (!threading_.has_value()) {
    return std::unexpected(ProviderError::unsupported_capability());
  }
  if (!renderer_->claim_voice()) {
    return std::unexpected(ProviderError{ProviderErrorCode::resource_exhausted,
                                         ProviderErrorDetail::voice_open});
  }
  auto voice = std::make_shared<Voice>(renderer_, options);
  if (auto started = voice->start(threading_->get()); !started.has_value()) {
    return std::unexpected(started.error());
  }
  return AppVoice{std::move(voice), options};
}

auto HorizonAudio::play(SoundRole role) -> std::expected<void, ProviderError> {
  return renderer_->play(role);
}

}
