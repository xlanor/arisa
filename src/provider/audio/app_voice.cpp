#include <arisa/provider/audio/app_voice.hpp>

#include <utility>

namespace arisa::provider {

AppVoice::Buffer::Buffer(std::shared_ptr<Implementation> voice,
                         std::uint32_t slot,
                         std::span<std::int16_t> samples) noexcept
    : voice_(std::move(voice)), slot_(slot), samples_(samples) {}

AppVoice::Buffer::Buffer(Buffer&& other) noexcept
    : voice_(std::move(other.voice_)),
      slot_(other.slot_),
      samples_(std::exchange(other.samples_, {})) {}

auto AppVoice::Buffer::operator=(Buffer&& other) noexcept -> Buffer& {
  if (this != &other) {
    if (voice_ != nullptr) {
      voice_->give_back(slot_);
    }
    voice_ = std::move(other.voice_);
    slot_ = other.slot_;
    samples_ = std::exchange(other.samples_, {});
  }
  return *this;
}

AppVoice::Buffer::~Buffer() {
  if (voice_ != nullptr) {
    voice_->give_back(slot_);
  }
}

auto AppVoice::Buffer::submit() -> std::expected<void, ProviderError> {
  return submit(frames());
}

auto AppVoice::Buffer::submit(std::uint32_t frames)
    -> std::expected<void, ProviderError> {
  if (voice_ == nullptr || frames == 0 || frames > this->frames()) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument});
  }
  const std::shared_ptr<Implementation> voice = std::exchange(voice_, nullptr);
  samples_ = {};
  return voice->submit(slot_, frames);
}

AppVoice::AppVoice(std::shared_ptr<Implementation> implementation,
                   AppVoiceOptions options) noexcept
    : implementation_(std::move(implementation)), options_(options) {}

auto AppVoice::wrap(std::optional<std::uint32_t> slot)
    -> std::optional<Buffer> {
  if (!slot.has_value()) {
    return std::nullopt;
  }
  return Buffer{implementation_, *slot, implementation_->samples(*slot)};
}

auto AppVoice::acquire() -> std::optional<Buffer> {
  return wrap(implementation_->take(std::nullopt));
}

auto AppVoice::acquire(std::chrono::nanoseconds timeout)
    -> std::optional<Buffer> {
  return wrap(implementation_->take(timeout));
}

auto AppVoice::statistics() const -> AppVoiceStatistics {
  return implementation_->statistics();
}

void AppVoice::set_ui_sounds(UiSounds sounds) {
  implementation_->set_ui_sounds(sounds);
}

}
