#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/composition/navigation_destination.hpp>
#include <arisa/composition/presentation_background.hpp>
#include <arisa/presentation/help.hpp>
#include <arisa/presentation/inspector.hpp>
#include <arisa/presentation/presentations.hpp>

#include <functional>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace arisa::attr {

class NavigationTitle {
 public:
  explicit NavigationTitle(std::string title) : title_(std::move(title)) {}

  auto apply(composition::Element element) && -> composition::Element {
    return std::move(element).navigation_title(std::move(title_));
  }

 private:
  std::string title_;
};

class InteractiveDismissDisabled {
 public:
  constexpr explicit InteractiveDismissDisabled(bool locked = true) noexcept
      : disabled_(locked) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).interactive_dismiss_disabled(disabled_);
  }

 private:
  bool disabled_;
};

class PresentationBackground {
 public:
  explicit PresentationBackground(theme::Material material) noexcept
      : background_(material) {}

  explicit PresentationBackground(theme::Glass glass) noexcept
      : background_(glass) {}

  explicit PresentationBackground(theme::Fill style) noexcept
      : background_(style) {}

  explicit PresentationBackground(
      composition::PresentationBackground chosen) noexcept
      : background_(chosen) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::visit(
        [&element](const auto& style) {
          return std::move(element).presentation_background(style);
        },
        background_.style());
  }

 private:
  composition::PresentationBackground background_;
};

class Presented {
 public:
  using Write =
      std::move_only_function<composition::Element(composition::Element)>;

  explicit Presented(Write write) noexcept : write_(std::move(write)) {}

  auto apply(composition::Element element) && -> composition::Element {
    return write_(std::move(element));
  }

 private:
  Write write_;
};

class NavigationDestination : public Presented {
 public:
  template <class First, class Build>
  NavigationDestination(First first, Build build)
      : Presented([first = std::move(first), held = std::move(build)](
                      composition::Element element) mutable {
          return std::move(element).navigation_destination(std::move(first),
                                                           std::move(held));
        }) {}
};

class Sheet : public Presented {
 public:
  template <class... Arguments>
    requires(sizeof...(Arguments) >= 2)
  explicit Sheet(Arguments... arguments)
      : Presented([... held = std::move(arguments)](
                      composition::Element element) mutable {
          return std::move(element).sheet(std::move(held)...);
        }) {}
};

class FullScreenCover : public Presented {
 public:
  template <class... Arguments>
    requires(sizeof...(Arguments) >= 2)
  explicit FullScreenCover(Arguments... arguments)
      : Presented([... held = std::move(arguments)](
                      composition::Element element) mutable {
          return std::move(element).full_screen_cover(std::move(held)...);
        }) {}
};

class Alert : public Presented {
 public:
  template <class... Arguments>
    requires(sizeof...(Arguments) >= 2)
  explicit Alert(Arguments... arguments)
      : Presented([... held = std::move(arguments)](
                      composition::Element element) mutable {
          return std::move(element).alert(std::move(held)...);
        }) {}
};

class ConfirmationDialog : public Presented {
 public:
  template <class... Arguments>
    requires(sizeof...(Arguments) >= 2)
  explicit ConfirmationDialog(Arguments... arguments)
      : Presented([... held = std::move(arguments)](
                      composition::Element element) mutable {
          return std::move(element).confirmation_dialog(std::move(held)...);
        }) {}
};

class Popover : public Presented {
 public:
  template <class... Arguments>
    requires(sizeof...(Arguments) >= 2)
  explicit Popover(Arguments... arguments)
      : Presented([... held = std::move(arguments)](
                      composition::Element element) mutable {
          return std::move(element).popover(std::move(held)...);
        }) {}
};

class Toast : public Presented {
 public:
  template <class... Arguments>
    requires(sizeof...(Arguments) >= 2)
  explicit Toast(Arguments... arguments)
      : Presented([... held = std::move(arguments)](
                      composition::Element element) mutable {
          return std::move(element).toast(std::move(held)...);
        }) {}
};

class PresentationDetents : public Presented {
 public:
  explicit PresentationDetents(
      std::vector<composition::PresentationDetent> detents)
      : Presented(
            [held = std::move(detents)](composition::Element element) mutable {
              return std::move(element).presentation_detents(std::move(held));
            }) {}

  PresentationDetents(
      std::vector<composition::PresentationDetent> detents,
      composition::Binding<composition::PresentationDetent> selection)
      : Presented([held = std::move(detents), chosen = std::move(selection)](
                      composition::Element element) mutable {
          return std::move(element).presentation_detents(std::move(held),
                                                         std::move(chosen));
        }) {}
};

class PresentationDragIndicator {
 public:
  constexpr explicit PresentationDragIndicator(
      composition::Visibility visibility) noexcept
      : visibility_(visibility) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).presentation_drag_indicator(visibility_);
  }

 private:
  composition::Visibility visibility_;
};

class PresentationContentInteractionAttribute {
 public:
  constexpr explicit PresentationContentInteractionAttribute(
      composition::PresentationContentInteraction behavior) noexcept
      : behavior_(behavior) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).presentation_content_interaction(behavior_);
  }

 private:
  composition::PresentationContentInteraction behavior_;
};

