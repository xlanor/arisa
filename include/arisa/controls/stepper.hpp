#pragma once

#include <arisa/components/control_size.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/move_command_direction.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/controls/closed_range.hpp>
#include <arisa/controls/number_format.hpp>
#include <arisa/theme/metrics.hpp>

#include <concepts>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace arisa::controls {

class Stepper : public composition::Element {
 public:
  class Content;

  using Adjust = std::function<void(int)>;
  using OnEditingChanged = std::function<void(bool)>;

  static constexpr float segment_width = theme::metrics::stepper.segment_width;
  static constexpr float segment_height = theme::metrics::stepper.height;
  static constexpr float glyph_extent = 12.0F;
  static constexpr std::string_view lower_key{"arisa.stepper.lower"};
  static constexpr std::string_view raise_key{"arisa.stepper.raise"};

  template <composition::BindingSource Source>
    requires std::is_arithmetic_v<composition::BoundValue<Source>>
  Stepper(
      std::string title, Source source,
      ClosedRange<composition::BoundValue<Source>> bounds,
      composition::BoundValue<Source> step = composition::BoundValue<Source>{1})
      : Stepper(std::move(title),
                composition::Binding<composition::BoundValue<Source>>{
                    std::move(source)},
                bounds, step) {}

  template <class T>
    requires std::is_arithmetic_v<T>
  Stepper(std::string title, composition::Binding<T> value,
          ClosedRange<T> bounds, T step,
          OnEditingChanged on_editing_changed = {})
      : Stepper(Parts{.title = std::move(title),
                      .label = nullptr,
                      .shown = {},
                      .adjust = stepped(value, bounds, step,
                                        std::move(on_editing_changed)),
                      .lowers = value.get() > bounds.lower_bound,
                      .raises = value.get() < bounds.upper_bound}) {}

  template <class T>
    requires std::is_arithmetic_v<T>
  Stepper(std::string title, composition::Binding<T> value, T step,
          OnEditingChanged on_editing_changed = {})
      : Stepper(std::move(title), std::move(value), unbounded<T>(), step,
                std::move(on_editing_changed)) {}

  template <class T>
    requires FormattedNumber<T>
  Stepper(std::string title, composition::Binding<T> value,
          ClosedRange<T> bounds, T step, NumberFormat format,
          OnEditingChanged on_editing_changed = {})
      : Stepper(Parts{.title = std::move(title),
                      .label = nullptr,
                      .shown = formatted(value, format),
                      .adjust = stepped(value, bounds, step,
                                        std::move(on_editing_changed)),
                      .lowers = value.get() > bounds.lower_bound,
                      .raises = value.get() < bounds.upper_bound}) {}

  template <class T>
    requires FormattedNumber<T>
  Stepper(std::string title, composition::Binding<T> value, T step,
          NumberFormat format, OnEditingChanged on_editing_changed = {})
      : Stepper(std::move(title), std::move(value), unbounded<T>(), step,
                format, std::move(on_editing_changed)) {}

  template <class T>
    requires std::is_arithmetic_v<T>
  Stepper(composition::Binding<T> value, ClosedRange<T> bounds, T step,
          composition::Element label, OnEditingChanged on_editing_changed = {})
      : Stepper(Parts{.title = {},
                      .label = source(std::move(label)),
                      .shown = {},
                      .adjust = stepped(value, bounds, step,
                                        std::move(on_editing_changed)),
                      .lowers = value.get() > bounds.lower_bound,
                      .raises = value.get() < bounds.upper_bound}) {}

  template <class T>
    requires std::is_arithmetic_v<T>
  Stepper(composition::Binding<T> value, T step, composition::Element label,
          OnEditingChanged on_editing_changed = {})
      : Stepper(std::move(value), unbounded<T>(), step, std::move(label),
                std::move(on_editing_changed)) {}

