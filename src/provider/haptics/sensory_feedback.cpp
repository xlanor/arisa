#include <arisa/provider/haptics/sensory_feedback.hpp>

#include <array>

namespace arisa::provider {

auto SensoryFeedback::catalogue() noexcept -> std::span<const SensoryFeedback> {
  static constexpr std::array all{
      selection,
      impact(Weight::light),
      impact(Weight::medium),
      impact(Weight::heavy),
      impact(Flexibility::rigid),
      impact(Flexibility::solid),
      impact(Flexibility::soft),
      increase,
      decrease,
      success,
      warning,
      error,
      start,
      stop,
      alignment,
      level_change,
      path_complete,
      selection_feedback(SelectionFeedback::on),
      selection_feedback(SelectionFeedback::off),
      selection_feedback(SelectionFeedback::minimum),
      selection_feedback(SelectionFeedback::maximum),
      press(PressFeedback::button),
      press(PressFeedback::button_icon_only),
      press(PressFeedback::slider),
      press(PressFeedback::toggle),
      press(PressFeedback::tab),
      release(ReleaseFeedback::slider)};
  return all;
}

auto SensoryFeedback::name() const noexcept -> std::string_view {
  switch (kind_) {
    case Kind::selection:
      switch (selection_variant().value_or(SelectionFeedback::on)) {
        case SelectionFeedback::on:
          return detail_ == 0 ? "Selection" : "Selection on";
        case SelectionFeedback::off:
          return "Selection off";
        case SelectionFeedback::minimum:
          return "Selection minimum";
        case SelectionFeedback::maximum:
          return "Selection maximum";
      }
      return "Selection";
    case Kind::impact:
      if (flexibility_ == Flexibility::rigid) {
        return "Impact rigid";
      }
      if (flexibility_ == Flexibility::solid) {
        return "Impact solid";
      }
      if (flexibility_ == Flexibility::soft) {
        return "Impact soft";
      }
      if (weight_ == Weight::light) {
        return "Impact light";
      }
      if (weight_ == Weight::heavy) {
        return "Impact heavy";
      }
      return "Impact medium";
    case Kind::increase:
      return "Increase";
    case Kind::decrease:
      return "Decrease";
    case Kind::success:
      return "Success";
    case Kind::warning:
      return "Warning";
    case Kind::error:
      return "Error";
    case Kind::start:
      return "Start";
    case Kind::stop:
      return "Stop";
    case Kind::alignment:
      return "Alignment";
    case Kind::level_change:
      return "Level change";
    case Kind::path_complete:
      return "Path complete";
    case Kind::press:
      switch (press_feedback().value_or(PressFeedback::button)) {
        case PressFeedback::button:
          return "Press button";
        case PressFeedback::button_icon_only:
          return "Press icon button";
        case PressFeedback::slider:
          return "Press slider";
        case PressFeedback::toggle:
          return "Press toggle";
        case PressFeedback::tab:
          return "Press tab";
      }
      return "Press button";
    case Kind::release:
      return "Release slider";
  }
  return "Selection";
}

}
