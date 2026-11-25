#include <arisa/provider/sdl3/sdl3_input.hpp>
#include <arisa/provider/sdl3/sdl3_owned.hpp>
#include <arisa/support/unique_handle.hpp>

#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_video.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <utility>

namespace arisa::provider::sdl3 {

class Sdl3Input::Gamepads {
 public:
  Gamepads() {
    int count = 0;
    const Sdl3Owned<SDL_JoystickID> ids{SDL_GetGamepads(&count)};
    if (ids != nullptr && count > 0) {
      for (const SDL_JoystickID id :
           std::span{ids.get(), static_cast<std::size_t>(count)}) {
        open(id);
      }
    }
  }

  void open(SDL_JoystickID id) {
    if (std::ranges::any_of(slots_, [id](const Slot& slot) {
          return slot.handle.valid() && slot.id == id;
        })) {
      return;
    }
    const auto free = std::ranges::find_if(
        slots_, [](const Slot& slot) { return !slot.handle.valid(); });
    if (free == slots_.end()) {
      return;
    }
    Handle handle{SDL_OpenGamepad(id)};
    if (handle.valid()) {
      free->id = id;
      free->handle = std::move(handle);
    }
  }

  void close(SDL_JoystickID id) {
    for (Slot& slot : slots_) {
      if (slot.handle.valid() && slot.id == id) {
        slot.handle.reset();
      }
    }
  }

  auto state(const InputTuning& tuning) const
      -> std::array<PadState, max_pads> {
    std::array<PadState, max_pads> pads{};
    for (std::size_t index = 0; index < max_pads; ++index) {
      if (slots_[index].handle.valid()) {
        pads[index] = read(slots_[index].handle.get(), tuning);
      }
    }
    return pads;
  }

 private:
  struct Close {
    using handle_type = SDL_Gamepad*;

    static auto empty() noexcept -> handle_type { return nullptr; }

    static auto is_valid(handle_type gamepad) noexcept -> bool {
      return gamepad != nullptr;
    }

    static void release(handle_type& gamepad) noexcept {
      SDL_CloseGamepad(gamepad);
    }
  };

  using Handle = support::UniqueHandle<Close>;

  struct Slot {
    SDL_JoystickID id{0};
    Handle handle;
  };

  static auto read(SDL_Gamepad* gamepad, const InputTuning& tuning)
      -> PadState {
    static constexpr std::array buttons{
        SDL_GAMEPAD_BUTTON_SOUTH,         SDL_GAMEPAD_BUTTON_EAST,
        SDL_GAMEPAD_BUTTON_WEST,          SDL_GAMEPAD_BUTTON_NORTH,
        SDL_GAMEPAD_BUTTON_BACK,          SDL_GAMEPAD_BUTTON_START,
        SDL_GAMEPAD_BUTTON_LEFT_STICK,    SDL_GAMEPAD_BUTTON_RIGHT_STICK,
        SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
        SDL_GAMEPAD_BUTTON_DPAD_UP,       SDL_GAMEPAD_BUTTON_DPAD_DOWN,
        SDL_GAMEPAD_BUTTON_DPAD_LEFT,     SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
    };
    static constexpr float trigger_threshold = 0.5F;
    PadState pad{.connected = true};
    for (const SDL_GamepadButton button : buttons) {
      const std::optional<Button> translated = translate_button(button);
      if (translated.has_value() && SDL_GetGamepadButton(gamepad, button)) {
        pad.buttons = pad.buttons.with(*translated);
      }
    }
    if (normalise_axis(SDL_GetGamepadAxis(
            gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER)) > trigger_threshold) {
      pad.buttons = pad.buttons.with(Button::zl);
    }
    if (normalise_axis(SDL_GetGamepadAxis(
            gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)) > trigger_threshold) {
      pad.buttons = pad.buttons.with(Button::zr);
    }
    pad.left = tuning.apply(Stick{.x = normalise_axis(SDL_GetGamepadAxis(
                                      gamepad, SDL_GAMEPAD_AXIS_LEFTX)),
                                  .y = -normalise_axis(SDL_GetGamepadAxis(
                                      gamepad, SDL_GAMEPAD_AXIS_LEFTY))});
    pad.right = tuning.apply(Stick{.x = normalise_axis(SDL_GetGamepadAxis(
                                       gamepad, SDL_GAMEPAD_AXIS_RIGHTX)),
                                   .y = -normalise_axis(SDL_GetGamepadAxis(
                                       gamepad, SDL_GAMEPAD_AXIS_RIGHTY))});
    return pad;
  }

