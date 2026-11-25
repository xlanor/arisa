#pragma once

#include <arisa/components/attributed_string.hpp>
#include <arisa/components/attributed_text_formatting.hpp>
#include <arisa/components/attributed_text_selection.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/key_press.hpp>
#include <arisa/composition/move_command_direction.hpp>
#include <arisa/composition/pasteboard.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/submission.hpp>
#include <arisa/composition/text_input_environment.hpp>
#include <arisa/composition/text_selection.hpp>
#include <arisa/controls/text_editing.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace arisa::controls {

class TextEditingSession {
 public:
  struct Rich {
    composition::Binding<components::AttributedString> text;
    composition::Binding<components::AttributedTextSelection> selection;
    std::shared_ptr<const components::AttributedTextFormattingDefinition>
        definition;
  };

  struct Binds {
    composition::Binding<std::string> text;
    composition::Binding<std::optional<composition::TextSelection>> selection;
    composition::Binding<bool> editing;
    composition::Binding<bool> menu;
    bool multiline{false};
    bool secure{false};
    bool enabled{true};
    bool focused{false};
    std::optional<composition::Submission> submit;
    std::optional<composition::Pasteboard> pasteboard;
    std::optional<Rich> rich;
    std::optional<composition::Binding<bool>> find;
    composition::TextInputEnvironment::FormattingControls controls{};
  };

  enum class Action : std::uint8_t {
    select,
    select_all,
    deselect,
    cut,
    copy,
    paste,
    bold,
    italic,
    underline,
    strikethrough,
  };

  static const composition::StateKey<std::optional<composition::TextSelection>>
      selection;
  static const composition::StateKey<bool> editing;
  static const composition::StateKey<bool> menu;
  static const composition::StateKey<components::AttributedTextSelection>
      rich_selection;

  static constexpr std::string_view select_label = "Select";
  static constexpr std::string_view select_all_label = "Select All";
  static constexpr std::string_view deselect_label = "Deselect";
  static constexpr std::string_view cut_label = "Cut";
  static constexpr std::string_view copy_label = "Copy";
  static constexpr std::string_view paste_label = "Paste";
  static constexpr std::string_view bold_label = "Bold";
  static constexpr std::string_view italic_label = "Italic";
  static constexpr std::string_view underline_label = "Underline";
  static constexpr std::string_view strikethrough_label = "Strikethrough";

  void bind(Binds binds);

  auto bound() const noexcept -> bool { return binds_.has_value(); }

  auto active() const -> bool;

  auto press(const composition::KeyPress& press) -> bool;

  auto steer(composition::MoveCommandDirection direction) -> bool;

  void perform(Action action);

  void open_menu();

  auto model() const -> TextEditing;

  void commit(const TextEditing& edited);

  auto shown_range(std::string_view shown) const
      -> composition::TextSelection::Range;

  static auto menu_items(const std::shared_ptr<TextEditingSession>& session)
      -> composition::Element;

  static auto formatting_bar(const std::shared_ptr<TextEditingSession>& session)
      -> composition::Element;

  auto shows_formatting_bar() const -> bool;

  static auto decorate(const std::shared_ptr<TextEditingSession>& session,
                       composition::Element shown, bool prompt)
      -> composition::Element;

 private:
  auto real_index(std::size_t shown) const -> std::size_t;

  auto shown_index(std::size_t real) const -> std::size_t;

  void touch(std::size_t from, std::size_t to);

  void format(Action action);

  void copy_selection(bool cut);

  void paste();

  std::optional<Binds> binds_;
};

}
