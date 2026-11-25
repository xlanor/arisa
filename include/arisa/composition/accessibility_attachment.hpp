#pragma once

#include <arisa/composition/accessibility.hpp>
#include <arisa/composition/accessibility_action.hpp>
#include <arisa/composition/accessibility_adjustment_direction.hpp>
#include <arisa/composition/accessibility_chart_descriptor.hpp>
#include <arisa/composition/accessibility_custom_content_key.hpp>
#include <arisa/composition/accessibility_direct_touch_options.hpp>
#include <arisa/composition/accessibility_labeled_pair_role.hpp>
#include <arisa/composition/accessibility_rotor.hpp>
#include <arisa/composition/accessibility_speech.hpp>
#include <arisa/composition/accessibility_text.hpp>
#include <arisa/composition/accessibility_zoom_gesture_action.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/namespace.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/theme/space.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace arisa::composition {

class Element;

struct AccessibilityAttachment {
  enum class Layer : std::uint8_t {
    representation,
    children,
    actions,
    label,
  };

  struct Location {
    std::variant<motion::UnitPoint, composition::Point> at;
    std::optional<AccessibilityText> description{};

    friend auto operator==(const Location&, const Location&) -> bool = default;
  };

  struct Pairing {
    ElementKey id;
    Namespace::ID space;

    friend auto operator==(const Pairing&, const Pairing&) -> bool = default;
  };

  struct LabeledPair {
    AccessibilityLabeledPairRole role;
    Pairing pairing;

    friend auto operator==(const LabeledPair&, const LabeledPair&)
        -> bool = default;
  };

  struct CustomContent {
    AccessibilityCustomContentKey key;
    AccessibilityText value;

    friend auto operator==(const CustomContent&, const CustomContent&)
        -> bool = default;
  };

  struct NavigationIcon {
    std::string system_image;
    bool resource{false};

    friend auto operator==(const NavigationIcon&, const NavigationIcon&)
        -> bool = default;
  };

  template <class Signature>
  using Handler = std::shared_ptr<const std::function<Signature>>;

  std::optional<AccessibilityText> label{};
  std::optional<AccessibilityText> value{};
  std::optional<AccessibilityText> hint{};
  std::optional<std::string> identifier{};
  AccessibilityTraits added{};
  AccessibilityTraits removed{};
  std::optional<AccessibilityHeadingLevel> heading{};
  std::optional<AccessibilityChildBehavior> children{};
  bool hidden{false};
  std::optional<AccessibilityTextContentType> text_content_type{};
  std::vector<AccessibilityText> input_labels{};
  std::optional<double> sort_priority{};
  std::vector<AccessibilityAction> actions{};
  std::optional<AccessibilityActionCategory> action_category{};
  Handler<void(AccessibilityAdjustmentDirection)> adjustable{};
  Handler<void(theme::Edge)> scroll{};
  Handler<void(AccessibilityZoomGestureAction)> zoom{};
  std::optional<Location> activation_point{};
  std::vector<Location> drag_points{};
  std::vector<Location> drop_points{};
  std::optional<bool> responds_to_user_interaction{};
  bool ignores_invert_colors{false};
  std::optional<AccessibilityDirectTouchOptions> direct_touch{};
  bool shows_large_content_viewer{false};
  Handler<Element()> large_content{};
  std::vector<AccessibilityRotor> rotors{};
  std::optional<Pairing> rotor_entry{};
  std::optional<LabeledPair> labeled_pair{};
  std::optional<Pairing> linked_group{};
  std::optional<AccessibilityChartDescriptor> chart{};
  std::optional<AccessibilityText> scroll_status{};
  std::optional<AccessibilitySpeech> speech{};
  std::vector<CustomContent> custom_content{};
  std::optional<NavigationIcon> navigation_icon{};
  std::optional<Layer> layer{};
  bool layer_content{false};
  bool label_placeholder{false};

  friend auto operator==(const AccessibilityAttachment&,
                         const AccessibilityAttachment&) -> bool = default;
};

}
