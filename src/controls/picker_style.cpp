#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/command_mapping.hpp>
#include <arisa/components/control_metrics.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/glyph.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/command.hpp>
#include <arisa/composition/command_options.hpp>
#include <arisa/composition/content_shape_kinds.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/drag_gesture.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/move_command_direction.hpp>
#include <arisa/controls/menu.hpp>
#include <arisa/controls/picker_style.hpp>
#include <arisa/controls/toggle_style.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/presentation/navigation_stack.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/theme/elevation.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/stroke.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <numbers>
#include <utility>

namespace arisa::controls {

const composition::EnvironmentKey<PickerStyle> PickerStyle::key{
    "picker_style", PickerStyle::automatic()};

auto PickerStyle::automatic() noexcept -> PickerStyle {
  return PickerStyle{Kind::automatic};
}

auto PickerStyle::segmented() noexcept -> PickerStyle {
  return PickerStyle{Kind::segmented};
}

auto PickerStyle::tabs() noexcept -> PickerStyle {
  return PickerStyle{Kind::tabs};
}

auto PickerStyle::menu() noexcept -> PickerStyle {
  return PickerStyle{Kind::menu};
}

auto PickerStyle::radio_group() noexcept -> PickerStyle {
  return PickerStyle{Kind::radio_group};
}

auto PickerStyle::navigation_link() noexcept -> PickerStyle {
  return PickerStyle{Kind::navigation_link};
}

auto PickerStyle::wheel() noexcept -> PickerStyle {
  return PickerStyle{Kind::wheel};
}

auto PickerStyle::palette() noexcept -> PickerStyle {
  return PickerStyle{Kind::palette};
}

auto operator==(const PickerStyle& left, const PickerStyle& right) -> bool {
  if (left.kind_ != right.kind_) {
    return false;
  }
  return left.kind_ != PickerStyle::Kind::custom ||
         left.custom_->equals(*right.custom_);
}

auto PickerStyle::make_body(Configuration configuration) const
    -> composition::Element {
  switch (kind_) {
    case Kind::automatic:
    case Kind::menu:
      return menued(std::move(configuration));
    case Kind::segmented:
    case Kind::tabs: {
      composition::Element body =
          Segmented(configuration.options(), configuration.control_size,
                    kind_ == Kind::tabs ? tabs_segment_padding : 0.0F)
              .frame({.max_width = composition::infinity});
      if (!configuration.is_enabled) {
        body = std::move(body).opacity(0.4F);
      }
      return body;
    }
    case Kind::inline_list:
      return listed(std::move(configuration), false);
    case Kind::radio_group:
      return listed(std::move(configuration), true);
    case Kind::navigation_link:
      return Chooser(std::move(configuration.label),
                     std::move(configuration.title),
                     std::move(configuration.options));
    case Kind::wheel:
    case Kind::palette: {
      std::vector<Option> current = configuration.options();
      composition::Element picked =
          kind_ == Kind::wheel
              ? composition::Element{Wheel(std::move(current))}
              : composition::Element{
                    Palette(std::move(current), configuration.palette_effect)};
      composition::Element body =
          components::VStack(
              components::VStackOptions{
                  .alignment = components::HorizontalAlignment::leading,
                  .spacing = theme::space::xs},
              std::move(configuration.label)
                  .foreground_style(theme::ColourRole::text_secondary),
              std::move(picked))
              .frame({.max_width = composition::infinity,
                      .alignment = composition::Alignment::leading});
      if (!configuration.is_enabled) {
        body = std::move(body).opacity(0.4F);
      }
      return body;
    }
    case Kind::custom:
      return custom_->make_body(std::move(configuration));
  }
  return menued(std::move(configuration));
}

auto PickerStyle::selected_index(const std::vector<Option>& options)
    -> std::optional<std::size_t> {
  const auto found = std::ranges::find_if(
      options, [](const Option& option) { return option.is_selected; });
  if (found == options.end()) {
    return std::nullopt;
  }
  return static_cast<std::size_t>(found - options.begin());
}

auto PickerStyle::stepping(std::vector<Option>& options, composition::Axis axis)
    -> std::shared_ptr<composition::Command> {
  std::vector<std::function<void()>> selects;
  selects.reserve(options.size());
  for (Option& option : options) {
    selects.push_back(option.select);
  }
  const std::optional<std::size_t> chosen = selected_index(options);
  return std::make_shared<composition::Command>(composition::Command::move(
      [selects = std::move(selects),
       chosen](composition::MoveCommandDirection direction) {
        if (selects.empty()) {
          return;
        }
        const bool forward =
            direction == composition::MoveCommandDirection::right ||
            direction == composition::MoveCommandDirection::down;
        std::size_t next = 0;
        if (chosen.has_value()) {
          if (forward) {
            next = std::min(*chosen + 1, selects.size() - 1);
          } else {
            next = *chosen == 0 ? 0 : *chosen - 1;
          }
        }
        if (selects[next]) {
          selects[next]();
        }
      },
      axis));
}

auto PickerStyle::radio(bool selected) -> composition::Element {
  composition::Element ring =
      components::ZStack()
          .frame({.width = indicator_extent, .height = indicator_extent})
          .background(
              selected
                  ? theme::ColourValue{theme::ColourRole::accent}
                  : theme::ColourValue{theme::ColourRole::control_background},
              theme::Shape::capsule())
          .border(selected ? theme::ColourValue{theme::ColourRole::accent}
                           : theme::ColourValue{theme::ColourRole::separator},
                  theme::Stroke{ToggleStyle::outline_width});
  if (!selected) {
    return ring;
  }
  return std::move(ring).overlay(
      components::ZStack()
          .frame({.width = indicator_extent * 0.4F,
                  .height = indicator_extent * 0.4F})
          .background(theme::ColourRole::on_accent, theme::Shape::capsule()));
}

auto PickerStyle::row(Option option, bool radio_mark,
                      std::function<void()> after) -> composition::Element {
  return marked(std::move(option), radio_mark, std::move(after), true);
}

auto PickerStyle::marked(Option option, bool radio_mark,
                         std::function<void()> after, bool fills)
    -> composition::Element {
  const bool selected = option.is_selected;
  composition::Element mark =
      option.is_mixed
          ? components::Glyph::minus(theme::ColourRole::accent,
                                     indicator_extent * 0.6F)
                .frame({.width = indicator_extent, .height = indicator_extent})
      : radio_mark
          ? radio(selected)
          : (selected ? components::Glyph::checkmark(theme::ColourRole::accent,
                                                     indicator_extent)
                      : composition::Element{components::ZStack().frame(
                            {.width = indicator_extent,
                             .height = indicator_extent})});
  composition::Element label =
      radio_mark
          ? composition::Element{components::HStack(
                components::HStackOptions{
                    .alignment = components::VerticalAlignment::center,
                    .spacing = theme::space::sm},
                std::move(mark), std::move(option.label))}
          : composition::Element{
                components::HStack(
                    components::HStackOptions{
                        .alignment = components::VerticalAlignment::center,
                        .spacing = theme::space::sm},
                    std::move(option.label), components::Spacer(),
                    std::move(mark))
                    .frame({.max_width = composition::infinity})};
  composition::Element button =
      components::Button(
          [select = std::move(option.select), after = std::move(after)] {
            if (select) {
              select();
            }
            if (after) {
              after();
            }
          },
          std::move(label))
          .button_style(Menu::ItemStyle{})
          .focus_effect_disabled();
  if (!fills) {
    return button;
  }
  return std::move(button).frame({.max_width = composition::infinity});
}

auto PickerStyle::menued(Configuration configuration) -> composition::Element {
  const bool unlabelled = components::Group::is_empty(configuration.label);
  std::vector<Option> current = configuration.options();
  const std::optional<std::size_t> chosen = selected_index(current);
  const bool mixed = std::ranges::any_of(
      current, [](const Option& option) { return option.is_mixed; });
  composition::Element shown =
      chosen.has_value()
          ? std::move(current[*chosen].label)
          : composition::Element{components::Text(text::LocalizedStringKey{
                mixed ? std::string{mixed_label} : std::string{"None"}})};
  Options options = std::move(configuration.options);
  composition::Element trigger = Menu(
      [options] {
        std::vector<composition::Element> rows;
        for (Option& option : options()) {
          rows.push_back(row(std::move(option), false, nullptr));
        }
        return composition::Element{components::Group(std::move(rows))};
      },
      components::HStack(
          components::HStackOptions{
              .alignment = components::VerticalAlignment::center,
              .spacing = theme::space::xs},
          std::move(shown),
          components::Glyph::chevron_down(theme::ColourRole::text_secondary,
                                          Menu::chevron_extent)));
  if (unlabelled) {
    return configuration.is_enabled ? std::move(trigger)
                                    : std::move(trigger).opacity(0.4F);
  }
  composition::Element body =
      components::HStack(
          components::HStackOptions{
              .alignment = components::VerticalAlignment::center,
              .spacing = std::nullopt},
          std::move(configuration.label), components::Spacer(),
          std::move(trigger))
          .frame({.max_width = composition::infinity});
  if (!configuration.is_enabled) {
    body = std::move(body).opacity(0.4F);
  }
  return body;
}

auto PickerStyle::listed(Configuration configuration, bool radio_mark)
    -> composition::Element {
  std::vector<composition::Element> rows;
  rows.push_back(std::move(configuration.label)
                     .foreground_style(theme::ColourRole::text_secondary));
  if (radio_mark &&
      configuration.radio_group_layout == composition::Axis::horizontal) {
    std::vector<composition::Element> items;
    for (Option& option : configuration.options()) {
      items.push_back(marked(std::move(option), true, nullptr, false));
    }
    rows.push_back(components::HStack(
        components::HStackOptions{
            .alignment = components::VerticalAlignment::center,
            .spacing = theme::Space{radio_group_spacing}},
        std::move(items)));
  } else {
    for (Option& option : configuration.options()) {
      rows.push_back(row(std::move(option), radio_mark, nullptr));
    }
  }
  composition::Element body =
      components::VStack(
          components::VStackOptions{
              .alignment = components::HorizontalAlignment::leading,
              .spacing = theme::space::xxs},
          std::move(rows))
          .frame({.max_width = composition::infinity,
                  .alignment = composition::Alignment::leading});
  if (!configuration.is_enabled) {
    body = std::move(body).opacity(0.4F);
  }
  return body;
}

PickerStyle::Segmented::Segmented(std::vector<Option> options,
                                  components::ControlSize size,
                                  float extra_padding)
    : Element(
          std::make_unique<Content>(),
          collect(components::HStack(
                      components::HStackOptions{
                          .alignment = components::VerticalAlignment::center,
                          .spacing = theme::space::none},
                      segments(options, size, extra_padding))
                      .frame({.max_width = composition::infinity})
                      .background(components::ButtonStyle::fill(
                          components::ButtonStyle::fill_role))
                      .animation(
                          motion::Animation::snappy(0.25),
                          selected_index(options).value_or(options.size())))) {
  declared().focusable = true;
  declared().focus_shape = theme::Shape::rounded(theme::radius::control, true);
  declared().commands.push_back(
      stepping(options, composition::Axis::horizontal));
}

auto PickerStyle::Segmented::segments(std::vector<Option>& options,
                                      components::ControlSize size,
                                      float extra_padding)
    -> std::vector<composition::Element> {
  const theme::ScaledMetric minimum =
      size == components::ControlSize::regular
          ? theme::ScaledMetric{height}
          : theme::ScaledMetric{
                components::ControlMetrics::of(size).segmented_height};
  std::vector<composition::Element> cells;
  cells.reserve(options.size());
  for (Option& option : options) {
    composition::Element padded =
        components::ZStack(std::move(option.label))
            .padding(theme::Edge::horizontal,
                     theme::metric::button_horizontal_padding);
    if (extra_padding > 0.0F) {
      padded = components::ZStack(std::move(padded))
                   .padding(theme::Edge::horizontal, extra_padding * 0.5F);
    }
    composition::Element cell =
        std::move(padded)
            .frame({.max_width = composition::infinity, .min_height = minimum})
            .content_shape(theme::Shape::rectangle());
    if (option.is_selected) {
      cell =
          std::move(cell)
              .background(theme::ColourRole::accent,
                          theme::Shape::rounded(theme::radius::control, true))
              .foreground_style(theme::ColourRole::on_accent);
    } else if (option.is_mixed) {
      cell = std::move(cell).background(
          theme::ColourValue{theme::ColourRole::accent}.opacity(0.35F),
          theme::Shape::rounded(theme::radius::control, true));
    }
    std::function<void()> select = option.select;
    cells.push_back(std::move(cell).on_tap_gesture([select] {
      if (select) {
        select();
      }
    }));
  }
  return cells;
}

auto PickerStyle::Segmented::Content::type() const noexcept
    -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

PickerStyle::Chooser::Chooser(composition::Element label, std::string title,
                              Options options)
    : Element(std::make_unique<Content>(
          std::make_shared<composition::PlaceholderContentView::Source>(
              std::move(label)),
          std::move(title), std::move(options))) {}

auto PickerStyle::Chooser::Content::type() const noexcept
    -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto PickerStyle::Chooser::rows(const Options& options,
                                const std::function<void()>& after)
    -> composition::Element {
  std::vector<composition::Element> listed;
  for (Option& option : options()) {
    listed.push_back(row(std::move(option), false, after));
  }
  return components::VStack(
             components::VStackOptions{
                 .alignment = components::HorizontalAlignment::leading,
                 .spacing = theme::space::xxs},
             std::move(listed))
      .frame({.max_width = composition::infinity,
              .alignment = composition::Alignment::leading});
}

auto PickerStyle::Chooser::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  const auto open = context.state(shown);
  const presentation::NavigationStack::Navigator& navigator =
      context.environment(presentation::NavigationStack::navigator);
  std::vector<Option> current = options_();
  const std::optional<std::size_t> chosen = selected_index(current);
  composition::Element value = chosen.has_value()
                                   ? std::move(current[*chosen].label)
                                   : composition::Element{components::Text(
                                         text::LocalizedStringKey{"None"})};
  composition::Element trigger =
      components::Button(
          [open, navigator, title = title_, options = options_] {
            const bool pushed = navigator.push([title, options] {
              return composition::Element{Page(title, options)};
            });
            if (!pushed) {
              open.set(true);
            }
          },
          components::HStack(
              components::HStackOptions{
                  .alignment = components::VerticalAlignment::center,
                  .spacing = theme::space::sm},
              composition::PlaceholderContentView(label_), components::Spacer(),
              std::move(value).foreground_style(
                  theme::ColourRole::text_secondary),
              components::Glyph::chevron_right(
                  theme::ColourRole::text_secondary, 14.0F))
              .frame({.max_width = composition::infinity}))
          .button_style(Menu::ItemStyle{})
          .frame({.max_width = composition::infinity})
          .sheet(open, [title = title_, options = options_, open] {
            return components::VStack(
                       components::VStackOptions{
                           .alignment =
                               components::HorizontalAlignment::leading,
                           .spacing = theme::space::lg},
                       components::Text(title).font(theme::Font::title2),
                       rows(options, [open] { open.set(false); }),
                       components::HStack(
                           components::Spacer(),
                           components::Button(
                               [open] { open.set(false); },
                               components::Text(
                                   text::LocalizedStringKey{"Done"}))))
                .frame({.max_width = composition::infinity,
                        .alignment = composition::Alignment::leading});
          });
  std::vector<composition::Element> produced;
  produced.push_back(std::move(trigger));
  return produced;
}

