#include <arisa/components/attributed_text_formatting.hpp>

#include <utility>

namespace arisa::components {

const composition::EnvironmentKey<
    std::shared_ptr<const AttributedTextFormattingDefinition>>
    AttributedTextFormatting::definition{
        "attributed_text_formatting_definition", nullptr};

auto AttributedTextFormatting::constrained(
    const AttributedString& text,
    const AttributedTextFormattingDefinition& rules) -> AttributedString {
  return text.transforming(0, static_cast<std::uint32_t>(text.text().size()),
                           [&rules](AttributedString::Attributes& attributes) {
                             AttributeContainerProxy proxy{attributes};
                             rules.constrain(proxy);
                           });
}

auto AttributedTextFormatting::Transferable::resolved() const
    -> AttributedString {
  if (definition_ == nullptr) {
    return text_;
  }
  return constrained(text_, *definition_);
}

auto AttributedTextFormatting::Transferable::encoded() const -> std::string {
  return resolved().markdown_source();
}

auto AttributedTextFormatting::Transferable::decoded(std::string_view text)
    -> std::optional<Transferable> {
  return Transferable{AttributedString::markdown(text)};
}

auto AttributedTextFormatting::Transferable::content_type() -> std::string {
  return "net.daringfireball.markdown";
}

}