  template <class T>
    requires FormattedNumber<T>
  Stepper(composition::Binding<T> value, ClosedRange<T> bounds, T step,
          NumberFormat format, composition::Element label,
          OnEditingChanged on_editing_changed = {})
      : Stepper(Parts{.title = {},
                      .label = source(std::move(label)),
                      .shown = formatted(value, format),
                      .adjust = stepped(value, bounds, step,
                                        std::move(on_editing_changed)),
                      .lowers = value.get() > bounds.lower_bound,
                      .raises = value.get() < bounds.upper_bound}) {}

  template <class T>
    requires FormattedNumber<T>
  Stepper(composition::Binding<T> value, T step, NumberFormat format,
          composition::Element label, OnEditingChanged on_editing_changed = {})
      : Stepper(std::move(value), unbounded<T>(), step, format,
                std::move(label), std::move(on_editing_changed)) {}

  Stepper(std::string title, std::function<void()> on_increment,
          std::function<void()> on_decrement,
          std::function<void(bool)> on_editing_changed = {});

  Stepper(composition::Element label, std::function<void()> on_increment,
          std::function<void()> on_decrement,
          std::function<void(bool)> on_editing_changed = {});

  template <class T>
    requires std::is_arithmetic_v<T>
  static constexpr auto unbounded() noexcept -> ClosedRange<T> {
    return ClosedRange<T>{.lower_bound = std::numeric_limits<T>::lowest(),
                          .upper_bound = std::numeric_limits<T>::max()};
  }

 private:
  using Source = std::shared_ptr<composition::PlaceholderContentView::Source>;
  using Shown = std::function<std::string()>;

  struct Parts {
    std::string title;
    Source label;
    Shown shown;
    Adjust adjust;
    bool lowers{true};
    bool raises{true};
  };

  explicit Stepper(Parts parts);

  static auto source(composition::Element label) -> Source;

  template <class T>
    requires std::is_arithmetic_v<T>
  static auto stepped(composition::Binding<T> value, ClosedRange<T> bounds,
                      T step, OnEditingChanged on_editing_changed) -> Adjust {
    return [value, bounds, step,
            changed = std::move(on_editing_changed)](int direction) {
      if (changed) {
        changed(true);
      }
      const T current = bounds.clamped(value.get());
      if (direction < 0) {
        value.set(current - bounds.lower_bound < step
                      ? bounds.lower_bound
                      : static_cast<T>(current - step));
      } else {
        value.set(bounds.upper_bound - current < step
                      ? bounds.upper_bound
                      : static_cast<T>(current + step));
      }
      if (changed) {
        changed(false);
      }
    };
  }

  template <class T>
    requires FormattedNumber<T>
  static auto formatted(composition::Binding<T> value, NumberFormat format)
      -> Shown {
    return [value, format] { return format.format(value.get()); };
  }

  static auto adjusted(std::function<void()> on_increment,
                       std::function<void()> on_decrement,
                       std::function<void(bool)> on_editing_changed) -> Adjust;

  Stepper(std::string title, Adjust adjust, bool lowers, bool raises);
};

class Stepper::Content final : public composition::ElementContent {
 public:
  static constexpr composition::StateKey<std::int64_t> steps{"stepper-steps",
                                                             0};

  using Source = std::shared_ptr<composition::PlaceholderContentView::Source>;
  using Shown = std::function<std::string()>;

  Content(std::string title, Adjust adjust, bool lowers, bool raises,
          Source label = nullptr, Shown shown = {}) noexcept
      : title_(std::move(title)),
        adjust_(std::move(adjust)),
        lowers_(lowers),
        raises_(raises),
        label_(std::move(label)),
        shown_(std::move(shown)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto framed() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

 private:
  auto segment(bool minus, bool available,
               composition::StateHandle<std::int64_t> taken,
               components::ControlSize size) const -> composition::Element;

  auto title_label() const -> composition::Element;

  std::string title_;
  Adjust adjust_;
  bool lowers_;
  bool raises_;
  Source label_;
  Shown shown_;
};

}
