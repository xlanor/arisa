#include <arisa/controls/text_input_suggestions.hpp>

#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/context.hpp>

namespace arisa::controls {

TextInputSuggestions::TextInputSuggestions(
    composition::Element field,
    composition::TextInputEnvironment::Builder build)
    : Element(std::make_unique<Content>(std::move(field), std::move(build))) {}

auto TextInputSuggestions::applied(
    composition::Element field,
    composition::TextInputEnvironment::Builder build) -> composition::Element {
  return TextInputSuggestions(std::move(field), std::move(build));
}

TextInputSuggestions::Content::Content(
    composition::Element field,
    composition::TextInputEnvironment::Builder build)
    : field_(std::make_shared<composition::PlaceholderContentView::Source>(
          std::move(field))),
      build_(std::move(build)) {}

auto TextInputSuggestions::Content::type() const noexcept
    -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto TextInputSuggestions::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  const auto completion =
      std::make_shared<composition::TextInputEnvironment::Completion>();
  std::vector<composition::Element> rows;
  rows.emplace_back(composition::PlaceholderContentView(field_));
  if (context.focus_within() && build_) {
    rows.push_back(build_().id(std::string{list_key}));
  }
  std::vector<composition::Element> produced;
  produced.push_back(
      components::VStack(
          components::VStackOptions{
              .alignment = components::HorizontalAlignment::leading},
          std::move(rows))
          .environment(composition::TextInputEnvironment::completer,
                       completion));
  return produced;
}

TextInputCompletion::TextInputCompletion(composition::Element row,
                                         std::string completion)
    : Element(
          std::make_unique<Content>(std::move(row), std::move(completion))) {}

TextInputCompletion::Content::Content(composition::Element row,
                                      std::string completion)
    : row_(std::make_shared<composition::PlaceholderContentView::Source>(
          std::move(row))),
      completion_(std::move(completion)) {}

auto TextInputCompletion::Content::type() const noexcept
    -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto TextInputCompletion::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  const std::shared_ptr<composition::TextInputEnvironment::Completion>&
      completer =
          context.environment(composition::TextInputEnvironment::completer);
  std::vector<composition::Element> produced;
  if (completer == nullptr) {
    produced.emplace_back(composition::PlaceholderContentView(row_));
    return produced;
  }
  produced.push_back(
      components::Button(
          [completer, completion = completion_] { (*completer)(completion); },
          composition::Element{composition::PlaceholderContentView(row_)})
          .button_style(components::ButtonStyle::borderless())
          .id(std::string{completion_key} + completion_));
  return produced;
}

}
