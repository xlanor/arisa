#pragma once

#include <arisa/async/task.hpp>
#include <arisa/composition/axis.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/move_command_direction.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/submission.hpp>
#include <arisa/composition/text_selection.hpp>
#include <arisa/controls/number_format.hpp>
#include <arisa/controls/text_editing_session.hpp>
#include <arisa/controls/text_value_format.hpp>
#include <arisa/provider/text_input/provider.hpp>
#include <arisa/runtime/text_input.hpp>
#include <arisa/text/locale.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::controls {

struct TextFieldOptions {
  std::optional<std::string> prompt{};
  composition::Axis axis{composition::Axis::horizontal};
};

struct TextFieldEditing {
  std::optional<composition::Binding<std::optional<composition::TextSelection>>>
      selection{};
  std::optional<composition::Binding<components::AttributedString>> rich{};
  std::optional<composition::Binding<components::AttributedTextSelection>>
      rich_selection{};
};

class TextField : public composition::Element {
 public:
  class Content;

  using Selection =
      composition::Binding<std::optional<composition::TextSelection>>;

  static constexpr std::string_view bullet = "•";

  TextField(std::string title, composition::Binding<std::string> text,
            TextFieldOptions options = {});

  TextField(std::string title, composition::Binding<std::string> text,
            Selection selection, TextFieldOptions options = {});

  TextField(composition::Binding<std::string> text, TextFieldOptions options,
            composition::Element label);

  TextField(composition::Binding<std::string> text, Selection selection,
            TextFieldOptions options, composition::Element label);

  using Localizer =
      std::function<composition::Binding<std::string>(const text::Locale&)>;

  template <composition::BindingSource Source>
    requires FormattedNumber<composition::BoundValue<Source>>
  TextField(std::string title, Source value, NumberFormat format,
            TextFieldOptions options = {})
      : TextField(
            std::move(title),
            format.binding(
                composition::Binding<composition::BoundValue<Source>>{value}),
            std::move(options), Kind::field,
            NumberFormat::keyboard<composition::BoundValue<Source>>(),
            Localizer{
                [format,
                 bound = composition::Binding<composition::BoundValue<Source>>{
                     value}](const text::Locale& locale) {
                  return format.binding(bound, locale);
                }}) {}

  template <composition::BindingSource Source, class Format>
    requires(TextValueFormat::parses<composition::BoundValue<Source>, Format>())
  TextField(std::string title, Source value, Format format,
            TextFieldOptions options = {})
      : TextField(
            std::move(title),
            formatted(
                composition::Binding<composition::BoundValue<Source>>{value},
                format, text::Locale::undetermined()),
            std::move(options), Kind::field,
            TextValueFormat::keyboard<composition::BoundValue<Source>>(),
            localized(
                composition::Binding<composition::BoundValue<Source>>{value},
                std::move(format))) {}

  template <composition::BindingSource Source, class Format>
    requires(TextValueFormat::parses<composition::BoundValue<Source>, Format>())
  TextField(Source value, Format format, TextFieldOptions options,
            composition::Element label)
      : TextField(
            std::string{},
            formatted(
                composition::Binding<composition::BoundValue<Source>>{value},
                format, text::Locale::undetermined()),
            std::move(options), Kind::field,
            TextValueFormat::keyboard<composition::BoundValue<Source>>(),
            localized(
                composition::Binding<composition::BoundValue<Source>>{value},
                std::move(format)),
            source(std::move(label))) {}

  template <composition::BindingSource Source>
  TextField(std::string title, Source value,
            Formatter<composition::BoundValue<Source>> formatter,
            TextFieldOptions options = {})
      : TextField(
            std::move(title),
            TextValueFormat::binding(
                composition::Binding<composition::BoundValue<Source>>{value},
                std::move(formatter)),
            std::move(options), Kind::field,
            TextValueFormat::keyboard<composition::BoundValue<Source>>()) {}

  template <composition::BindingSource Source>
  TextField(Source value, Formatter<composition::BoundValue<Source>> formatter,
            TextFieldOptions options, composition::Element label)
      : TextField(
            std::string{},
            TextValueFormat::binding(
                composition::Binding<composition::BoundValue<Source>>{value},
                std::move(formatter)),
            std::move(options), Kind::field,
            TextValueFormat::keyboard<composition::BoundValue<Source>>(),
            Localizer{}, source(std::move(label))) {}

