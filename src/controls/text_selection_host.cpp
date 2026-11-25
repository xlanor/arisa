#include <arisa/controls/text_selection_host.hpp>

#include <arisa/components/button.hpp>
#include <arisa/components/command_mapping.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/geometry_reader.hpp>
#include <arisa/components/shape_view.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/coordinate_space.hpp>
#include <arisa/composition/drag_gesture.hpp>
#include <arisa/composition/event_modifiers.hpp>
#include <arisa/composition/key_equivalent.hpp>
#include <arisa/composition/pasteboard.hpp>
#include <arisa/composition/preference_observer.hpp>
#include <arisa/composition/preference_reader.hpp>
#include <arisa/composition/transfer_item.hpp>
#include <arisa/controls/text_editing.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/text/utf8.hpp>
#include <arisa/theme/shape.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace arisa::controls {

const composition::StateKey<std::optional<TextSelectionHost::Selected>>
    TextSelectionHost::Content::selection{"text-selection-host", std::nullopt};
const composition::StateKey<bool> TextSelectionHost::Content::menu{
    "text-selection-menu", false};
const composition::StateKey<components::Text::LayoutKey::Value>
    TextSelectionHost::Content::seen{"text-selection-layouts", {}};

TextSelectionHost::TextSelectionHost(composition::Element content)
    : Element(std::make_unique<Content>(std::move(content))) {}

TextSelectionHost::Content::Content(composition::Element content)
    : content_(std::make_shared<composition::PlaceholderContentView::Source>(
          std::move(content))) {}

auto TextSelectionHost::Content::type() const noexcept
    -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto TextSelectionHost::placed(
    const components::Text::LayoutKey::Value& layouts,
    const composition::GeometryProxy& proxy) -> std::vector<Placed> {
  std::vector<Placed> found;
  found.reserve(layouts.size());
  for (const components::Text::LayoutKey::AnchoredLayout& anchored : layouts) {
    found.push_back(
        Placed{.origin = proxy[anchored.origin], .layout = anchored.layout});
  }
  return found;
}

auto TextSelectionHost::index_at(const std::vector<Placed>& placed,
                                 composition::Point at)
    -> std::optional<Selected> {
  std::optional<Selected> best;
  float nearest = std::numeric_limits<float>::max();
  for (std::size_t which = 0; which < placed.size(); ++which) {
    const Placed& each = placed[which];
    const composition::Size extent = each.layout.extent();
    const float dx = std::max(
        {each.origin.x - at.x, 0.0F, at.x - (each.origin.x + extent.width)});
    const float dy = std::max(
        {each.origin.y - at.y, 0.0F, at.y - (each.origin.y + extent.height)});
    const float distance = std::hypot(dx, dy);
    if (distance < nearest) {
      nearest = distance;
      const std::size_t index =
          each.layout
              .character_at(composition::Point{.x = at.x - each.origin.x,
                                               .y = at.y - each.origin.y})
              .value();
      best = Selected{.layout = which,
                      .range = Range{.lower = index, .upper = index}};
    }
  }
  return best;
}

auto TextSelectionHost::marks(const components::Text::LayoutKey::Value& layouts,
                              std::size_t which, Range range,
                              theme::ColourValue colour)
    -> composition::Element {
  return components::GeometryReader(
      [layouts, which, range, colour](const composition::GeometryProxy& proxy) {
        std::vector<composition::Element> rects;
        if (which < layouts.size()) {
          const composition::Point origin = proxy[layouts[which].origin];
          for (const composition::Rect& rect :
               layouts[which].layout.rects(range)) {
            rects.push_back(
                components::ShapeView(theme::Shape::rectangle())
                    .fill(colour)
                    .frame({.width = rect.width, .height = rect.height})
                    .offset(origin.x + rect.x, origin.y + rect.y));
          }
        }
        return composition::Element{
            components::ZStack(
                components::ZStackOptions{
                    .alignment = composition::Alignment::top_leading},
                std::move(rects))
                .frame({.max_width = composition::infinity,
                        .max_height = composition::infinity,
                        .alignment = composition::Alignment::top_leading})
                .allows_hit_testing(false)};
      });
}

