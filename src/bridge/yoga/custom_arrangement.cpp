#include <bridge/yoga/custom_arrangement.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace arisa::bridge::yoga {

CustomArrangement::CustomArrangement(
    std::shared_ptr<YogaSlot::ConfigHandle> config, YogaSlot& owner,
    YGDirection direction)
    : config_(std::move(config)),
      host_(YGNodeNewWithConfig(config_->get())),
      scratch_(YGNodeNewWithConfig(config_->get())),
      direction_(direction) {
  YGNodeSetContext(host_.get(), &owner);
  const YGNodeRef scratch = scratch_.get();
  YGNodeStyleSetFlexDirection(scratch, YGFlexDirectionColumn);
  YGNodeStyleSetAlignItems(scratch, YGAlignFlexStart);
  YGNodeStyleSetJustifyContent(scratch, YGJustifyFlexStart);
}

CustomArrangement::~CustomArrangement() {
  if (scratch_.valid()) {
    YGNodeRemoveAllChildren(scratch_.get());
  }
  for (YogaSlot::NodeHandle& wrapper : wrappers_) {
    if (wrapper.valid()) {
      YGNodeRemoveAllChildren(wrapper.get());
    }
  }
  wrappers_.clear();
  if (host_.valid()) {
    YGNodeRemoveAllChildren(host_.get());
  }
  host_.reset();
  scratch_.reset();
}

auto CustomArrangement::update(
    std::shared_ptr<const components::AnyLayout::Box> box,
    std::span<const Member> members, YGDirection direction, Reader reader)
    -> bool {
  reader_ = std::move(reader);
  const bool same_members =
      members.size() == members_.size() &&
      std::ranges::equal(members, members_,
                         [](const Member& left, const Member& right) {
                           return left.node == right.node &&
                                  &left.source.get() == &right.source.get();
                         });
  const bool same_box = box_ != nullptr && (box_ == box || box_->equals(*box));
  const bool same_kind = box_ != nullptr && box_->kind() == box->kind();
  bool dirty = revalued(members);
  for (const Member& member : members) {
    dirty = dirty || YGNodeIsDirty(member.node);
  }
  if (same_members && same_box && direction_ == direction && !dirty && laid_ &&
      !YGNodeIsDirty(host_.get())) {
    std::ranges::copy(members, members_.begin());
    box_ = std::move(box);
    return false;
  }
  box_ = std::move(box);
  direction_ = direction;
  if (!same_members) {
    members_.assign(members.begin(), members.end());
    adopt();
  } else {
    for (std::size_t index = 0; index < members.size(); ++index) {
      members_[index] = members[index];
    }
  }
  orient();
  remember_values();
  for (std::size_t index = 0; index < members_.size(); ++index) {
    if (!same_members || YGNodeIsDirty(members_[index].node)) {
      measured_[index].clear();
    }
  }
  if (state_ == nullptr || !same_kind) {
    state_ = box_->make_state(subviews());
  } else if (!same_members || !same_box || dirty) {
    box_->update_state(*state_, subviews());
  }
  laid_ = false;
  return true;
}

void CustomArrangement::adopt() {
  const YGNodeRef host = host_.get();
  YGNodeRemoveAllChildren(host);
  for (YogaSlot::NodeHandle& wrapper : wrappers_) {
    YGNodeRemoveAllChildren(wrapper.get());
  }
  wrappers_.resize(std::min(wrappers_.size(), members_.size()));
  while (wrappers_.size() < members_.size()) {
    YogaSlot::NodeHandle wrapper{YGNodeNewWithConfig(config_->get())};
    YGNodeStyleSetPositionType(wrapper.get(), YGPositionTypeAbsolute);
    YGNodeStyleSetFlexDirection(wrapper.get(), YGFlexDirectionColumn);
    YGNodeStyleSetAlignItems(wrapper.get(), YGAlignFlexStart);
    YGNodeStyleSetJustifyContent(wrapper.get(), YGJustifyFlexStart);
    wrappers_.push_back(std::move(wrapper));
  }
  subviews_.clear();
  subviews_.reserve(members_.size());
  measured_.resize(members_.size());
  placements_.assign(members_.size(), Placement{});
  for (std::size_t index = 0; index < members_.size(); ++index) {
    const YGNodeRef child = members_[index].node;
    const YGNodeRef wrapper = wrappers_[index].get();
    if (const YGNodeRef owner = YGNodeGetOwner(child)) {
      YGNodeRemoveChild(owner, child);
    }
    YGNodeInsertChild(wrapper, child, 0);
    YGNodeInsertChild(host, wrapper, index);
    subviews_.emplace_back(*this, index);
  }
}

