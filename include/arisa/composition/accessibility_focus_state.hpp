#pragma once

#include <arisa/composition/accessibility_technologies.hpp>
#include <arisa/composition/focus_binding.hpp>
#include <arisa/composition/identity.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>

#include <string_view>
#include <utility>

namespace arisa::composition {

template <class T>
class AccessibilityFocusBinding {
 public:
  using Value = typename FocusBinding<T>::Value;

  AccessibilityFocusBinding(StateHandle<Value> handle, bool active) noexcept
      : handle_(std::move(handle)), active_(active) {}

  auto get() const noexcept -> const Value& { return handle_.get(); }

  void set(Value value) const { handle_.set(std::move(value)); }

  auto projected_value() const -> AccessibilityFocusBinding { return *this; }

  auto active() const noexcept -> bool { return active_; }

  auto focus() const -> FocusBinding<T> { return FocusBinding<T>{handle_}; }

  auto identity() const noexcept -> Identity { return handle_.identity(); }

 private:
  StateHandle<Value> handle_;
  bool active_;
};

template <class T>
class AccessibilityFocusState {
 public:
  using Binding = AccessibilityFocusBinding<T>;
  using Value = typename Binding::Value;

  constexpr explicit AccessibilityFocusState(std::string_view name)
      : key_(name, Value{}),
        technologies_(AccessibilityTechnologies::voice_over |
                      AccessibilityTechnologies::switch_control) {}

  constexpr AccessibilityFocusState(std::string_view name,
                                    AccessibilityTechnologies technologies)
      : key_(name, Value{}), technologies_(technologies) {}

  AccessibilityFocusState(const AccessibilityFocusState&) = delete;
  auto operator=(const AccessibilityFocusState&)
      -> AccessibilityFocusState& = delete;
  AccessibilityFocusState(AccessibilityFocusState&&) = delete;
  auto operator=(AccessibilityFocusState&&)
      -> AccessibilityFocusState& = delete;
  ~AccessibilityFocusState() = default;

  constexpr auto key() const noexcept -> const StateKey<Value>& { return key_; }

  constexpr auto technologies() const noexcept -> AccessibilityTechnologies {
    return technologies_;
  }

 private:
  StateKey<Value> key_;
  AccessibilityTechnologies technologies_;
};

}
