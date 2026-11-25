#pragma once

#include <arisa/provider/input/provider.hpp>
#include <arisa/provider/sdl3/sdl3_context.hpp>
#include <arisa/provider/sdl3/sdl3_event_pump.hpp>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_touch.h>

#include <expected>
#include <flat_map>
#include <functional>
#include <memory>
#include <optional>

namespace arisa::provider::sdl3 {

class Sdl3Input final : public InputProvider {
 public:
  [[nodiscard]] static auto create(Sdl3EventPump& pump, InputTuning tuning = {})
      -> std::expected<Sdl3Input, ProviderError>;

  Sdl3Input(const Sdl3Input&) = delete;
  auto operator=(const Sdl3Input&) -> Sdl3Input& = delete;
  Sdl3Input(Sdl3Input&& other) noexcept;
  auto operator=(Sdl3Input&& other) noexcept -> Sdl3Input&;
  ~Sdl3Input() override;

  auto sample() -> InputFrame override;

  auto tuning() const noexcept -> InputTuning override;

  void set_tuning(InputTuning tuning) noexcept override;

  auto pointer_host() const noexcept -> PointerHost override;

  void present_pointer(PointerAppearance appearance) override;

  static auto system_cursor(PointerShape shape) noexcept -> SDL_SystemCursor;

  static auto translate_key(SDL_Keycode key) noexcept -> std::optional<Key>;

  static auto translate_button(SDL_GamepadButton button) noexcept
      -> std::optional<Button>;

  static auto normalise_axis(Sint16 value) noexcept -> float;

 private:
  class Gamepads;
  class Cursors;

  Sdl3Input(Sdl3Context gamepad_subsystem, Sdl3EventPump& pump,
            InputTuning tuning, std::unique_ptr<Gamepads> gamepads,
            std::unique_ptr<Cursors> cursors);

  void handle(const SDL_Event& event, InputFrame& frame);
  void handle_finger(const SDL_TouchFingerEvent& finger, InputFrame& frame);

  static void handle_pinch(const SDL_PinchFingerEvent& pinch,
                           InputFrame& frame);
  void handle_mouse(const SDL_Event& event);

  static auto pixel_density(SDL_WindowID window) noexcept -> float;
  static auto window_pixels(SDL_WindowID window) noexcept -> PixelPoint;

  Sdl3Context gamepad_subsystem_;
  std::reference_wrapper<Sdl3EventPump> pump_;
  Sdl3EventPump::Subscription input_events_;
  Sdl3EventPump::Subscription text_events_;
  InputTuning tuning_;
  std::unique_ptr<Gamepads> gamepads_;
  std::unique_ptr<Cursors> cursors_;
  std::flat_map<SDL_FingerID, PixelPoint> fingers_;
  std::optional<PointerState> pointer_;
};

}
