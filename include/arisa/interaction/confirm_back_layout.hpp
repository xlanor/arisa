#pragma once

#include <arisa/provider/input/buttons.hpp>

namespace arisa::interaction {

class ConfirmBackLayout {
 public:
  static const ConfirmBackLayout nintendo;
  static const ConfirmBackLayout xbox;

  constexpr auto confirm() const noexcept -> provider::Button {
    return confirm_;
  }

  constexpr auto back() const noexcept -> provider::Button { return back_; }

  friend constexpr auto operator==(const ConfirmBackLayout&,
                                   const ConfirmBackLayout&) -> bool = default;

 private:
  constexpr ConfirmBackLayout(provider::Button confirm,
                              provider::Button back) noexcept
      : confirm_(confirm), back_(back) {}

  provider::Button confirm_;
  provider::Button back_;
};

inline constexpr ConfirmBackLayout ConfirmBackLayout::nintendo{
    provider::Button::a, provider::Button::b};
inline constexpr ConfirmBackLayout ConfirmBackLayout::xbox{provider::Button::b,
                                                           provider::Button::a};

}
