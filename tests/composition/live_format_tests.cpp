#include <arisa/components/date_interval.hpp>
#include <arisa/components/system_format_style.hpp>
#include <arisa/components/time_data_source.hpp>
#include <arisa/components/timeline_schedule.hpp>
#include <arisa/text/locale.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <format>
#include <fstream>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace arisa::components {

using namespace std::chrono_literals;

class LiveFormatTest : public ::testing::Test {
 protected:
  using Date = SystemFormatStyle::Date;
  using Key = std::tuple<std::string, std::string, std::string>;

  static constexpr std::chrono::seconds reference_now{1778307200};

  static auto now() -> Date { return Date{reference_now}; }

  static auto rows() -> const std::map<Key, std::string>& {
    static const std::map<Key, std::string> loaded = [] {
      std::map<Key, std::string> found;
      std::ifstream stream{ARISA_SWIFTUI_FORMATS};
      std::string line;
      std::getline(stream, line);
      while (std::getline(stream, line)) {
        std::vector<std::string> cells;
        std::size_t start = 0;
        for (std::size_t at = 0; at <= line.size(); ++at) {
          if (at == line.size() || line[at] == '\t') {
            cells.push_back(line.substr(start, at - start));
            start = at + 1;
          }
        }
        if (cells.size() >= 5 && cells[0] == "en") {
          found.emplace(Key{cells[0], cells[1], cells[2]}, cells[4]);
        }
      }
      return found;
    }();
    return loaded;
  }

  static auto expected(std::string_view kind, std::string_view name)
      -> std::string {
    const auto found =
        rows().find(Key{"en", std::string{kind}, std::string{name}});
    return found == rows().end() ? std::string{"<missing>"} : found->second;
  }

  static auto seconds_until(std::optional<Date> next) -> std::string {
    if (!next.has_value()) {
      return "none";
    }
    const double value =
        std::chrono::duration_cast<std::chrono::duration<double>>(*next - now())
            .count();
    return std::format("{:.3f}", value);
  }

  static auto english() -> text::Locale { return *text::Locale::create("en"); }

  static auto cases() -> std::vector<std::pair<std::string, Date>> {
    return {{"future_2h5m7s", now() + 2h + 5min + 7s},
            {"past_40s", now() - 40s},
            {"past_3d1h", now() - std::chrono::days{3} - 1h},
            {"future_95s", now() + 95s},
            {"past_400d", now() - std::chrono::days{400}}};
  }
};

TEST_F(LiveFormatTest, DateOffsetMatchesFoundation) {
  ASSERT_FALSE(rows().empty());
  for (const auto& [name, anchor] : cases()) {
    const SystemFormatStyle::DateOffset offset{anchor};
    EXPECT_EQ(offset.format(now(), english()), expected("live_offset", name))
        << name;
    const SystemFormatStyle::DateOffset single{
        anchor, SystemFormatStyle::Fields::all, 1};
    EXPECT_EQ(single.format(now(), english()),
              expected("live_offset_one", name))
        << name;
  }
  const auto listed = cases();
  for (const std::string_view name : {"past_40s", "past_3d1h"}) {
    const auto found = std::ranges::find(listed, std::string{name},
                                         &std::pair<std::string, Date>::first);
    EXPECT_EQ(
        seconds_until(
            SystemFormatStyle::DateOffset{found->second}.discrete_input(now())),
        expected("live_offset_next", name))
        << name;
  }
  const SystemFormatStyle::DateOffset always{now() - std::chrono::days{3} - 1h,
                                             SystemFormatStyle::Fields::all, 2,
                                             SystemFormatStyle::Sign::always};
  EXPECT_EQ(always.format(now(), english()), "+3 days, 1 hour");
  EXPECT_EQ(always.locale(english()).format(now()), "+3 days, 1 hour");
  EXPECT_EQ(always.calendar(Calendar{}).format(now(), english()),
            "+3 days, 1 hour");
  EXPECT_LT(*always.discrete_input_before(now()), now());
  EXPECT_EQ(*always.input(now()), now() + SystemFormatStyle::step);
  EXPECT_EQ(*always.input_before(now()), now() - SystemFormatStyle::step);
  EXPECT_EQ(always, always);
}

TEST_F(LiveFormatTest, DateReferenceMatchesFoundation) {
  for (const auto& [name, anchor] : cases()) {
    const SystemFormatStyle::DateReference reference{anchor};
    EXPECT_EQ(reference.format(now(), english()),
              expected("live_reference", name))
        << name;
  }
  const SystemFormatStyle::DateReference soon{now() + 2h + 5min + 7s};
  EXPECT_EQ(seconds_until(soon.discrete_input(now())),
            expected("live_reference_next", "future_2h5m7s"));
  const SystemFormatStyle::DateReference minute{now() - 40s};
  EXPECT_EQ(seconds_until(minute.discrete_input(now())),
            expected("live_reference_next", "past_40s"));
  EXPECT_LT(*minute.discrete_input_before(now()), now());
  EXPECT_EQ(*minute.input(now()), now() + SystemFormatStyle::step);
  EXPECT_EQ(*minute.input_before(now()), now() - SystemFormatStyle::step);
  EXPECT_EQ(minute.locale(english()).calendar(Calendar{}).format(now()),
            "1 minute ago");
  EXPECT_EQ(
      SystemFormatStyle::DateReference{now() - 5s}.format(now(), english()),
      "now");
  EXPECT_EQ(minute, minute);
}

