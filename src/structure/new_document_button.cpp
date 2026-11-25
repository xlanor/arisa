#include <arisa/structure/new_document_button.hpp>

#include <arisa/components/button.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/text.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/text/localized_string_key.hpp>

#include <memory>
#include <utility>

namespace arisa::structure {

NewDocumentButton::NewDocumentButton(
    std::optional<std::string> label,
    std::optional<composition::UTType> content_type)
    : Element(std::make_unique<Content>(
          label.value_or(std::string{default_title}),
          components::NewDocumentAction::Request{
              .content_type = std::move(content_type)})) {}

NewDocumentButton::NewDocumentButton(
    std::optional<std::string> label,
    std::optional<composition::UTType> content_type,
    components::DocumentCreationSource source)
    : Element(
          std::make_unique<Content>(label.value_or(std::string{default_title}),
                                    components::NewDocumentAction::Request{
                                        .content_type = std::move(content_type),
                                        .source = std::move(source)})) {}

NewDocumentButton::NewDocumentButton(std::optional<std::string> label,
                                     composition::UTType content_type,
                                     components::DocumentCreationSource source,
                                     Prepare prepare_document_url)
    : Element(std::make_unique<Content>(
          label.value_or(std::string{default_title}),
          components::NewDocumentAction::Request{
              .content_type = std::move(content_type),
              .source = std::move(source),
              .prepare = std::move(prepare_document_url)})) {}

NewDocumentButton::NewDocumentButton(std::optional<std::string> label,
                                     composition::UTType content_type,
                                     Prepare prepare_document_url)
    : Element(std::make_unique<Content>(
          label.value_or(std::string{default_title}),
          components::NewDocumentAction::Request{
              .content_type = std::move(content_type),
              .prepare = std::move(prepare_document_url)})) {}

auto NewDocumentButton::Content::type() const noexcept
    -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto NewDocumentButton::Content::activate(composition::Context& context,
                                          const composition::Node&,
                                          std::optional<composition::Point>)
    -> bool {
  const components::NewDocumentAction& action =
      context.environment(components::Environment::new_document);
  if (!action.available()) {
    return false;
  }
  action.perform(request_);
  return true;
}

auto NewDocumentButton::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  const components::NewDocumentAction& action =
      context.environment(components::Environment::new_document);
  std::vector<composition::Element> produced;
  produced.push_back(composition::Element{
      components::Button(
          [action, request = request_] { action.perform(request); },
          components::Text(text::LocalizedStringKey{label_}))}
                         .disabled(!action.available()));
  return produced;
}

}
