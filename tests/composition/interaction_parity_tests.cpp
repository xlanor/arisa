#include <arisa/composition/content_shape_kinds.hpp>
#include <arisa/composition/default_focus_evaluation_priority.hpp>
#include <arisa/composition/drag_configuration.hpp>
#include <arisa/composition/drag_drop_previews_formation.hpp>
#include <arisa/composition/drag_session.hpp>
#include <arisa/composition/drop_configuration.hpp>
#include <arisa/composition/drop_info.hpp>
#include <arisa/composition/drop_operation.hpp>
#include <arisa/composition/drop_proposal.hpp>
#include <arisa/composition/drop_session.hpp>
#include <arisa/composition/event_modifiers.hpp>
#include <arisa/composition/focused_binding.hpp>
#include <arisa/composition/focused_value_key.hpp>
#include <arisa/composition/focused_values.hpp>
#include <arisa/composition/gesture_input_kinds.hpp>
#include <arisa/composition/key_equivalent.hpp>
#include <arisa/composition/keyboard_shortcut.hpp>
#include <arisa/composition/modifier_keys_handler.hpp>
#include <arisa/composition/reorder_difference.hpp>
#include <arisa/composition/spring_loading_behavior.hpp>
#include <arisa/composition/toolbar_customization.hpp>
#include <arisa/composition/transfer_item.hpp>
#include <arisa/composition/ut_type.hpp>
#include <arisa/motion/keyframe_timeline.hpp>
#include <arisa/motion/keyframe_track.hpp>
#include <arisa/motion/keyframes.hpp>
#include <arisa/motion/linear_keyframe.hpp>
#include <arisa/provider/input/key_repeat.hpp>

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace arisa::composition {

using namespace std::chrono_literals;

TEST(InteractionParityTest, GestureInputKindsAddArisaControllerKind) {
  EXPECT_FALSE(GestureInputKinds::all.contains(GestureInputKinds::controller));
  const GestureInputKinds both =
      GestureInputKinds::pointer | GestureInputKinds::controller;
  EXPECT_TRUE(both.contains(GestureInputKinds::controller));
  EXPECT_TRUE(both.intersects(GestureInputKinds::all));
  EXPECT_FALSE(GestureInputKinds::direct_touch.intersects(
      GestureInputKinds::controller));
}

TEST(InteractionParityTest, DropOperationsFormAnOptionSet) {
  EXPECT_EQ(DropOperation::copy, DropOperation::copy);
  EXPECT_NE(DropOperation::move, DropOperation::remove);
  const DropOperationSet set = DropOperationSet::copy | DropOperationSet::move;
  EXPECT_TRUE(set.contains(DropOperation::move));
  EXPECT_FALSE(set.contains(DropOperationSet::remove));
  EXPECT_FALSE(set.contains(DropOperationSet::alias));
  EXPECT_FALSE(set.contains(DropOperationSet::cancel));
  EXPECT_FALSE(set.contains(DropOperationSet::forbidden));
  EXPECT_TRUE(DropOperationSet{}.empty());
}

TEST(InteractionParityTest, DropProposalCarriesBothOperations) {
  const DropProposal inside{DropOperation::move};
  EXPECT_EQ(inside.operation(), DropOperation::move);
  EXPECT_FALSE(inside.operation_outside_application().has_value());
  const DropProposal both{DropOperation::move, DropOperation::copy};
  EXPECT_EQ(both.operation_outside_application(), DropOperation::copy);
}

TEST(InteractionParityTest, DropInfoFiltersItemsByType) {
  const std::array items{TransferItem::of(std::string{"one"}),
                         TransferItem{UTType{"public.png"}, "png"}};
  const DropInfo info{Point{.x = 3.0F, .y = 4.0F}, items};
  EXPECT_EQ(info.location(), (Point{.x = 3.0F, .y = 4.0F}));
  const std::array text{UTType::plain_text()};
  const std::array movie{UTType{"public.movie"}};
  EXPECT_TRUE(info.has_items_conforming_to(text));
  EXPECT_FALSE(info.has_items_conforming_to(movie));
  EXPECT_EQ(info.item_providers(text).size(), 1U);
}

TEST(InteractionParityTest, DragConfigurationPermitsOperations) {
  const DragConfiguration plain;
  EXPECT_TRUE(plain.permits(DropOperation::copy));
  EXPECT_FALSE(plain.permits(DropOperation::move));
  EXPECT_TRUE(DragConfiguration{true}.permits(DropOperation::move));
  const DragConfiguration deleting{true, true};
  EXPECT_TRUE(deleting.permits(DropOperation::remove));
  DragConfiguration::OperationsWithinApp within{false, true, false};
  within.set_allow_alias(true);
  EXPECT_TRUE(within.allow_alias());
  EXPECT_TRUE(within.allowed().contains(DropOperation::alias));
  EXPECT_FALSE(within.allowed().contains(DropOperation::copy));
  EXPECT_TRUE(DragConfiguration::OperationsWithinApp{true}.allowed().contains(
      DropOperation::move));
  DragConfiguration::OperationsOutsideApp outside{false};
  outside.set_allow_alias(true);
  EXPECT_TRUE(outside.allow_alias());
  EXPECT_FALSE(outside.allowed().contains(DropOperation::copy));
  EXPECT_TRUE(DragConfiguration::OperationsOutsideApp(true, true, true)
                  .allowed()
                  .contains(DropOperation::remove));
  DragConfiguration configured{within, outside};
  EXPECT_EQ(configured.operations_within_app(), within);
  EXPECT_EQ(configured.operations_outside_app(), outside);
  configured.set_operations_within_app(
      DragConfiguration::OperationsWithinApp{});
  configured.set_operations_outside_app(
      DragConfiguration::OperationsOutsideApp{});
  EXPECT_TRUE(configured.permits(DropOperation::copy));
}

TEST(InteractionParityTest, DropConfigurationCarriesOperationAndDestination) {
  DropConfiguration plain{DropOperation::move};
  EXPECT_EQ(plain.operation(), DropOperation::move);
  EXPECT_FALSE(plain.accepted_item_count().has_value());
  plain.set_accepted_item_count(2U);
  EXPECT_EQ(plain.accepted_item_count(), 2U);
  using Difference = ReorderDifference<int, std::string>;
  const DropConfiguration targeted{
      DropOperation::move,
      Difference::Destination{Difference::Destination::Position::before(4),
                              std::string{"right"}}};
  ASSERT_TRUE(targeted.destination().has_value());
  EXPECT_EQ(targeted.destination()->before, ElementKey{4});
  EXPECT_EQ(targeted.destination()->collection, ElementKey{"right"});
}

TEST(InteractionParityTest, DragSessionReportsPhaseItemsAndLocation) {
  const DragSession session{DragSession::ID{3},
                            DragSession::Phase::ended(DropOperation::move),
                            {ElementKey{1}, ElementKey{2}},
                            1U,
                            Point{.x = 5.0F, .y = 6.0F}};
  EXPECT_EQ(session.id(), DragSession::ID{3});
  EXPECT_EQ(session.phase(), DragSession::Phase::ended(DropOperation::move));
  EXPECT_NE(session.phase(), DragSession::Phase::ending(DropOperation::move));
  EXPECT_EQ(session.phase().operation(), DropOperation::move);
  EXPECT_EQ(DragSession::Phase::initial().kind(),
            DragSession::Phase::Kind::initial);
  EXPECT_EQ(DragSession::Phase::active().kind(),
            DragSession::Phase::Kind::active);
  EXPECT_EQ(DragSession::Phase::data_transfer_completed().kind(),
            DragSession::Phase::Kind::data_transfer_completed);
  EXPECT_EQ(session.dragged_item_index(), 1U);
  EXPECT_EQ(session.dragged_item_ids<int>(), (std::vector<int>{1, 2}));
  EXPECT_EQ(session.location(), (Point{.x = 5.0F, .y = 6.0F}));
}

TEST(InteractionParityTest, DropSessionReportsLocalItemsAndReorderDestination) {
  const DropSession session{DropSession::Snapshot{
      .id = 9,
      .phase = DropSession::Phase::active(),
      .local = DropSession::LocalSession{{ElementKey{7}}},
      .items_count = 1,
      .suggested = DropOperationSet::copy,
      .size = Size{.width = 10.0F, .height = 20.0F},
      .location = Point{.x = 1.0F, .y = 2.0F},
      .reorder = DropSession::ReorderTarget{.before = ElementKey{8},
                                            .collection = ElementKey{"left"}}}};
  EXPECT_EQ(session.id(), DropSession::ID{9});
  EXPECT_EQ(session.phase(), DropSession::Phase::active());
  EXPECT_NE(DropSession::Phase::entering(), DropSession::Phase::exiting());
  EXPECT_EQ(DropSession::Phase::ended(DropOperation::copy).operation(),
            DropOperation::copy);
  EXPECT_EQ(DropSession::Phase::data_transfer_completed().kind(),
            DropSession::Phase::Kind::data_transfer_completed);
  ASSERT_TRUE(session.local_session().has_value());
  EXPECT_EQ(session.local_session()->dragged_item_ids<int>(),
            (std::vector<int>{7}));
  EXPECT_EQ(session.items_count(), 1U);
  EXPECT_EQ(session.suggested_operations(), DropOperationSet::copy);
  EXPECT_EQ(session.size(), (Size{.width = 10.0F, .height = 20.0F}));
  EXPECT_EQ(session.location(), (Point{.x = 1.0F, .y = 2.0F}));
  const auto single = session.reorder_destination<int>();
  ASSERT_TRUE(single.has_value());
  EXPECT_EQ(single->position().item(), 8);
  const auto named = session.reorder_destination<int, std::string>();
  ASSERT_TRUE(named.has_value());
  EXPECT_EQ(named->collection_id(), "left");
}

TEST(InteractionParityTest, ReorderDifferenceMovesSourcesBeforeDestination) {
  using Difference = ReorderDifference<int>;
  const Difference difference{
      {1, 3},
      Difference::Destination{Difference::Destination::Position::end()}};
  EXPECT_EQ(difference.sources(), (std::vector<int>{1, 3}));
  EXPECT_TRUE(difference.destination().position().is_end());
  EXPECT_EQ(difference.destination().collection_id(),
            ReorderableSingleCollectionIdentifier{});
  std::vector<int> items{1, 2, 3, 4};
  difference.apply(items, [](int each) { return each; });
  EXPECT_EQ(items, (std::vector<int>{2, 4, 1, 3}));
  const Difference before{
      {4},
      Difference::Destination{Difference::Destination::Position::before(2)}};
  before.apply(items, [](int each) { return each; });
  EXPECT_EQ(items, (std::vector<int>{4, 2, 1, 3}));
  EXPECT_EQ(Difference::Destination::Position::before(2),
            Difference::Destination::Position::before(2));
  EXPECT_FALSE(difference == before);
  using Named = ReorderDifference<int, int>;
  const Named::Destination crossed{Named::Destination::Position::before(1), 2};
  EXPECT_EQ(crossed.collection_id(), 2);
  EXPECT_EQ(crossed, crossed);
}

TEST(InteractionParityTest, PreviewFormationsAndSpringLoadingAreDistinct) {
  EXPECT_NE(DragDropPreviewsFormation::automatic,
            DragDropPreviewsFormation::pile);
  EXPECT_NE(DragDropPreviewsFormation::list, DragDropPreviewsFormation::stack);
  EXPECT_NE(DragDropPreviewsFormation::none, DragDropPreviewsFormation::stack);
  EXPECT_EQ(SpringLoading::behavior.fallback(),
            SpringLoadingBehavior::automatic);
  EXPECT_NE(SpringLoadingBehavior::enabled, SpringLoadingBehavior::disabled);
  EXPECT_TRUE(ContentShapeKinds::drag_preview.contains(
      ContentShapeKinds::drag_preview));
  EXPECT_FALSE(
      ContentShapeKinds::interaction.contains(ContentShapeKinds::drag_preview));
  EXPECT_NE(ToolbarCustomizationBehavior::reorderable,
            ToolbarCustomizationBehavior::default_behavior);
}

TEST(InteractionParityTest, KeyboardShortcutLocalizationMirrorsAutomaticOnly) {
  const KeyboardShortcut back{.key = KeyEquivalent::left_arrow,
                              .modifiers = EventModifiers::command};
  EXPECT_EQ(back.localization, KeyboardShortcut::Localization::automatic);
  EXPECT_EQ(back.mirrored().key, KeyEquivalent::right_arrow);
  EXPECT_EQ((KeyboardShortcut{.key = '[', .modifiers = EventModifiers::command})
                .mirrored()
                .key,
            KeyEquivalent{']'});
  const KeyboardShortcut fixed{
      .key = KeyEquivalent::left_arrow,
      .modifiers = EventModifiers::command,
      .localization = KeyboardShortcut::Localization::without_mirroring};
  EXPECT_EQ(fixed.mirrored(), fixed);
  const KeyboardShortcut custom{
      .key = '{',
      .modifiers = EventModifiers::command,
      .localization = KeyboardShortcut::Localization::custom};
  EXPECT_EQ(custom.mirrored(), custom);
  EXPECT_NE(KeyEquivalent::clear, KeyEquivalent::escape);
}

TEST(InteractionParityTest, ModifierKeysHandlerMasksChanges) {
  std::vector<EventModifiers> seen;
  ModifierKeysHandler handler{EventModifiers::shift, true,
                              [&seen](EventModifiers old, EventModifiers next) {
                                seen.push_back(old);
                                seen.push_back(next);
                              }};
  EXPECT_TRUE(handler.initial());
  EXPECT_EQ(handler.mask(), EventModifiers::shift);
  EXPECT_FALSE(handler.differs(EventModifiers{}, EventModifiers::command));
  EXPECT_TRUE(handler.differs(EventModifiers{},
                              EventModifiers::command | EventModifiers::shift));
  handler.changed(EventModifiers{},
                  EventModifiers::command | EventModifiers::shift);
  ASSERT_EQ(seen.size(), 2U);
  EXPECT_EQ(seen[1], EventModifiers::shift);
  EXPECT_EQ((EventModifiers::command & EventModifiers::shift),
            EventModifiers{});
  EXPECT_NE(DefaultFocusEvaluationPriority::automatic,
            DefaultFocusEvaluationPriority::user_initiated);
}

TEST(InteractionParityTest, FocusedBindingReadsAndWritesItsBinding) {
  auto held = std::make_shared<int>(4);
  const Binding<int> binding{[held] { return *held; },
                             [held](int value) { *held = value; }};
  const FocusedBinding<int> focused{binding};
  EXPECT_TRUE(focused.bound());
  EXPECT_EQ(focused.wrapped_value(), 4);
  focused.set(6);
  EXPECT_EQ(*held, 6);
  const Binding<std::optional<int>> projected = focused.projected_value();
  EXPECT_EQ(projected.get(), 6);
  projected.set(8);
  EXPECT_EQ(*held, 8);
  const FocusedBinding<int> empty{std::nullopt};
  EXPECT_FALSE(empty.wrapped_value().has_value());
  empty.set(1);
  EXPECT_FALSE(empty.projected_value().get().has_value());
}

TEST(InteractionParityTest, FocusedValuesReadKeysAndCompare) {
  static const FocusedValueKey<int> count{"interaction-parity-count"};
  static const FocusedValueKey<Binding<int>> editing{
      "interaction-parity-editing"};
  FocusedValues::Values values;
  values.emplace(count.identity(), count.declare(3));
  const FocusedValues first{values};
  EXPECT_EQ(first[count], 3);
  EXPECT_FALSE(first[editing].has_value());
  EXPECT_EQ(first.size(), 1U);
  EXPECT_EQ(first, FocusedValues{values});
  EXPECT_NE(first, FocusedValues{});
  FocusedValues::Values other;
  other.emplace(count.identity(), count.declare(4));
  EXPECT_NE(first, FocusedValues{other});
}

TEST(InteractionParityTest, KeyframeTimelineReadsProgressAndWholeValueTracks) {
  const motion::Keyframes<double> keyframes{
      motion::KeyframeTrack<double, double>(
          motion::LinearKeyframe<double>(10.0, 1.0))};
  const motion::KeyframeTimeline<double> timeline{0.0, keyframes};
  EXPECT_DOUBLE_EQ(timeline.duration(), 1.0);
  EXPECT_DOUBLE_EQ(timeline.value(motion::KeyframeTimeline<double>::Progress{
                       .fraction = 0.5}),
                   5.0);
  EXPECT_DOUBLE_EQ(timeline.value(motion::KeyframeTimeline<double>::Progress{
                       .fraction = 2.0}),
                   10.0);
  EXPECT_TRUE((motion::KeyframeTrack<double, double>(
                   motion::LinearKeyframe<double>(1.0, 1.0)))
                  .whole());
}

TEST(InteractionParityTest, UniformTypesMapToMimeTypes) {
  EXPECT_EQ(UTType::plain_text().mime_type(), "text/plain;charset=utf-8");
  EXPECT_EQ(UTType{"public.png"}.mime_type(), "image/png");
  EXPECT_EQ(UTType::from_mime_type("text/plain"), UTType::plain_text());
  const UTType custom{"com.example.card"};
  EXPECT_EQ(UTType::from_mime_type(custom.mime_type()), custom);
  EXPECT_FALSE(UTType::from_mime_type("video/mp4").has_value());
}

TEST(InteractionParityTest, KeyRepeatRepeatsTheNewestHeldKey) {
  provider::KeyRepeat repeat{100ms, 20ms};
  const std::array held{provider::Key::a};
  EXPECT_FALSE(repeat.update(held, 0ms).has_value());
  EXPECT_FALSE(repeat.update(held, 90ms).has_value());
  EXPECT_EQ(repeat.update(held, 100ms), provider::Key::a);
  EXPECT_FALSE(repeat.update(held, 110ms).has_value());
  EXPECT_EQ(repeat.update(held, 120ms), provider::Key::a);
  const std::array shifted{provider::Key::left_shift};
  EXPECT_FALSE(repeat.update(shifted, 200ms).has_value());
  EXPECT_FALSE(repeat.update(shifted, 900ms).has_value());
  const std::array none{provider::Key::b};
  EXPECT_FALSE(repeat.update({}, 1000ms).has_value());
  EXPECT_FALSE(repeat.update(none, 1001ms).has_value());
  EXPECT_EQ(repeat.update(none, 1101ms), provider::Key::b);
}

}
