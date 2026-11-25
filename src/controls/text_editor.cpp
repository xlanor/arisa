#include <arisa/controls/text_editor.hpp>

#include <utility>

namespace arisa::controls {

TextEditor::TextEditor(composition::Binding<std::string> text)
    : TextField(std::string{}, std::move(text),
                TextFieldOptions{.prompt = std::nullopt,
                                 .axis = composition::Axis::vertical},
                Kind::editor, provider::KeyboardType::automatic) {}

TextEditor::TextEditor(composition::Binding<std::string> text,
                       Selection selection)
    : TextField(std::string{}, std::move(text),
                TextFieldOptions{.prompt = std::nullopt,
                                 .axis = composition::Axis::vertical},
                Kind::editor, provider::KeyboardType::automatic, Localizer{},
                nullptr, Editing{.selection = std::move(selection)}) {}

TextEditor::TextEditor(composition::Binding<components::AttributedString> text)
    : TextField(std::string{}, plain(text),
                TextFieldOptions{.prompt = std::nullopt,
                                 .axis = composition::Axis::vertical},
                Kind::editor, provider::KeyboardType::automatic, Localizer{},
                nullptr, Editing{.rich = text}) {}

TextEditor::TextEditor(
    composition::Binding<components::AttributedString> text,
    composition::Binding<components::AttributedTextSelection> selection)
    : TextField(std::string{}, plain(text),
                TextFieldOptions{.prompt = std::nullopt,
                                 .axis = composition::Axis::vertical},
                Kind::editor, provider::KeyboardType::automatic, Localizer{},
                nullptr,
                Editing{.rich = text, .rich_selection = std::move(selection)}) {
}

auto TextEditor::plain(composition::Binding<components::AttributedString> text)
    -> composition::Binding<std::string> {
  return composition::Binding<std::string>{
      [text] { return std::string{text.get().text()}; },
      [text](std::string value) {
        const components::AttributedString before = text.get();
        text.set(before.replacing(
            0, static_cast<std::uint32_t>(before.text().size()),
            components::AttributedString{
                std::move(value),
                before.text().empty()
                    ? components::AttributedString::Attributes{}
                    : before.attributes(0)}));
      }};
}

}
