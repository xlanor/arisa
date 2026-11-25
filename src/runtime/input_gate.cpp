#include <runtime/input_gate.hpp>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

namespace arisa::runtime {

InputGate::InputGate(Dispatcher wake)
    : scopes_(std::make_shared<Scopes>()), wake_(std::move(wake)) {}

auto InputGate::open(Scope scope, Pass pass,
                     std::shared_ptr<ButtonCapture::Held> held)
    -> support::ScopedCallback {
  std::uint64_t id = 0;
  {
    const std::scoped_lock lock{scopes_->mutex};
    id = scopes_->next++;
    if (scope == Scope::block) {
      scopes_->blocks.emplace(id, std::move(held));
    } else {
      scopes_->passes.emplace(id, pass);
    }
  }
  static_cast<void>(wake_.post([] {}));
  return support::ScopedCallback{[scopes = std::weak_ptr<Scopes>{scopes_},
                                  wake = wake_, id, scope]() noexcept {
    if (const std::shared_ptr<Scopes> live = scopes.lock()) {
      {
        const std::scoped_lock lock{live->mutex};
        if (scope == Scope::block) {
          live->blocks.erase(id);
        } else {
          live->passes.erase(id);
        }
      }
      static_cast<void>(wake.post([] {}));
    }
  }};
}

auto InputGate::block() -> InputBlock {
  return InputBlock{open(Scope::block, Pass{}, nullptr)};
}

auto InputGate::pass(provider::Buttons buttons, PassedInput passed)
    -> InputPassthrough {
  return InputPassthrough{
      buttons, passed,
      open(Scope::pass, Pass{.buttons = buttons, .passed = passed}, nullptr)};
}

auto InputGate::capture() -> ButtonCapture {
  auto held = std::make_shared<ButtonCapture::Held>();
  return ButtonCapture{held, open(Scope::block, Pass{}, held)};
}

auto InputGate::blocked() const -> bool { return snapshot().blocked; }

auto InputGate::snapshot() const -> Snapshot {
  const std::scoped_lock lock{scopes_->mutex};
  Snapshot taken{
      .blocked = !scopes_->blocks.empty(), .passing = {}, .captures = {}};
  const std::uint64_t newest =
      scopes_->blocks.empty() ? 0 : scopes_->blocks.keys().back();
  for (const auto& [id, pass] : scopes_->passes) {
    if (id > newest) {
      taken.passing.buttons = taken.passing.buttons | pass.buttons;
      PassedInput& passing = taken.passing.passed;
      passing.left_stick = passing.left_stick || pass.passed.left_stick;
      passing.right_stick = passing.right_stick || pass.passed.right_stick;
      passing.touch = passing.touch || pass.passed.touch;
      passing.pointer = passing.pointer || pass.passed.pointer;
    }
  }
  for (const auto& [id, held] : scopes_->blocks) {
    if (held != nullptr) {
      taken.captures.push_back(held);
    }
  }
  return taken;
}

auto InputGate::masked(provider::Buttons buttons,
                       provider::Buttons mask) noexcept -> provider::Buttons {
  provider::Buttons kept;
  for (const provider::Button button : provider::all_buttons) {
    if (buttons.contains(button) && !mask.contains(button)) {
      kept = kept.with(button);
    }
  }
  return kept;
}

auto InputGate::shared(provider::Buttons first,
                       provider::Buttons second) noexcept -> provider::Buttons {
  return masked(first, masked(first, second));
}

auto InputGate::key_button(provider::Key key, provider::Button confirm,
                           provider::Button back) noexcept
    -> std::optional<provider::Button> {
  switch (key) {
    case provider::Key::up:
      return provider::Button::dpad_up;
    case provider::Key::down:
      return provider::Button::dpad_down;
    case provider::Key::left:
      return provider::Button::dpad_left;
    case provider::Key::right:
      return provider::Button::dpad_right;
    case provider::Key::enter:
    case provider::Key::space:
      return confirm;
    case provider::Key::escape:
    case provider::Key::backspace:
      return back;
    default:
      return std::nullopt;
  }
}

auto InputGate::filter(provider::InputFrame& frame, provider::Button confirm,
                       provider::Button back) -> bool {
  const Snapshot taken = snapshot();
  const PassedInput& passed = taken.passing.passed;
  const bool touching = !taken.blocked || passed.touch || passed.pointer;
  const bool began = (taken.blocked && !blocking_) || (touching_ && !touching);
  blocking_ = taken.blocked;
  touching_ = touching;
  for (const std::shared_ptr<ButtonCapture::Held>& held : taken.captures) {
    held->store(frame.pads);
  }
  for (std::size_t index = 0; index < frame.pads.size(); ++index) {
    provider::PadState& pad = frame.pads[index];
    provider::Buttons& latched = latched_[index];
    if (taken.blocked) {
      latched = masked(pad.buttons, taken.passing.buttons);
      pad.buttons = shared(pad.buttons, taken.passing.buttons);
      if (!passed.left_stick) {
        pad.left = provider::Stick{};
      }
      if (!passed.right_stick) {
        pad.right = provider::Stick{};
      }
      continue;
    }
    latched = shared(latched, pad.buttons);
    pad.buttons = masked(pad.buttons, latched);
  }
  if (taken.blocked) {
    if (!passed.touch) {
      frame.touches.clear();
    }
    if (!passed.pointer) {
      frame.pointer.reset();
      frame.pinch.reset();
    }
    frame.text.clear();
    std::erase_if(frame.keys, [&](const provider::KeyEvent& event) {
      const std::optional<provider::Button> mapped =
          key_button(event.key, confirm, back);
      return !mapped.has_value() || !taken.passing.buttons.contains(*mapped);
    });
  }
  return began;
}

void InputGate::latch(
    const std::array<provider::PadState, provider::max_pads>& pads) {
  for (std::size_t index = 0; index < pads.size(); ++index) {
    latched_[index] = latched_[index] | pads[index].buttons;
  }
}

}
