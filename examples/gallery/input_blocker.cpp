#include <input_blocker.hpp>

#include <arisa/provider/input/buttons.hpp>

namespace arisa::examples {

const composition::EnvironmentKey<std::shared_ptr<InputBlocker>>
    InputBlocker::key{"gallery-input-blocker", nullptr};

void InputBlocker::hold() {
  if (!root_.has_value() || block_.has_value()) {
    return;
  }
  block_.emplace(root_->get().block_input());
  pass_.emplace(root_->get().pass_input({provider::Button::minus}));
}

void InputBlocker::release() noexcept {
  pass_.reset();
  block_.reset();
}

}
