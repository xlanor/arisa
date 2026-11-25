#include <arisa/components/button_style.hpp>
#include <arisa/components/command_mapping.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/labeled_content.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/text_layout_key.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/command.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/enablement.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/preference_reader.hpp>
#include <arisa/composition/scroll_environment.hpp>
#include <arisa/composition/text_input_configuration.hpp>
#include <arisa/composition/text_input_environment.hpp>
#include <arisa/controls/find_navigator.hpp>
#include <arisa/controls/text_editor.hpp>
#include <arisa/controls/text_editor_style.hpp>
#include <arisa/controls/text_field.hpp>
#include <arisa/controls/text_field_style.hpp>
#include <arisa/controls/text_selection_host.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/theme/shape.hpp>

#include <cstdint>
#include <memory>
#include <utility>

namespace arisa::controls {

TextField::TextField(std::string title, composition::Binding<std::string> text,
                     TextFieldOptions options)
    : TextField(std::move(title), std::move(text), std::move(options),
                Kind::field, provider::KeyboardType::automatic) {}

TextField::TextField(std::string title, composition::Binding<std::string> text,
                     Selection selection, TextFieldOptions options)
    : TextField(std::move(title), std::move(text), std::move(options),
                Kind::field, provider::KeyboardType::automatic, Localizer{},
                nullptr, Editing{.selection = std::move(selection)}) {}

TextField::TextField(composition::Binding<std::string> text,
                     TextFieldOptions options, composition::Element label)
    : TextField(std::string{}, std::move(text), std::move(options), Kind::field,
                provider::KeyboardType::automatic, Localizer{},
                source(std::move(label))) {}

TextField::TextField(composition::Binding<std::string> text,
                     Selection selection, TextFieldOptions options,
                     composition::Element label)
    : TextField(std::string{}, std::move(text), std::move(options), Kind::field,
                provider::KeyboardType::automatic, Localizer{},
                source(std::move(label)),
                Editing{.selection = std::move(selection)}) {}

auto TextField::source(composition::Element label) -> Source {
  return std::make_shared<composition::PlaceholderContentView::Source>(
      std::move(label));
}

TextField::TextField(std::string title, composition::Binding<std::string> text,
                     TextFieldOptions options, Kind kind,
                     provider::KeyboardType keyboard, Localizer localizer,
                     Source label, Editing editing)
    : Element(std::make_unique<Content>(
          std::move(title), std::move(text), std::move(options), kind, keyboard,
          std::move(localizer), std::move(label), std::move(editing),
          std::make_shared<TextEditingSession>())) {
  declared().focus_shape = theme::Shape::rounded(theme::radius::control, true);
  const std::shared_ptr<TextEditingSession> session = edit<Content>().session();
  declared().commands.push_back(
      std::make_shared<composition::Command>(composition::Command::button(
          components::CommandMapping::options,
          std::string{components::CommandMapping::options_label},
          [session] { session->open_menu(); }, composition::CommandOptions{})));
}

auto TextField::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto TextField::Content::multiline() const noexcept -> bool {
  return options_.axis == composition::Axis::vertical || kind_ == Kind::editor;
}

auto TextField::Content::shown(const std::string& value) const -> std::string {
  if (kind_ != Kind::secure) {
    return value;
  }
  std::string hidden;
  for (const char byte : value) {
    if ((static_cast<unsigned char>(byte) & 0xC0U) != 0x80U) {
      hidden.append(bullet);
    }
  }
  return hidden;
}

auto TextField::Content::request(composition::Context& context,
                                 const std::string& value) const
    -> provider::TextInputRequest {
  return provider::TextInputRequest{
      .initial = value,
      .hint = options_.prompt.value_or(title_),
      .max_length = provider::TextInputLimit::of<256>(),
      .kind = kind_ == Kind::secure ? provider::TextInputKind::password
                                    : provider::TextInputKind::text,
      .traits = composition::TextInputConfiguration::traits(
          context, keyboard_, options_.axis == composition::Axis::vertical)};
}

auto TextField::Content::presented(composition::Context& context,
                                   const std::string& value) const
    -> composition::Element {
  const TextFieldStyle& style = context.environment(TextFieldStyle::key);
  const bool empty = value.empty();
  components::Text text{empty ? options_.prompt.value_or(title_)
                              : shown(value)};
  if (options_.axis != composition::Axis::vertical) {
    text = std::move(text).line_limit(1);
  }
  composition::Element label =
      empty
          ? std::move(text).foreground_style(theme::ColourRole::text_secondary)
          : composition::Element{std::move(text)};
  label = TextEditingSession::decorate(session_, std::move(label), empty);
  return style.make_body(TextFieldStyle::Configuration{
      .label = std::move(label),
      .is_empty = empty,
      .is_pressed = context.is_pressed(),
      .is_focused = context.is_focused(),
      .is_enabled = context.environment(composition::Enablement::is_enabled),
      .control_size =
          context.environment(components::Environment::control_size),
      .border_shape = context.environment(TextInputBorderShape::key)});
}

auto TextField::Content::editor(composition::Context& context,
                                const std::string& value) const
    -> composition::Element {
  composition::Element shown =
      editing_.rich.has_value()
          ? composition::Element{components::Text(editing_.rich->get())}
          : composition::Element{components::Text(value)};
  shown = TextEditingSession::decorate(session_, std::move(shown), false);
  composition::Element body =
      components::VStack(
          components::VStackOptions{
              .alignment = components::HorizontalAlignment::leading},
          std::move(shown))
          .padding(theme::Insets{.top = TextEditor::text_inset_top,
                                 .leading = TextEditor::text_inset_leading,
                                 .bottom = TextEditor::text_inset_top,
                                 .trailing = TextEditor::text_inset_leading})
          .frame({.max_width = composition::infinity,
                  .min_height = TextEditor::minimum_height,
                  .max_height = composition::infinity,
                  .alignment = composition::Alignment::top_leading});
  body = context.environment(TextEditorStyle::key)
             .make_body(TextEditorStyle::Configuration{
                 .content = std::move(body),
                 .shows_background =
                     context.environment(
                         composition::ScrollEnvironment::content_background) !=
                     composition::Visibility::hidden});
  if (!context.environment(composition::Enablement::is_enabled)) {
    body = std::move(body).opacity(components::ButtonStyle::disabled_opacity);
  }
  body = found(context, std::move(body), value);
  if (session_->shows_formatting_bar()) {
    body = components::VStack(
        components::VStackOptions{.alignment =
                                      components::HorizontalAlignment::leading},
        TextEditingSession::formatting_bar(session_), std::move(body));
  }
  return body;
}

auto TextField::Content::found(composition::Context& context,
                               composition::Element body,
                               const std::string& value) const
    -> composition::Element {
  const std::optional<composition::FindContext> find =
      context.environment(composition::TextInputEnvironment::find_context);
  if (!find.has_value() || !find->is_presented.has_value() ||
      context.environment(composition::TextInputEnvironment::find_disabled)) {
    return body;
  }
  const bool replaces =
      !context.environment(composition::TextInputEnvironment::replace_disabled);
  body = std::move(body).environment(
      composition::TextInputEnvironment::find_context,
      composition::FindContext{.is_presented = find->is_presented,
                               .supports_replace = replaces});
  if (!find->is_presented->get()) {
    return body;
  }
  const composition::StateHandle<std::string> query = context.state(find_query);
  const std::vector<composition::TextSelection::Range> matches =
      FindNavigator::matches(value, query.get());
  const theme::ColourValue highlight{theme::ColourRole::find_highlight};
  for (const composition::TextSelection::Range& match : matches) {
    body = std::move(body).background_preference_value(
        components::Text::LayoutKey::key,
        [match, highlight](const components::Text::LayoutKey::Value& layouts) {
          return TextSelectionHost::marks(layouts, 0, match, highlight);
        });
  }
  return components::VStack(
      components::VStackOptions{.alignment =
                                    components::HorizontalAlignment::leading},
      FindNavigator(session_, composition::Binding<std::string>{query},
                    *find->is_presented, replaces),
      std::move(body));
}

auto TextField::Content::bind(composition::Context& context,
                              const composition::Binding<std::string>& bound)
    const -> composition::Binding<std::optional<composition::TextSelection>> {
  const composition::Binding<std::optional<composition::TextSelection>>
      selection =
          editing_.selection.has_value()
              ? *editing_.selection
              : composition::Binding<std::optional<composition::TextSelection>>{
                    context.state(TextEditingSession::selection)};
  std::optional<TextEditingSession::Rich> rich;
  if (editing_.rich.has_value()) {
    rich = TextEditingSession::Rich{
        .text = *editing_.rich,
        .selection =
            editing_.rich_selection.has_value()
                ? *editing_.rich_selection
                : composition::Binding<
                      components::AttributedTextSelection>{context.state(
                      TextEditingSession::rich_selection)},
        .definition = context.environment(
            components::AttributedTextFormatting::definition)};
  }
  const std::optional<composition::FindContext> find =
      context.environment(composition::TextInputEnvironment::find_context);
  const bool finds =
      find.has_value() &&
      !context.environment(composition::TextInputEnvironment::find_disabled);
  session_->bind(TextEditingSession::Binds{
      .text = bound,
      .selection = selection,
      .editing = composition::Binding<bool>{context.state(
          TextEditingSession::editing)},
      .menu =
          composition::Binding<bool>{context.state(TextEditingSession::menu)},
      .multiline = multiline(),
      .secure = kind_ == Kind::secure,
      .enabled = context.environment(composition::Enablement::is_enabled),
      .focused = context.is_focused(),
      .submit = composition::Submission::collect(context),
      .pasteboard = context.environment(components::Environment::pasteboard),
      .rich = std::move(rich),
      .find = finds ? find->is_presented : std::nullopt,
      .controls = context.environment(
          composition::TextInputEnvironment::formatting_controls)});
  return selection;
}

auto TextField::Content::suggested(
    composition::Context& context, composition::Element body,
    const composition::Binding<std::string>& bound) const
    -> composition::Element {
  if (const std::shared_ptr<composition::TextInputEnvironment::Completion>&
          completion =
              context.environment(composition::TextInputEnvironment::completer);
      completion != nullptr && kind_ == Kind::field) {
    completion->set([bound, session = session_](const std::string& chosen) {
      bound.set(chosen);
      TextEditing edited = session->model();
      edited.place(chosen.size());
      session->commit(edited);
    });
  }
  return body;
}

auto TextField::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  const auto pending = context.state(requests);
  request_ = pending;
  const auto showing = context.state(presenting);
  presenting_ = showing;
  const composition::Binding<std::string> bound =
      localizer_
          ? localizer_(context.environment(components::Environment::locale))
          : text_;
  const composition::Binding<std::optional<composition::TextSelection>>
      selection = bind(context, bound);
  const std::string value = bound.get();
  composition::Element body = kind_ == Kind::editor ? editor(context, value)
                                                    : presented(context, value);
  provider::TextInputRequest details = request(context, value);
  const std::optional<runtime::TextInput>& input =
      context.environment(components::Environment::text_input);
  notes_.clear();
  if (input.has_value()) {
    const provider::TextInputTraitSet refused = input->unhonoured(details);
    for (std::uint8_t index = 0; index < provider::TextInputTraitSet::count;
         ++index) {
      const auto trait = static_cast<provider::TextInputTrait>(index);
      if (refused.contains(trait)) {
        notes_.emplace_back(provider::TextInputTraitSet::name(trait));
      }
    }
  }
  if (pending.get() > 0 && input.has_value()) {
    body = std::move(body).task(
        composition::ElementKey{pending.get()},
        [request = pending.get(), input = *input, details = std::move(details),
         text = bound, submit = composition::Submission::collect(context),
         showing, selection]() mutable {
          return edit(request, input, std::move(details), text,
                      std::move(submit), showing, selection);
        });
  }
  body = suggested(context, std::move(body), bound);
  if (const composition::StateHandle<bool> menu =
          context.state(TextEditingSession::menu);
      menu.get()) {
    body = std::move(body).popover(
        composition::Binding<bool>{menu}, [session = session_] {
          return TextEditingSession::menu_items(session);
        });
  }
  if (showing.get() || session_->active()) {
    body = std::move(body).preference(
        composition::TextInputEnvironment::keyboard, true);
  }
  if (label_ != nullptr) {
    if (const std::optional<components::LabelColumn> column =
            components::LabeledContent::column(context)) {
      body = components::LabeledContent::columned(
          *column, composition::PlaceholderContentView(label_),
          std::move(body));
    }
  }
  std::vector<composition::Element> produced;
  produced.push_back(std::move(body));
  return produced;
}

auto TextField::Content::activate() -> bool {
  if (!request_.has_value()) {
    return false;
  }
  if (presenting_.has_value()) {
    presenting_->set(true);
  }
  request_->set(request_->get() + 1);
  return true;
}

auto TextField::Content::press(composition::Context&, const composition::Node&,
                               const composition::KeyPress& press) -> bool {
  return session_->press(press);
}

auto TextField::Content::steer(composition::Context&, const composition::Node&,
                               composition::MoveCommandDirection direction)
    -> bool {
  return session_->steer(direction);
}

auto TextField::Content::edit(
    int request, runtime::TextInput input, provider::TextInputRequest details,
    composition::Binding<std::string> text, composition::Submission submit,
    composition::StateHandle<bool> presented,
    composition::Binding<std::optional<composition::TextSelection>> selection)
    -> async::Task<void> {
  if (request == 0) {
    co_return;
  }
  const runtime::TextInput::Result result =
      co_await input.request(std::move(details));
  presented.set(false);
  if (result.has_value()) {
    text.set(*result);
    selection.set(composition::TextSelection{
        composition::TextSelection::InsertionPoint{.index = result->size()}});
    submit();
  }
}

}
