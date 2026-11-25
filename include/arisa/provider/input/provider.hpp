#pragma once

#include <arisa/provider/input/buttons.hpp>
#include <arisa/provider/input/input_tuning.hpp>
#include <arisa/provider/input/stick.hpp>
#include <arisa/support/strong_value.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace arisa::provider {

inline constexpr std::size_t max_pads = 8;

struct PadState {
  bool connected{false};
  Buttons buttons{};
  Stick left{};
  Stick right{};

  friend auto operator==(const PadState&, const PadState&) -> bool = default;
};

struct TouchIdTag;

using TouchId = support::StrongValue<std::uint64_t, TouchIdTag>;

enum class TouchPhase : std::uint8_t {
  began,
  moved,
  stationary,
  ended,
};

struct PixelPoint {
  float x{0.0F};
  float y{0.0F};

  friend auto operator==(const PixelPoint&, const PixelPoint&)
      -> bool = default;
};

struct Touch {
  TouchId id{0};
  PixelPoint position{};
  TouchPhase phase{TouchPhase::began};

  friend auto operator==(const Touch&, const Touch&) -> bool = default;
};

struct PointerState {
  PixelPoint position{};
  bool left{false};
  bool right{false};
  bool middle{false};
  float wheel_x{0.0F};
  float wheel_y{0.0F};

  friend auto operator==(const PointerState&, const PointerState&)
      -> bool = default;
};

struct TrackpadPinch {
  TouchPhase phase{TouchPhase::began};
  float scale{1.0F};

  friend auto operator==(const TrackpadPinch&, const TrackpadPinch&)
      -> bool = default;
};

enum class PointerHost : std::uint8_t {
  system,
  application,
};

enum class PointerShape : std::uint8_t {
  arrow,
  text,
  vertical_text,
  crosshair,
  pointing_hand,
  open_hand,
  closed_hand,
  zoom_in,
  zoom_out,
  resize_horizontal,
  resize_vertical,
  resize_leading,
  resize_trailing,
  resize_top,
  resize_bottom,
  resize_descending,
  resize_ascending,
  resize_top_leading,
  resize_top_trailing,
  resize_bottom_leading,
  resize_bottom_trailing,
  custom,
};

struct PointerAppearance {
  PointerShape shape{PointerShape::arrow};
  bool visible{true};

  friend auto operator==(const PointerAppearance&, const PointerAppearance&)
      -> bool = default;
};

enum class Key : std::uint8_t {
  tab,
  left,
  right,
  up,
  down,
  page_up,
  page_down,
  home,
  end,
  insert,
  delete_forward,
  backspace,
  space,
  enter,
  escape,
  a,
  c,
  v,
  x,
  y,
  z,
  left_ctrl,
  right_ctrl,
  left_shift,
  right_shift,
  left_alt,
  right_alt,
  left_super,
  right_super,
  f12,
  b,
  d,
  e,
  f,
  g,
  h,
  i,
  j,
  k,
  l,
  m,
  n,
  o,
  p,
  q,
  r,
  s,
  t,
  u,
  w,
  digit0,
  digit1,
  digit2,
  digit3,
  digit4,
  digit5,
  digit6,
  digit7,
  digit8,
  digit9,
  f1,
  f2,
  f3,
  f4,
  f5,
  f6,
  f7,
  f8,
  f9,
  f10,
  f11,
  minus,
  equals,
  left_bracket,
  right_bracket,
  backslash,
  semicolon,
  apostrophe,
  grave,
  comma,
  period,
  slash,
};

struct KeyEvent {
  Key key{Key::escape};
  bool down{false};
  bool repeat{false};

  friend auto operator==(const KeyEvent&, const KeyEvent&) -> bool = default;
};

struct InputFrame {
  std::array<PadState, max_pads> pads{};
  std::vector<Touch> touches;
  std::optional<PointerState> pointer;
  std::optional<TrackpadPinch> pinch;
  std::string text;
  std::vector<KeyEvent> keys;

  friend auto operator==(const InputFrame&, const InputFrame&)
      -> bool = default;
};

class InputProvider {
 public:
  virtual ~InputProvider() = default;

  virtual auto sample() -> InputFrame = 0;

  virtual auto tuning() const noexcept -> InputTuning = 0;

  virtual void set_tuning(InputTuning tuning) noexcept = 0;

  virtual auto pointer_host() const noexcept -> PointerHost = 0;

  virtual void present_pointer(PointerAppearance appearance) = 0;
};

}
