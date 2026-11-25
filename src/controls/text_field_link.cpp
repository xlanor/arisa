#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/text.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/text_input_configuration.hpp>
#include <arisa/controls/text_field_link.hpp>
#include <arisa/text/localized_string_key.hpp>

#include <memory>
#include <utility>

namespace arisa::controls {

TextFieldLink::TextFieldLink(std::string title,
                             std::optional<std::string> prompt,
                             OnSubmit on_submit)
    : Element(std::make_unique<Content>(std::move(title), std::move(prompt),
                                        nullptr, std::move(on_submit))) {}

TextFieldLink::TextFieldLink(std::optional<std::string> prompt,
                             composition::Element label, OnSubmit on_submit)
    : Element(std::make_unique<Content>(
          std::string{}, std::move(prompt),
          std::make_shared<composition::PlaceholderContentView::Source>(
              std::move(label)),
          std::move(on_submit))) {}

auto TextFieldLink::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto TextFieldLink::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  const auto pending = context.state(requested);
  composition::Element shown =
      label_ != nullptr
          ? composition::Element{composition::PlaceholderContentView(label_)}
          : composition::Element{
                components::Text(text::LocalizedStringKey{title_})};
  composition::Element body =
      components::Button([pending] { pending.set(pending.get() + 1); },
                         std::move(shown))
          .button_style(components::ButtonStyle::link());
  const std::optional<runtime::TextInput>& input =
      context.environment(components::Environment::text_input);
  if (pending.get() > 0 && input.has_value()) {
    provider::TextInputRequest details{
        .initial = {},
        .hint = prompt_.value_or(title_),
        .max_length = provider::TextInputLimit::of<256>(),
        .kind = provider::TextInputKind::text,
        .traits = composition::TextInputConfiguration::traits(
            context, provider::KeyboardType::automatic, false),
        .live = false};
    body = std::move(body).task(
        composition::ElementKey{pending.get()},
        [request = pending.get(), input = *input, details = std::move(details),
         submit = on_submit_]() mutable {
          return ask(request, input, std::move(details), std::move(submit));
        });
  }
  std::vector<composition::Element> produced;
  produced.push_back(std::move(body));
  return produced;
}

auto TextFieldLink::Content::ask(int request, runtime::TextInput input,
                                 provider::TextInputRequest details,
                                 OnSubmit on_submit) -> async::Task<void> {
  if (request == 0) {
    co_return;
  }
  const runtime::TextInput::Result result =
      co_await input.request(std::move(details));
  if (result.has_value() && on_submit) {
    on_submit(*result);
  }
}

}