PickerStyle::Page::Page(std::string title, Options options)
    : Element(std::make_unique<Content>(std::move(title), std::move(options))) {
}

auto PickerStyle::Page::Content::type() const noexcept
    -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto PickerStyle::Page::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  const composition::DismissAction& dismiss =
      context.environment(components::Environment::dismiss);
  std::vector<composition::Element> produced;
  produced.push_back(
      components::ScrollView(Chooser::rows(options_, [dismiss] { dismiss(); })
                                 .padding(theme::space::xl))
          .navigation_title(title_));
  return produced;
}

PickerStyle::Wheel::Wheel(std::vector<Option> options)
    : Element(std::make_unique<Content>([&options] {
        std::vector<Content::Slot> slots;
        slots.reserve(options.size());
        for (Option& option : options) {
          slots.push_back(Content::Slot{
              .label =
                  std::make_shared<composition::PlaceholderContentView::Source>(
                      std::move(option.label)),
              .is_selected = option.is_selected,
              .select = std::move(option.select)});
        }
        return slots;
      }())) {}

auto PickerStyle::Wheel::Content::type() const noexcept
    -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

void PickerStyle::Wheel::Content::step(const Selects& selects,
                                       std::size_t centre, std::ptrdiff_t by) {
  if (selects == nullptr || selects->empty()) {
    return;
  }
  const auto last = static_cast<std::ptrdiff_t>(selects->size()) - 1;
  const std::ptrdiff_t next =
      std::clamp(static_cast<std::ptrdiff_t>(centre) + by,
                 static_cast<std::ptrdiff_t>(0), last);
  const std::function<void()>& target =
      (*selects)[static_cast<std::size_t>(next)];
  if (target) {
    target();
  }
}

