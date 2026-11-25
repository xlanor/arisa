#pragma once

#include <arisa/provider/input/buttons.hpp>
#include <arisa/provider/input/provider.hpp>
#include <arisa/runtime/button_capture.hpp>
#include <arisa/runtime/dispatcher.hpp>
#include <arisa/runtime/input_block.hpp>
#include <arisa/runtime/input_passthrough.hpp>
#include <arisa/runtime/passed_input.hpp>

#include <array>
#include <cstdint>
#include <flat_map>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace arisa::runtime {

class InputGate {
 public:
  explicit InputGate(Dispatcher wake);

  InputGate(const InputGate&) = delete;
  auto operator=(const InputGate&) -> InputGate& = delete;
  InputGate(InputGate&&) = delete;
  auto operator=(InputGate&&) -> InputGate& = delete;
  ~InputGate() = default;

  auto block() -> InputBlock;

  auto pass(provider::Buttons buttons, PassedInput passed = {})
      -> InputPassthrough;

  auto capture() -> ButtonCapture;

  auto blocked() const -> bool;

  auto filter(provider::InputFrame& frame, provider::Button confirm,
              provider::Button back) -> bool;

  void latch(const std::array<provider::PadState, provider::max_pads>& pads);

 private:
  struct Pass {
    provider::Buttons buttons;
    PassedInput passed;
  };

  struct Scopes {
    std::mutex mutex;
    std::flat_map<std::uint64_t, std::shared_ptr<ButtonCapture::Held>> blocks;
    std::flat_map<std::uint64_t, Pass> passes;
    std::uint64_t next{1};
  };

  struct Snapshot {
    bool blocked{false};
    Pass passing;
    std::vector<std::shared_ptr<ButtonCapture::Held>> captures;
  };

  enum class Scope : std::uint8_t { block, pass };

  static auto masked(provider::Buttons buttons, provider::Buttons mask) noexcept
      -> provider::Buttons;

  static auto shared(provider::Buttons first, provider::Buttons second) noexcept
      -> provider::Buttons;

  auto open(Scope scope, Pass pass, std::shared_ptr<ButtonCapture::Held> held)
      -> support::ScopedCallback;

  auto snapshot() const -> Snapshot;

  static auto key_button(provider::Key key, provider::Button confirm,
                         provider::Button back) noexcept
      -> std::optional<provider::Button>;

  std::shared_ptr<Scopes> scopes_;
  Dispatcher wake_;
  std::array<provider::Buttons, provider::max_pads> latched_{};
  bool blocking_{false};
  bool touching_{false};
};

}