 protected:
  enum class Kind : std::uint8_t {
    field,
    secure,
    editor,
  };

  using Source = std::shared_ptr<composition::PlaceholderContentView::Source>;

  using Editing = TextFieldEditing;

  TextField(std::string title, composition::Binding<std::string> text,
            TextFieldOptions options, Kind kind,
            provider::KeyboardType keyboard, Localizer localizer = {},
            Source label = nullptr, Editing editing = {});

  static auto source(composition::Element label) -> Source;

  template <class Value, class Format>
  static auto formatted(composition::Binding<Value> value, Format format,
                        const text::Locale& locale)
      -> composition::Binding<std::string> {
    return TextValueFormat::binding(std::move(value), std::move(format),
                                    locale);
  }

  template <class Value, class Format>
  static auto localized(composition::Binding<Value> value, Format format)
      -> Localizer {
    return Localizer{[value = std::move(value),
                      format = std::move(format)](const text::Locale& locale) {
      return TextValueFormat::binding(value, format, locale);
    }};
  }
};

class TextField::Content final : public composition::ElementContent {
 public:
  static constexpr composition::StateKey<int> requests{"text-field-requests",
                                                       0};

  static constexpr composition::StateKey<bool> presenting{
      "text-field-presenting", false};
  static inline const composition::StateKey<std::string> find_query{
      "text-field-find-query", ""};

  Content(std::string title, composition::Binding<std::string> text,
          TextFieldOptions options, Kind kind, provider::KeyboardType keyboard,
          Localizer localizer, Source label, Editing editing,
          std::shared_ptr<TextEditingSession> session) noexcept
      : title_(std::move(title)),
        text_(std::move(text)),
        options_(std::move(options)),
        kind_(kind),
        keyboard_(keyboard),
        localizer_(std::move(localizer)),
        label_(std::move(label)),
        editing_(std::move(editing)),
        session_(std::move(session)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto framed() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

  auto activate() -> bool override;

  auto activatable() const noexcept -> bool override { return true; }

  auto press(composition::Context& context, const composition::Node& node,
             const composition::KeyPress& press) -> bool override;

  auto steer(composition::Context& context, const composition::Node& node,
             composition::MoveCommandDirection direction) -> bool override;

  auto notes() const -> std::vector<std::string> override { return notes_; }

  auto session() const noexcept -> const std::shared_ptr<TextEditingSession>& {
    return session_;
  }

  auto shown(const std::string& value) const -> std::string;

 private:
  static auto edit(
      int request, runtime::TextInput input, provider::TextInputRequest details,
      composition::Binding<std::string> text, composition::Submission submit,
      composition::StateHandle<bool> presented,
      composition::Binding<std::optional<composition::TextSelection>> selection)
      -> async::Task<void>;

  auto bind(composition::Context& context,
            const composition::Binding<std::string>& bound) const
      -> composition::Binding<std::optional<composition::TextSelection>>;

  auto found(composition::Context& context, composition::Element body,
             const std::string& value) const -> composition::Element;

  auto suggested(composition::Context& context, composition::Element body,
                 const composition::Binding<std::string>& bound) const
      -> composition::Element;

  auto request(composition::Context& context, const std::string& value) const
      -> provider::TextInputRequest;

  auto presented(composition::Context& context, const std::string& value) const
      -> composition::Element;

  auto editor(composition::Context& context, const std::string& value) const
      -> composition::Element;

  auto multiline() const noexcept -> bool;

  std::string title_;
  composition::Binding<std::string> text_;
  TextFieldOptions options_;
  Kind kind_;
  provider::KeyboardType keyboard_;
  Localizer localizer_;
  Source label_;
  Editing editing_;
  std::shared_ptr<TextEditingSession> session_;
  mutable std::optional<composition::StateHandle<int>> request_;
  mutable std::optional<composition::StateHandle<bool>> presenting_;
  mutable std::vector<std::string> notes_;
};

}