auto PickerStyle::Wheel::Content::row(std::ptrdiff_t index,
                                      std::size_t centre) const
    -> composition::Element {
  const std::ptrdiff_t offset = index - static_cast<std::ptrdiff_t>(centre);
  const auto distance = static_cast<float>(offset < 0 ? -offset : offset);
  composition::Element cell =
      index < 0 || index >= static_cast<std::ptrdiff_t>(slots_.size())
          ? composition::Element{components::ZStack()}
          : composition::Element{composition::PlaceholderContentView(
                slots_[static_cast<std::size_t>(index)].label)};
  cell = components::ZStack(std::move(cell))
             .frame({.height = row_height, .max_width = composition::infinity})
             .content_shape(theme::Shape::rectangle());
  if (offset != 0) {
    const float turned = std::clamp(
        static_cast<float>(offset) * row_height / drum_radius,
        -std::numbers::pi_v<float> * 0.5F, std::numbers::pi_v<float> * 0.5F);
    const float lifted = (drum_radius * std::sin(turned)) -
                         (static_cast<float>(offset) * row_height);
    cell = std::move(cell)
               .scale_effect(1.0F - (shrink_step * distance),
                             std::max(std::cos(turned), 0.0F))
               .offset(0.0F, lifted)
               .opacity(std::max(1.0F - (fade_step * distance), 0.0F))
               .foreground_style(theme::ColourRole::text_secondary);
  }
  if (index >= 0 && index < static_cast<std::ptrdiff_t>(slots_.size()) &&
      offset != 0) {
    std::function<void()> select =
        slots_[static_cast<std::size_t>(index)].select;
    cell = std::move(cell).on_tap_gesture([select] {
      if (select) {
        select();
      }
    });
  }
  return cell;
}

