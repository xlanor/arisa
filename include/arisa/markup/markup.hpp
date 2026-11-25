#pragma once

#include <arisa/attr/appearance.hpp>
#include <arisa/attr/arrangement.hpp>
#include <arisa/attr/behaviour.hpp>
#include <arisa/attr/controls.hpp>
#include <arisa/attr/focus.hpp>
#include <arisa/attr/geometry.hpp>
#include <arisa/attr/gesture.hpp>
#include <arisa/attr/motion.hpp>
#include <arisa/attr/navigation.hpp>
#include <arisa/attr/presentation.hpp>
#include <arisa/attr/scrolling.hpp>
#include <arisa/attr/structure.hpp>
#include <arisa/attr/typography.hpp>
#include <arisa/components/async_image.hpp>
#include <arisa/components/button.hpp>
#include <arisa/components/button_role.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/content_unavailable_view.hpp>
#include <arisa/components/divider.hpp>
#include <arisa/components/edit_actions.hpp>
#include <arisa/components/edit_mode.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/for_each.hpp>
#include <arisa/components/geometry_reader.hpp>
#include <arisa/components/glass_panel.hpp>
#include <arisa/components/glyph.hpp>
#include <arisa/components/grid.hpp>
#include <arisa/components/grid_row.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hint_bar.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/index_set.hpp>
#include <arisa/components/label.hpp>
#include <arisa/components/label_style.hpp>
#include <arisa/components/labeled_content.hpp>
#include <arisa/components/lazy_hgrid.hpp>
#include <arisa/components/lazy_hstack.hpp>
#include <arisa/components/lazy_vgrid.hpp>
#include <arisa/components/lazy_vstack.hpp>
#include <arisa/components/link.hpp>
#include <arisa/components/pinned_scrollable_views.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/scroll_view_reader.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/status/battery.hpp>
#include <arisa/components/status/clock.hpp>
#include <arisa/components/status/wireless.hpp>
#include <arisa/components/system_symbols.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/view_that_fits.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/any_scroll_target_behavior.hpp>
#include <arisa/composition/axes.hpp>
#include <arisa/composition/axis.hpp>
#include <arisa/composition/bindable.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/command_options.hpp>
#include <arisa/composition/container_relative_frame.hpp>
#include <arisa/composition/content_shape_kinds.hpp>
#include <arisa/composition/coordinate_space.hpp>
#include <arisa/composition/dismiss_action.hpp>
#include <arisa/composition/dismiss_search_action.hpp>
#include <arisa/composition/drag_gesture.hpp>
#include <arisa/composition/focus_scope_behavior.hpp>
#include <arisa/composition/focus_state.hpp>
#include <arisa/composition/geometry_effect.hpp>
#include <arisa/composition/geometry_proxy.hpp>
#include <arisa/composition/gesture.hpp>
#include <arisa/composition/gesture_state.hpp>
#include <arisa/composition/horizontal_edge.hpp>
#include <arisa/composition/keyframe_animator.hpp>
#include <arisa/composition/long_press_gesture.hpp>
#include <arisa/composition/move_command_direction.hpp>
#include <arisa/composition/namespace.hpp>
#include <arisa/composition/navigation_destination.hpp>
#include <arisa/composition/navigation_path.hpp>
#include <arisa/composition/overlay_layer.hpp>
#include <arisa/composition/paging_scroll_target_behavior.hpp>
#include <arisa/composition/phase_animator.hpp>
#include <arisa/composition/popover_attachment_anchor.hpp>
#include <arisa/composition/prominence.hpp>
#include <arisa/composition/scroll_anchor_role.hpp>
#include <arisa/composition/scroll_bounce_behavior.hpp>
#include <arisa/composition/scroll_deceleration_rate.hpp>
#include <arisa/composition/scroll_geometry.hpp>
#include <arisa/composition/scroll_indicator_visibility.hpp>
#include <arisa/composition/scroll_phase.hpp>
#include <arisa/composition/scroll_phase_change_context.hpp>
#include <arisa/composition/scroll_position.hpp>
#include <arisa/composition/scroll_target.hpp>
#include <arisa/composition/scroll_target_behavior.hpp>
#include <arisa/composition/scroll_target_behavior_properties.hpp>
#include <arisa/composition/scroll_transition_configuration.hpp>
#include <arisa/composition/scroll_transition_phase.hpp>
#include <arisa/composition/scroll_view_proxy.hpp>
#include <arisa/composition/search_field_placement.hpp>
#include <arisa/composition/search_scope_activation.hpp>
#include <arisa/composition/sensory_feedback.hpp>
#include <arisa/composition/submission.hpp>
#include <arisa/composition/submit_triggers.hpp>
#include <arisa/composition/symbol_effect_animator.hpp>
#include <arisa/composition/tap_gesture.hpp>
#include <arisa/composition/text_input_configuration.hpp>
#include <arisa/composition/toolbar_item.hpp>
#include <arisa/composition/vertical_edges.hpp>
#include <arisa/composition/view_aligned_scroll_target_behavior.hpp>
#include <arisa/composition/visibility.hpp>
#include <arisa/composition/visual_effect.hpp>
#include <arisa/controls/closed_range.hpp>
#include <arisa/controls/menu.hpp>
#include <arisa/controls/number_format.hpp>
#include <arisa/controls/picker.hpp>
#include <arisa/controls/picker_style.hpp>
#include <arisa/controls/progress_view.hpp>
#include <arisa/controls/progress_view_style.hpp>
#include <arisa/controls/secure_field.hpp>
#include <arisa/controls/slider.hpp>
#include <arisa/controls/stepper.hpp>
#include <arisa/controls/text_editor.hpp>
#include <arisa/controls/text_field.hpp>
#include <arisa/controls/text_field_style.hpp>
#include <arisa/controls/toggle.hpp>
#include <arisa/controls/toggle_style.hpp>
#include <arisa/markup/tag.hpp>
#include <arisa/motion/angle.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/content_transition.hpp>
#include <arisa/motion/cubic_keyframe.hpp>
#include <arisa/motion/hover_effect.hpp>
#include <arisa/motion/keyframe_timeline.hpp>
#include <arisa/motion/keyframe_track.hpp>
#include <arisa/motion/keyframes.hpp>
#include <arisa/motion/linear_keyframe.hpp>
#include <arisa/motion/matched_geometry_properties.hpp>
#include <arisa/motion/motion_path.hpp>
#include <arisa/motion/move_keyframe.hpp>
#include <arisa/motion/spring.hpp>
#include <arisa/motion/spring_keyframe.hpp>
#include <arisa/motion/transaction.hpp>
#include <arisa/motion/transition.hpp>
#include <arisa/motion/unit_curve.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/presentation/navigation_link.hpp>
#include <arisa/presentation/navigation_stack.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/provider/lifecycle/provider.hpp>
#include <arisa/structure/cell_style.hpp>
#include <arisa/structure/dock_bar.hpp>
#include <arisa/structure/form.hpp>
#include <arisa/structure/list.hpp>
#include <arisa/structure/list_style.hpp>
#include <arisa/structure/navigation_split_view.hpp>
#include <arisa/structure/scaffold.hpp>
#include <arisa/structure/section.hpp>
#include <arisa/structure/tab.hpp>
#include <arisa/structure/tab_view.hpp>
#include <arisa/structure/tab_view_style.hpp>

