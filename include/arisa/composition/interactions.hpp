#pragma once

#include <arisa/composition/accessibility_attachment.hpp>
#include <arisa/composition/change_observer.hpp>
#include <arisa/composition/command.hpp>
#include <arisa/composition/coordinate_space.hpp>
#include <arisa/composition/default_focus_evaluation_priority.hpp>
#include <arisa/composition/drag_configuration.hpp>
#include <arisa/composition/drag_container.hpp>
#include <arisa/composition/drag_drop_previews_formation.hpp>
#include <arisa/composition/drag_selection.hpp>
#include <arisa/composition/drag_session.hpp>
#include <arisa/composition/drag_source.hpp>
#include <arisa/composition/drop_configuration.hpp>
#include <arisa/composition/drop_session.hpp>
#include <arisa/composition/drop_target.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/environment_override.hpp>
#include <arisa/composition/exclusive_frame.hpp>
#include <arisa/composition/feedback_origin.hpp>
#include <arisa/composition/feedback_trigger.hpp>
#include <arisa/composition/focus_interactions.hpp>
#include <arisa/composition/focus_request.hpp>
#include <arisa/composition/focus_scope_behavior.hpp>
#include <arisa/composition/geometry_observer.hpp>
#include <arisa/composition/gesture_attachment.hpp>
#include <arisa/composition/hover_effect_group.hpp>
#include <arisa/composition/hover_phase.hpp>
#include <arisa/composition/key_press_handler.hpp>
#include <arisa/composition/keyboard_shortcut.hpp>
#include <arisa/composition/modifier_keys_handler.hpp>
#include <arisa/composition/namespace.hpp>
#include <arisa/composition/pointer_style.hpp>
#include <arisa/composition/popover_placement.hpp>
#include <arisa/composition/presentation_background.hpp>
#include <arisa/composition/presentation_options.hpp>
#include <arisa/composition/reorder_item.hpp>
#include <arisa/composition/scroll_effects.hpp>
#include <arisa/composition/scrolling.hpp>
#include <arisa/composition/spring_loading_behavior.hpp>
#include <arisa/composition/visibility.hpp>
#include <arisa/graphics/any_shape.hpp>
#include <arisa/graphics/stroke_style.hpp>
#include <arisa/theme/shape.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace arisa::composition {

struct Interactions {
  struct Scope {
    Namespace::ID id;
    FocusScopeBehavior behavior{FocusScopeBehavior::open};
  };

  struct ContinuousHover {
    CoordinateSpace space{CoordinateSpace::local};
    std::move_only_function<void(HoverPhase)> action;
  };

  struct HoverContainer {
    enum class Kind : std::uint8_t {
      implicit,
      named,
      cleared,
    };

    Kind kind{Kind::implicit};
    std::optional<HoverEffectGroup> group;
  };

  struct DefaultFocus {
    std::unique_ptr<FocusRequest> request;
    DefaultFocusEvaluationPriority priority{
        DefaultFocusEvaluationPriority::automatic};
  };

  struct Feedback {
    FeedbackOrigin origin{FeedbackOrigin::application};
    std::unique_ptr<FeedbackTrigger> trigger;
  };

  std::optional<bool> focusable;
  std::optional<FocusInteractions> focus_interactions;
  bool focus_fallback{false};
  bool disabled{false};
  bool covered{false};
  bool relative_container{false};
  bool hit_testing{true};
  bool dismiss_disabled{false};
  std::optional<PresentationBackground> presentation_background;
  std::optional<PopoverPlacement> popover_placement;
  std::shared_ptr<const PresentationOptions> presentation;
  std::optional<ExclusiveFrame::Options> exclusive_frame;
  bool focus_section{false};
  std::optional<Scope> focus_scope;
  std::optional<Namespace::ID> prefers_default_focus;
  std::vector<std::unique_ptr<FocusRequest>> focused;
  std::vector<DefaultFocus> default_focus;
  std::vector<std::shared_ptr<Command>> commands;
  std::vector<std::shared_ptr<KeyPressHandler>> key_presses;
  std::optional<KeyboardShortcut> keyboard_shortcut;
  std::vector<std::shared_ptr<ModifierKeysHandler>> modifier_keys;
  std::optional<std::string> type_select;
  bool type_select_cleared{false};
  std::vector<std::shared_ptr<const EnvironmentOverride>> focused_values;
  std::vector<std::shared_ptr<const EnvironmentOverride>> scene_values;
  std::shared_ptr<const DragSource> drag;
  std::shared_ptr<DropTarget> drop;
  std::optional<DragConfiguration> drag_configuration;
  std::shared_ptr<std::move_only_function<void(const DragSession&)>>
      drag_updates;
  std::shared_ptr<std::move_only_function<void(const DropSession&)>>
      drop_updates;
  std::shared_ptr<
      std::move_only_function<DropConfiguration(const DropSession&)>>
      drop_configuration;
  std::optional<DragDropPreviewsFormation> drag_previews;
  std::optional<DragDropPreviewsFormation> drop_previews;
  std::shared_ptr<const DragContainer> drag_container;
  std::shared_ptr<const DragSelection> drag_selection;
  std::optional<SpringLoadingBehavior> spring_loading;
  std::optional<ReorderItem> reorder_item;
  bool reorder_container{false};
  std::optional<theme::Shape> drag_preview_shape;
  std::vector<Feedback> feedback;
  std::vector<GestureAttachment> gestures;
  std::vector<std::shared_ptr<GeometryObserver>> geometry;
  std::optional<ElementKey> coordinate_space;
  std::optional<theme::Shape> hit_shape;
  std::optional<graphics::AnyShape> hit_path;
  graphics::FillStyle hit_fill{};
  std::optional<graphics::AnyShape> accessibility_shape;
  std::optional<graphics::AnyShape> context_menu_preview_shape;
  std::optional<theme::Shape> focus_shape;
  std::optional<theme::Shape> hover_shape;
  std::shared_ptr<std::move_only_function<void(bool)>> hover;
  std::shared_ptr<ContinuousHover> continuous_hover;
  std::optional<HoverContainer> hover_container;
  std::optional<PointerStyle> pointer_style;
  Visibility pointer_visibility{Visibility::automatic};
  float focus_ring_inset{0.0F};
  Scrolling scrolling;
  ScrollEffects effects;
  std::shared_ptr<const AccessibilityAttachment> accessibility;
};

}