auto PickerStyle::Wheel::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  const auto active = context.state(engaged);
  const auto pulled = context.state(dragged);
  const bool turning = active.get();
  const auto found = std::ranges::find_if(
      slots_, [](const Slot& slot) { return slot.is_selected; });
  const std::size_t centre =
      found == slots_.end() ? 0
                            : static_cast<std::size_t>(found - slots_.begin());
  auto listed = std::make_shared<std::vector<std::function<void()>>>();
  listed->reserve(slots_.size());
  for (const Slot& slot : slots_) {
    listed->push_back(slot.select);
  }
  const Selects selects = std::move(listed);
  std::vector<composition::Element> rows;
  const auto window = static_cast<std::ptrdiff_t>(reach);
  for (std::ptrdiff_t index = static_cast<std::ptrdiff_t>(centre) - window;
       index <= static_cast<std::ptrdiff_t>(centre) + window; ++index) {
    rows.push_back(row(index, centre).id(static_cast<std::int64_t>(index)));
  }
  composition::Element band =
      components::ZStack()
          .frame({.height = row_height, .max_width = composition::infinity})
          .background(
              turning
                  ? theme::ColourValue{theme::ColourRole::selection}
                  : theme::ColourValue{theme::ColourRole::control_background},
              theme::Shape::rounded(theme::radius::control, true))
          .allows_hit_testing(false);
  const auto toggle = [active] { active.set(!active.get()); };
  composition::Element drum =
      components::ZStack(
          std::move(band),
          components::VStack(
              components::VStackOptions{
                  .alignment = components::HorizontalAlignment::center,
                  .spacing = theme::space::none},
              std::move(rows))
              .transaction(centre,
                           [](motion::Transaction& transaction) {
                             if (!transaction.animation().has_value() &&
                                 !transaction.disables_animations()) {
                               transaction.set_animation(
                                   motion::Animation::snappy(0.2));
                             }
                           }))
          .frame({.max_width = composition::infinity})
          .clip_shape(theme::Shape::rounded(theme::radius::control, true))
          .focusable()
          .content_shape(composition::ContentShapeKinds::focus_effect,
                         theme::Shape::rounded(theme::radius::control, true))
          .on_command(components::CommandMapping::select,
                      std::string{turning ? done_label : adjust_label}, toggle,
                      composition::CommandOptions{})
          .gesture(
              composition::DragGesture(4.0F)
                  .on_changed([selects, pulled, centre](
                                  const composition::DragGesture::Value& drag) {
                    const float moved = drag.translation.height;
                    const auto rows_moved = static_cast<std::ptrdiff_t>(
                        (moved - pulled.get()) / row_height);
                    if (rows_moved != 0) {
                      pulled.set(pulled.get() +
                                 (static_cast<float>(rows_moved) * row_height));
                      step(selects, centre, -rows_moved);
                    }
                  })
                  .on_ended([selects, pulled, centre](
                                const composition::DragGesture::Value& drag) {
                    const float flung =
                        drag.predicted_end_translation.height - pulled.get();
                    const auto rows_flung = static_cast<std::ptrdiff_t>(
                        std::round(flung / row_height));
                    pulled.set(0.0F);
                    if (rows_flung == 0) {
                      return;
                    }
                    const float seconds =
                        std::min(flick_base_seconds +
                                     (flick_seconds_per_row *
                                      static_cast<float>(std::abs(rows_flung))),
                                 flick_limit_seconds);
                    motion::with_animation(motion::Animation::timing_curve(
                                               0.2, 0.8, 0.3, 1.0, seconds),
                                           [&selects, centre, rows_flung] {
                                             step(selects, centre, -rows_flung);
                                           });
                  }));
  if (turning) {
    drum =
        std::move(drum)
            .on_move_command(
                [selects, centre](composition::MoveCommandDirection direction) {
                  step(selects, centre,
                       direction == composition::MoveCommandDirection::down
                           ? 1
                           : -1);
                },
                composition::Axis::vertical)
            .on_exit_command(std::string{done_label},
                             [active] { active.set(false); });
  }
  std::vector<composition::Element> produced;
  produced.push_back(std::move(drum));
  return produced;
}