namespace arisa::markup {

using namespace arisa::attr;

using components::AutomaticButtonStyle;
using components::BorderedButtonStyle;
using components::BorderedProminentButtonStyle;
using components::BorderlessButtonStyle;
using components::ButtonBorderShape;
using components::ButtonRepeatBehavior;
using components::ButtonRole;
using components::ButtonSizing;
using components::ButtonStyle;
using components::ControlSize;
using components::DefaultLabelStyle;
using components::EditActions;
using components::EditMode;
using components::GlassPanel;
using components::Glyph;
using components::GridItem;
using components::GridSection;
using components::HintBar;
using components::IconOnlyLabelStyle;
using components::IndexSet;
using components::LabelStyle;
using components::PanelRadius;
using components::PinnedScrollableViews;
using components::PlainButtonStyle;
using components::SystemSymbols;
using components::TitleAndIconLabelStyle;
using components::TitleOnlyLabelStyle;
using components::status::Battery;
using components::status::Clock;
using components::status::Wireless;
using composition::AnyScrollTargetBehavior;
using composition::Axes;
using composition::Axis;
using composition::Bindable;
using composition::Binding;
using composition::CommandOptions;
using composition::CommandTrigger;
using composition::ContainerRelativeFrame;
using composition::ContentShapeKinds;
using composition::CoordinateSpace;
using composition::DialogSeverity;
using composition::DismissAction;
using composition::DismissSearchAction;
using composition::DragGesture;
using composition::FocusScopeBehavior;
using composition::FocusState;
using composition::for_type;
using composition::GeometryProxy;
using composition::Gesture;
using composition::GestureState;
using composition::HorizontalEdge;
using composition::ImageScale;
using composition::KeyboardType;
using composition::KeyframeAnimator;
using composition::LongPressGesture;
using composition::MoveCommandDirection;
using composition::Namespace;
using composition::NavigationBarTitleDisplayMode;
using composition::NavigationPath;
using composition::NavigationSplitViewStyle;
using composition::NavigationSplitViewVisibility;
using composition::OverlayLayer;
using composition::PagingScrollTargetBehavior;
using composition::PopoverAttachmentAnchor;
using composition::PresentationAdaptation;
using composition::PresentationBackgroundInteraction;
using composition::PresentationContentInteraction;
using composition::PresentationDetent;
using composition::PresentationSizing;
using composition::Prominence;
using composition::RedactionReasons;
using composition::ScrollAnchorRole;
using composition::ScrollBounceBehavior;
using composition::ScrollDecelerationRate;
using composition::ScrollGeometry;
using composition::ScrollIndicatorVisibility;
using composition::ScrollPhase;
using composition::ScrollPhaseChangeContext;
using composition::ScrollPosition;
using composition::ScrollTarget;
using composition::ScrollTargetBehavior;
using composition::ScrollTargetBehaviorProperties;
using composition::ScrollTargetBehaviorPropertiesContext;
using composition::ScrollTargetContext;
using composition::ScrollTransitionConfiguration;
using composition::ScrollTransitionPhase;
using composition::ScrollViewProxy;
using composition::SearchFieldPlacement;
using composition::SearchScopeActivation;
using composition::SensoryFeedback;
using composition::SpacerSizing;
using composition::SubmitLabel;
using composition::SubmitTriggers;
using composition::SymbolRenderingMode;
using composition::SymbolVariants;
using composition::TapGesture;
using composition::TextContentType;
using composition::TextInputAutocapitalization;
using composition::ToolbarItem;
using composition::ToolbarItemGroup;
using composition::ToolbarItemPlacement;
using composition::ToolbarPlacement;
using composition::ToolbarSpacer;
using composition::VerticalEdge;
using composition::VerticalEdges;
using composition::ViewAlignedScrollTargetBehavior;
using composition::Visibility;
using composition::VisualEffect;
using controls::ButtonToggleStyle;
using controls::CheckboxToggleStyle;
using controls::CircularProgressViewStyle;
using controls::ClosedRange;
using controls::DatePickerComponents;
using controls::DatePickerStyle;
using controls::DefaultPickerStyle;
using controls::DefaultProgressViewStyle;
using controls::DefaultTextFieldStyle;
using controls::DefaultToggleStyle;
using controls::GaugeStyle;
using controls::InlinePickerStyle;
using controls::LinearProgressViewStyle;
using controls::MenuPickerStyle;
using controls::MenuStyle;
using controls::NavigationLinkPickerStyle;
using controls::NumberFormat;
using controls::PalettePickerStyle;
using controls::PickerStyle;
using controls::PlainTextFieldStyle;
using controls::ProgressViewStyle;
using controls::RadioGroupPickerStyle;
using controls::RoundedBorderTextFieldStyle;
using controls::SegmentedPickerStyle;
using controls::SegmentedProgressViewStyle;
using controls::SwitchToggleStyle;
using controls::TextEditorStyle;
using controls::TextFieldOptions;
using controls::TextFieldStyle;
using controls::ToggleStyle;
using controls::WheelPickerStyle;
using motion::Angle;
using motion::Animation;
using motion::ContentTransition;
using motion::CubicKeyframe;
using motion::HoverEffect;
using motion::Keyframes;
using motion::KeyframeTimeline;
using motion::KeyframeTrack;
using motion::LinearKeyframe;
using motion::MatchedGeometryProperties;
using motion::MotionPath;
using motion::MoveKeyframe;
using motion::Spring;
using motion::SpringKeyframe;
using motion::Transaction;
using motion::Transition;
using motion::UnitCurve;
using motion::UnitPoint;
using motion::with_animation;
using motion::with_transaction;
using provider::ScenePhase;
using structure::CellStyle;
using structure::ControlGroupStyle;
using structure::DefaultCellStyle;
using structure::DefaultListStyle;
using structure::DefaultTabViewStyle;
using structure::DockBar;
using structure::FormStyle;
using structure::GroupBoxStyle;
using structure::GroupedListStyle;
using structure::InsetGroupedListStyle;
using structure::InsetListStyle;
using structure::ListStyle;
using structure::NavigationSplitView;
using structure::PageTabViewStyle;
using structure::PlainListStyle;
using structure::ScaffoldOptions;
using structure::SidebarAdaptableTabViewStyle;
using structure::SidebarListStyle;
using structure::TabBarOnlyTabViewStyle;
using structure::TabCustomizationBehavior;
using structure::TabRole;
using structure::TabSection;
using structure::TabViewCustomization;
using structure::TabViewStyle;

inline constexpr Tag<components::VStack> VStack{};
inline constexpr Tag<components::HStack> HStack{};
inline constexpr Tag<components::ZStack> ZStack{};
inline constexpr Tag<components::Grid> Grid{};
inline constexpr Tag<components::GridRow> GridRow{};
inline constexpr Tag<components::LazyVGrid> LazyVGrid{};
inline constexpr Tag<components::LazyHGrid> LazyHGrid{};
inline constexpr Tag<components::Text> Text{};
inline constexpr Tag<components::Button> Button{};
inline constexpr Tag<components::Spacer> Spacer{};
inline constexpr Tag<components::ForEach> ForEach{};
inline constexpr Tag<components::ScrollView> ScrollView{};
inline constexpr Tag<components::ScrollViewReader> ScrollViewReader{};
inline constexpr Tag<components::GeometryReader> GeometryReader{};
inline constexpr Tag<components::Link> Link{};
inline constexpr Tag<components::LazyVStack> LazyVStack{};
inline constexpr Tag<components::LazyHStack> LazyHStack{};
inline constexpr Tag<components::Group> Group{};
inline constexpr Tag<components::ViewThatFits> ViewThatFits{};
inline constexpr Tag<presentation::NavigationStack> NavigationStack{};
inline constexpr Tag<presentation::NavigationLink> NavigationLink{};
inline constexpr Tag<controls::Toggle> Toggle{};
inline constexpr Tag<controls::Slider> Slider{};
inline constexpr Tag<controls::Stepper> Stepper{};
inline constexpr Tag<controls::Picker> Picker{};
inline constexpr Tag<controls::Menu> Menu{};
inline constexpr Tag<controls::ProgressView> ProgressView{};
inline constexpr Tag<controls::TextField> TextField{};
inline constexpr Tag<controls::SecureField> SecureField{};
inline constexpr Tag<controls::TextEditor> TextEditor{};
inline constexpr Tag<components::Label> Label{};
inline constexpr Tag<components::Divider> Divider{};
inline constexpr Tag<components::Image> Image{};
inline constexpr Tag<components::LabeledContent> LabeledContent{};
inline constexpr Tag<structure::GroupBox> GroupBox{};
inline constexpr Tag<structure::ControlGroup> ControlGroup{};
inline constexpr Tag<controls::Gauge> Gauge{};
inline constexpr Tag<controls::DatePicker> DatePicker{};
inline constexpr Tag<controls::MultiDatePicker> MultiDatePicker{};
inline constexpr Tag<controls::ColorPicker> ColorPicker{};
inline constexpr Tag<structure::List> List{};
inline constexpr Tag<structure::Form> Form{};
inline constexpr Tag<structure::Section> Section{};
inline constexpr Tag<structure::DisclosureGroup> DisclosureGroup{};
inline constexpr Tag<structure::Tab> Tab{};
inline constexpr Tag<structure::TabView> TabView{};
inline constexpr Tag<structure::Scaffold> Scaffold{};

}
