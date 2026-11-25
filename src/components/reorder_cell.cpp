#include <arisa/components/command_mapping.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/reorder_cell.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/drag_gesture.hpp>
#include <arisa/composition/geometry_proxy.hpp>
#include <arisa/composition/move_command_direction.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/theme/elevation.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <type_traits>
#include <utility>

namespace arisa::components {

const composition::EnvironmentKey<ReorderCell::Lanes> ReorderCell::lanes{
    "reorder_lanes", Lanes{}};

ReorderCell::ReorderCell(composition::Element content, std::size_t index,
                         std::size_t count, ForEach::MoveAction move)
    : Element(std::make_unique<Content>(
          std::make_shared<composition::PlaceholderContentView::Source>(
              std::move(content)),
          index, count, std::move(move))) {}

auto ReorderCell::wrap(std::shared_ptr<const ForEach::Source> source,
                       ForEach::MoveAction move)
    -> std::shared_ptr<const ForEach::Source> {
  if (source == nullptr || !move) {
    return source;
  }
  return std::make_shared<const Source>(std::move(source), std::move(move));
}

auto ReorderCell::Source::build(std::size_t index) const
    -> composition::Element {
  return ReorderCell(inner_->build(index), index, inner_->count(), move_);
}

auto ReorderCell::Source::same_item(std::size_t index,
                                    const ForEach::Source& other,
                                    std::size_t other_index) const -> bool {
  if (&other == this) {
    return other_index == index;
  }
  if (other.tag() != tag()) {
    return false;
  }
  const auto& wrapped = static_cast<const Source&>(other);
  return inner_->same_item(index, *wrapped.inner_, other_index);
}

auto ReorderCell::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

void ReorderCell::Content::shift(const ForEach::MoveAction& move,
                                 std::size_t from, long steps,
                                 std::size_t count) {
  if (!move || count == 0) {
    return;
  }
  const long wanted = std::clamp(static_cast<long>(from) + steps, 0L,
                                 static_cast<long>(count) - 1L);
  const auto target = static_cast<std::size_t>(wanted);
  if (target == from) {
    return;
  }
  motion::with_animation(motion::Animation::snappy(0.3), [&] {
    move(IndexSet{from}, target > from ? target + 1 : target);
  });
}

auto ReorderCell::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  const auto up = context.state(lifted);
  const auto drag = context.state(dragged);
  const auto size = context.state(extent);
  const Lanes across = context.environment(lanes);
  const bool editing =
      context.environment(Environment::edit_mode) != EditMode::inactive;
  const bool raised = up.get();
  composition::Element cell =
      composition::Element{composition::PlaceholderContentView(content_)}
          .on_geometry_change(
              std::type_identity<composition::Size>{},
              [](const composition::GeometryProxy& proxy) {
                return proxy.size();
              },
              [size](composition::Size value) { size.set(value); });
  const composition::Size moved = drag.get();
  if (raised) {
    cell = std::move(cell)
               .scale_effect(lifted_scale)
               .shadow(theme::Elevation::high)
               .offset(moved.width, moved.height);
  }
  cell = std::move(cell).animation(motion::Animation::snappy(0.25), raised);
  const std::size_t index = index_;
  const std::size_t count = count_;
  const ForEach::MoveAction move = move_;
  const auto lane_count =
      static_cast<long>(std::max<std::size_t>(across.count, 1));
  const bool vertical = across.axis == composition::Axis::vertical;
  if (editing) {
    cell = std::move(cell).high_priority_gesture(
        composition::DragGesture(4.0F, composition::CoordinateSpace::global)
            .on_changed(
                [up, drag](const composition::DragGesture::Value& value) {
                  up.set(true);
                  drag.set(value.translation);
                })
            .on_ended([up, drag, size, index, count, move, lane_count,
                       vertical](const composition::DragGesture::Value& value) {
              const composition::Size cellular = size.get();
              const long across_steps = std::lround(
                  (vertical ? value.translation.width
                            : value.translation.height) /
                  std::max(vertical ? cellular.width : cellular.height, 1.0F));
              const long along_steps = std::lround(
                  (vertical ? value.translation.height
                            : value.translation.width) /
                  std::max(vertical ? cellular.height : cellular.width, 1.0F));
              shift(move, index, across_steps + (along_steps * lane_count),
                    count);
              motion::with_animation(motion::Animation::snappy(0.25), [&] {
                up.set(false);
                drag.set(composition::Size{});
              });
            }));
  }
  if (!raised) {
    cell = std::move(cell).on_command(
        components::CommandMapping::move,
        std::string{components::CommandMapping::move_label},
        [up] { up.set(true); });
  } else {
    const auto drop = [up] {
      motion::with_animation(motion::Animation::snappy(0.25),
                             [&up] { up.set(false); });
    };
    const std::string label{components::CommandMapping::drop_label};
    cell =
        std::move(cell)
            .on_command(components::CommandMapping::move, label, drop)
            .on_command(components::CommandMapping::select, label, drop)
            .on_exit_command(label, drop)
            .on_move_command([index, count, move, lane_count, vertical](
                                 composition::MoveCommandDirection direction) {
              long steps = 0;
              switch (direction) {
                case composition::MoveCommandDirection::left:
                  steps = vertical ? -1 : -lane_count;
                  break;
                case composition::MoveCommandDirection::right:
                  steps = vertical ? 1 : lane_count;
                  break;
                case composition::MoveCommandDirection::up:
                  steps = vertical ? -lane_count : -1;
                  break;
                case composition::MoveCommandDirection::down:
                  steps = vertical ? lane_count : 1;
                  break;
              }
              shift(move, index, steps, count);
            });
  }
  std::vector<composition::Element> produced;
  produced.push_back(std::move(cell));
  return produced;
}

}