PickerStyle::Palette::Palette(std::vector<Option> options,
                              PaletteSelectionEffect effect)
    : Element(std::make_unique<Content>(),
              collect(components::HStack(
                  components::HStackOptions{
                      .alignment = components::VerticalAlignment::center,
                      .spacing = theme::space::sm},
                  swatches(options, effect)))) {
  declared().focusable = true;
  declared().focus_shape = theme::Shape::capsule();
  declared().commands.push_back(
      stepping(options, composition::Axis::horizontal));
}

auto PickerStyle::Palette::swatches(std::vector<Option>& options,
                                    PaletteSelectionEffect effect)
    -> std::vector<composition::Element> {
  using Effect = PaletteSelectionEffect::Kind;
  std::vector<composition::Element> cells;
  cells.reserve(options.size());
  for (Option& option : options) {
    composition::Element shown = std::move(option.label);
    if (effect.kind() == Effect::symbol_variant && option.is_selected) {
      shown = std::move(shown).symbol_variant(effect.variant());
    }
    composition::Element cell =
        components::ZStack(std::move(shown))
            .frame({.width = swatch_extent, .height = swatch_extent})
            .content_shape(theme::Shape::capsule());
    if (effect.kind() != Effect::automatic) {
      std::function<void()> chosen = option.select;
      cells.push_back(std::move(cell).on_tap_gesture([chosen] {
        if (chosen) {
          chosen();
        }
      }));
      continue;
    }
    cell =
        option.is_selected
            ? std::move(cell)
                  .background(theme::ColourRole::accent,
                              theme::Shape::capsule())
                  .foreground_style(theme::ColourRole::on_accent)
            : std::move(cell).background(theme::ColourRole::control_background,
                                         theme::Shape::capsule());
    std::function<void()> select = option.select;
    cells.push_back(std::move(cell).on_tap_gesture([select] {
      if (select) {
        select();
      }
    }));
  }
  return cells;
}