  std::array<Slot, max_pads> slots_{};
};

class Sdl3Input::Cursors {
 public:
  void present(PointerAppearance appearance) {
    if (shown_ == appearance) {
      return;
    }
    shown_ = appearance;
    if (!appearance.visible) {
      static_cast<void>(SDL_HideCursor());
      return;
    }
    const auto index = static_cast<std::size_t>(appearance.shape);
    if (index < cursors_.size() && appearance.shape != PointerShape::custom) {
      if (!cursors_[index].valid()) {
        cursors_[index] =
            Handle{SDL_CreateSystemCursor(system_cursor(appearance.shape))};
      }
      if (cursors_[index].valid()) {
        static_cast<void>(SDL_SetCursor(cursors_[index].get()));
      }
    }
    static_cast<void>(SDL_ShowCursor());
  }

 private:
  struct Destroy {
    using handle_type = SDL_Cursor*;

    static auto empty() noexcept -> handle_type { return nullptr; }

    static auto is_valid(handle_type cursor) noexcept -> bool {
      return cursor != nullptr;
    }

    static void release(handle_type& cursor) noexcept {
      SDL_DestroyCursor(cursor);
    }
  };

  using Handle = support::UniqueHandle<Destroy>;

  static constexpr std::size_t shape_count =
      static_cast<std::size_t>(PointerShape::custom) + 1;

