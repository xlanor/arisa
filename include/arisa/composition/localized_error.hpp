#pragma once

#include <concepts>
#include <string>

namespace arisa::composition {

template <class E>
concept LocalizedError = requires(const E& error) {
  { error.error_description() } -> std::convertible_to<std::string>;
};

template <class E>
concept SuggestsRecovery = requires(const E& error) {
  { error.recovery_suggestion() } -> std::convertible_to<std::string>;
};

}