auto PickerStyle::Palette::Content::type() const noexcept
    -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

PickerStyle::PickerStyle(DefaultPickerStyle) noexcept
    : kind_(Kind::automatic) {}

auto DefaultPickerStyle::make_body(
    PickerStyle::Configuration configuration) const -> composition::Element {
  return PickerStyle{*this}.make_body(std::move(configuration));
}

PickerStyle::PickerStyle(SegmentedPickerStyle) noexcept
    : kind_(Kind::segmented) {}

auto SegmentedPickerStyle::make_body(
    PickerStyle::Configuration configuration) const -> composition::Element {
  return PickerStyle{*this}.make_body(std::move(configuration));
}

PickerStyle::PickerStyle(TabsPickerStyle) noexcept : kind_(Kind::tabs) {}

auto TabsPickerStyle::make_body(PickerStyle::Configuration configuration) const
    -> composition::Element {
  return PickerStyle{*this}.make_body(std::move(configuration));
}

PickerStyle::PickerStyle(MenuPickerStyle) noexcept : kind_(Kind::menu) {}

auto MenuPickerStyle::make_body(PickerStyle::Configuration configuration) const
    -> composition::Element {
  return PickerStyle{*this}.make_body(std::move(configuration));
}

PickerStyle::PickerStyle(InlinePickerStyle) noexcept
    : kind_(Kind::inline_list) {}