  std::array<Handle, shape_count> cursors_{};
  std::optional<PointerAppearance> shown_;
};

Sdl3Input::Sdl3Input(Sdl3Context gamepad_subsystem, Sdl3EventPump& pump,
                     InputTuning tuning, std::unique_ptr<Gamepads> gamepads,
                     std::unique_ptr<Cursors> cursors)
    : gamepad_subsystem_(std::move(gamepad_subsystem)),
      pump_(pump),
      input_events_(pump.subscribe(Sdl3EventCategory::input)),
      text_events_(pump.subscribe(Sdl3EventCategory::text)),
      tuning_(tuning),
      gamepads_(std::move(gamepads)),
      cursors_(std::move(cursors)) {}

Sdl3Input::Sdl3Input(Sdl3Input&& other) noexcept = default;
auto Sdl3Input::operator=(Sdl3Input&& other) noexcept -> Sdl3Input& = default;
Sdl3Input::~Sdl3Input() = default;

auto Sdl3Input::create(Sdl3EventPump& pump, InputTuning tuning)
    -> std::expected<Sdl3Input, ProviderError> {
  auto subsystem = Sdl3Context::create(SDL_INIT_GAMEPAD);
  if (!subsystem.has_value()) {
    return std::unexpected(subsystem.error());
  }
  return Sdl3Input{std::move(*subsystem), pump, tuning,
                   std::make_unique<Gamepads>(), std::make_unique<Cursors>()};
}

auto Sdl3Input::sample() -> InputFrame {
  pump_.get().pump();
  InputFrame frame;
  if (pointer_.has_value()) {
    pointer_->wheel_x = 0.0F;
    pointer_->wheel_y = 0.0F;
  }
  while (const std::optional<SDL_Event> event = input_events_.next()) {
    handle(*event, frame);
  }
  while (const std::optional<SDL_Event> event = text_events_.next()) {
    if (event->type == SDL_EVENT_TEXT_INPUT && event->text.text != nullptr) {
      frame.text.append(event->text.text);
    }
  }
  for (const auto& [finger, position] : fingers_) {
    const bool reported = std::ranges::any_of(
        frame.touches,
        [finger](const Touch& touch) { return touch.id.value() == finger; });
    if (!reported) {
      frame.touches.push_back(Touch{.id = TouchId{finger},
                                    .position = position,
                                    .phase = TouchPhase::stationary});
    }
  }
  frame.pads = gamepads_->state(tuning_);
  frame.pointer = pointer_;
  return frame;
}

auto Sdl3Input::tuning() const noexcept -> InputTuning { return tuning_; }

void Sdl3Input::set_tuning(InputTuning tuning) noexcept { tuning_ = tuning; }

auto Sdl3Input::pointer_host() const noexcept -> PointerHost {
  return PointerHost::system;
}

void Sdl3Input::present_pointer(PointerAppearance appearance) {
  cursors_->present(appearance);
}

auto Sdl3Input::system_cursor(PointerShape shape) noexcept -> SDL_SystemCursor {
  switch (shape) {
    case PointerShape::arrow:
    case PointerShape::custom:
      return SDL_SYSTEM_CURSOR_DEFAULT;
    case PointerShape::text:
    case PointerShape::vertical_text:
      return SDL_SYSTEM_CURSOR_TEXT;
    case PointerShape::crosshair:
    case PointerShape::zoom_in:
    case PointerShape::zoom_out:
      return SDL_SYSTEM_CURSOR_CROSSHAIR;
    case PointerShape::pointing_hand:
      return SDL_SYSTEM_CURSOR_POINTER;
    case PointerShape::open_hand:
    case PointerShape::closed_hand:
      return SDL_SYSTEM_CURSOR_MOVE;
    case PointerShape::resize_horizontal:
      return SDL_SYSTEM_CURSOR_EW_RESIZE;
    case PointerShape::resize_vertical:
      return SDL_SYSTEM_CURSOR_NS_RESIZE;
    case PointerShape::resize_leading:
      return SDL_SYSTEM_CURSOR_W_RESIZE;
    case PointerShape::resize_trailing:
      return SDL_SYSTEM_CURSOR_E_RESIZE;
    case PointerShape::resize_top:
      return SDL_SYSTEM_CURSOR_N_RESIZE;
    case PointerShape::resize_bottom:
      return SDL_SYSTEM_CURSOR_S_RESIZE;
    case PointerShape::resize_descending:
      return SDL_SYSTEM_CURSOR_NWSE_RESIZE;
    case PointerShape::resize_ascending:
      return SDL_SYSTEM_CURSOR_NESW_RESIZE;
    case PointerShape::resize_top_leading:
      return SDL_SYSTEM_CURSOR_NW_RESIZE;
    case PointerShape::resize_top_trailing:
      return SDL_SYSTEM_CURSOR_NE_RESIZE;
    case PointerShape::resize_bottom_leading:
      return SDL_SYSTEM_CURSOR_SW_RESIZE;
    case PointerShape::resize_bottom_trailing:
      return SDL_SYSTEM_CURSOR_SE_RESIZE;
  }
  return SDL_SYSTEM_CURSOR_DEFAULT;
}

void Sdl3Input::handle(const SDL_Event& event, InputFrame& frame) {
  switch (static_cast<SDL_EventType>(event.type)) {
    case SDL_EVENT_GAMEPAD_ADDED:
      gamepads_->open(event.gdevice.which);
      break;
    case SDL_EVENT_GAMEPAD_REMOVED:
      gamepads_->close(event.gdevice.which);
      break;
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_MOTION:
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_CANCELED:
      handle_finger(event.tfinger, frame);
      break;
    case SDL_EVENT_MOUSE_MOTION:
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
    case SDL_EVENT_MOUSE_WHEEL:
      handle_mouse(event);
      break;
    case SDL_EVENT_PINCH_BEGIN:
    case SDL_EVENT_PINCH_UPDATE:
    case SDL_EVENT_PINCH_END:
      handle_pinch(event.pinch, frame);
      break;
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
      if (const std::optional<Key> key = translate_key(event.key.key)) {
        frame.keys.push_back(KeyEvent{
            .key = *key, .down = event.key.down, .repeat = event.key.repeat});
      }
      break;
    default:
      break;
  }
}

void Sdl3Input::handle_pinch(const SDL_PinchFingerEvent& pinch,
                             InputFrame& frame) {
  TrackpadPinch next = frame.pinch.value_or(
      TrackpadPinch{.phase = TouchPhase::moved, .scale = 1.0F});
  if (pinch.type == SDL_EVENT_PINCH_BEGIN) {
    next.phase = TouchPhase::began;
  } else if (pinch.type == SDL_EVENT_PINCH_END) {
    if (next.phase == TouchPhase::began) {
      frame.pinch.reset();
      return;
    }
    next.phase = TouchPhase::ended;
  } else {
    next.scale *= pinch.scale;
  }
  frame.pinch = next;
}

void Sdl3Input::handle_finger(const SDL_TouchFingerEvent& finger,
                              InputFrame& frame) {
  if (finger.touchID == SDL_MOUSE_TOUCHID) {
    return;
  }
  const PixelPoint extent = window_pixels(finger.windowID);
  const PixelPoint position{.x = std::clamp(finger.x, 0.0F, 1.0F) * extent.x,
                            .y = std::clamp(finger.y, 0.0F, 1.0F) * extent.y};
  TouchPhase phase = TouchPhase::moved;
  if (finger.type == SDL_EVENT_FINGER_DOWN) {
    phase = TouchPhase::began;
    fingers_.insert_or_assign(finger.fingerID, position);
  } else if (finger.type == SDL_EVENT_FINGER_MOTION) {
    fingers_.insert_or_assign(finger.fingerID, position);
  } else {
    phase = TouchPhase::ended;
    fingers_.erase(finger.fingerID);
  }
  const auto existing =
      std::ranges::find_if(frame.touches, [&finger](const Touch& touch) {
        return touch.id.value() == finger.fingerID;
      });
  const Touch touch{
      .id = TouchId{finger.fingerID}, .position = position, .phase = phase};
  if (existing == frame.touches.end()) {
    frame.touches.push_back(touch);
  } else if (existing->phase != TouchPhase::began ||
             phase == TouchPhase::ended) {
    *existing = touch;
  } else {
    existing->position = position;
  }
}

void Sdl3Input::handle_mouse(const SDL_Event& event) {
  PointerState pointer = pointer_.value_or(PointerState{});
  switch (static_cast<SDL_EventType>(event.type)) {
    case SDL_EVENT_MOUSE_MOTION: {
      if (event.motion.which == SDL_TOUCH_MOUSEID) {
        return;
      }
      const float density = pixel_density(event.motion.windowID);
      pointer.position = PixelPoint{.x = event.motion.x * density,
                                    .y = event.motion.y * density};
      break;
    }
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
      if (event.button.which == SDL_TOUCH_MOUSEID) {
        return;
      }
      const float density = pixel_density(event.button.windowID);
      pointer.position = PixelPoint{.x = event.button.x * density,
                                    .y = event.button.y * density};
      if (event.button.button == SDL_BUTTON_LEFT) {
        pointer.left = event.button.down;
      } else if (event.button.button == SDL_BUTTON_RIGHT) {
        pointer.right = event.button.down;
      } else if (event.button.button == SDL_BUTTON_MIDDLE) {
        pointer.middle = event.button.down;
      }
      break;
    }
    case SDL_EVENT_MOUSE_WHEEL: {
      if (event.wheel.which == SDL_TOUCH_MOUSEID) {
        return;
      }
      const float direction =
          event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1.0F : 1.0F;
      pointer.wheel_x += event.wheel.x * direction;
      pointer.wheel_y += event.wheel.y * direction;
      break;
    }
    default:
      return;
  }
  pointer_ = pointer;
}

