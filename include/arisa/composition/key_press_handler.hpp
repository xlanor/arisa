#pragma once

#include <arisa/composition/key_equivalent.hpp>
#include <arisa/composition/key_press.hpp>

#include <algorithm>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace arisa::composition {

class KeyPressHandler {
 public:
  using Action = std::move_only_function<KeyPress::Result(const KeyPress&)>;

  KeyPressHandler(std::vector<KeyEquivalent> keys, std::string characters,
                  KeyPress::Phases phases, Action action) noexcept
      : keys_(std::move(keys)),
        characters_(std::move(characters)),
        phases_(phases),
        action_(std::move(action)) {}

  auto matches(const KeyPress& press) const -> bool {
    if ((phases_ & press.phase) != press.phase) {
      return false;
    }
    if (!keys_.empty() && !std::ranges::contains(keys_, press.key)) {
      return false;
    }
    if (!characters_.empty() &&
        (press.characters.empty() ||
         characters_.find(press.characters.front()) == std::string::npos)) {
      return false;
    }
    return true;
  }

  auto perform(const KeyPress& press) -> KeyPress::Result {
    return action_(press);
  }

 private:
  std::vector<KeyEquivalent> keys_;
  std::string characters_;
  KeyPress::Phases phases_;
  Action action_;
};

}