auto TextSelectionHost::caret(const components::Text::LayoutKey::Value& layouts,
                              std::size_t which, std::size_t index,
                              theme::ColourValue colour)
    -> composition::Element {
  return components::GeometryReader(
      [layouts, which, index, colour](const composition::GeometryProxy& proxy) {
        std::vector<composition::Element> bars;
        if (which < layouts.size()) {
          const composition::Point origin = proxy[layouts[which].origin];
          const composition::Rect rect = layouts[which].layout.caret(
              components::Text::Layout::CharacterIndex{index});
          bars.push_back(
              components::ShapeView(theme::Shape::rectangle())
                  .fill(colour)
                  .frame({.width = rect.width, .height = rect.height})
                  .offset(origin.x + rect.x, origin.y + rect.y)
                  .id("text-caret"));
        }
        return composition::Element{
            components::ZStack(
                components::ZStackOptions{
                    .alignment = composition::Alignment::top_leading},
                std::move(bars))
                .frame({.max_width = composition::infinity,
                        .max_height = composition::infinity,
                        .alignment = composition::Alignment::top_leading})
                .allows_hit_testing(false)};
      });
}

auto TextSelectionHost::handles(
    const components::Text::LayoutKey::Value& layouts, std::size_t which,
    Range range, theme::ColourValue colour, Moved moved)
    -> composition::Element {
  return components::GeometryReader(
      [layouts, which, range, colour,
       moved = std::move(moved)](const composition::GeometryProxy& proxy) {
        std::vector<composition::Element> knobs;
        const std::vector<Placed> where = placed(layouts, proxy);
        if (which < where.size() && !range.empty()) {
          const Placed& target = where[which];
          for (const End end : {End::lower, End::upper}) {
            const std::size_t at =
                end == End::lower ? range.lower : range.upper;
            const composition::Rect bar = target.layout.caret(
                components::Text::Layout::CharacterIndex{at});
            const float x = target.origin.x + bar.x + (bar.width * 0.5F);
            const float y = end == End::lower
                                ? target.origin.y + bar.y - handle_size
                                : target.origin.y + bar.y + bar.height;
            knobs.push_back(
                components::ShapeView(theme::Shape::capsule())
                    .fill(colour)
                    .frame({.width = handle_size, .height = handle_size})
                    .content_shape(theme::Shape::rectangle())
                    .gesture(composition::DragGesture(0.0F).on_changed(
                        [moved, end, target, x,
                         y](const composition::DragGesture::Value& value) {
                          const composition::Point point{
                              .x = x + value.location.x -
                                   value.start_location.x - target.origin.x,
                              .y = (end == End::lower ? y + handle_size : y) +
                                   value.location.y - value.start_location.y -
                                   target.origin.y - 1.0F};
                          moved(end, target.layout.character_at(point).value());
                        }))
                    .offset(x - (handle_size * 0.5F), y)
                    .id(end == End::lower ? "selection-start"
                                          : "selection-end"));
          }
        }
        return composition::Element{
            components::ZStack(
                components::ZStackOptions{
                    .alignment = composition::Alignment::top_leading},
                std::move(knobs))
                .frame({.max_width = composition::infinity,
                        .max_height = composition::infinity,
                        .alignment = composition::Alignment::top_leading})};
      });
}

auto TextSelectionHost::Content::selected_text(
    const components::Text::LayoutKey::Value& layouts,
    const std::optional<Selected>& selected) -> std::string {
  if (!selected.has_value() || selected->layout >= layouts.size()) {
    return {};
  }
  const std::string_view text = layouts[selected->layout].layout.text();
  const Range range = selected->range.clamped(text.size());
  return std::string{text.substr(range.lower, range.length())};
}