auto Sdl3Input::translate_key(SDL_Keycode key) noexcept -> std::optional<Key> {
  static constexpr std::array<std::pair<SDL_Keycode, Key>, 83> mapping{{
      {SDLK_TAB, Key::tab},
      {SDLK_LEFT, Key::left},
      {SDLK_RIGHT, Key::right},
      {SDLK_UP, Key::up},
      {SDLK_DOWN, Key::down},
      {SDLK_PAGEUP, Key::page_up},
      {SDLK_PAGEDOWN, Key::page_down},
      {SDLK_HOME, Key::home},
      {SDLK_END, Key::end},
      {SDLK_INSERT, Key::insert},
      {SDLK_DELETE, Key::delete_forward},
      {SDLK_BACKSPACE, Key::backspace},
      {SDLK_SPACE, Key::space},
      {SDLK_RETURN, Key::enter},
      {SDLK_KP_ENTER, Key::enter},
      {SDLK_ESCAPE, Key::escape},
      {SDLK_A, Key::a},
      {SDLK_B, Key::b},
      {SDLK_C, Key::c},
      {SDLK_D, Key::d},
      {SDLK_E, Key::e},
      {SDLK_F, Key::f},
      {SDLK_G, Key::g},
      {SDLK_H, Key::h},
      {SDLK_I, Key::i},
      {SDLK_J, Key::j},
      {SDLK_K, Key::k},
      {SDLK_L, Key::l},
      {SDLK_M, Key::m},
      {SDLK_N, Key::n},
      {SDLK_O, Key::o},
      {SDLK_P, Key::p},
      {SDLK_Q, Key::q},
      {SDLK_R, Key::r},
      {SDLK_S, Key::s},
      {SDLK_T, Key::t},
      {SDLK_U, Key::u},
      {SDLK_V, Key::v},
      {SDLK_W, Key::w},
      {SDLK_X, Key::x},
      {SDLK_Y, Key::y},
      {SDLK_Z, Key::z},
      {SDLK_0, Key::digit0},
      {SDLK_1, Key::digit1},
      {SDLK_2, Key::digit2},
      {SDLK_3, Key::digit3},
      {SDLK_4, Key::digit4},
      {SDLK_5, Key::digit5},
      {SDLK_6, Key::digit6},
      {SDLK_7, Key::digit7},
      {SDLK_8, Key::digit8},
      {SDLK_9, Key::digit9},
      {SDLK_F1, Key::f1},
      {SDLK_F2, Key::f2},
      {SDLK_F3, Key::f3},
      {SDLK_F4, Key::f4},
      {SDLK_F5, Key::f5},
      {SDLK_F6, Key::f6},
      {SDLK_F7, Key::f7},
      {SDLK_F8, Key::f8},
      {SDLK_F9, Key::f9},
      {SDLK_F10, Key::f10},
      {SDLK_F11, Key::f11},
      {SDLK_F12, Key::f12},
      {SDLK_MINUS, Key::minus},
      {SDLK_EQUALS, Key::equals},
      {SDLK_LEFTBRACKET, Key::left_bracket},
      {SDLK_RIGHTBRACKET, Key::right_bracket},
      {SDLK_BACKSLASH, Key::backslash},
      {SDLK_SEMICOLON, Key::semicolon},
      {SDLK_APOSTROPHE, Key::apostrophe},
      {SDLK_GRAVE, Key::grave},
      {SDLK_COMMA, Key::comma},
      {SDLK_PERIOD, Key::period},
      {SDLK_SLASH, Key::slash},
      {SDLK_LCTRL, Key::left_ctrl},
      {SDLK_RCTRL, Key::right_ctrl},
      {SDLK_LSHIFT, Key::left_shift},
      {SDLK_RSHIFT, Key::right_shift},
      {SDLK_LALT, Key::left_alt},
      {SDLK_RALT, Key::right_alt},
      {SDLK_LGUI, Key::left_super},
      {SDLK_RGUI, Key::right_super},
  }};
  const auto* found = std::ranges::find(
      mapping, key, [](const auto& entry) { return entry.first; });
  if (found == mapping.end()) {
    return std::nullopt;
  }
  return found->second;
}

