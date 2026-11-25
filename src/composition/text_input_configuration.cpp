#include <arisa/composition/context.hpp>
#include <arisa/composition/submission.hpp>
#include <arisa/composition/text_input_configuration.hpp>

namespace arisa::composition {

const EnvironmentKey<KeyboardType> TextInputConfiguration::keyboard_type{
    "keyboard-type", KeyboardType::automatic};

const EnvironmentKey<std::optional<TextContentType>>
    TextInputConfiguration::text_content_type{"text-content-type",
                                              std::nullopt};

const EnvironmentKey<std::optional<TextInputAutocapitalization>>
    TextInputConfiguration::autocapitalization{"text-input-autocapitalization",
                                               std::nullopt};

const EnvironmentKey<std::optional<bool>>
    TextInputConfiguration::autocorrection_disabled{"autocorrection-disabled",
                                                    std::nullopt};

auto TextInputConfiguration::traits(Context& context, KeyboardType fallback,
                                    bool multiline)
    -> provider::TextInputTraits {
  const KeyboardType chosen = context.environment(keyboard_type);
  return provider::TextInputTraits{
      .keyboard = chosen == KeyboardType::automatic ? fallback : chosen,
      .content = context.environment(text_content_type),
      .capitalization = context.environment(autocapitalization),
      .autocorrection_disabled = context.environment(autocorrection_disabled),
      .submit_label = context.environment(Submission::label),
      .multiline = multiline};
}

}