class PresentationCornerRadius {
 public:
  constexpr explicit PresentationCornerRadius(
      std::optional<float> radius) noexcept
      : radius_(radius) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).presentation_corner_radius(radius_);
  }

 private:
  std::optional<float> radius_;
};

class PresentationBackgroundInteractionAttribute {
 public:
  constexpr explicit PresentationBackgroundInteractionAttribute(
      composition::PresentationBackgroundInteraction interaction) noexcept
      : interaction_(interaction) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).presentation_background_interaction(interaction_);
  }

 private:
  composition::PresentationBackgroundInteraction interaction_;
};

class PresentationSizingAttribute {
 public:
  constexpr explicit PresentationSizingAttribute(
      composition::PresentationSizing sizing) noexcept
      : sizing_(sizing) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).presentation_sizing(sizing_);
  }

 private:
  composition::PresentationSizing sizing_;
};

class PresentationCompactAdaptation {
 public:
  constexpr explicit PresentationCompactAdaptation(
      composition::PresentationAdaptation adaptation) noexcept
      : adaptation_(adaptation) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).presentation_compact_adaptation(adaptation_);
  }

 private:
  composition::PresentationAdaptation adaptation_;
};

class InspectorColumnWidthAttribute {
 public:
  constexpr explicit InspectorColumnWidthAttribute(float width) noexcept
      : width_(width) {}

  constexpr InspectorColumnWidthAttribute(std::optional<float> minimum,
                                          float ideal,
                                          std::optional<float> maximum) noexcept
      : width_(minimum, ideal, maximum) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).inspector_column_width(width_.resolved());
  }

 private:
  composition::InspectorColumnWidth width_;
};

class DialogIcon : public Presented {
 public:
  explicit DialogIcon(composition::Element icon)
      : Presented(
            [held = std::move(icon)](composition::Element element) mutable {
              return std::move(element).dialog_icon(std::move(held));
            }) {}
};

class DialogSeverityAttribute {
 public:
  constexpr explicit DialogSeverityAttribute(
      composition::DialogSeverity severity) noexcept
      : severity_(severity) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).dialog_severity(severity_);
  }

 private:
  composition::DialogSeverity severity_;
};

class DialogSuppressionToggle : public Presented {
 public:
  template <class... Arguments>
    requires(sizeof...(Arguments) >= 1)
  explicit DialogSuppressionToggle(Arguments... arguments)
      : Presented([... held = std::move(arguments)](
                      composition::Element element) mutable {
          return std::move(element).dialog_suppression_toggle(
              std::move(held)...);
        }) {}
};

class DismissalConfirmationDialog : public Presented {
 public:
  template <class... Arguments>
    requires(sizeof...(Arguments) >= 3)
  explicit DismissalConfirmationDialog(Arguments... arguments)
      : Presented([... held = std::move(arguments)](
                      composition::Element element) mutable {
          return std::move(element).dismissal_confirmation_dialog(
              std::move(held)...);
        }) {}
};

class HelpAttribute : public Presented {
 public:
  explicit HelpAttribute(std::string text)
      : Presented(
            [held = std::move(text)](composition::Element element) mutable {
              return std::move(element).help(std::move(held));
            }) {}

  HelpAttribute(std::string title, std::string body)
      : Presented([title = std::move(title), body = std::move(body)](
                      composition::Element element) mutable {
          return std::move(element).help(std::move(title), std::move(body));
        }) {}
};

class InspectorAttribute : public Presented {
 public:
  template <class Content>
  InspectorAttribute(composition::Binding<bool> is_presented, Content content)
      : Presented([shown = std::move(is_presented), held = std::move(content)](
                      composition::Element element) mutable {
          return std::move(element).inspector(std::move(shown),
                                              std::move(held));
        }) {}
};

using navigation_title = NavigationTitle;
using navigation_destination = NavigationDestination;
using interactive_dismiss_disabled = InteractiveDismissDisabled;
using presentation_background = PresentationBackground;
using sheet = Sheet;
using full_screen_cover = FullScreenCover;
using alert = Alert;
using confirmation_dialog = ConfirmationDialog;
using popover = Popover;
using toast = Toast;
using presentation_detents = PresentationDetents;
using presentation_drag_indicator = PresentationDragIndicator;
using presentation_content_interaction =
    PresentationContentInteractionAttribute;
using presentation_corner_radius = PresentationCornerRadius;
using presentation_background_interaction =
    PresentationBackgroundInteractionAttribute;
using presentation_sizing = PresentationSizingAttribute;
using presentation_compact_adaptation = PresentationCompactAdaptation;
using inspector_column_width = InspectorColumnWidthAttribute;
using dialog_icon = DialogIcon;
using dialog_severity = DialogSeverityAttribute;
using dialog_suppression_toggle = DialogSuppressionToggle;
using dismissal_confirmation_dialog = DismissalConfirmationDialog;
using help = HelpAttribute;

}
