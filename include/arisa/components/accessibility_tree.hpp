#pragma once

#include <arisa/composition/accessibility.hpp>
#include <arisa/composition/accessibility_action.hpp>
#include <arisa/composition/accessibility_adjustment_direction.hpp>
#include <arisa/composition/accessibility_attachment.hpp>
#include <arisa/composition/accessibility_chart_descriptor.hpp>
#include <arisa/composition/accessibility_direct_touch_options.hpp>
#include <arisa/composition/accessibility_rotor.hpp>
#include <arisa/composition/accessibility_rotor_entry.hpp>
#include <arisa/composition/accessibility_speech.hpp>
#include <arisa/composition/accessibility_zoom_gesture_action.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/node_id.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/composition/tree.hpp>
#include <arisa/graphics/path.hpp>
#include <arisa/support/full_order_walk.hpp>
#include <arisa/text/locale.hpp>
#include <arisa/text/string_catalog.hpp>
#include <arisa/theme/space.hpp>

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::components {

class AccessibilityElement {
 public:
  class Action {
   public:
    Action(std::string name, composition::AccessibilityActionKind::Kind kind,
           std::optional<std::string> category,
           std::optional<composition::AccessibilityAction> handler,
           std::optional<composition::NodeId> target)
        : name_(std::move(name)),
          kind_(kind),
          category_(std::move(category)),
          handler_(std::move(handler)),
          target_(target) {}

    auto name() const noexcept -> std::string_view { return name_; }

    auto kind() const noexcept -> composition::AccessibilityActionKind::Kind {
      return kind_;
    }

    auto category() const noexcept -> const std::optional<std::string>& {
      return category_;
    }

    auto perform(composition::Tree& tree) const -> bool;

    auto renamed(std::string name, std::optional<std::string> category) const
        -> Action {
      return Action{std::move(name),
                    composition::AccessibilityActionKind::Kind::named,
                    std::move(category), handler_, target_};
    }

   private:
    std::string name_;
    composition::AccessibilityActionKind::Kind kind_;
    std::optional<std::string> category_;
    std::optional<composition::AccessibilityAction> handler_;
    std::optional<composition::NodeId> target_;
  };

  struct Location {
    composition::Point point{};
    std::string description;

    friend auto operator==(const Location&, const Location&) -> bool = default;
  };

  struct CustomContent {
    std::string id;
    std::string label;
    std::string value;

    friend auto operator==(const CustomContent&, const CustomContent&)
        -> bool = default;
  };

  struct Details {
    composition::NodeId anchor{0};
    std::vector<std::string> input_labels{};
    double sort_priority{0.0};
    std::vector<Action> actions{};
    bool adjustable{false};
    composition::AccessibilityAttachment::Handler<void(
        composition::AccessibilityAdjustmentDirection)>
        adjust{};
    composition::AccessibilityAttachment::Handler<void(theme::Edge)> scroll{};
    composition::AccessibilityAttachment::Handler<void(
        composition::AccessibilityZoomGestureAction)>
        zoom{};
    std::optional<composition::Point> activation_point{};
    std::vector<Location> drag_points{};
    std::vector<Location> drop_points{};
    bool responds_to_user_interaction{false};
    bool ignores_invert_colors{false};
    std::optional<composition::AccessibilityDirectTouchOptions> direct_touch{};
    bool shows_large_content_viewer{false};
    composition::AccessibilityAttachment::Handler<composition::Element()>
        large_content{};
    std::vector<composition::AccessibilityRotor> rotors{};
    std::optional<composition::AccessibilityAttachment::Pairing> rotor_entry{};
    std::optional<composition::AccessibilityAttachment::LabeledPair>
        labeled_pair{};
    std::optional<composition::AccessibilityAttachment::Pairing> linked_group{};
    std::optional<composition::AccessibilityChartDescriptor> chart{};
    std::optional<std::string> scroll_status{};
    composition::AccessibilitySpeech speech{};
    std::vector<CustomContent> custom_content{};
    std::optional<composition::AccessibilityAttachment::NavigationIcon>
        navigation_icon{};
    std::optional<composition::AccessibilityTextContentType>
        text_content_type{};
  };

  AccessibilityElement(
      composition::NodeId node, composition::Rect frame, std::string label,
      std::optional<std::string> value, std::optional<std::string> hint,
      std::optional<std::string> identifier,
      composition::AccessibilityTraits traits,
      std::optional<composition::AccessibilityHeadingLevel> heading,
      std::optional<graphics::Path> shape = std::nullopt)
      : AccessibilityElement(node, frame, std::move(label), std::move(value),
                             std::move(hint), std::move(identifier), traits,
                             heading, std::move(shape), Details{}) {}

  AccessibilityElement(
      composition::NodeId node, composition::Rect frame, std::string label,
      std::optional<std::string> value, std::optional<std::string> hint,
      std::optional<std::string> identifier,
      composition::AccessibilityTraits traits,
      std::optional<composition::AccessibilityHeadingLevel> heading,
      std::optional<graphics::Path> shape, Details details)
      : node_(node),
        frame_(frame),
        label_(std::move(label)),
        value_(std::move(value)),
        hint_(std::move(hint)),
        identifier_(std::move(identifier)),
        traits_(traits),
        heading_(heading),
        shape_(std::move(shape)),
        details_(std::move(details)) {}

  auto node() const noexcept -> composition::NodeId { return node_; }

  auto anchor() const noexcept -> composition::NodeId {
    return details_.anchor;
  }

  auto frame() const noexcept -> const composition::Rect& { return frame_; }

  auto label() const noexcept -> std::string_view { return label_; }

  auto value() const noexcept -> const std::optional<std::string>& {
    return value_;
  }

  auto hint() const noexcept -> const std::optional<std::string>& {
    return hint_;
  }

  auto identifier() const noexcept -> const std::optional<std::string>& {
    return identifier_;
  }

  auto traits() const noexcept -> composition::AccessibilityTraits {
    return traits_;
  }

  auto heading() const noexcept
      -> const std::optional<composition::AccessibilityHeadingLevel>& {
    return heading_;
  }

  auto shape() const noexcept -> const std::optional<graphics::Path>& {
    return shape_;
  }

  auto input_labels() const noexcept -> const std::vector<std::string>& {
    return details_.input_labels;
  }

  auto sort_priority() const noexcept -> double {
    return details_.sort_priority;
  }

  auto actions() const noexcept -> const std::vector<Action>& {
    return details_.actions;
  }

  auto adjustable() const noexcept -> bool {
    return details_.adjustable || details_.adjust != nullptr;
  }

  auto scrollable() const noexcept -> bool {
    return details_.scroll != nullptr;
  }

  auto zoomable() const noexcept -> bool { return details_.zoom != nullptr; }

  auto activation_point() const noexcept
      -> const std::optional<composition::Point>& {
    return details_.activation_point;
  }

  auto drag_points() const noexcept -> const std::vector<Location>& {
    return details_.drag_points;
  }

  auto drop_points() const noexcept -> const std::vector<Location>& {
    return details_.drop_points;
  }

  auto responds_to_user_interaction() const noexcept -> bool {
    return details_.responds_to_user_interaction;
  }

  auto ignores_invert_colors() const noexcept -> bool {
    return details_.ignores_invert_colors;
  }

  auto direct_touch() const noexcept
      -> const std::optional<composition::AccessibilityDirectTouchOptions>& {
    return details_.direct_touch;
  }

  auto shows_large_content_viewer() const noexcept -> bool {
    return details_.shows_large_content_viewer;
  }

  auto large_content() const -> std::optional<composition::Element>;

  auto rotors() const noexcept
      -> const std::vector<composition::AccessibilityRotor>& {
    return details_.rotors;
  }

  auto rotor_entry() const noexcept
      -> const std::optional<composition::AccessibilityAttachment::Pairing>& {
    return details_.rotor_entry;
  }

  auto labeled_pair() const noexcept -> const
      std::optional<composition::AccessibilityAttachment::LabeledPair>& {
    return details_.labeled_pair;
  }

  auto linked_group() const noexcept
      -> const std::optional<composition::AccessibilityAttachment::Pairing>& {
    return details_.linked_group;
  }

  auto chart() const noexcept
      -> const std::optional<composition::AccessibilityChartDescriptor>& {
    return details_.chart;
  }

  auto scroll_status() const noexcept -> const std::optional<std::string>& {
    return details_.scroll_status;
  }

  auto speech() const noexcept -> const composition::AccessibilitySpeech& {
    return details_.speech;
  }

  auto custom_content() const noexcept -> const std::vector<CustomContent>& {
    return details_.custom_content;
  }

  auto navigation_icon() const noexcept -> const
      std::optional<composition::AccessibilityAttachment::NavigationIcon>& {
    return details_.navigation_icon;
  }

  auto text_content_type() const noexcept
      -> const std::optional<composition::AccessibilityTextContentType>& {
    return details_.text_content_type;
  }

  auto activate(composition::Tree& tree) const -> bool;

  auto adjust(composition::Tree& tree,
              composition::AccessibilityAdjustmentDirection direction) const
      -> bool;

  auto scroll(theme::Edge edge) const -> bool;

  auto zoom(composition::AccessibilityZoomGestureAction action) const -> bool;

  auto description() const -> std::string;

 private:
  friend class AccessibilityTree;

  composition::NodeId node_;
  composition::Rect frame_;
  std::string label_;
  std::optional<std::string> value_;
  std::optional<std::string> hint_;
  std::optional<std::string> identifier_;
  composition::AccessibilityTraits traits_;
  std::optional<composition::AccessibilityHeadingLevel> heading_;
  std::optional<graphics::Path> shape_;
  Details details_;
};

class AccessibilityTree {
 public:
  struct Language {
    const text::Locale* locale{nullptr};
    const text::StringCatalog* catalog{nullptr};
    composition::AccessibilitySpeech speech{};
  };

  explicit AccessibilityTree(const composition::Tree& tree);

  auto elements() const -> std::vector<AccessibilityElement>;

  auto elements(const composition::Node& within) const
      -> std::vector<AccessibilityElement>;

  auto element(composition::NodeId node) const
      -> std::optional<AccessibilityElement>;

  auto find(std::string_view identifier) const
      -> std::optional<AccessibilityElement>;

  auto target(const composition::AccessibilityRotorEntry& entry) const
      -> std::optional<AccessibilityElement>;

  auto rotors() const -> std::vector<composition::AccessibilityRotor>;

  auto entries(const composition::AccessibilityRotor& rotor) const
      -> std::vector<composition::AccessibilityRotorEntry>;

  auto label(const composition::AccessibilityRotorEntry& entry) const
      -> std::string;

  auto label(const composition::AccessibilityRotor& rotor) const -> std::string;

  auto paired(const AccessibilityElement& element) const
      -> std::optional<AccessibilityElement>;

  auto linked(const AccessibilityElement& element) const
      -> std::vector<AccessibilityElement>;

 private:
  struct NodeAccess {
    using Node = const composition::Node;

    static auto children(const composition::Node& node)
        -> decltype(std::declval<const composition::Node&>().children()) {
      return node.children();
    }
  };

  struct Visit {
    Language language{};
    std::size_t begin{0};
    bool container{false};
    double priority{0.0};
  };

  using Walk = support::FullOrderWalk<NodeAccess, Visit>;

  struct Spoken {
    Language language{};
  };

  using SpokenWalk = support::FullOrderWalk<NodeAccess, Spoken>;

  class Collector {
   public:
    Collector(const AccessibilityTree& owner, const composition::Node& top)
        : owner_(owner), top_(top) {}

    auto enter(const composition::Node& node, const Visit& above) -> Walk::Step;

    void leave(const composition::Node& node, Visit& visit);

    auto take() -> std::vector<AccessibilityElement> {
      return std::move(found_);
    }

   private:
    void add(std::vector<AccessibilityElement> made);

    std::reference_wrapper<const AccessibilityTree> owner_;
    std::reference_wrapper<const composition::Node> top_;
    std::vector<AccessibilityElement> found_;
    std::vector<double> keys_;
  };

  class Speaker {
   public:
    Speaker(const AccessibilityTree& owner, const composition::Node& top,
            std::optional<std::string> placeholder)
        : owner_(owner), top_(top), placeholder_(std::move(placeholder)) {}

    auto enter(const composition::Node& node, const Spoken& above)
        -> SpokenWalk::Step;

    void leave(const composition::Node&, Spoken&) {}

    auto take() -> std::vector<std::string> { return std::move(words_); }

   private:
    std::reference_wrapper<const AccessibilityTree> owner_;
    std::reference_wrapper<const composition::Node> top_;
    std::optional<std::string> placeholder_;
    std::vector<std::string> words_;
  };

  using GatherWalk = support::FullOrderWalk<NodeAccess, bool>;

  class Gatherer {
   public:
    auto enter(const composition::Node& node, const bool& above)
        -> GatherWalk::Step;

    void leave(const composition::Node&, bool&) {}

    auto take() -> std::vector<composition::AccessibilityRotor> {
      return std::move(found_);
    }

   private:
    std::vector<composition::AccessibilityRotor> found_;
  };

  auto collect(const composition::Node& node, const Language& language) const
      -> std::vector<AccessibilityElement>;

  auto unit(const composition::Node& wrapper, const Language& language) const
      -> std::vector<AccessibilityElement>;

  auto host_elements(const composition::Node& host,
                     const Language& language) const
      -> std::vector<AccessibilityElement>;

  auto make(const composition::Node& node, const Language& language) const
      -> AccessibilityElement;

  void overlay(AccessibilityElement& element,
               const composition::AccessibilityAttachment& declared,
               const Language& language) const;

  auto layered(std::vector<AccessibilityElement> base,
               const composition::Node& wrapper, const Language& language) const
      -> std::vector<AccessibilityElement>;

  auto spoken(const composition::Node& node, const Language& language,
              std::optional<std::string> placeholder = std::nullopt) const
      -> std::vector<std::string>;

  auto defaults(const composition::Node& node, const Language& language) const
      -> composition::AccessibilityDefaults;

  auto language_of(const composition::Node& node, const Language& above) const
      -> Language;

  auto language_at(const composition::Node& node) const -> Language;

  auto resolve(const composition::AccessibilityText& text,
               const Language& language) const -> std::string;

  auto resolve(const std::optional<composition::AccessibilityText>& text,
               const Language& language) const -> std::optional<std::string>;

  void pair(std::vector<AccessibilityElement>& elements) const;

  static void finish(std::vector<AccessibilityElement>& elements);

  static auto located(const composition::AccessibilityAttachment::Location& at,
                      const composition::Rect& frame) -> composition::Point;

  static auto action_name(const composition::AccessibilityActionKind& kind,
                          const std::string& resolved) -> std::string;

  static auto stands_alone(const composition::Node& node,
                           const composition::AccessibilityDefaults& defaults)
      -> bool;

  static auto hidden(const composition::Node& node) -> bool;

  static auto wraps(const composition::Node& node) -> bool;

  static auto attachment(const composition::Node& node)
      -> const composition::AccessibilityAttachment*;

  std::reference_wrapper<const composition::Tree> tree_;
  text::Locale fallback_locale_;
  text::StringCatalog fallback_catalog_;
};

}