void CustomArrangement::orient() {
  const YGFlexDirection flow =
      box_->properties().stack_orientation == composition::Axis::horizontal
          ? YGFlexDirectionRow
          : YGFlexDirectionColumn;
  for (const YogaSlot::NodeHandle& wrapper : wrappers_) {
    YGNodeStyleSetFlexDirection(wrapper.get(), flow);
  }
  if (flow == flow_) {
    return;
  }
  flow_ = flow;
  YGNodeStyleSetFlexDirection(scratch_.get(), flow);
  for (std::vector<Measured>& known : measured_) {
    known.clear();
  }
}

auto CustomArrangement::subviews() const -> components::LayoutSubviews {
  return components::LayoutSubviews{
      subviews_, direction_ == YGDirectionRTL
                     ? composition::LayoutDirection::right_to_left
                     : composition::LayoutDirection::left_to_right};
}

auto CustomArrangement::proposed(float length, YGMeasureMode mode) noexcept
    -> std::optional<float> {
  if (mode == YGMeasureModeUndefined || std::isnan(length)) {
    return std::nullopt;
  }
  return length;
}

void CustomArrangement::restore(YGNodeRef node, YGValue value,
                                bool across) noexcept {
  switch (value.unit) {
    case YGUnitPoint:
      if (across) {
        YGNodeStyleSetWidth(node, value.value);
      } else {
        YGNodeStyleSetHeight(node, value.value);
      }
      break;
    case YGUnitPercent:
      if (across) {
        YGNodeStyleSetWidthPercent(node, value.value);
      } else {
        YGNodeStyleSetHeightPercent(node, value.value);
      }
      break;
    case YGUnitAuto:
      if (across) {
        YGNodeStyleSetWidthAuto(node);
      } else {
        YGNodeStyleSetHeightAuto(node);
      }
      break;
    case YGUnitUndefined:
      if (across) {
        YGNodeStyleSetWidth(node, YGUndefined);
      } else {
        YGNodeStyleSetHeight(node, YGUndefined);
      }
      break;
  }
}

auto CustomArrangement::offered(std::optional<float> length) noexcept -> float {
  if (!length.has_value()) {
    return YGUndefined;
  }
  if (!std::isfinite(*length)) {
    return huge;
  }
  return std::clamp(*length, 0.0F, huge);
}

auto CustomArrangement::reported(float length) noexcept -> float {
  if (length >= huge * 0.5F) {
    return std::numeric_limits<float>::infinity();
  }
  return length;
}

auto CustomArrangement::yielded(float length, float offered,
                                YGMeasureMode mode) noexcept -> float {
  if (std::isfinite(length)) {
    return std::max(length, 0.0F);
  }
  if (mode != YGMeasureModeUndefined && std::isfinite(offered)) {
    return offered;
  }
  return 0.0F;
}

auto CustomArrangement::measure(float width, YGMeasureMode width_mode,
                                float height, YGMeasureMode height_mode)
    -> YGSize {
  if (box_ == nullptr || state_ == nullptr) {
    return YGSize{.width = 0.0F, .height = 0.0F};
  }
  const components::ProposedViewSize proposal{proposed(width, width_mode),
                                              proposed(height, height_mode)};
  const composition::Size size =
      box_->size_that_fits(proposal, subviews(), *state_);
  return YGSize{.width = yielded(size.width, width, width_mode),
                .height = yielded(size.height, height, height_mode)};
}

