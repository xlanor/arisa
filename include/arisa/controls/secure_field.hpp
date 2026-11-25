#pragma once

#include <arisa/composition/binding.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/controls/text_field.hpp>

#include <string>

namespace arisa::controls {

class SecureField : public TextField {
 public:
  SecureField(std::string title, composition::Binding<std::string> text,
              TextFieldOptions options = {});

  SecureField(composition::Binding<std::string> text, TextFieldOptions options,
              composition::Element label);
};

}
