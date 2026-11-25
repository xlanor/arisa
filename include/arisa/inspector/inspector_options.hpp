#pragma once

#include <arisa/provider/input/buttons.hpp>
#include <arisa/provider/input/provider.hpp>
#include <arisa/provider/storage/provider.hpp>

#include <cstddef>
#include <functional>
#include <optional>

namespace arisa::inspector {

struct InspectorOptions {
  provider::Buttons chord{provider::Button::l, provider::Button::r,
                          provider::Button::minus};
  provider::Key key{provider::Key::f12};
  std::size_t history{300};
  std::size_t ranked{10};
  float panel_width{560.0F};
  std::optional<std::reference_wrapper<provider::StorageProvider>> exports{};
};

}
