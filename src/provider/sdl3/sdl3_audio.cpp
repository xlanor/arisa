#include <arisa/provider/sdl3/sdl3_audio.hpp>

#include <arisa/provider/audio/pcm_sound.hpp>
#include <arisa/provider/audio/voice_slots.hpp>
#include <arisa/support/unique_handle.hpp>

#include <SDL3/SDL_audio.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>
#include <utility>
#include <vector>

namespace arisa::provider::sdl3 {

class Sdl3Audio::Mixer {
 public:
  static constexpr std::size_t voice_count = 4;

  explicit Mixer(SoundBank::Sounds sounds) noexcept
      : sounds_(std::move(sounds)) {}

  Mixer(const Mixer&) = delete;
  auto operator=(const Mixer&) -> Mixer& = delete;
  Mixer(Mixer&&) = delete;
  auto operator=(Mixer&&) -> Mixer& = delete;
  ~Mixer() = default;

  [[nodiscard]] static auto create(SoundBank::Sounds sounds)
      -> std::expected<std::unique_ptr<Mixer>, ProviderError> {
    auto mixer = std::make_unique<Mixer>(std::move(sounds));
    mixer->device_ = support::UniqueHandle<DeviceRelease>{
        SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr)};
    if (!mixer->device_) {
      return std::unexpected(failure());
    }
    static constexpr SDL_AudioSpec initial{
        .format = SDL_AUDIO_S16, .channels = 1, .freq = 48000};
    for (Stream& voice : mixer->voices_) {
      voice.reset(SDL_CreateAudioStream(&initial, &initial));
      if (!voice || !SDL_BindAudioStream(mixer->device_.get(), voice.get())) {
        return std::unexpected(failure());
      }
    }
    return mixer;
  }

  [[nodiscard]] auto play(SoundRole role)
      -> std::expected<void, ProviderError> {
    const std::optional<PcmSound>& sound = sounds_[std::to_underlying(role)];
    const float gain = ui_gain_.load(std::memory_order_acquire);
    if (!sound.has_value() || gain <= 0.0F) {
      return {};
    }
    const std::scoped_lock lock{mutex_};
    SDL_AudioStream* voice = free_voice();
    static_cast<void>(SDL_SetAudioStreamGain(voice, gain));
    const SDL_AudioSpec spec{.format = SDL_AUDIO_S16,
                             .channels = sound->channels(),
                             .freq = static_cast<int>(sound->sample_rate())};
    const auto samples = sound->samples();
    if (!SDL_ClearAudioStream(voice) ||
        !SDL_SetAudioStreamFormat(voice, &spec, nullptr) ||
        !SDL_PutAudioStreamData(voice, samples.data(),
                                static_cast<int>(samples.size_bytes())) ||
        !SDL_FlushAudioStream(voice)) {
      return std::unexpected(failure());
    }
    return {};
  }

  auto device() const noexcept -> SDL_AudioDeviceID { return device_.get(); }

  void set_ui_gain(float gain) noexcept {
    ui_gain_.store(gain, std::memory_order_release);
  }

  auto ui_gain() const noexcept -> float {
    return ui_gain_.load(std::memory_order_acquire);
  }

  auto open_voice() noexcept -> bool {
    return !voice_open_.exchange(true, std::memory_order_acq_rel);
  }

  void close_voice() noexcept {
    ui_gain_.store(1.0F, std::memory_order_release);
    voice_open_.store(false, std::memory_order_release);
  }

  struct StreamDestroy {
    void operator()(SDL_AudioStream* stream) const noexcept {
      SDL_DestroyAudioStream(stream);
    }
  };

  using Stream = std::unique_ptr<SDL_AudioStream, StreamDestroy>;

 private:
  struct DeviceRelease {
    using handle_type = SDL_AudioDeviceID;

    static auto empty() noexcept -> handle_type { return 0; }

    static auto is_valid(handle_type device) noexcept -> bool {
      return device != 0;
    }

    static void release(handle_type& device) noexcept {
      SDL_CloseAudioDevice(device);
    }
  };

  static auto failure() noexcept -> ProviderError {
    return ProviderError{ProviderErrorCode::platform_failure};
  }

  auto free_voice() noexcept -> SDL_AudioStream* {
    for (const Stream& voice : voices_) {
      if (SDL_GetAudioStreamQueued(voice.get()) == 0) {
        return voice.get();
      }
    }
    SDL_AudioStream* stolen = voices_[next_steal_].get();
    next_steal_ = (next_steal_ + 1) % voices_.size();
    return stolen;
  }

  SoundBank::Sounds sounds_;
  support::UniqueHandle<DeviceRelease> device_;
  std::array<Stream, voice_count> voices_{};
  std::size_t next_steal_{0};
  std::mutex mutex_;
  std::atomic<float> ui_gain_{1.0F};
  std::atomic<bool> voice_open_{false};
};

class Sdl3Audio::Voice final : public AppVoice::Implementation {
 public:
  static constexpr std::chrono::milliseconds poll_interval{2};
  static constexpr std::size_t frame_bytes =
      AppVoice::channels * sizeof(std::int16_t);

