#pragma once

#include <arisa/components/attributed_string.hpp>
#include <arisa/components/attributed_text_selection.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/controls/text_editor_style.hpp>
#include <arisa/controls/text_field.hpp>
#include <arisa/theme/metrics.hpp>

#include <string>

namespace arisa::controls {

class TextEditor : public TextField {
 public:
  static constexpr float minimum_height = 96.0F;
  static constexpr float text_inset_leading = 5.0F;
  static constexpr float text_inset_top = 1.0F;

  explicit TextEditor(composition::Binding<std::string> text);

  TextEditor(composition::Binding<std::string> text, Selection selection);

  explicit TextEditor(composition::Binding<components::AttributedString> text);

  TextEditor(
      composition::Binding<components::AttributedString> text,
      composition::Binding<components::AttributedTextSelection> selection);

 private:
  static auto plain(composition::Binding<components::AttributedString> text)
      -> composition::Binding<std::string>;
};

}
