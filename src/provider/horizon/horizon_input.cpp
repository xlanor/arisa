#include <arisa/provider/horizon/horizon_input.hpp>

#include <arisa/provider/horizon/console.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace arisa::provider::horizon {

HorizonInput::HorizonInput(
    const SurfaceProvider& surface, InputTuning tuning,
    std::shared_ptr<const std::atomic<bool>> keyboard_shown)
    : surface_(surface),
      tuning_(tuning),
      keyboard_shown_(std::move(keyboard_shown)),
      pointer_(surface.size()) {
  padConfigureInput(max_pads, HidNpadStyleSet_NpadStandard);
  for (std::size_t index = 0; index < max_pads; ++index) {
    std::uint64_t mask = 1ULL << (HidNpadIdType_No1 + index);
    if (index == 0) {
      mask |= 1ULL << HidNpadIdType_Handheld;
    }
    padInitializeWithMask(&pads_[index], mask);
  }
  hidInitializeTouchScreen();
  hidInitializeKeyboard();
}

auto HorizonInput::sample() -> InputFrame {
  InputFrame frame;
  const bool typing = keyboard_shown_ != nullptr &&
                      keyboard_shown_->load(std::memory_order_acquire);
  for (std::size_t index = 0; index < max_pads; ++index) {
    padUpdate(&pads_[index]);
    if (padIsConnected(&pads_[index])) {
      frame.pads[index] =
          typing ? PadState{.connected = true} : read_pad(pads_[index]);
    }
  }
  if (typing) {
    release_touches(frame);
    release_keys(frame);
    return frame;
  }
  const PointerReading reading = pointer_.sample(surface_.get().size());
  frame.pointer = reading.pointer;
  claim(frame.pads, reading);
  sample_touches(frame);
  sample_keys(frame);
  return frame;
}

void HorizonInput::claim(std::array<PadState, max_pads>& pads,
                         const PointerReading& reading) noexcept {
  for (std::size_t index = 0; index < pads.size(); ++index) {
    PadState& pad = pads[index];
    if (reading.claims_shoulders &&
        (!reading.claimed_pad.has_value() || *reading.claimed_pad == index)) {
      pad.buttons = pad.buttons.without(Button::l)
                        .without(Button::r)
                        .without(Button::zl)
                        .without(Button::zr);
    }
    if (reading.claims_sticks) {
      pad.left = Stick{};
      pad.right = Stick{};
    }
  }
}

auto HorizonInput::pointer_host() const noexcept -> PointerHost {
  return PointerHost::application;
}

void HorizonInput::present_pointer(PointerAppearance) {}

auto HorizonInput::pointer_failure() const noexcept
    -> std::optional<ProviderError> {
  return pointer_.failure();
}

