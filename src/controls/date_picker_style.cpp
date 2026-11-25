#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/glyph.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/popover_attachment_anchor.hpp>
#include <arisa/controls/date_format.hpp>
#include <arisa/controls/date_picker_style.hpp>
#include <arisa/controls/picker.hpp>
#include <arisa/controls/picker_style.hpp>
#include <arisa/controls/stepper.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/stroke.hpp>
#include <arisa/theme/typography.hpp>

#include <algorithm>
#include <format>
#include <utility>
#include <vector>

namespace arisa::controls {

const composition::EnvironmentKey<DatePickerStyle> DatePickerStyle::key{
    "date_picker_style", DatePickerStyle::automatic()};

auto DatePickerStyle::automatic() noexcept -> DatePickerStyle {
  return DatePickerStyle{Kind::automatic};
}

auto DatePickerStyle::compact() noexcept -> DatePickerStyle {
  return DatePickerStyle{Kind::compact};
}

auto DatePickerStyle::graphical() noexcept -> DatePickerStyle {
  return DatePickerStyle{Kind::graphical};
}

auto DatePickerStyle::wheel() noexcept -> DatePickerStyle {
  return DatePickerStyle{Kind::wheel};
}

auto DatePickerStyle::field() noexcept -> DatePickerStyle {
  return DatePickerStyle{Kind::field};
}

auto DatePickerStyle::stepper_field() noexcept -> DatePickerStyle {
  return DatePickerStyle{Kind::stepper_field};
}

auto operator==(const DatePickerStyle& left, const DatePickerStyle& right)
    -> bool {
  if (left.kind_ != right.kind_) {
    return false;
  }
  return left.kind_ != DatePickerStyle::Kind::custom ||
         left.custom_->equals(*right.custom_);
}

auto DatePickerStyle::day_of(std::chrono::sys_seconds instant,
                             components::TimeZone zone) noexcept
    -> std::chrono::year_month_day {
  return std::chrono::year_month_day{
      std::chrono::floor<std::chrono::days>(instant + zone.offset())};
}

auto DatePickerStyle::minutes_of(std::chrono::sys_seconds instant,
                                 components::TimeZone zone) noexcept
    -> std::chrono::minutes {
  const std::chrono::sys_seconds local = instant + zone.offset();
  return std::chrono::floor<std::chrono::minutes>(
      local - std::chrono::floor<std::chrono::days>(local));
}

auto DatePickerStyle::compose(std::chrono::year_month_day day,
                              std::chrono::minutes since_midnight,
                              components::TimeZone zone) noexcept
    -> std::chrono::sys_seconds {
  const std::chrono::year_month_day valid =
      day.ok() ? day
               : std::chrono::year_month_day{std::chrono::year_month_day_last{
                     day.year() / day.month() / std::chrono::last}};
  return std::chrono::sys_seconds{std::chrono::sys_days{valid}} +
         since_midnight - zone.offset();
}

auto DatePickerStyle::clamped(std::chrono::sys_seconds instant,
                              const std::optional<Range>& range) noexcept
    -> std::chrono::sys_seconds {
  return range.has_value() ? range->clamped(instant) : instant;
}

auto DatePickerStyle::day_key(std::chrono::year_month_day day) -> std::string {
  return std::format(
      "arisa.date_picker.day.{}-{}-{}", static_cast<int>(day.year()),
      static_cast<unsigned>(day.month()), static_cast<unsigned>(day.day()));
}

auto DatePickerStyle::make_body(Configuration configuration) const
    -> composition::Element {
  switch (kind_) {
    case Kind::automatic:
    case Kind::compact:
      return labelled(std::move(configuration.label),
                      Compact{std::move(configuration.inputs)}, false);
    case Kind::graphical:
      return labelled(std::move(configuration.label),
                      Graphical{std::move(configuration.inputs)}, true);
    case Kind::wheel:
      return wheeled(std::move(configuration));
    case Kind::field:
      return labelled(std::move(configuration.label),
                      Field{std::move(configuration.inputs), false}, false);
    case Kind::stepper_field:
      return labelled(std::move(configuration.label),
                      Field{std::move(configuration.inputs), true}, false);
    case Kind::custom:
      return custom_->make_body(std::move(configuration));
  }
  return labelled(std::move(configuration.label),
                  Compact{std::move(configuration.inputs)}, false);
}

auto DatePickerStyle::labelled(composition::Element label,
                               composition::Element control, bool stacked)
    -> composition::Element {
  if (components::Group::is_empty(label)) {
    return control;
  }
  if (stacked) {
    return components::VStack(
               components::VStackOptions{
                   .alignment = components::HorizontalAlignment::leading,
                   .spacing = theme::space::sm},
               std::move(label), std::move(control))
        .frame({.max_width = composition::infinity,
                .alignment = composition::Alignment::leading});
  }
  return components::HStack(
             components::HStackOptions{
                 .alignment = components::VerticalAlignment::center,
                 .spacing = theme::space::md},
             std::move(label), components::Spacer(), std::move(control))
      .frame({.max_width = composition::infinity});
}

auto DatePickerStyle::month(Month month) -> composition::Element {
  const CalendarMonth shown = month.shown;
  std::vector<composition::Element> header;
  header.push_back(
      components::Text(DateFormat::month_title(shown.month(), month.locale))
          .font(theme::Font::headline));
  header.push_back(components::Spacer());
  header.push_back(components::Button(
                       [turn = month.turn, shown] {
                         if (turn) {
                           turn(shown.previous());
                         }
                       },
                       components::Glyph::symbol(
                           "chevron_left", theme::ColourRole::accent, 14.0F)
                           .flips_for_right_to_left_layout_direction(true))
                       .button_style(components::ButtonStyle::borderless())
                       .id(std::string{previous_key}));
  header.push_back(
      components::Button(
          [turn = month.turn, shown] {
            if (turn) {
              turn(shown.next());
            }
          },
          components::Glyph::chevron_right(theme::ColourRole::accent, 14.0F))
          .button_style(components::ButtonStyle::borderless())
          .id(std::string{next_key}));
  std::vector<composition::Element> rows;
  rows.push_back(components::HStack(
                     components::HStackOptions{
                         .alignment = components::VerticalAlignment::center,
                         .spacing = theme::space::sm},
                     std::move(header))
                     .frame({.max_width = composition::infinity}));
  std::vector<composition::Element> names;
  for (const std::chrono::weekday weekday : shown.weekdays()) {
    names.push_back(components::Text(std::string{DateFormat::weekday_symbol(
                                         weekday, month.locale)})
                        .font(theme::Font::caption)
                        .foreground_style(theme::ColourRole::text_secondary)
                        .frame({.width = cell_extent}));
  }
  rows.push_back(components::HStack(
      components::HStackOptions{.spacing = theme::Space{cell_spacing}},
      std::move(names)));
  for (unsigned row = 0; row < shown.rows(); ++row) {
    std::vector<composition::Element> cells;
    for (unsigned column = 0; column < CalendarMonth::columns; ++column) {
      const std::optional<std::chrono::year_month_day> day =
          shown.day((row * CalendarMonth::columns) + column);
      if (!day.has_value()) {
        cells.push_back(components::ZStack().frame(
            {.width = cell_extent, .height = cell_extent}));
        continue;
      }
      const bool chosen = month.is_selected && month.is_selected(*day);
      const bool allowed =
          (!month.lower.has_value() ||
           std::chrono::sys_days{*day} >=
               std::chrono::sys_days{*month.lower}) &&
          (!month.upper.has_value() ||
           std::chrono::sys_days{*day} <= std::chrono::sys_days{*month.upper});
      composition::Element label =
          components::ZStack(components::Text(std::format(
                                 "{}", static_cast<unsigned>(day->day()))))
              .frame({.width = cell_extent, .height = cell_extent});
      label = chosen
                  ? std::move(label)
                        .background(theme::ColourRole::accent,
                                    theme::Shape::capsule())
                        .foreground_style(theme::ColourRole::on_accent)
                  : std::move(label).foreground_style(theme::ColourRole::label);
      cells.push_back(components::Button(
                          [choose = month.choose, picked = *day] {
                            if (choose) {
                              choose(picked);
                            }
                          },
                          std::move(label))
                          .button_style(components::ButtonStyle::plain())
                          .disabled(!allowed)
                          .id(day_key(*day)));
    }
    rows.push_back(components::HStack(
        components::HStackOptions{.spacing = theme::Space{cell_spacing}},
        std::move(cells)));
  }
  return components::VStack(
             components::VStackOptions{
                 .alignment = components::HorizontalAlignment::leading,
                 .spacing = theme::Space{cell_spacing}},
             std::move(rows))
      .fixed_size(true, true)
      .focus_section();
}

auto DatePickerStyle::Graphical::time(const Inputs& inputs)
    -> composition::Element {
  const composition::Binding<std::chrono::sys_seconds> selection =
      inputs.selection;
  const components::TimeZone zone = inputs.time_zone;
  const std::optional<Range> range = inputs.range;
  const auto minutes = minutes_of(selection.get(), zone);
  const auto part = [selection, zone, range](bool hours) {
    return composition::Binding<int>{
        [selection, zone, hours] {
          const auto total =
              static_cast<int>(minutes_of(selection.get(), zone).count());
          return hours ? total / 60 : total % 60;
        },
        [selection, zone, range, hours](int value) {
          const std::chrono::sys_seconds now = selection.get();
          const auto total = static_cast<int>(minutes_of(now, zone).count());
          const int hour = hours ? value : total / 60;
          const int minute = hours ? total % 60 : value;
          selection.set(
              clamped(compose(day_of(now, zone),
                              std::chrono::minutes{(hour * 60) + minute}, zone),
                      range));
        }};
  };
  const auto total = static_cast<int>(minutes.count());
  return components::VStack(
      components::VStackOptions{
          .alignment = components::HorizontalAlignment::leading,
          .spacing = theme::space::sm},
      composition::Element{
          Stepper(std::format("Hour {:02}", total / 60), part(true),
                  ClosedRange<int>{.lower_bound = 0, .upper_bound = 23}, 1)}
          .id("arisa.date_picker.hour"),
      composition::Element{
          Stepper(std::format("Minute {:02}", total % 60), part(false),
                  ClosedRange<int>{.lower_bound = 0, .upper_bound = 59}, 1)}
          .id("arisa.date_picker.minute"));
}

auto DatePickerStyle::Graphical::body(composition::Context& context) const
    -> composition::Element {
  const auto paged = context.state(page);
  const composition::Binding<std::chrono::sys_seconds> selection =
      inputs_.selection;
  const components::TimeZone zone = inputs_.time_zone;
  const std::optional<Range> range = inputs_.range;
  const std::chrono::year_month_day picked = day_of(selection.get(), zone);
  const std::chrono::year_month shown =
      paged.get().value_or(picked.year() / picked.month());
  std::vector<composition::Element> parts;
  if (inputs_.components.contains(DatePickerComponents::date)) {
    parts.push_back(month(Month{
        .shown = CalendarMonth{shown, inputs_.calendar.first_weekday()},
        .is_selected =
            [picked](std::chrono::year_month_day day) { return day == picked; },
        .choose =
            [selection, zone, range](std::chrono::year_month_day day) {
              const std::chrono::sys_seconds now = selection.get();
              selection.set(
                  clamped(compose(day, minutes_of(now, zone), zone), range));
            },
        .turn = [paged](CalendarMonth next) { paged.set(next.month()); },
        .lower = range.has_value()
                     ? std::optional<std::chrono::year_month_day>{day_of(
                           range->lower_bound, zone)}
                     : std::nullopt,
        .upper = range.has_value()
                     ? std::optional<std::chrono::year_month_day>{day_of(
                           range->upper_bound, zone)}
                     : std::nullopt,
        .locale = inputs_.locale}));
  }
  if (inputs_.components.contains(DatePickerComponents::hour_and_minute)) {
    parts.push_back(time(inputs_));
  }
  return components::VStack(
      components::VStackOptions{
          .alignment = components::HorizontalAlignment::leading,
          .spacing = theme::space::md},
      std::move(parts));
}

auto DatePickerStyle::Field::stepped(std::chrono::sys_seconds instant,
                                     DatePickerComponents components,
                                     int direction) noexcept
    -> std::chrono::sys_seconds {
  const std::chrono::seconds unit =
      components.contains(DatePickerComponents::date)
          ? std::chrono::seconds{std::chrono::days{1}}
          : std::chrono::seconds{std::chrono::minutes{1}};
  return direction < 0 ? instant - unit : instant + unit;
}

auto DatePickerStyle::Field::body(composition::Context& context) const
    -> composition::Element {
  const auto showing = context.state(presented);
  const std::chrono::sys_seconds now = inputs_.selection.get();
  std::vector<std::string> parts;
  if (inputs_.components.contains(DatePickerComponents::date)) {
    parts.push_back(
        DateFormat::date_text(day_of(now, inputs_.time_zone), inputs_.locale));
  }
  if (inputs_.components.contains(DatePickerComponents::hour_and_minute)) {
    parts.push_back(DateFormat::time_text(minutes_of(now, inputs_.time_zone),
                                          inputs_.locale));
  }
  std::string written;
  for (const std::string& part : parts) {
    written += written.empty() ? part : ", " + part;
  }
  composition::Element field =
      components::Button(
          [showing] { showing.set(true); },
          components::Text(std::move(written)).monospaced_digit())
          .button_style(components::ButtonStyle::plain())
          .padding(
              theme::Insets::symmetric(theme::space::sm, theme::space::xxs))
          .background(theme::ColourRole::text_background,
                      theme::Shape::rounded(theme::radius::control, true))
          .border(theme::ColourRole::separator, theme::Stroke::hairline())
          .id(std::string{field_key})
          .popover(showing, composition::PopoverAttachmentAnchor::rect(),
                   theme::Edge::top, [inputs = inputs_] {
                     return composition::Element{Graphical{inputs}}.padding(
                         theme::space::lg);
                   });
  if (!steps_) {
    return field;
  }
  const composition::Binding<std::chrono::sys_seconds> selection =
      inputs_.selection;
  const DatePickerComponents parts_shown = inputs_.components;
  const std::optional<Range> range = inputs_.range;
  return components::HStack(
      components::HStackOptions{
          .alignment = components::VerticalAlignment::center,
          .spacing = theme::space::xs},
      std::move(field),
      Stepper(
          std::string{},
          [selection, parts_shown, range] {
            selection.set(
                clamped(stepped(selection.get(), parts_shown, 1), range));
          },
          [selection, parts_shown, range] {
            selection.set(
                clamped(stepped(selection.get(), parts_shown, -1), range));
          })
          .labels_hidden());
}

auto DatePickerStyle::Compact::body(composition::Context& context) const
    -> composition::Element {
  const auto dated = context.state(date_shown);
  const auto timed = context.state(time_shown);
  const std::chrono::sys_seconds now = inputs_.selection.get();
  std::vector<composition::Element> chips;
  if (inputs_.components.contains(DatePickerComponents::date)) {
    Inputs dates = inputs_;
    dates.components = DatePickerComponents::date;
    chips.push_back(
        components::Button([dated] { dated.set(true); },
                           components::Text(DateFormat::date_text(
                               day_of(now, inputs_.time_zone), inputs_.locale)))
            .button_style(components::ButtonStyle::bordered())
            .id(std::string{date_key})
            .popover(dated, composition::PopoverAttachmentAnchor::rect(),
                     theme::Edge::top, [dates] {
                       return composition::Element{Graphical{dates}}.padding(
                           theme::space::lg);
                     }));
  }
  if (inputs_.components.contains(DatePickerComponents::hour_and_minute)) {
    chips.push_back(
        components::Button(
            [timed] { timed.set(true); },
            components::Text(DateFormat::time_text(
                minutes_of(now, inputs_.time_zone), inputs_.locale)))
            .button_style(components::ButtonStyle::bordered())
            .id(std::string{time_key})
            .popover(timed, composition::PopoverAttachmentAnchor::rect(),
                     theme::Edge::top, [inputs = inputs_] {
                       return Graphical::time(inputs)
                           .padding(theme::space::lg)
                           .frame({.width = 260.0F});
                     }));
  }
  return components::HStack(
      components::HStackOptions{
          .alignment = components::VerticalAlignment::center,
          .spacing = theme::space::xs},
      std::move(chips));
}

auto DatePickerStyle::wheeled(Configuration configuration)
    -> composition::Element {
  const Inputs inputs = configuration.inputs;
  const composition::Binding<std::chrono::sys_seconds> selection =
      inputs.selection;
  const components::TimeZone zone = inputs.time_zone;
  const std::optional<Range> range = inputs.range;
  const auto field = [selection, zone, range](DateFormat::Field which) {
    return composition::Binding<int>{
        [selection, zone, which] {
          const std::chrono::year_month_day day = day_of(selection.get(), zone);
          switch (which) {
            case DateFormat::Field::year:
              return static_cast<int>(day.year());
            case DateFormat::Field::month:
              return static_cast<int>(static_cast<unsigned>(day.month()));
            case DateFormat::Field::day:
              break;
          }
          return static_cast<int>(static_cast<unsigned>(day.day()));
        },
        [selection, zone, range, which](int value) {
          const std::chrono::sys_seconds now = selection.get();
          const std::chrono::year_month_day day = day_of(now, zone);
          std::chrono::year_month_day next = day;
          switch (which) {
            case DateFormat::Field::year:
              next = std::chrono::year{value} / day.month() / day.day();
              break;
            case DateFormat::Field::month:
              next = day.year() /
                     std::chrono::month{static_cast<unsigned>(value)} /
                     day.day();
              break;
            case DateFormat::Field::day:
              next = day.year() / day.month() /
                     std::chrono::day{static_cast<unsigned>(value)};
              break;
          }
          selection.set(
              clamped(compose(next, minutes_of(now, zone), zone), range));
        }};
  };
  const std::chrono::year_month_day today = day_of(selection.get(), zone);
  const int year = static_cast<int>(today.year());
  const unsigned last = static_cast<unsigned>(std::chrono::year_month_day_last{
      today.year() / today.month() / std::chrono::last}
                                                  .day());
  std::vector<composition::Element> wheels;
  if (inputs.components.contains(DatePickerComponents::date)) {
    for (const DateFormat::Field which : DateFormat::order(inputs.locale)) {
      Picker::Builder options;
      std::string_view title = "Day";
      switch (which) {
        case DateFormat::Field::year:
          title = "Year";
          options = [year] {
            std::vector<composition::Element> years;
            for (int each = year - 5; each <= year + 5; ++each) {
              years.push_back(
                  components::Text(std::format("{}", each)).tag(each));
            }
            return composition::Element{components::Group(std::move(years))};
          };
          break;
        case DateFormat::Field::month:
          title = "Month";
          options = [locale = inputs.locale] {
            std::vector<composition::Element> months;
            for (unsigned each = 1; each <= 12; ++each) {
              months.push_back(
                  components::Text(std::string{DateFormat::month_name(
                                       std::chrono::month{each}, true, locale)})
                      .tag(static_cast<int>(each)));
            }
            return composition::Element{components::Group(std::move(months))};
          };
          break;
        case DateFormat::Field::day:
          options = [last] {
            std::vector<composition::Element> days;
            for (unsigned each = 1; each <= last; ++each) {
              days.push_back(components::Text(std::format("{}", each))
                                 .tag(static_cast<int>(each)));
            }
            return composition::Element{components::Group(std::move(days))};
          };
          break;
      }
      wheels.push_back(composition::Element{
          Picker(std::string{title}, field(which), std::move(options))}
                           .picker_style(PickerStyle::wheel())
                           .labels_hidden()
                           .frame({.max_width = composition::infinity}));
    }
  }
  if (inputs.components.contains(DatePickerComponents::hour_and_minute)) {
    wheels.push_back(Graphical::time(inputs));
  }
  return labelled(std::move(configuration.label),
                  components::HStack(
                      components::HStackOptions{
                          .alignment = components::VerticalAlignment::center,
                          .spacing = theme::space::sm},
                      std::move(wheels))
                      .frame({.max_width = composition::infinity}),
                  true);
}

}