TEST_F(LiveFormatTest, TimersAndStopwatchesMatchFoundation) {
  for (const auto& [name, anchor] : cases()) {
    const SystemFormatStyle::Timer::Interval range{
        .start = std::min(anchor, now()), .end = std::max(anchor, now()) + 1s};
    const auto down = SystemFormatStyle::Timer::counting_down(range);
    EXPECT_EQ(down.format(now(), english()), expected("live_timer_down", name))
        << name;
    EXPECT_EQ(seconds_until(down.discrete_input(now())),
              expected("live_timer_down_next", name))
        << name;
    EXPECT_EQ(
        SystemFormatStyle::Timer::counting_up(range).format(now(), english()),
        expected("live_timer_up", name))
        << name;
    EXPECT_EQ(SystemFormatStyle::Timer::counting_up(range, false)
                  .format(now(), english()),
              expected("live_timer_no_hours", name))
        << name;
  }
  const SystemFormatStyle::Stopwatch watch{
      now() - std::chrono::duration_cast<Date::duration>(
                  std::chrono::duration<double>{65.3})};
  EXPECT_EQ(watch.format(now(), english()), "01:05.30");
  const SystemFormatStyle::Stopwatch coarse{
      now() - std::chrono::duration_cast<Date::duration>(
                  std::chrono::duration<double>{3725.3}),
      true, 4, SystemFormatStyle::Duration{1.0}};
  EXPECT_EQ(coarse.format(now(), english()),
            expected("live_stopwatch", "hours_seconds"));
  const SystemFormatStyle::Stopwatch tenths{now() - 5250ms, true, 4,
                                            SystemFormatStyle::Duration{0.1}};
  EXPECT_EQ(tenths.format(now(), english()),
            expected("live_stopwatch", "tenths"));
  EXPECT_EQ(*coarse.discrete_input(now()) - now(), 700ms);
  EXPECT_LT(*coarse.discrete_input_before(now()), now());
  EXPECT_EQ(coarse.locale(english()).format(now()), "01:02:05");
  const SystemFormatStyle::Stopwatch fields{now() - 3725s, true, 2};
  EXPECT_EQ(fields.format(now(), english()), "1 hour, 2 minutes");
  const auto paused = SystemFormatStyle::Timer::counting_down(
      {.start = now(), .end = now() + 100s});
  EXPECT_FALSE(paused.discrete_input(now() + 200s).has_value());
  EXPECT_EQ(paused.discrete_input_before(now() + 10s).value() - now(), 9s);
  EXPECT_EQ(paused, paused);
  EXPECT_EQ(paused.locale(english()).format(now()), "1:40");
}

TEST_F(LiveFormatTest, TimeDataSourcesMapNowToTheirValues) {
  const auto current = TimeDataSource<Date>::current_date();
  EXPECT_EQ(current.value(now()), now());
  const auto offset =
      TimeDataSource<SystemFormatStyle::Duration>::duration_offset(now() - 10s);
  EXPECT_DOUBLE_EQ(offset.value(now()).count(), 10.0);
  EXPECT_EQ(offset.date(SystemFormatStyle::Duration{12.0}), now() + 2s);
  const auto starting =
      TimeDataSource<DateInterval>::date_range_starting_at(now() - 60s);
  EXPECT_EQ(starting.value(now()).start, now() - 60s);
  EXPECT_EQ(starting.value(now()).end, now());
  const auto ending =
      TimeDataSource<DateInterval>::date_range_ending_at(now() + 60s);
  EXPECT_EQ(ending.value(now()).start, now());
  EXPECT_EQ(ending.value(now()).end, now() + 60s);
  EXPECT_EQ(current, current);
}

TEST_F(LiveFormatTest, IntervalsDurationsAndScheduleModes) {
  const DateInterval meeting{.start = now(), .end = now() + 1h};
  const std::string shown =
      DateIntervalFormatStyle{}.format(meeting, english());
  EXPECT_NE(shown.find(DateIntervalFormatStyle::separator), std::string::npos);
  EXPECT_TRUE(DateIntervalFormatStyle{}.discrete_input(meeting).has_value());
  EXPECT_EQ(meeting.duration(), std::chrono::system_clock::duration{1h});
  EXPECT_EQ(DurationFormatStyle{}.format(SystemFormatStyle::Duration{3725.0},
                                         english()),
            "1:02:05");
  EXPECT_EQ(
      DurationFormatStyle{DurationFormatStyle::Pattern::units_wide}.format(
          SystemFormatStyle::Duration{3725.0}, english()),
      "1 hour, 2 minutes");
  EXPECT_EQ(DurationFormatStyle{}
                .discrete_input(SystemFormatStyle::Duration{4.5})
                ->count(),
            5.0);
  const TimelineSchedule fast = TimelineSchedule::animation(1s);
  EXPECT_EQ(*fast.next(now(), TimelineScheduleMode::normal), now() + 1s);
  EXPECT_EQ(*fast.next(now(), TimelineScheduleMode::low_frequency),
            now() + TimelineSchedule::low_frequency_interval);
  EXPECT_NE(TimelineScheduleMode::normal, TimelineScheduleMode::low_frequency);
}

TEST_F(LiveFormatTest, PhrasesJoinPartsInTheLocale) {
  using Part = SystemFormatStyle::Part;
  using Unit = text::LocaleData::Unit;
  const std::vector<Part> parts{Part{.unit = Unit::hour, .value = 2},
                                Part{.unit = Unit::minute, .value = 5}};
  EXPECT_EQ(SystemFormatStyle::phrase(parts, english(), false),
            "2 hours, 5 minutes");
  EXPECT_EQ(
      SystemFormatStyle::phrase(parts, *text::Locale::create("fr"), false),
      "2\u00A0heures et 5 minutes");
  EXPECT_EQ(
      SystemFormatStyle::phrase(std::span{parts}.first(1), english(), true),
      "2h");
}

}
