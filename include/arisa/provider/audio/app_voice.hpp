#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/support/bounded_value.hpp>
#include <arisa/support/strong_value.hpp>

#include <chrono>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>

namespace arisa::provider {

struct AudioFramesTag;
struct AudioBufferCountTag;

using AudioFrames =
    support::BoundedValue<std::uint32_t, 64, 48000, AudioFramesTag>;
using AudioBufferCount =
    support::BoundedValue<std::uint32_t, 2, 16, AudioBufferCountTag>;

enum class UiSounds : std::uint8_t {
  play,
  duck,
  mute,
};

struct AppVoiceOptions {
  AudioFrames buffer_frames{AudioFrames::of<960>()};
  AudioBufferCount buffer_count{AudioBufferCount::of<4>()};
  UiSounds ui_sounds{UiSounds::duck};

  friend constexpr auto operator==(const AppVoiceOptions&,
                                   const AppVoiceOptions&) -> bool = default;
};

struct AppVoiceStatistics {
  std::uint64_t submitted{0};
  std::uint64_t played{0};
  std::uint64_t underruns{0};
  std::uint32_t queued{0};
  std::chrono::nanoseconds latency{0};

  friend constexpr auto operator==(const AppVoiceStatistics&,
                                   const AppVoiceStatistics&) -> bool = default;
};

class AppVoice {
 public:
  static constexpr std::uint32_t sample_rate = 48000;
  static constexpr std::uint32_t channels = 2;
  static constexpr float ducked_gain = 0.3F;

  class Implementation {
   public:
    Implementation() = default;
    Implementation(const Implementation&) = delete;
    auto operator=(const Implementation&) -> Implementation& = delete;
    Implementation(Implementation&&) = delete;
    auto operator=(Implementation&&) -> Implementation& = delete;
    virtual ~Implementation() = default;

    [[nodiscard]] virtual auto take(
        std::optional<std::chrono::nanoseconds> timeout)
        -> std::optional<std::uint32_t> = 0;
    virtual auto samples(std::uint32_t slot) noexcept
        -> std::span<std::int16_t> = 0;
    [[nodiscard]] virtual auto submit(std::uint32_t slot, std::uint32_t frames)
        -> std::expected<void, ProviderError> = 0;
    virtual void give_back(std::uint32_t slot) noexcept = 0;
    virtual auto statistics() const -> AppVoiceStatistics = 0;
    virtual void set_ui_sounds(UiSounds sounds) = 0;
  };

  class Buffer {
   public:
    Buffer(Buffer&& other) noexcept;
    auto operator=(Buffer&& other) noexcept -> Buffer&;
    Buffer(const Buffer&) = delete;
    auto operator=(const Buffer&) -> Buffer& = delete;
    ~Buffer();

    auto samples() const noexcept -> std::span<std::int16_t> {
      return samples_;
    }

    auto frames() const noexcept -> std::uint32_t {
      return static_cast<std::uint32_t>(samples_.size() / channels);
    }

    [[nodiscard]] auto submit() -> std::expected<void, ProviderError>;

    [[nodiscard]] auto submit(std::uint32_t frames)
        -> std::expected<void, ProviderError>;

   private:
    friend class AppVoice;

    Buffer(std::shared_ptr<Implementation> voice, std::uint32_t slot,
           std::span<std::int16_t> samples) noexcept;

    std::shared_ptr<Implementation> voice_;
    std::uint32_t slot_{0};
    std::span<std::int16_t> samples_;
  };

  AppVoice(std::shared_ptr<Implementation> implementation,
           AppVoiceOptions options) noexcept;

  [[nodiscard]] auto acquire() -> std::optional<Buffer>;

  [[nodiscard]] auto acquire(std::chrono::nanoseconds timeout)
      -> std::optional<Buffer>;

  auto statistics() const -> AppVoiceStatistics;

  void set_ui_sounds(UiSounds sounds);

  auto options() const noexcept -> const AppVoiceOptions& { return options_; }

 private:
  auto wrap(std::optional<std::uint32_t> slot) -> std::optional<Buffer>;

  std::shared_ptr<Implementation> implementation_;
  AppVoiceOptions options_;
};

}