auto InlinePickerStyle::make_body(
    PickerStyle::Configuration configuration) const -> composition::Element {
  return PickerStyle{*this}.make_body(std::move(configuration));
}

PickerStyle::PickerStyle(RadioGroupPickerStyle) noexcept
    : kind_(Kind::radio_group) {}

auto RadioGroupPickerStyle::make_body(
    PickerStyle::Configuration configuration) const -> composition::Element {
  return PickerStyle{*this}.make_body(std::move(configuration));
}

PickerStyle::PickerStyle(NavigationLinkPickerStyle) noexcept
    : kind_(Kind::navigation_link) {}

auto NavigationLinkPickerStyle::make_body(
    PickerStyle::Configuration configuration) const -> composition::Element {
  return PickerStyle{*this}.make_body(std::move(configuration));
}

PickerStyle::PickerStyle(WheelPickerStyle) noexcept : kind_(Kind::wheel) {}

auto WheelPickerStyle::make_body(PickerStyle::Configuration configuration) const
    -> composition::Element {
  return PickerStyle{*this}.make_body(std::move(configuration));
}

PickerStyle::PickerStyle(PalettePickerStyle) noexcept : kind_(Kind::palette) {}

auto PalettePickerStyle::make_body(
    PickerStyle::Configuration configuration) const -> composition::Element {
  return PickerStyle{*this}.make_body(std::move(configuration));
}

}