auto CustomArrangement::measured(std::size_t index,
                                 components::ProposedViewSize proposal,
                                 bool read) -> const Measured& {
  std::vector<Measured>& known = measured_[index];
  const auto found = std::ranges::find(known, proposal, &Measured::proposal);
  if (found != known.end() && (!read || found->baselines.has_value())) {
    return *found;
  }
  const YGNodeRef child = members_[index].node;
  const YGNodeRef wrapper = wrappers_[index].get();
  const YGNodeRef scratch = scratch_.get();
  const float width = offered(proposal.width);
  const float height = offered(proposal.height);
  if (std::isnan(width)) {
    YGNodeStyleSetWidthAuto(scratch);
  } else {
    YGNodeStyleSetWidth(scratch, width);
  }
  if (std::isnan(height)) {
    YGNodeStyleSetHeightAuto(scratch);
  } else {
    YGNodeStyleSetHeight(scratch, height);
  }
  const Member& member = members_[index];
  const bool ideal_across = std::isnan(width) && member.ideal_width.has_value();
  const bool ideal_down = std::isnan(height) && member.ideal_height.has_value();
  const YGValue declared_width = YGNodeStyleGetWidth(child);
  const YGValue declared_height = YGNodeStyleGetHeight(child);
  if (ideal_across) {
    YGNodeStyleSetWidth(child, *member.ideal_width);
  }
  if (ideal_down) {
    YGNodeStyleSetHeight(child, *member.ideal_height);
  }
  YGNodeRemoveChild(wrapper, child);
  YGNodeInsertChild(scratch, child, 0);
  YGNodeCalculateLayout(scratch, width, height, direction_);
  const composition::Size size{
      .width = reported(YGNodeLayoutGetWidth(child) +
                        YGNodeLayoutGetMargin(child, YGEdgeLeft) +
                        YGNodeLayoutGetMargin(child, YGEdgeRight)),
      .height = reported(YGNodeLayoutGetHeight(child) +
                         YGNodeLayoutGetMargin(child, YGEdgeTop) +
                         YGNodeLayoutGetMargin(child, YGEdgeBottom))};
  std::optional<composition::TextBaselines> baselines;
  if (read && reader_) {
    const float top = YGNodeLayoutGetMargin(child, YGEdgeTop);
    const composition::TextBaselines read_back =
        reader_(members_[index].source.get());
    baselines = composition::TextBaselines{
        .first = read_back.first.transform(
            [top](float value) { return value + top; }),
        .last = read_back.last.transform(
            [top](float value) { return value + top; })};
  }
  YGNodeRemoveChild(scratch, child);
  YGNodeInsertChild(wrapper, child, 0);
  if (ideal_across) {
    restore(child, declared_width, true);
  }
  if (ideal_down) {
    restore(child, declared_height, false);
  }
  if (found != known.end()) {
    found->size = size;
    found->baselines = baselines;
    return *found;
  }
  if (known.size() >= remembered) {
    known.erase(known.begin());
  }
  known.push_back(
      Measured{.proposal = proposal, .size = size, .baselines = baselines});
  return known.back();
}

auto CustomArrangement::size_that_fits(std::size_t index,
                                       components::ProposedViewSize proposal)
    -> composition::Size {
  if (index >= members_.size()) {
    return composition::Size{};
  }
  return measured(index, proposal, false).size;
}

auto CustomArrangement::modifiers_of(const Member& member)
    -> const composition::Modifiers& {
  const composition::Modifiers& own = member.source.get().modifiers();
  if (member.carried.has_value() && own.layout_values.empty() &&
      own.alignment_guides.empty() && own.layout_priority == 0.0) {
    return member.carried->get();
  }
  return own;
}

auto CustomArrangement::modifiers(std::size_t index) const
    -> const composition::Modifiers& {
  return modifiers_of(members_[index]);
}

auto CustomArrangement::revalued(std::span<const Member> members) const
    -> bool {
  return !std::ranges::equal(
      members, valued_, [](const Member& member, const Valued& kept) {
        const composition::Modifiers& current = modifiers_of(member);
        return current.layout_priority == kept.priority &&
               current.layout_values == kept.values;
      });
}

void CustomArrangement::remember_values() {
  valued_.resize(members_.size());
  for (std::size_t index = 0; index < members_.size(); ++index) {
    const composition::Modifiers& current = modifiers(index);
    Valued& kept = valued_[index];
    kept.priority = current.layout_priority;
    if (kept.values != current.layout_values) {
      kept.values = current.layout_values;
    }
  }
}

auto CustomArrangement::dimensions(std::size_t index,
                                   components::ProposedViewSize proposal)
    -> composition::ViewDimensions {
  if (index >= members_.size()) {
    return composition::ViewDimensions{0.0F, 0.0F};
  }
  const Measured& found = measured(index, proposal, true);
  return composition::ViewDimensions{
      found.size.width, found.size.height,
      found.baselines.value_or(composition::TextBaselines{}),
      modifiers(index).alignment_guides};
}

