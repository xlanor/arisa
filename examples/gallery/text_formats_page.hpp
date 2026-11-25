#pragma once

#include <arisa/components/attributed_string.hpp>
#include <arisa/components/timed_wake.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>

#include <string_view>

namespace arisa::examples {

class TextFormatsPage final : public composition::Component {
 public:
  class Opener;

  using Date = components::TimedWake::Date;

  static constexpr std::string_view close_button = "formats-close";
  static constexpr std::string_view timer_text = "formats-timer";
  static constexpr std::string_view relative_text = "formats-relative";
  static constexpr std::string_view offset_text = "formats-offset";
  static constexpr std::string_view stopwatch_text = "formats-stopwatch";
  static constexpr std::string_view runs_text = "formats-runs";
  static constexpr std::string_view count_button = "formats-count";

  static constexpr composition::StateKey<int> items{"formats-items", 3};

  explicit TextFormatsPage(composition::StateHandle<bool> shown) noexcept
      : shown_(std::move(shown)) {}

  auto operator==(const TextFormatsPage& other) const -> bool {
    return shown_.identity() == other.shown_.identity();
  }

  auto body(composition::Context& context) const
      -> composition::Element override;

  static auto styled_runs() -> components::AttributedString;

 private:
  static auto section(std::string_view title, composition::Element content)
      -> composition::Element;

  static auto live(Date now) -> composition::Element;

  static auto formats(composition::StateHandle<int> count)
      -> composition::Element;

  static auto runs() -> composition::Element;

  static auto lines() -> composition::Element;

  composition::StateHandle<bool> shown_;
};

class TextFormatsPage::Opener final : public composition::Component {
 public:
  static constexpr composition::StateKey<bool> shown_key{"formats-shown",
                                                         false};
  static constexpr std::string_view button = "formats-open";

  auto operator==(const Opener&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;
};

}