  Voice(std::shared_ptr<Mixer> mixer, Mixer::Stream stream,
        AppVoiceOptions options)
      : mixer_(std::move(mixer)),
        stream_(std::move(stream)),
        frames_(options.buffer_frames.value()),
        storage_(static_cast<std::size_t>(frames_) * AppVoice::channels *
                 options.buffer_count.value()),
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
    stream_.reset();
    mixer_->close_voice();
  }

  [[nodiscard]] auto start(ThreadingProvider& threading)
      -> std::expected<void, ProviderError> {
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
    const std::size_t width =
        static_cast<std::size_t>(frames_) * AppVoice::channels;
    return std::span<std::int16_t>{storage_}.subspan(slot * width, width);
  }

  [[nodiscard]] auto submit(std::uint32_t slot, std::uint32_t frames)
      -> std::expected<void, ProviderError> override {
    const std::size_t bytes = frames * frame_bytes;
    return slots_.queue(slot, frames, [&]() {
      if (!SDL_PutAudioStreamData(stream_.get(), samples(slot).data(),
                                  static_cast<int>(bytes)) ||
          !SDL_FlushAudioStream(stream_.get())) {
        return false;
      }
      put_.fetch_add(bytes, std::memory_order_acq_rel);
      return true;
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
        mixer_->set_ui_gain(1.0F);
        return;
      case UiSounds::duck:
        mixer_->set_ui_gain(AppVoice::ducked_gain);
        return;
      case UiSounds::mute:
        mixer_->set_ui_gain(0.0F);
        return;
    }
  }

 private:
  void service(const std::stop_token& stop) {
    while (!stop.stop_requested()) {
      retire();
      std::this_thread::sleep_for(poll_interval);
    }
  }

  void retire() {
    const std::uint64_t put = put_.load(std::memory_order_acquire);
    const int queued = SDL_GetAudioStreamQueued(stream_.get());
    if (queued < 0) {
      return;
    }
    const std::uint64_t waiting = static_cast<std::uint64_t>(queued);
    const std::uint64_t consumed = put > waiting ? put - waiting : 0;
    for (const VoiceSlots::Queued& each : slots_.queued()) {
      const std::uint64_t bytes = each.frames * frame_bytes;
      if (retired_ + bytes > consumed) {
        return;
      }
      retired_ += bytes;
      slots_.played(each.slot);
    }
  }

  std::shared_ptr<Mixer> mixer_;
  Mixer::Stream stream_;
  std::uint32_t frames_;
  std::vector<std::int16_t> storage_;
  VoiceSlots slots_;
  std::atomic<std::uint64_t> put_{0};
  std::uint64_t retired_{0};
  std::optional<Thread> thread_;
};

Sdl3Audio::Sdl3Audio(Sdl3Context audio, std::shared_ptr<Mixer> mixer,
                     Threading threading) noexcept
    : audio_(std::move(audio)),
      mixer_(std::move(mixer)),
      threading_(threading) {}

Sdl3Audio::Sdl3Audio(Sdl3Audio&& other) noexcept = default;

auto Sdl3Audio::operator=(Sdl3Audio&& other) noexcept -> Sdl3Audio& = default;

Sdl3Audio::~Sdl3Audio() = default;

auto Sdl3Audio::create(const AssetProvider& assets,
                       DiagnosticsProvider& diagnostics,
                       const SoundBank& sounds, Threading threading)
    -> std::expected<Sdl3Audio, ProviderError> {
  auto audio = Sdl3Context::create(SDL_INIT_AUDIO);
  if (!audio.has_value()) {
    return std::unexpected(audio.error());
  }
  return Mixer::create(sounds.load(assets, diagnostics, "arisa.sdl3.audio"))
      .transform([&audio, threading](std::unique_ptr<Mixer> mixer) {
        return Sdl3Audio{std::move(*audio),
                         std::shared_ptr<Mixer>{std::move(mixer)}, threading};
      });
}

auto Sdl3Audio::open_voice(AppVoiceOptions options)
    -> std::expected<AppVoice, ProviderError> {
  if (!threading_.has_value()) {
    return std::unexpected(ProviderError::unsupported_capability());
  }
  if (!mixer_->open_voice()) {
    return std::unexpected(ProviderError{ProviderErrorCode::resource_exhausted,
                                         ProviderErrorDetail::voice_open});
  }
  static constexpr SDL_AudioSpec spec{.format = SDL_AUDIO_S16,
                                      .channels = AppVoice::channels,
                                      .freq = AppVoice::sample_rate};
  Mixer::Stream stream{SDL_CreateAudioStream(&spec, &spec)};
  if (!stream || !SDL_BindAudioStream(mixer_->device(), stream.get())) {
    mixer_->close_voice();
    return std::unexpected(ProviderError{ProviderErrorCode::platform_failure});
  }
  auto voice = std::make_shared<Voice>(mixer_, std::move(stream), options);
  if (auto started = voice->start(threading_->get()); !started.has_value()) {
    return std::unexpected(started.error());
  }
  return AppVoice{std::move(voice), options};
}

auto Sdl3Audio::ui_gain() const noexcept -> float { return mixer_->ui_gain(); }

auto Sdl3Audio::play(SoundRole role) -> std::expected<void, ProviderError> {
  return mixer_->play(role);
}

}