auto TextSelectionHost::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  const composition::StateHandle<std::optional<Selected>> selected =
      context.state(selection);
  const composition::StateHandle<bool> shown = context.state(menu);
  const composition::StateHandle<components::Text::LayoutKey::Value> layouts =
      context.state(seen);
  const std::optional<composition::Pasteboard> pasteboard =
      context.environment(components::Environment::pasteboard);
  const theme::ColourValue highlight{
      theme::ColourRole::selected_text_background};
  const theme::ColourValue accent{theme::ColourRole::accent};
  const auto copy = [selected, layouts, pasteboard, shown] {
    const std::string text = selected_text(layouts.get(), selected.get());
    if (!text.empty() && pasteboard.has_value()) {
      pasteboard->set_items({composition::TransferItem::of(text)});
    }
    shown.set(false);
  };
  const auto select_all = [selected, layouts, shown] {
    const components::Text::LayoutKey::Value& known = layouts.get();
    if (!known.empty()) {
      selected.set(Selected{
          .layout = 0,
          .range =
              Range{.lower = 0, .upper = known.front().layout.text().size()}});
    }
    shown.set(false);
  };
  composition::Element body =
      composition::Element{composition::PlaceholderContentView(content_)}
          .environment(composition::TextEnvironment::publishes_layout, true)
          .environment(composition::TextEnvironment::selectable, true)
          .on_preference_change(
              components::Text::LayoutKey::key,
              [layouts](const components::Text::LayoutKey::Value& value) {
                layouts.set(value);
              });
  if (const std::optional<Selected>& current = selected.get();
      current.has_value() && !current->range.empty()) {
    body = std::move(body).background_preference_value(
        components::Text::LayoutKey::key,
        [chosen = *current,
         highlight](const components::Text::LayoutKey::Value& value) {
          return marks(value, chosen.layout, chosen.range, highlight);
        });
  }
  body = std::move(body).overlay_preference_value(
      components::Text::LayoutKey::key,
      [selected, shown, accent](const components::Text::LayoutKey::Value& value)
          -> composition::Element {
        composition::Element surface = components::GeometryReader(
            [value, selected, shown](const composition::GeometryProxy& proxy) {
              const std::vector<Placed> where = placed(value, proxy);
              return composition::Element{
                  components::ShapeView(theme::Shape::rectangle())
                      .fill(theme::Colour{0.0F, 0.0F, 0.0F, 0.0F})
                      .content_shape(theme::Shape::rectangle())
                      .on_tap_gesture(2, composition::CoordinateSpace::local,
                                      [where, selected](composition::Point at) {
                                        if (const std::optional<Selected> hit =
                                                index_at(where, at)) {
                                          const std::string_view text =
                                              where[hit->layout].layout.text();
                                          selected.set(Selected{
                                              .layout = hit->layout,
                                              .range = TextEditing::word_at(
                                                  text, hit->range.lower)});
                                        }
                                      })
                      .on_tap_gesture(
                          [selected] { selected.set(std::nullopt); })
                      .on_long_press_gesture([value, selected, shown] {
                        if (!value.empty()) {
                          selected.set(Selected{
                              .layout = 0,
                              .range = Range{
                                  .lower = 0,
                                  .upper =
                                      value.front().layout.text().size()}});
                        }
                        shown.set(true);
                      })};
            });
        const std::optional<Selected>& current = selected.get();
        if (!current.has_value() || current->range.empty()) {
          return surface;
        }
        return components::ZStack(
            components::ZStackOptions{.alignment =
                                          composition::Alignment::top_leading},
            std::move(surface),
            handles(value, current->layout, current->range, accent,
                    [selected, chosen = *current](End end, std::size_t index) {
                      Selected next = chosen;
                      if (end == End::lower) {
                        next.range.lower = std::min(index, chosen.range.upper);
                      } else {
                        next.range.upper = std::max(index, chosen.range.lower);
                      }
                      selected.set(next);
                    }));
      });
  body = std::move(body)
             .focusable()
             .on_key_press(
                 composition::KeyPress::Phases::down |
                     composition::KeyPress::Phases::repeat,
                 [selected, layouts, copy,
                  select_all](const composition::KeyPress& press) mutable {
                   using composition::EventModifiers;
                   using composition::KeyEquivalent;
                   const bool command =
                       press.modifiers.contains(EventModifiers::command) ||
                       press.modifiers.contains(EventModifiers::control);
                   if (command && press.key == KeyEquivalent{U'c'}) {
                     copy();
                     return composition::KeyPress::Result::handled;
                   }
                   if (command && press.key == KeyEquivalent{U'a'}) {
                     select_all();
                     return composition::KeyPress::Result::handled;
                   }
                   const std::optional<Selected>& current = selected.get();
                   if (!current.has_value()) {
                     return composition::KeyPress::Result::ignored;
                   }
                   if (press.key == KeyEquivalent::escape) {
                     selected.set(std::nullopt);
                     return composition::KeyPress::Result::handled;
                   }
                   const bool shift =
                       press.modifiers.contains(EventModifiers::shift);
                   if (shift &&
                       (press.key == KeyEquivalent::left_arrow ||
                        press.key == KeyEquivalent::right_arrow) &&
                       current->layout < layouts.get().size()) {
                     const std::string_view text =
                         layouts.get()[current->layout].layout.text();
                     Selected next = *current;
                     next.range.upper =
                         press.key == KeyEquivalent::left_arrow
                             ? std::max(
                                   next.range.lower,
                                   text::Utf8::previous(text, next.range.upper))
                             : text::Utf8::next(text, next.range.upper);
                     selected.set(next);
                     return composition::KeyPress::Result::handled;
                   }
                   return composition::KeyPress::Result::ignored;
                 })
             .on_command(components::CommandMapping::options,
                         std::string{components::CommandMapping::options_label},
                         [shown] { shown.set(true); });
  body = std::move(body).popover(
      composition::Binding<bool>{shown}, [copy, select_all] {
        return components::VStack(
                   components::Button(std::string{copy_label}, copy),
                   components::Button(std::string{select_all_label},
                                      select_all))
            .id("selection-menu");
      });
  std::vector<composition::Element> produced;
  produced.push_back(std::move(body));
  return produced;
}

}
