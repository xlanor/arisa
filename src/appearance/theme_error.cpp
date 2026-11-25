#include <arisa/appearance/theme_error.hpp>

#include <format>

namespace arisa::appearance {

auto ThemeError::description() const -> std::string {
  if (key_.empty()) {
    return std::format("{}:{}: {}", file_, line_, message_);
  }
  return std::format("{}:{}: {}: {}", file_, line_, key_, message_);
}

}
