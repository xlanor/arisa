#pragma once

#include <arisa/composition/environment_key.hpp>
#include <arisa/provider/text_input/text_input_traits.hpp>

#include <optional>

namespace arisa::composition {

class Context;

using KeyboardType = provider::KeyboardType;
using TextContentType = provider::TextContentType;
using TextInputAutocapitalization = provider::TextInputAutocapitalization;

class TextInputConfiguration {
 public:
  static const EnvironmentKey<KeyboardType> keyboard_type;
  static const EnvironmentKey<std::optional<TextContentType>> text_content_type;
  static const EnvironmentKey<std::optional<TextInputAutocapitalization>>
      autocapitalization;
  static const EnvironmentKey<std::optional<bool>> autocorrection_disabled;

  static auto traits(Context& context, KeyboardType fallback, bool multiline)
      -> provider::TextInputTraits;
};

}
