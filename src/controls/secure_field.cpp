#include <arisa/controls/secure_field.hpp>

#include <utility>

namespace arisa::controls {

SecureField::SecureField(std::string title,
                         composition::Binding<std::string> text,
                         TextFieldOptions options)
    : TextField(std::move(title), std::move(text), std::move(options),
                Kind::secure, provider::KeyboardType::automatic) {}

SecureField::SecureField(composition::Binding<std::string> text,
                         TextFieldOptions options, composition::Element label)
    : TextField(std::string{}, std::move(text), std::move(options),
                Kind::secure, provider::KeyboardType::automatic, Localizer{},
                source(std::move(label))) {}

}