auto HorizonInput::translate_key(std::uint32_t hid) noexcept
    -> std::optional<Key> {
  static constexpr std::array<std::pair<std::uint32_t, Key>, 95> mapping{{
      {HidKeyboardKey_A, Key::a},
      {HidKeyboardKey_B, Key::b},
      {HidKeyboardKey_C, Key::c},
      {HidKeyboardKey_D, Key::d},
      {HidKeyboardKey_E, Key::e},
      {HidKeyboardKey_F, Key::f},
      {HidKeyboardKey_G, Key::g},
      {HidKeyboardKey_H, Key::h},
      {HidKeyboardKey_I, Key::i},
      {HidKeyboardKey_J, Key::j},
      {HidKeyboardKey_K, Key::k},
      {HidKeyboardKey_L, Key::l},
      {HidKeyboardKey_M, Key::m},
      {HidKeyboardKey_N, Key::n},
      {HidKeyboardKey_O, Key::o},
      {HidKeyboardKey_P, Key::p},
      {HidKeyboardKey_Q, Key::q},
      {HidKeyboardKey_R, Key::r},
      {HidKeyboardKey_S, Key::s},
      {HidKeyboardKey_T, Key::t},
      {HidKeyboardKey_U, Key::u},
      {HidKeyboardKey_V, Key::v},
      {HidKeyboardKey_W, Key::w},
      {HidKeyboardKey_X, Key::x},
      {HidKeyboardKey_Y, Key::y},
      {HidKeyboardKey_Z, Key::z},
      {HidKeyboardKey_D1, Key::digit1},
      {HidKeyboardKey_D2, Key::digit2},
      {HidKeyboardKey_D3, Key::digit3},
      {HidKeyboardKey_D4, Key::digit4},
      {HidKeyboardKey_D5, Key::digit5},
      {HidKeyboardKey_D6, Key::digit6},
      {HidKeyboardKey_D7, Key::digit7},
      {HidKeyboardKey_D8, Key::digit8},
      {HidKeyboardKey_D9, Key::digit9},
      {HidKeyboardKey_D0, Key::digit0},
      {HidKeyboardKey_Return, Key::enter},
      {HidKeyboardKey_NumPadEnter, Key::enter},
      {HidKeyboardKey_Escape, Key::escape},
      {HidKeyboardKey_Backspace, Key::backspace},
      {HidKeyboardKey_Tab, Key::tab},
      {HidKeyboardKey_Space, Key::space},
      {HidKeyboardKey_Minus, Key::minus},
      {HidKeyboardKey_Plus, Key::equals},
      {HidKeyboardKey_OpenBracket, Key::left_bracket},
      {HidKeyboardKey_CloseBracket, Key::right_bracket},
      {HidKeyboardKey_Pipe, Key::backslash},
      {HidKeyboardKey_Semicolon, Key::semicolon},
      {HidKeyboardKey_Quote, Key::apostrophe},
      {HidKeyboardKey_Backquote, Key::grave},
      {HidKeyboardKey_Comma, Key::comma},
      {HidKeyboardKey_Period, Key::period},
      {HidKeyboardKey_Slash, Key::slash},
      {HidKeyboardKey_F1, Key::f1},
      {HidKeyboardKey_F2, Key::f2},
      {HidKeyboardKey_F3, Key::f3},
      {HidKeyboardKey_F4, Key::f4},
      {HidKeyboardKey_F5, Key::f5},
      {HidKeyboardKey_F6, Key::f6},
      {HidKeyboardKey_F7, Key::f7},
      {HidKeyboardKey_F8, Key::f8},
      {HidKeyboardKey_F9, Key::f9},
      {HidKeyboardKey_F10, Key::f10},
      {HidKeyboardKey_F11, Key::f11},
      {HidKeyboardKey_F12, Key::f12},
      {HidKeyboardKey_Insert, Key::insert},
      {HidKeyboardKey_Home, Key::home},
      {HidKeyboardKey_PageUp, Key::page_up},
      {HidKeyboardKey_Delete, Key::delete_forward},
      {HidKeyboardKey_End, Key::end},
      {HidKeyboardKey_PageDown, Key::page_down},
      {HidKeyboardKey_RightArrow, Key::right},
      {HidKeyboardKey_LeftArrow, Key::left},
      {HidKeyboardKey_DownArrow, Key::down},
      {HidKeyboardKey_UpArrow, Key::up},
      {HidKeyboardKey_NumPad1, Key::digit1},
      {HidKeyboardKey_NumPad2, Key::digit2},
      {HidKeyboardKey_NumPad3, Key::digit3},
      {HidKeyboardKey_NumPad4, Key::digit4},
      {HidKeyboardKey_NumPad5, Key::digit5},
      {HidKeyboardKey_NumPad6, Key::digit6},
      {HidKeyboardKey_NumPad7, Key::digit7},
      {HidKeyboardKey_NumPad8, Key::digit8},
      {HidKeyboardKey_NumPad9, Key::digit9},
      {HidKeyboardKey_NumPad0, Key::digit0},
      {HidKeyboardKey_Backslash, Key::backslash},
      {HidKeyboardKey_LeftControl, Key::left_ctrl},
      {HidKeyboardKey_LeftShift, Key::left_shift},
      {HidKeyboardKey_LeftAlt, Key::left_alt},
      {HidKeyboardKey_LeftGui, Key::left_super},
      {HidKeyboardKey_RightControl, Key::right_ctrl},
      {HidKeyboardKey_RightShift, Key::right_shift},
      {HidKeyboardKey_RightAlt, Key::right_alt},
      {HidKeyboardKey_RightGui, Key::right_super},
      {HidKeyboardKey_NumPadSubtract, Key::minus},
  }};
  const auto* found = std::ranges::find(
      mapping, hid, [](const auto& entry) { return entry.first; });
  if (found == mapping.end()) {
    return std::nullopt;
  }
  return found->second;
}

void HorizonInput::sample_keys(InputFrame& frame) {
  HidKeyboardState state{};
  if (hidGetKeyboardStates(&state, 1) == 0) {
    return;
  }
  std::vector<Key> down;
  for (std::uint32_t hid = 0; hid < 256; ++hid) {
    if ((state.keys[hid / 64] & (1ULL << (hid % 64))) == 0) {
      continue;
    }
    if (const std::optional<Key> key = translate_key(hid);
        key.has_value() && !std::ranges::contains(down, *key)) {
      down.push_back(*key);
    }
  }
  for (const Key key : held_keys_) {
    if (!std::ranges::contains(down, key)) {
      frame.keys.push_back(KeyEvent{.key = key, .down = false});
    }
  }
  for (const Key key : down) {
    if (!std::ranges::contains(held_keys_, key)) {
      frame.keys.push_back(KeyEvent{.key = key, .down = true});
    }
  }
  held_keys_ = std::move(down);
  if (const std::optional<Key> repeated = repeat_.update(
          held_keys_, std::chrono::steady_clock::now().time_since_epoch())) {
    frame.keys.push_back(
        KeyEvent{.key = *repeated, .down = true, .repeat = true});
  }
}

void HorizonInput::release_keys(InputFrame& frame) {
  for (const Key key : held_keys_) {
    frame.keys.push_back(KeyEvent{.key = key, .down = false});
  }
  held_keys_.clear();
}

