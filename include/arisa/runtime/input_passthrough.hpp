#pragma once

#include <arisa/provider/input/buttons.hpp>
#include <arisa/runtime/passed_input.hpp>
#include <arisa/support/scoped_callback.hpp>

#include <utility>

namespace arisa::runtime {

class InputGate;

class InputPassthrough {
 public:
  auto active() const noexcept -> bool { return release_.valid(); }

  auto buttons() const noexcept -> provider::Buttons { return buttons_; }

  auto passed() const noexcept -> PassedInput { return passed_; }

  void release() noexcept { release_.reset(); }

 private:
  friend class InputGate;

  InputPassthrough(provider::Buttons buttons, PassedInput passed,
                   support::ScopedCallback release) noexcept
      : buttons_(buttons), passed_(passed), release_(std::move(release)) {}

  provider::Buttons buttons_;
  PassedInput passed_;
  support::ScopedCallback release_;
};

}
