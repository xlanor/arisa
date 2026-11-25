#pragma once

#include <arisa/composition/binding.hpp>
#include <arisa/composition/dialog_severity.hpp>
#include <arisa/composition/inspector_column_width.hpp>
#include <arisa/composition/presentation_adaptation.hpp>
#include <arisa/composition/presentation_background_interaction.hpp>
#include <arisa/composition/presentation_content_interaction.hpp>
#include <arisa/composition/presentation_detent.hpp>
#include <arisa/composition/presentation_placement.hpp>
#include <arisa/composition/presentation_sizing.hpp>
#include <arisa/composition/visibility.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace arisa::composition {

class Element;

struct PresentationOptions {
  struct Confirmation {
    std::string title;
    bool should_present{true};
    std::function<Element()> actions;
    std::function<Element()> message;
  };

  struct Suppression {
    std::string title;
    Binding<bool> is_suppressed;

    friend auto operator==(const Suppression& left, const Suppression& right)
        -> bool {
      return left.title == right.title &&
             left.is_suppressed.identity() == right.is_suppressed.identity();
    }
  };

  std::vector<PresentationDetent> detents;
  std::optional<Binding<PresentationDetent>> selection;
  Visibility drag_indicator{Visibility::automatic};
  PresentationContentInteraction content_interaction{
      PresentationContentInteraction::automatic};
  std::optional<float> corner_radius;
  PresentationBackgroundInteraction background_interaction{
      PresentationBackgroundInteraction::automatic};
  PresentationSizing sizing{PresentationSizing::automatic};
  PresentationAdaptation compact_adaptation{PresentationAdaptation::automatic};
  PresentationAdaptation vertical_adaptation{PresentationAdaptation::automatic};
  PresentationPlacement placement{PresentationPlacement::automatic};
  std::optional<InspectorColumnWidth> inspector_width;
  std::optional<Confirmation> dismissal_confirmation;
  std::shared_ptr<Element> dialog_icon;
  DialogSeverity dialog_severity{DialogSeverity::automatic};
  std::optional<Suppression> suppression;
};

}
