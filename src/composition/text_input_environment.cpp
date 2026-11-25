#include <arisa/composition/text_input_environment.hpp>

namespace arisa::composition {

const EnvironmentKey<std::shared_ptr<TextInputEnvironment::Completion>>
    TextInputEnvironment::completer{"text_input_completer", nullptr};
const EnvironmentKey<TextInputEnvironment::FormattingControls>
    TextInputEnvironment::formatting_controls{"text_input_formatting_controls",
                                              FormattingControls{}};
const EnvironmentKey<std::optional<FindContext>>
    TextInputEnvironment::find_context{"find_context", std::nullopt};
const EnvironmentKey<bool> TextInputEnvironment::find_disabled{"find_disabled",
                                                               false};
const EnvironmentKey<bool> TextInputEnvironment::replace_disabled{
    "replace_disabled", false};
const PreferenceKey<bool> TextInputEnvironment::keyboard{
    "text_input_keyboard", false,
    [](bool& value, bool next) { value = value || next; }};

}