auto Sdl3Input::translate_button(SDL_GamepadButton button) noexcept
    -> std::optional<Button> {
  switch (button) {
    case SDL_GAMEPAD_BUTTON_EAST:
      return Button::a;
    case SDL_GAMEPAD_BUTTON_SOUTH:
      return Button::b;
    case SDL_GAMEPAD_BUTTON_NORTH:
      return Button::x;
    case SDL_GAMEPAD_BUTTON_WEST:
      return Button::y;
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
      return Button::l;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
      return Button::r;
    case SDL_GAMEPAD_BUTTON_START:
      return Button::plus;
    case SDL_GAMEPAD_BUTTON_BACK:
      return Button::minus;
    case SDL_GAMEPAD_BUTTON_DPAD_UP:
      return Button::dpad_up;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
      return Button::dpad_down;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
      return Button::dpad_left;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
      return Button::dpad_right;
    case SDL_GAMEPAD_BUTTON_LEFT_STICK:
      return Button::left_stick;
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK:
      return Button::right_stick;
    default:
      return std::nullopt;
  }
}

auto Sdl3Input::normalise_axis(Sint16 value) noexcept -> float {
  static constexpr float extent = 32767.0F;
  return std::clamp(static_cast<float>(value) / extent, -1.0F, 1.0F);
}

auto Sdl3Input::pixel_density(SDL_WindowID window) noexcept -> float {
  SDL_Window* const found = SDL_GetWindowFromID(window);
  if (found == nullptr) {
    return 1.0F;
  }
  const float density = SDL_GetWindowPixelDensity(found);
  return density > 0.0F ? density : 1.0F;
}

auto Sdl3Input::window_pixels(SDL_WindowID window) noexcept -> PixelPoint {
  SDL_Window* const found = SDL_GetWindowFromID(window);
  int width = 0;
  int height = 0;
  if (found == nullptr || !SDL_GetWindowSizeInPixels(found, &width, &height)) {
    return {};
  }
  return PixelPoint{.x = static_cast<float>(width),
                    .y = static_cast<float>(height)};
}

}