void HorizonInput::release_touches(InputFrame& frame) {
  for (const auto& [finger, tracked] : touches_) {
    frame.touches.push_back(Touch{.id = tracked.id,
                                  .position = tracked.position,
                                  .phase = TouchPhase::ended});
  }
  touches_.clear();
}

auto HorizonInput::tuning() const noexcept -> InputTuning { return tuning_; }

void HorizonInput::set_tuning(InputTuning tuning) noexcept { tuning_ = tuning; }

auto HorizonInput::translate_buttons(std::uint64_t held) noexcept -> Buttons {
  static constexpr std::array<std::pair<std::uint64_t, Button>, 20> mapping{{
      {HidNpadButton_A, Button::a},
      {HidNpadButton_B, Button::b},
      {HidNpadButton_X, Button::x},
      {HidNpadButton_Y, Button::y},
      {HidNpadButton_L, Button::l},
      {HidNpadButton_R, Button::r},
      {HidNpadButton_ZL, Button::zl},
      {HidNpadButton_ZR, Button::zr},
      {HidNpadButton_Plus, Button::plus},
      {HidNpadButton_Minus, Button::minus},
      {HidNpadButton_Up, Button::dpad_up},
      {HidNpadButton_Down, Button::dpad_down},
      {HidNpadButton_Left, Button::dpad_left},
      {HidNpadButton_Right, Button::dpad_right},
      {HidNpadButton_StickL, Button::left_stick},
      {HidNpadButton_StickR, Button::right_stick},
      {HidNpadButton_LeftSL, Button::sl},
      {HidNpadButton_RightSL, Button::sl},
      {HidNpadButton_LeftSR, Button::sr},
      {HidNpadButton_RightSR, Button::sr},
  }};
  Buttons buttons;
  for (const auto& [bit, button] : mapping) {
    if ((held & bit) != 0) {
      buttons = buttons.with(button);
    }
  }
  return buttons;
}

auto HorizonInput::normalise_stick(HidAnalogStickState stick) noexcept
    -> Stick {
  return Stick{
      .x = std::clamp(static_cast<float>(stick.x) / stick_extent, -1.0F, 1.0F),
      .y = std::clamp(static_cast<float>(stick.y) / stick_extent, -1.0F, 1.0F)};
}

auto HorizonInput::read_pad(const ::PadState& pad) const noexcept -> PadState {
  return PadState{
      .connected = true,
      .buttons = translate_buttons(padGetButtons(&pad)),
      .left = tuning_.apply(normalise_stick(padGetStickPos(&pad, 0))),
      .right = tuning_.apply(normalise_stick(padGetStickPos(&pad, 1)))};
}

void HorizonInput::sample_touches(InputFrame& frame) {
  HidTouchScreenState state{};
  if (hidGetTouchScreenStates(&state, 1) == 0) {
    for (const auto& [finger, tracked] : touches_) {
      frame.touches.push_back(Touch{.id = tracked.id,
                                    .position = tracked.position,
                                    .phase = TouchPhase::stationary});
    }
    return;
  }
  const auto count = static_cast<std::size_t>(std::clamp<std::int32_t>(
      state.count, 0, static_cast<std::int32_t>(std::size(state.touches))));
  std::flat_map<std::uint32_t, TrackedTouch> current;
  for (const HidTouchState& raw : std::span{state.touches}.first(count)) {
    if (current.contains(raw.finger_id)) {
      continue;
    }
    const PixelPoint position = touch_position(raw);
    const auto previous = touches_.find(raw.finger_id);
    const bool restarted = previous != touches_.end() &&
                           (raw.attributes & HidTouchAttribute_Start) != 0;
    if (previous == touches_.end() || restarted) {
      if (restarted) {
        frame.touches.push_back(Touch{.id = previous->second.id,
                                      .position = previous->second.position,
                                      .phase = TouchPhase::ended});
      }
      const TouchId id{next_touch_id_++};
      frame.touches.push_back(
          Touch{.id = id, .position = position, .phase = TouchPhase::began});
      current.insert_or_assign(raw.finger_id,
                               TrackedTouch{.id = id, .position = position});
      continue;
    }
    const TouchPhase phase = previous->second.position == position
                                 ? TouchPhase::stationary
                                 : TouchPhase::moved;
    frame.touches.push_back(
        Touch{.id = previous->second.id, .position = position, .phase = phase});
    current.insert_or_assign(
        raw.finger_id,
        TrackedTouch{.id = previous->second.id, .position = position});
  }
  for (const auto& [finger, tracked] : touches_) {
    if (!current.contains(finger)) {
      frame.touches.push_back(Touch{.id = tracked.id,
                                    .position = tracked.position,
                                    .phase = TouchPhase::ended});
    }
  }
  touches_ = std::move(current);
}

auto HorizonInput::touch_position(const HidTouchState& touch) const noexcept
    -> PixelPoint {
  return Console::touch_panel.to_pixels(
      PixelPoint{.x = static_cast<float>(touch.x),
                 .y = static_cast<float>(touch.y)},
      surface_.get().size());
}

}
