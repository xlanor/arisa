#include <arisa/controls/text_editing_session.hpp>

#include <arisa/components/button.hpp>
#include <arisa/components/geometry_reader.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/shape_view.hpp>
#include <arisa/components/text_layout_key.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/drag_gesture.hpp>
#include <arisa/composition/event_modifiers.hpp>
#include <arisa/composition/preference_reader.hpp>
#include <arisa/composition/text_environment.hpp>
#include <arisa/composition/transfer_item.hpp>
#include <arisa/controls/text_selection_host.hpp>
#include <arisa/text/utf8.hpp>
#include <arisa/theme/shape.hpp>

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace arisa::controls {

const composition::StateKey<std::optional<composition::TextSelection>>
    TextEditingSession::selection{"text-editing-selection", std::nullopt};
const composition::StateKey<bool> TextEditingSession::editing{
    "text-editing-active", false};
const composition::StateKey<bool> TextEditingSession::menu{"text-editing-menu",
                                                           false};
const composition::StateKey<components::AttributedTextSelection>
    TextEditingSession::rich_selection{"text-editing-rich-selection",
                                       components::AttributedTextSelection{}};

void TextEditingSession::bind(Binds binds) { binds_ = std::move(binds); }

auto TextEditingSession::active() const -> bool {
  return binds_.has_value() && binds_->enabled && binds_->focused &&
         binds_->editing.get();
}

auto TextEditingSession::model() const -> TextEditing {
  if (!binds_.has_value()) {
    return TextEditing{std::string{}, composition::TextSelection{}, false};
  }
  if (binds_->rich.has_value()) {
    const components::AttributedString attributed = binds_->rich->text.get();
    return TextEditing{std::string{attributed.text()},
                       binds_->rich->selection.get().text_selection(attributed),
                       binds_->multiline};
  }
  std::string text = binds_->text.get();
  const std::size_t size = text.size();
  return TextEditing{
      std::move(text),
      binds_->selection.get().value_or(composition::TextSelection{
          composition::TextSelection::InsertionPoint{.index = size}}),
      binds_->multiline};
}

void TextEditingSession::commit(const TextEditing& edited) {
  if (!binds_.has_value()) {
    return;
  }
  if (binds_->rich.has_value()) {
    const Rich& rich = *binds_->rich;
    const components::AttributedString before = rich.text.get();
    const components::AttributedTextSelection held = rich.selection.get();
    const std::string_view old = before.text();
    const std::string& now = edited.text();
    components::AttributedString after = before;
    std::optional<components::AttributedString::Attributes> typing;
    if (old != now) {
      std::size_t prefix = 0;
      while (prefix < old.size() && prefix < now.size() &&
             old[prefix] == now[prefix]) {
        ++prefix;
      }
      std::size_t suffix = 0;
      while (suffix < old.size() - prefix && suffix < now.size() - prefix &&
             old[old.size() - 1 - suffix] == now[now.size() - 1 - suffix]) {
        ++suffix;
      }
      const components::AttributedString::Attributes attributes =
          held.typing_attributes(before);
      after = before.replacing(
          static_cast<std::uint32_t>(prefix),
          static_cast<std::uint32_t>(old.size() - suffix),
          components::AttributedString{
              now.substr(prefix, now.size() - suffix - prefix), attributes});
      if (rich.definition != nullptr) {
        after = components::AttributedTextFormatting::constrained(
            after, *rich.definition);
      }
      rich.text.set(after);
    } else if (edited.selection().is_insertion() &&
               held.indices(before).is_insertion_point() &&
               held.indices(before).insertion() ==
                   edited.selection().range().lower) {
      typing = held.typing_attributes(before);
    }
    rich.selection.set(
        components::AttributedTextSelection::of(edited.selection(), typing));
    return;
  }
  if (binds_->text.get() != edited.text()) {
    binds_->text.set(edited.text());
  }
  binds_->selection.set(edited.selection());
}

auto TextEditingSession::shown_index(std::size_t real) const -> std::size_t {
  if (!binds_.has_value() || !binds_->secure) {
    return real;
  }
  const std::string text = binds_->text.get();
  std::size_t count = 0;
  for (std::size_t at = 0; at < std::min(real, text.size());
       at = text::Utf8::next(text, at)) {
    ++count;
  }
  return count * std::string_view{"•"}.size();
}

auto TextEditingSession::real_index(std::size_t shown) const -> std::size_t {
  if (!binds_.has_value() || !binds_->secure) {
    return shown;
  }
  const std::string text = binds_->text.get();
  std::size_t wanted = shown / std::string_view{"•"}.size();
  std::size_t at = 0;
  while (wanted > 0 && at < text.size()) {
    at = text::Utf8::next(text, at);
    --wanted;
  }
  return at;
}

auto TextEditingSession::shown_range(std::string_view shown) const
    -> composition::TextSelection::Range {
  const composition::TextSelection::Range real = model().range();
  return composition::TextSelection::Range{.lower = shown_index(real.lower),
                                           .upper = shown_index(real.upper)}
      .clamped(shown.size());
}

void TextEditingSession::copy_selection(bool cut) {
  if (!binds_.has_value() || binds_->secure) {
    return;
  }
  TextEditing edited = model();
  const std::string chosen = edited.selected();
  if (chosen.empty()) {
    return;
  }
  if (binds_->pasteboard.has_value()) {
    binds_->pasteboard->set_items({composition::TransferItem::of(chosen)});
  }
  if (cut) {
    edited.insert("");
    commit(edited);
  }
}

void TextEditingSession::paste() {
  if (!binds_.has_value() || !binds_->pasteboard.has_value()) {
    return;
  }
  for (const composition::TransferItem& item : binds_->pasteboard->items()) {
    if (const std::optional<std::string> pasted = item.as<std::string>()) {
      TextEditing edited = model();
      edited.insert(*pasted);
      commit(edited);
      return;
    }
  }
}

void TextEditingSession::format(Action action) {
  if (!binds_.has_value() || !binds_->rich.has_value()) {
    return;
  }
  const Rich& rich = *binds_->rich;
  components::AttributedString text = rich.text.get();
  components::AttributedTextSelection chosen = rich.selection.get();
  const components::AttributedTextSelection::Attributes current =
      chosen.attributes(text);
  const auto all = [&current](const auto& test) {
    return std::ranges::all_of(current, test);
  };
  std::function<void(components::AttributedString::Attributes&)> change;
  switch (action) {
    case Action::bold: {
      const bool on = all([](const auto& held) {
        return held.weight.has_value() &&
               *held.weight >= theme::Font::Weight::semibold;
      });
      change = [on](components::AttributedString::Attributes& held) {
        held.weight =
            on ? std::nullopt : std::optional{theme::Font::Weight::bold};
      };
      break;
    }
    case Action::italic: {
      const bool on = all([](const auto& held) { return held.italic; });
      change = [on](components::AttributedString::Attributes& held) {
        held.italic = !on;
      };
      break;
    }
    case Action::underline: {
      const bool on = all([](const auto& held) { return held.underline; });
      change = [on](components::AttributedString::Attributes& held) {
        held.underline = !on;
      };
      break;
    }
    case Action::strikethrough: {
      const bool on = all([](const auto& held) { return held.strikethrough; });
      change = [on](components::AttributedString::Attributes& held) {
        held.strikethrough = !on;
      };
      break;
    }
    default:
      return;
  }
  const std::shared_ptr<const components::AttributedTextFormattingDefinition>
      definition = rich.definition;
  text.transform_attributes(
      chosen,
      [&change, &definition](components::AttributedString::Attributes& held) {
        change(held);
        if (definition != nullptr) {
          components::AttributedTextFormatting::AttributeContainerProxy proxy{
              held};
          definition->constrain(proxy);
        }
      });
  rich.text.set(std::move(text));
  rich.selection.set(std::move(chosen));
  binds_->editing.set(true);
}

void TextEditingSession::perform(Action action) {
  if (!binds_.has_value()) {
    return;
  }
  binds_->menu.set(false);
  TextEditing edited = model();
  switch (action) {
    case Action::select:
      edited.select_word();
      break;
    case Action::select_all:
      edited.select_all();
      break;
    case Action::deselect:
      edited.place(edited.range().upper);
      break;
    case Action::cut:
      copy_selection(true);
      binds_->editing.set(true);
      return;
    case Action::copy:
      copy_selection(false);
      return;
    case Action::paste:
      paste();
      binds_->editing.set(true);
      return;
    case Action::bold:
    case Action::italic:
    case Action::underline:
    case Action::strikethrough:
      format(action);
      return;
  }
  binds_->editing.set(true);
  commit(edited);
}

void TextEditingSession::open_menu() {
  if (binds_.has_value() && binds_->enabled) {
    binds_->menu.set(true);
  }
}

auto TextEditingSession::press(const composition::KeyPress& press) -> bool {
  if (!binds_.has_value() || !binds_->enabled) {
    return false;
  }
  const bool was = binds_->editing.get();
  TextEditing edited = model();
  const TextEditing::Outcome outcome = edited.apply(press);
  switch (outcome) {
    case TextEditing::Outcome::ignored:
      return false;
    case TextEditing::Outcome::moved:
      if (!was &&
          !press.modifiers.contains(composition::EventModifiers::command) &&
          !press.modifiers.contains(composition::EventModifiers::control)) {
        return false;
      }
      binds_->editing.set(true);
      commit(edited);
      return true;
    case TextEditing::Outcome::edited:
      binds_->editing.set(true);
      commit(edited);
      return true;
    case TextEditing::Outcome::copy:
      copy_selection(false);
      return true;
    case TextEditing::Outcome::cut:
      copy_selection(true);
      binds_->editing.set(true);
      return true;
    case TextEditing::Outcome::paste:
      paste();
      binds_->editing.set(true);
      return true;
    case TextEditing::Outcome::submit:
      if (!was) {
        return false;
      }
      binds_->editing.set(false);
      if (binds_->submit.has_value()) {
        (*binds_->submit)();
      }
      return true;
    case TextEditing::Outcome::cancel:
      if (!was) {
        return false;
      }
      binds_->editing.set(false);
      return true;
    case TextEditing::Outcome::bold:
      format(Action::bold);
      return binds_->rich.has_value();
    case TextEditing::Outcome::italic:
      format(Action::italic);
      return binds_->rich.has_value();
    case TextEditing::Outcome::underline:
      format(Action::underline);
      return binds_->rich.has_value();
    case TextEditing::Outcome::find:
      if (binds_->find.has_value()) {
        binds_->find->set(!binds_->find->get());
        return true;
      }
      return false;
  }
  return false;
}

auto TextEditingSession::steer(composition::MoveCommandDirection direction)
    -> bool {
  if (!active()) {
    return false;
  }
  TextEditing edited = model();
  const composition::TextSelection before = edited.selection();
  const bool extend = !before.is_insertion();
  switch (direction) {
    case composition::MoveCommandDirection::left:
      edited.move(TextEditing::Direction::backward,
                  TextEditing::Motion::character, extend);
      break;
    case composition::MoveCommandDirection::right:
      edited.move(TextEditing::Direction::forward,
                  TextEditing::Motion::character, extend);
      break;
    case composition::MoveCommandDirection::up:
    case composition::MoveCommandDirection::down:
      if (!binds_->multiline) {
        return false;
      }
      edited.move(direction == composition::MoveCommandDirection::up
                      ? TextEditing::Direction::up
                      : TextEditing::Direction::down,
                  TextEditing::Motion::line, extend);
      break;
  }
  if (edited.selection().range() == before.range()) {
    return false;
  }
  commit(edited);
  return true;
}

void TextEditingSession::touch(std::size_t from, std::size_t to) {
  if (!binds_.has_value()) {
    return;
  }
  TextEditing edited = model();
  edited.place(real_index(from));
  if (to != from) {
    edited.extend_to(real_index(to));
  }
  binds_->editing.set(true);
  commit(edited);
}

auto TextEditingSession::shows_formatting_bar() const -> bool {
  return binds_.has_value() && binds_->rich.has_value() && active() &&
         binds_->controls.accessory_bar != composition::Visibility::hidden &&
         binds_->controls.input_assistant != composition::Visibility::hidden;
}

auto TextEditingSession::formatting_bar(
    const std::shared_ptr<TextEditingSession>& session)
    -> composition::Element {
  std::vector<composition::Element> buttons;
  for (const auto& [action, label] :
       {std::pair{Action::bold, bold_label},
        std::pair{Action::italic, italic_label},
        std::pair{Action::underline, underline_label},
        std::pair{Action::strikethrough, strikethrough_label}}) {
    buttons.push_back(components::Button(
        std::string{label}, [session, action] { session->perform(action); }));
  }
  return components::HStack(components::HStackOptions{}, std::move(buttons))
      .id("formatting-bar");
}

auto TextEditingSession::menu_items(
    const std::shared_ptr<TextEditingSession>& session)
    -> composition::Element {
  std::vector<composition::Element> items;
  const TextEditing edited = session->model();
  const bool range = !edited.range().empty();
  const std::optional<Binds>& binds = session->binds_;
  const bool secure = binds.has_value() && binds->secure;
  const auto add = [&items, &session](std::string_view label, Action action) {
    items.push_back(components::Button(
        std::string{label}, [session, action] { session->perform(action); }));
  };
  if (range) {
    if (!secure) {
      add(cut_label, Action::cut);
      add(copy_label, Action::copy);
    }
    add(paste_label, Action::paste);
    add(deselect_label, Action::deselect);
  } else {
    if (!edited.text().empty()) {
      add(select_label, Action::select);
      add(select_all_label, Action::select_all);
    }
    add(paste_label, Action::paste);
  }
  if (binds.has_value() && binds->rich.has_value() &&
      binds->controls.context_menu != composition::Visibility::hidden) {
    add(bold_label, Action::bold);
    add(italic_label, Action::italic);
    add(underline_label, Action::underline);
    add(strikethrough_label, Action::strikethrough);
  }
  return components::VStack(components::VStackOptions{}, std::move(items))
      .id("edit-menu");
}

auto TextEditingSession::decorate(
    const std::shared_ptr<TextEditingSession>& session,
    composition::Element shown, bool prompt) -> composition::Element {
  composition::Element published = std::move(shown);
  if (!session->active()) {
    return published;
  }
  const theme::ColourValue highlight{
      theme::ColourRole::selected_text_background};
  const theme::ColourValue accent{theme::ColourRole::accent};
  const TextEditing edited = session->model();
  const composition::TextSelection::Range real = edited.range();
  const composition::TextSelection::Range range =
      prompt ? composition::TextSelection::Range{}
             : composition::TextSelection::Range{
                   .lower = session->shown_index(real.lower),
                   .upper = session->shown_index(real.upper)};
  if (!range.empty()) {
    published = std::move(published).background_preference_value(
        components::Text::LayoutKey::key,
        [range, highlight](const components::Text::LayoutKey::Value& layouts) {
          return TextSelectionHost::marks(layouts, 0, range, highlight);
        });
  }
  return std::move(published).overlay_preference_value(
      components::Text::LayoutKey::key,
      [session, range,
       accent](const components::Text::LayoutKey::Value& layouts)
          -> composition::Element {
        composition::Element surface = components::GeometryReader(
            [session, layouts](const composition::GeometryProxy& proxy) {
              const std::vector<TextSelectionHost::Placed> where =
                  TextSelectionHost::placed(layouts, proxy);
              const auto index = [where](composition::Point at) {
                const std::optional<TextSelectionHost::Selected> hit =
                    TextSelectionHost::index_at(where, at);
                return hit.has_value() ? hit->range.lower : 0;
              };
              return composition::Element{
                  components::ShapeView(theme::Shape::rectangle())
                      .fill(theme::Colour{0.0F, 0.0F, 0.0F, 0.0F})
                      .content_shape(theme::Shape::rectangle())
                      .gesture(composition::DragGesture(0.0F).on_changed(
                          [session, index](
                              const composition::DragGesture::Value& value) {
                            session->touch(index(value.start_location),
                                           index(value.location));
                          }))
                      .on_long_press_gesture([session] {
                        session->perform(Action::select);
                        session->open_menu();
                      })};
            });
        std::vector<composition::Element> layers;
        layers.push_back(std::move(surface));
        if (range.empty()) {
          layers.push_back(
              TextSelectionHost::caret(layouts, 0, range.lower, accent));
        } else {
          layers.push_back(TextSelectionHost::handles(
              layouts, 0, range, accent,
              [session, range](TextSelectionHost::End end, std::size_t index) {
                if (end == TextSelectionHost::End::lower) {
                  session->touch(range.upper, index);
                } else {
                  session->touch(range.lower, index);
                }
              }));
        }
        return components::ZStack(
            components::ZStackOptions{.alignment =
                                          composition::Alignment::top_leading},
            std::move(layers));
      });
}

}
