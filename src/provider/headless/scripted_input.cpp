#include <arisa/provider/headless/scripted_input.hpp>

#include <utility>

namespace arisa::provider::headless {

auto ScriptedInput::sample() -> InputFrame {
  const std::scoped_lock lock{mutex_};
  if (frames_.empty()) {
    return {};
  }
  InputFrame frame = std::move(frames_.front());
  frames_.pop_front();
  for (PadState& pad : frame.pads) {
    pad.left = tuning_.apply(pad.left);
    pad.right = tuning_.apply(pad.right);
  }
  return frame;
}

auto ScriptedInput::tuning() const noexcept -> InputTuning {
  const std::scoped_lock lock{mutex_};
  return tuning_;
}

void ScriptedInput::set_tuning(InputTuning tuning) noexcept {
  const std::scoped_lock lock{mutex_};
  tuning_ = tuning;
}

void ScriptedInput::push(InputFrame frame) {
  const std::scoped_lock lock{mutex_};
  frames_.push_back(std::move(frame));
}

auto ScriptedInput::pointer_host() const noexcept -> PointerHost {
  const std::scoped_lock lock{mutex_};
  return host_;
}

void ScriptedInput::present_pointer(PointerAppearance appearance) {
  const std::scoped_lock lock{mutex_};
  presented_ = appearance;
  ++presentations_;
}

void ScriptedInput::set_pointer_host(PointerHost host) noexcept {
  const std::scoped_lock lock{mutex_};
  host_ = host;
}

auto ScriptedInput::presented_pointer() const
    -> std::optional<PointerAppearance> {
  const std::scoped_lock lock{mutex_};
  return presented_;
}

auto ScriptedInput::pointer_presentations() const -> std::size_t {
  const std::scoped_lock lock{mutex_};
  return presentations_;
}

auto ScriptedInput::pending() const -> std::size_t {
  const std::scoped_lock lock{mutex_};
  return frames_.size();
}

}
