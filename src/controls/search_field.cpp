#include <arisa/components/environment.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/enablement.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/search_environment.hpp>
#include <arisa/composition/submit_triggers.hpp>
#include <arisa/composition/symbol_variants.hpp>
#include <arisa/composition/text_input_configuration.hpp>
#include <arisa/controls/search_field.hpp>
#include <arisa/theme/metrics.hpp>
#include <arisa/theme/scaled_metric.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>

#include <memory>
#include <utility>

namespace arisa::controls {

SearchField::SearchField(composition::Binding<std::string> text,
                         Options options)
    : Element(std::make_unique<Content>(std::move(text), std::move(options))) {
  declared().focus_shape =
      theme::Shape::rounded(theme::metrics::text_field.corner_radius, true);
}

auto SearchField::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto SearchField::Content::chips() const -> std::vector<composition::Element> {
  std::vector<composition::Element> found;
  if (!options_.chips.has_value() || !options_.chips->count) {
    return found;
  }
  const Chips& chips = *options_.chips;
  const std::size_t count = chips.count();
  found.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    found.push_back(
        chips.label(index)
            .font(theme::Font::subheadline)
            .foreground_style(theme::ColourRole::on_accent)
            .line_limit(1)
            .padding(
                theme::Insets::symmetric(theme::space::sm, theme::space::xxs))
            .background(theme::ColourRole::accent, theme::Shape::capsule())
            .on_tap_gesture([remove = chips.remove, index] {
              if (remove) {
                remove(index);
              }
            })
            .id(composition::ElementKey{static_cast<std::int64_t>(index)}));
  }
  return found;
}

auto SearchField::Content::presented(composition::Context& context,
                                     const std::string& value) const
    -> composition::Element {
  const bool empty = value.empty();
  std::vector<composition::Element> row;
  row.push_back(components::Image::system_name(std::string{symbol})
                    .foreground_style(theme::ColourRole::secondary_label));
  for (composition::Element& chip : chips()) {
    row.push_back(std::move(chip));
  }
  components::Text shown{empty ? options_.prompt : value};
  row.push_back(TextEditingSession::decorate(
      session_,
      std::move(shown).line_limit(1).foreground_style(
          empty ? theme::ColourRole::placeholder_text
                : theme::ColourRole::label),
      empty));
  row.push_back(components::Spacer());
  if (!empty) {
    row.push_back(
        components::Image::system_name(std::string{clear_symbol})
            .symbol_variant(composition::SymbolVariants::fill)
            .foreground_style(theme::ColourRole::tertiary_label)
            .on_tap_gesture([cleared = options_.cleared, text = text_] {
              text.set(std::string{});
              if (cleared) {
                cleared();
              }
            })
            .id("arisa.search.clear"));
  }
  composition::Element body =
      components::HStack(components::HStackOptions{.spacing = theme::space::xs},
                         std::move(row))
          .padding(theme::Insets::symmetric(
              theme::metric::text_field_inset,
              theme::metric::text_field_vertical_inset))
          .frame({.max_width = composition::infinity,
                  .min_height = theme::metric::text_field_height})
          .background(theme::ColourRole::tertiary_system_fill,
                      theme::Shape::rounded(
                          theme::metrics::text_field.corner_radius, true));
  if (!context.environment(composition::Enablement::is_enabled)) {
    body = std::move(body).opacity(0.5F);
  }
  return body;
}

auto SearchField::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  const auto pending = context.state(requests);
  request_ = pending;
  const std::string value = text_.get();
  const std::shared_ptr<
      const composition::Binding<std::optional<composition::TextSelection>>>&
      bound = context.environment(composition::SearchEnvironment::selection);
  session_->bind(TextEditingSession::Binds{
      .text = text_,
      .selection =
          bound != nullptr
              ? *bound
              : composition::Binding<
                    std::optional<composition::TextSelection>>{context.state(
                    TextEditingSession::selection)},
      .editing = composition::Binding<bool>{context.state(
          TextEditingSession::editing)},
      .menu =
          composition::Binding<bool>{context.state(TextEditingSession::menu)},
      .multiline = false,
      .secure = false,
      .enabled = context.environment(composition::Enablement::is_enabled),
      .focused = context.is_focused(),
      .submit = composition::Submission::collect(
          context, composition::SubmitTriggers::search),
      .pasteboard = context.environment(components::Environment::pasteboard),
      .rich = std::nullopt,
      .find = std::nullopt,
      .controls = {}});
  composition::Element body = presented(context, value);
  const std::optional<runtime::TextInput>& input =
      context.environment(components::Environment::text_input);
  if (pending.get() > 0 && input.has_value()) {
    provider::TextInputRequest details{
        .initial = value,
        .hint = options_.prompt,
        .max_length = provider::TextInputLimit::of<256>(),
        .kind = provider::TextInputKind::text,
        .traits = composition::TextInputConfiguration::traits(
            context, provider::KeyboardType::web_search, false),
        .live = true};
    details.traits.submit_label = provider::SubmitLabel::search;
    body = std::move(body).task(
        composition::ElementKey{pending.get()},
        [request = pending.get(), input = *input, details = std::move(details),
         text = text_,
         submit = composition::Submission::collect(
             context, composition::SubmitTriggers::search)]() mutable {
          return edit(request, input, std::move(details), text,
                      std::move(submit));
        });
  }
  std::vector<composition::Element> produced;
  produced.push_back(std::move(body));
  return produced;
}

auto SearchField::Content::press(composition::Context&,
                                 const composition::Node&,
                                 const composition::KeyPress& press) -> bool {
  return session_->press(press);
}

auto SearchField::Content::steer(composition::Context&,
                                 const composition::Node&,
                                 composition::MoveCommandDirection direction)
    -> bool {
  return session_->steer(direction);
}

auto SearchField::Content::activate() -> bool {
  if (!request_.has_value()) {
    return false;
  }
  if (options_.began) {
    options_.began();
  }
  request_->set(request_->get() + 1);
  return true;
}

auto SearchField::Content::edit(int request, runtime::TextInput input,
                                provider::TextInputRequest details,
                                composition::Binding<std::string> text,
                                composition::Submission submit)
    -> async::Task<void> {
  if (request == 0) {
    co_return;
  }
  const std::string before = details.initial;
  runtime::TextInput::Draft drafted{
      [text](const std::string& draft) { text.set(draft); }};
  const runtime::TextInput::Result result =
      co_await input.request(std::move(details), std::move(drafted));
  if (result.has_value()) {
    text.set(*result);
    submit();
    co_return;
  }
  text.set(before);
}

}
