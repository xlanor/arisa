#include <arisa/provider/sdl3/sdl3_haptics.hpp>

#include <arisa/provider/sdl3/sdl3_owned.hpp>

#include <SDL3/SDL_gamepad.h>

#include <algorithm>
#include <cstddef>
#include <mutex>
#include <span>
#include <utility>

namespace arisa::provider::sdl3 {

class Sdl3Haptics::Motor {
 public:
  void play(const Rumble& rumble, std::size_t pad = 0) {
    const std::scoped_lock lock{mutex_};
    if (!refresh(pad)) {
      return;
    }
    static_cast<void>(
        SDL_RumbleGamepad(gamepad_.get(), rumble.low, rumble.high,
                          static_cast<Uint32>(rumble.duration.count())));
  }

 private:
  struct GamepadClose {
    void operator()(SDL_Gamepad* gamepad) const noexcept {
      SDL_CloseGamepad(gamepad);
    }
  };

  auto refresh(std::size_t pad) -> bool {
    int count = 0;
    const Sdl3Owned<SDL_JoystickID> ids{SDL_GetGamepads(&count)};
    if (!ids || count <= 0 || pad >= static_cast<std::size_t>(count)) {
      gamepad_.reset();
      return false;
    }
    const SDL_JoystickID wanted =
        std::span{ids.get(), static_cast<std::size_t>(count)}[pad];
    if (!gamepad_ || SDL_GetGamepadID(gamepad_.get()) != wanted) {
      gamepad_.reset(SDL_OpenGamepad(wanted));
    }
    return static_cast<bool>(gamepad_);
  }

  std::mutex mutex_;
  std::unique_ptr<SDL_Gamepad, GamepadClose> gamepad_;
};

Sdl3Haptics::Sdl3Haptics(Sdl3Context gamepads,
                         std::unique_ptr<Motor> motor) noexcept
    : gamepads_(std::move(gamepads)), motor_(std::move(motor)) {}

Sdl3Haptics::Sdl3Haptics(Sdl3Haptics&& other) noexcept = default;

auto Sdl3Haptics::operator=(Sdl3Haptics&& other) noexcept
    -> Sdl3Haptics& = default;

Sdl3Haptics::~Sdl3Haptics() = default;

auto Sdl3Haptics::create() -> std::expected<Sdl3Haptics, ProviderError> {
  return Sdl3Context::create(SDL_INIT_GAMEPAD)
      .transform([](Sdl3Context gamepads) {
        return Sdl3Haptics{std::move(gamepads), std::make_unique<Motor>()};
      });
}

auto Sdl3Haptics::play(const SensoryFeedback& feedback, HapticStrength strength)
    -> std::expected<void, ProviderError> {
  if (strength.silent()) {
    return {};
  }
  motor_->play(rumble(feedback, strength));
  return {};
}

auto Sdl3Haptics::rumble(RumblePlayer player, RumbleFrequency low_frequency,
                         RumbleFrequency high_frequency,
                         RumbleAmplitude low_amplitude,
                         RumbleAmplitude high_amplitude)
    -> std::expected<void, ProviderError> {
  motor_->play(raw(RumbleValue{low_frequency, high_frequency, low_amplitude,
                               high_amplitude}),
               player.index().value_or(0));
  return {};
}

auto Sdl3Haptics::raw(const RumbleValue& value) noexcept -> Rumble {
  const RumbleValue clamped = value.clamped();
  const auto level = [](RumbleAmplitude amplitude) {
    return static_cast<std::uint16_t>(amplitude.value() * 65535.0F);
  };
  return Rumble{.low = level(clamped.low_amplitude()),
                .high = level(clamped.high_amplitude()),
                .duration = raw_hold};
}

auto Sdl3Haptics::rumble(const SensoryFeedback& feedback,
                         HapticStrength strength) noexcept -> Rumble {
  using std::chrono::milliseconds;
  using Kind = SensoryFeedback::Kind;
  const auto level = [scale = strength.value()](float share) {
    return static_cast<std::uint16_t>(std::min(share * scale, 1.0F) * 65535.0F);
  };
  const auto make = [&level](float low, float high, int duration) {
    return Rumble{.low = level(low),
                  .high = level(high),
                  .duration = milliseconds{duration}};
  };
  switch (feedback.kind()) {
    case Kind::selection:
      return make(0.08F, 0.20F, 12);
    case Kind::impact: {
      const float intensity = feedback.intensity().value();
      Rumble chosen = make(0.24F, 0.30F, 18);
      if (feedback.flexibility() == SensoryFeedback::Flexibility::rigid) {
        chosen = make(0.12F, 0.36F, 12);
      } else if (feedback.flexibility() == SensoryFeedback::Flexibility::soft) {
        chosen = make(0.24F, 0.10F, 40);
      } else if (feedback.weight() == SensoryFeedback::Weight::light) {
        chosen = make(0.16F, 0.22F, 14);
      } else if (feedback.weight() == SensoryFeedback::Weight::heavy) {
        chosen = make(0.36F, 0.38F, 24);
      }
      const auto rescale = [intensity](std::uint16_t scaled) {
        return static_cast<std::uint16_t>(static_cast<float>(scaled) *
                                          intensity);
      };
      chosen.low = rescale(chosen.low);
      chosen.high = rescale(chosen.high);
      return chosen;
    }
    case Kind::increase:
      return make(0.06F, 0.24F, 20);
    case Kind::decrease:
      return make(0.10F, 0.18F, 20);
    case Kind::success:
      return make(0.30F, 0.36F, 110);
    case Kind::warning:
      return make(0.30F, 0.22F, 120);
    case Kind::error:
      return make(0.40F, 0.16F, 150);
    case Kind::start:
      return make(0.18F, 0.26F, 50);
    case Kind::stop:
      return make(0.20F, 0.20F, 50);
    case Kind::alignment:
      return make(0.0F, 0.28F, 8);
    case Kind::level_change:
      return make(0.18F, 0.24F, 70);
    case Kind::path_complete:
      return make(0.22F, 0.32F, 130);
    case Kind::press:
      return feedback.press_feedback() == SensoryFeedback::PressFeedback::toggle
                 ? make(0.14F, 0.26F, 14)
                 : make(0.08F, 0.22F, 10);
    case Kind::release:
      return make(0.04F, 0.14F, 8);
  }
  return Rumble{.low = 0, .high = 0, .duration = milliseconds{0}};
}

}