auto CustomArrangement::priority(std::size_t index) const -> double {
  if (index >= members_.size()) {
    return 0.0;
  }
  return modifiers(index).layout_priority;
}

auto CustomArrangement::spacing(std::size_t index) const
    -> components::ViewSpacing {
  if (index >= members_.size()) {
    return components::ViewSpacing{};
  }
  return members_[index].spacing;
}

auto CustomArrangement::values(std::size_t index) const
    -> const composition::KeyedValues& {
  return modifiers(index).layout_values;
}

auto CustomArrangement::container_values(std::size_t index) const
    -> const composition::KeyedValues& {
  const Member& member = members_[index];
  const composition::KeyedValues& own = member.source.get().container_values();
  if (own.empty() && member.carried_values.has_value()) {
    return member.carried_values->get();
  }
  return own;
}

auto CustomArrangement::points(std::size_t index, theme::Space space) const
    -> float {
  if (index >= members_.size()) {
    return 0.0F;
  }
  return members_[index].theme.get().space(space);
}

auto CustomArrangement::layout_spacing() -> components::ViewSpacing {
  if (box_ == nullptr || state_ == nullptr) {
    return components::ViewSpacing{};
  }
  return box_->spacing(subviews(), *state_);
}

auto CustomArrangement::explicit_alignment(const composition::AlignmentKey& key,
                                           float width, float height)
    -> std::optional<float> {
  if (box_ == nullptr || state_ == nullptr || !box_->aligns()) {
    return std::nullopt;
  }
  return box_->explicit_alignment(
      key,
      composition::Rect{.x = 0.0F, .y = 0.0F, .width = width, .height = height},
      components::ProposedViewSize{width, height}, subviews(), *state_);
}

void CustomArrangement::place(std::size_t index, composition::Point position,
                              motion::UnitPoint anchor,
                              components::ProposedViewSize proposal) {
  if (index >= placements_.size()) {
    return;
  }
  Placement& placement = placements_[index];
  placement.position = position;
  placement.anchor = anchor;
  placement.proposal = proposal;
  placement.placed = true;
}

void CustomArrangement::hide(std::size_t index) {
  if (index < placements_.size()) {
    placements_[index].hidden = true;
  }
}

auto CustomArrangement::hidden(std::size_t index) const -> bool {
  return index < placements_.size() && placements_[index].hidden;
}

void CustomArrangement::settle(float width, float height) {
  if (box_ == nullptr || state_ == nullptr) {
    return;
  }
  if (laid_ && width == width_ && height == height_ &&
      !YGNodeIsDirty(host_.get())) {
    return;
  }
  for (Placement& placement : placements_) {
    placement = Placement{};
  }
  box_->place_subviews(
      composition::Rect{.x = 0.0F, .y = 0.0F, .width = width, .height = height},
      components::ProposedViewSize{width, height}, subviews(), *state_);
  for (std::size_t index = 0; index < members_.size(); ++index) {
    Placement& placement = placements_[index];
    if (!placement.placed) {
      placement.position =
          composition::Point{.x = width * 0.5F, .y = height * 0.5F};
      placement.anchor = motion::UnitPoint::center;
      placement.proposal = components::ProposedViewSize::unspecified;
    }
    const composition::Size size =
        measured(index, placement.proposal, false).size;
    const float shown_width = std::isfinite(size.width) ? size.width : width;
    const float shown_height =
        std::isfinite(size.height) ? size.height : height;
    const YGNodeRef wrapper = wrappers_[index].get();
    YGNodeStyleSetPosition(
        wrapper, YGEdgeLeft,
        placement.position.x - (placement.anchor.x * shown_width));
    YGNodeStyleSetPosition(
        wrapper, YGEdgeTop,
        placement.position.y - (placement.anchor.y * shown_height));
    YGNodeStyleSetWidth(wrapper, shown_width);
    YGNodeStyleSetHeight(wrapper, shown_height);
  }
  const YGNodeRef host = host_.get();
  YGNodeStyleSetWidth(host, width);
  YGNodeStyleSetHeight(host, height);
  YGNodeCalculateLayout(host, width, height, direction_);
  width_ = width;
  height_ = height;
  laid_ = true;
}

}
