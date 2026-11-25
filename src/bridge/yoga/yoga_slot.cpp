#include <bridge/yoga/custom_arrangement.hpp>
#include <bridge/yoga/grid_arrangement.hpp>
#include <bridge/yoga/yoga_layout.hpp>
#include <bridge/yoga/yoga_slot.hpp>

#include <arisa/components/text.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace arisa::bridge::yoga {

void YogaSlot::NodeRelease::release(handle_type& node) noexcept {
  if (const YGNodeRef owner = YGNodeGetOwner(node)) {
    YGNodeRemoveChild(owner, node);
  }
  YGNodeFree(node);
}

YogaSlot::YogaSlot(Token, std::shared_ptr<ConfigHandle> config, NodeHandle node,
                   std::uint64_t engine) noexcept
    : config_(std::move(config)), node_(std::move(node)), engine_id_(engine) {
  YGNodeSetContext(node_.get(), this);
}

YogaSlot::~YogaSlot() {
  custom_.reset();
  grid_.reset();
  scratch_.reset();
  host_.reset();
  if (inner_.valid()) {
    YGNodeRemoveAllChildren(inner_.get());
  }
  inner_.reset();
  node_.reset();
}

auto YogaSlot::create(std::shared_ptr<ConfigHandle> config,
                      std::uint64_t engine)
    -> std::expected<std::unique_ptr<YogaSlot>, layout::LayoutError> {
  NodeHandle node{YGNodeNewWithConfig(config->get())};
  if (!node.valid()) {
    return std::unexpected(
        layout::LayoutError{layout::LayoutErrorCode::engine_failure});
  }
  return std::make_unique<YogaSlot>(Token{}, std::move(config), std::move(node),
                                    engine);
}

void YogaSlot::set_length(YGNodeRef node, Length length, bool horizontal) {
  switch (length.unit) {
    case Length::Unit::automatic:
      if (horizontal) {
        YGNodeStyleSetWidthAuto(node);
      } else {
        YGNodeStyleSetHeightAuto(node);
      }
      return;
    case Length::Unit::points:
      if (horizontal) {
        YGNodeStyleSetWidth(node, length.value);
      } else {
        YGNodeStyleSetHeight(node, length.value);
      }
      return;
    case Length::Unit::percent:
      if (horizontal) {
        YGNodeStyleSetWidthPercent(node, length.value);
      } else {
        YGNodeStyleSetHeightPercent(node, length.value);
      }
      return;
  }
}

void YogaSlot::apply(const Style& style) {
  if (style_ == style) {
    return;
  }
  write(node_.get(), style, gap_);
  style_ = style;
  if (guide_ != theme::Edges{} || corner_ != theme::Edges{}) {
    write_margins(node_.get());
  }
  if (inset_.has_value()) {
    write_inset(node_.get());
  }
  if (cap_.has_value()) {
    YGNodeStyleSetMaxWidth(node_.get(),
                           std::min(style.max_width.value_or(*cap_), *cap_));
  }
  if (extent_.has_value() || share_.has_value()) {
    write_bounds();
  }
}

auto YogaSlot::set_shift(theme::Edges shift) -> bool {
  if (std::abs(shift_.leading - shift.leading) < 0.01F &&
      std::abs(shift_.top - shift.top) < 0.01F) {
    return false;
  }
  shift_ = shift;
  YGNodeStyleSetPosition(node_.get(), YGEdgeStart, shift.leading);
  YGNodeStyleSetPosition(node_.get(), YGEdgeTop, shift.top);
  return true;
}

auto YogaSlot::set_extent(std::optional<std::array<float, 2>> extent) -> bool {
  if (extent_.has_value() == extent.has_value() &&
      (!extent.has_value() ||
       (std::abs((*extent_)[0] - (*extent)[0]) < 0.01F &&
        std::abs((*extent_)[1] - (*extent)[1]) < 0.01F))) {
    return false;
  }
  extent_ = extent;
  write_bounds();
  return true;
}

auto YogaSlot::set_share(std::optional<float> height) -> bool {
  if (share_.has_value() == height.has_value() &&
      (!height.has_value() || std::abs(*share_ - *height) < 0.01F)) {
    return false;
  }
  share_ = height;
  write_bounds();
  return true;
}

void YogaSlot::write_bounds() const {
  const YGNodeRef node = node_.get();
  const std::optional<float> min_width =
      style_.has_value() ? style_->min_width : std::nullopt;
  const std::optional<float> min_height =
      style_.has_value() ? style_->min_height : std::nullopt;
  const std::optional<float> max_height =
      style_.has_value() ? style_->max_height : std::nullopt;
  if (extent_.has_value()) {
    YGNodeStyleSetMinWidth(node,
                           std::max(min_width.value_or(0.0F), (*extent_)[0]));
    YGNodeStyleSetMinHeight(node,
                            std::max(min_height.value_or(0.0F), (*extent_)[1]));
  } else {
    YGNodeStyleSetMinWidth(node, min_width.value_or(YGUndefined));
    YGNodeStyleSetMinHeight(node, min_height.value_or(YGUndefined));
  }
  if (share_.has_value()) {
    YGNodeStyleSetMaxHeight(node,
                            std::min(max_height.value_or(*share_), *share_));
  } else {
    YGNodeStyleSetMaxHeight(node, max_height.value_or(YGUndefined));
  }
}

auto YogaSlot::set_cap(std::optional<float> width) -> bool {
  if (cap_.has_value() == width.has_value() &&
      (!width.has_value() || std::abs(*cap_ - *width) < 0.01F)) {
    return false;
  }
  cap_ = width;
  const std::optional<float> declared =
      style_.has_value() ? style_->max_width : std::nullopt;
  if (cap_.has_value()) {
    YGNodeStyleSetMaxWidth(node_.get(),
                           std::min(declared.value_or(*cap_), *cap_));
  } else {
    YGNodeStyleSetMaxWidth(node_.get(), declared.value_or(YGUndefined));
  }
  return true;
}

void YogaSlot::write_margins(YGNodeRef node) const {
  const theme::Edges margin =
      style_.has_value() ? style_->margin : theme::Edges{};
  YGNodeStyleSetMargin(node, YGEdgeTop,
                       margin.top + gap_.top + guide_.top + corner_.top);
  YGNodeStyleSetMargin(
      node, YGEdgeStart,
      margin.leading + gap_.leading + guide_.leading + corner_.leading);
  YGNodeStyleSetMargin(node, YGEdgeBottom, margin.bottom + corner_.bottom);
  YGNodeStyleSetMargin(node, YGEdgeEnd, margin.trailing + corner_.trailing);
}

auto YogaSlot::set_corner(theme::Edges corner) -> bool {
  if (std::abs(corner_.top - corner.top) < 0.01F &&
      std::abs(corner_.leading - corner.leading) < 0.01F &&
      std::abs(corner_.bottom - corner.bottom) < 0.01F &&
      std::abs(corner_.trailing - corner.trailing) < 0.01F) {
    return false;
  }
  corner_ = corner;
  write_margins(node_.get());
  return true;
}

void YogaSlot::write_inset(YGNodeRef node) const {
  const theme::Edges padding =
      style_.has_value() ? style_->padding : theme::Edges{};
  YGNodeStyleSetPadding(node, YGEdgeStart,
                        inset_.has_value() ? inset_->leading : padding.leading);
  YGNodeStyleSetPadding(
      node, YGEdgeEnd,
      inset_.has_value() ? inset_->trailing : padding.trailing);
}

void YogaSlot::set_inset(std::optional<theme::Edges> inset) {
  if (inset_ == inset) {
    return;
  }
  inset_ = inset;
  write_inset(node_.get());
}

void YogaSlot::set_guide(theme::Edges guide) {
  if (guide_ == guide) {
    return;
  }
  guide_ = guide;
  write_margins(node_.get());
}

void YogaSlot::align(const std::optional<Style>& inner) {
  const YGNodeRef node = node_.get();
  if (!inner.has_value()) {
    if (!inner_.valid()) {
      return;
    }
    const bool measuring = YGNodeHasMeasureFunc(inner_.get());
    YGNodeRemoveAllChildren(inner_.get());
    inner_.reset();
    inner_style_.reset();
    if (measuring) {
      YGNodeSetMeasureFunc(node, &YogaSlot::measure_text);
      YGNodeSetBaselineFunc(node, &YogaSlot::baseline);
      YGNodeMarkDirty(node);
    }
    return;
  }
  if (!inner_.valid()) {
    const bool measuring = YGNodeHasMeasureFunc(node);
    if (measuring) {
      YGNodeSetMeasureFunc(node, nullptr);
      YGNodeSetBaselineFunc(node, nullptr);
    }
    inner_ = NodeHandle{YGNodeNewWithConfig(config_->get())};
    YGNodeSetContext(inner_.get(), this);
    if (YGNodeGetChildCount(node) > 0) {
      YGNodeRemoveAllChildren(node);
    }
    YGNodeInsertChild(node, inner_.get(), 0);
    if (measuring) {
      YGNodeSetMeasureFunc(inner_.get(), &YogaSlot::measure_text);
      YGNodeSetBaselineFunc(inner_.get(), &YogaSlot::baseline);
      YGNodeMarkDirty(inner_.get());
    }
  }
  if (inner_style_ != inner) {
    write(inner_.get(), *inner, theme::Edges{});
    inner_style_ = inner;
    apply_fit();
  }
}

void YogaSlot::aspect(const std::optional<composition::AspectRatio>& wanted) {
  if (aspect_ == wanted) {
    return;
  }
  aspect_ = wanted;
  fitted_.reset();
  natural_ratio_.reset();
  if (inner_.valid() && inner_style_.has_value()) {
    write(inner_.get(), *inner_style_, theme::Edges{});
  }
}

auto YogaSlot::fit() -> bool {
  if (!fits()) {
    return false;
  }
  const YGNodeRef node = node_.get();
  const YGNodeRef inner = inner_.get();
  const float width = YGNodeLayoutGetWidth(node) -
                      YGNodeLayoutGetPadding(node, YGEdgeLeft) -
                      YGNodeLayoutGetPadding(node, YGEdgeRight);
  const float height = YGNodeLayoutGetHeight(node) -
                       YGNodeLayoutGetPadding(node, YGEdgeTop) -
                       YGNodeLayoutGetPadding(node, YGEdgeBottom);
  if (!natural_ratio_.has_value() && !fitted_.has_value()) {
    const float natural_width = YGNodeLayoutGetWidth(inner);
    const float natural_height = YGNodeLayoutGetHeight(inner);
    if (natural_width > 0.0F && natural_height > 0.0F) {
      natural_ratio_ = natural_width / natural_height;
    }
  }
  const std::optional<float> ratio =
      aspect_->ratio.has_value() ? aspect_->ratio : natural_ratio_;
  if (!ratio.has_value() || !(width > 0.0F) || !(height > 0.0F)) {
    return false;
  }
  const float across = height * *ratio;
  const float chosen = aspect_->mode == composition::ContentMode::fit
                           ? std::min(width, across)
                           : std::max(width, across);
  const std::array<float, 2> size{chosen, chosen / *ratio};
  if (fitted_ == size) {
    return false;
  }
  fitted_ = size;
  apply_fit();
  return true;
}

void YogaSlot::apply_fit() {
  if (!fitted_.has_value() || !inner_.valid()) {
    return;
  }
  const YGNodeRef inner = inner_.get();
  YGNodeStyleSetWidth(inner, (*fitted_)[0]);
  YGNodeStyleSetHeight(inner, (*fitted_)[1]);
  YGNodeStyleSetFlexShrink(inner, 0.0F);
  YGNodeStyleSetFlexGrow(inner, 0.0F);
  YGNodeStyleSetMinWidth(inner, YGUndefined);
  YGNodeStyleSetMaxWidth(inner, YGUndefined);
  YGNodeStyleSetMinHeight(inner, YGUndefined);
  YGNodeStyleSetMaxHeight(inner, YGUndefined);
  YGNodeStyleSetAlignSelf(inner, YGAlignAuto);
}

void YogaSlot::write(YGNodeRef node, const Style& style, theme::Edges gap) {
  YGNodeStyleSetDirection(node, style.direction);
  YGNodeStyleSetFlexDirection(node, style.flex_direction);
  YGNodeStyleSetJustifyContent(node, style.justify);
  YGNodeStyleSetAlignItems(node, style.align_items);
  YGNodeStyleSetAlignSelf(node, style.align_self);
  YGNodeStyleSetAlignContent(node, style.align_content);
  YGNodeStyleSetFlexWrap(node, style.wrap);
  YGNodeStyleSetPositionType(node, style.position);
  YGNodeStyleSetOverflow(node, style.overflow);
  YGNodeStyleSetFlexGrow(node, style.grow);
  YGNodeStyleSetFlexShrink(node, style.shrink);
  if (style.basis.has_value()) {
    YGNodeStyleSetFlexBasis(node, *style.basis);
  } else {
    YGNodeStyleSetFlexBasisAuto(node);
  }
  YGNodeStyleSetPadding(node, YGEdgeTop, style.padding.top);
  YGNodeStyleSetPadding(node, YGEdgeStart, style.padding.leading);
  YGNodeStyleSetPadding(node, YGEdgeBottom, style.padding.bottom);
  YGNodeStyleSetPadding(node, YGEdgeEnd, style.padding.trailing);
  YGNodeStyleSetMargin(node, YGEdgeTop, style.margin.top + gap.top);
  YGNodeStyleSetMargin(node, YGEdgeStart, style.margin.leading + gap.leading);
  YGNodeStyleSetMargin(node, YGEdgeBottom, style.margin.bottom);
  YGNodeStyleSetMargin(node, YGEdgeEnd, style.margin.trailing);
  YGNodeStyleSetGap(node, YGGutterRow, style.gap_row);
  YGNodeStyleSetGap(node, YGGutterColumn, style.gap_column);
  set_length(node, style.width, true);
  set_length(node, style.height, false);
  YGNodeStyleSetAspectRatio(node, style.aspect_ratio.value_or(YGUndefined));
  YGNodeStyleSetMinWidth(node, style.min_width.value_or(YGUndefined));
  YGNodeStyleSetMaxWidth(node, style.max_width.value_or(YGUndefined));
  YGNodeStyleSetMinHeight(node, style.min_height.value_or(YGUndefined));
  YGNodeStyleSetMaxHeight(node, style.max_height.value_or(YGUndefined));
  if (style.within_parent) {
    YGNodeStyleSetMaxWidthPercent(node, 100.0F);
    YGNodeStyleSetMaxHeightPercent(node, 100.0F);
  }
}

void YogaSlot::set_gap(theme::Edges gap) {
  if (gap_ == gap) {
    return;
  }
  gap_ = gap;
  write_margins(node_.get());
}

void YogaSlot::measure(const std::optional<Measurement>& measurement,
                       YogaLayout& engine, const composition::NodeCost& cost) {
  engine_ = engine;
  cost_ = cost;
  if (!measurement.has_value()) {
    return;
  }
  const YGNodeRef node = content();
  if (!YGNodeHasMeasureFunc(node)) {
    YGNodeSetMeasureFunc(node, &YogaSlot::measure_text);
    YGNodeSetBaselineFunc(node, &YogaSlot::baseline);
  }
  const bool same = measured_.has_value() &&
                    measured_->text == measurement->text &&
                    measured_->style == measurement->style &&
                    measured_->wraps == measurement->wraps &&
                    measured_->scale == measurement->scale &&
                    measured_->typesetter == measurement->typesetter &&
                    measured_->fitting == measurement->fitting &&
                    measured_->reserved == measurement->reserved &&
                    measured_->variants == measurement->variants;
  if (same) {
    return;
  }
  if (!measured_.has_value()) {
    measured_.emplace(Measured{.text = std::string{measurement->text},
                               .style = measurement->style,
                               .wraps = measurement->wraps,
                               .scale = measurement->scale,
                               .typesetter = measurement->typesetter,
                               .fitting = measurement->fitting,
                               .reserved = measurement->reserved,
                               .variants = measurement->variants});
  } else {
    measured_->text.assign(measurement->text);
    measured_->style = measurement->style;
    measured_->wraps = measurement->wraps;
    measured_->scale = measurement->scale;
    measured_->typesetter = measurement->typesetter;
    measured_->fitting = measurement->fitting;
    measured_->reserved = measurement->reserved;
    measured_->variants = measurement->variants;
  }
  YGNodeMarkDirty(node);
}

auto YogaSlot::measure_text(YGNodeConstRef node, float width,
                            YGMeasureMode width_mode, float height,
                            YGMeasureMode height_mode) -> YGSize {
  const auto& slot = *static_cast<const YogaSlot*>(YGNodeGetContext(node));
  if (!slot.engine_.has_value() || !slot.measured_.has_value()) {
    return YGSize{.width = 0.0F, .height = 0.0F};
  }
  const Measured& measured = *slot.measured_;
  std::string_view shown = measured.text;
  if (!measured.variants.empty() && width_mode != YGMeasureModeUndefined) {
    const std::optional<std::size_t> chosen = components::Text::variant_for(
        measured.variants, width, [&](const std::string& variant) {
          return slot.engine_->get()
              .measure(variant, measured.style, false, 0.0F,
                       YGMeasureModeUndefined, std::nullopt, measured.fitting,
                       std::nullopt, slot.cost_)
              .width;
        });
    if (chosen.has_value()) {
      shown = measured.variants[*chosen];
    }
  }
  return slot.engine_->get().measure(
      shown, measured.style, measured.wraps, width, width_mode,
      height_mode == YGMeasureModeUndefined ? std::nullopt
                                            : std::optional<float>{height},
      measured.fitting, measured.reserved, slot.cost_);
}

auto YogaSlot::baseline(YGNodeConstRef node, float width, float) -> float {
  const auto& slot = *static_cast<const YogaSlot*>(YGNodeGetContext(node));
  if (!slot.engine_.has_value() || !slot.measured_.has_value()) {
    return 0.0F;
  }
  const float content = width - YGNodeLayoutGetPadding(node, YGEdgeLeft) -
                        YGNodeLayoutGetPadding(node, YGEdgeRight);
  const Measured& measured = *slot.measured_;
  return YGNodeLayoutGetPadding(node, YGEdgeTop) +
         slot.engine_->get().baseline(measured.text, measured.style,
                                      measured.wraps, content);
}

void YogaSlot::adopt(YGNodeRef parent, std::span<const YGNodeRef> children) {
  const std::size_t count = YGNodeGetChildCount(parent);
  bool same = count == children.size();
  for (std::size_t index = 0; same && index < count; ++index) {
    same = YGNodeGetChild(parent, index) == children[index];
  }
  if (same) {
    return;
  }
  for (std::size_t index = count; index-- > 0;) {
    const YGNodeRef child = YGNodeGetChild(parent, index);
    if (std::ranges::find(children, child) == children.end()) {
      YGNodeRemoveChild(parent, child);
    }
  }
  for (const YGNodeRef child : children) {
    const YGNodeRef owner = YGNodeGetOwner(child);
    if (owner != nullptr && owner != parent) {
      YGNodeRemoveChild(owner, child);
    }
  }
  for (std::size_t index = 0; index < children.size(); ++index) {
    const YGNodeRef child = children[index];
    if (YGNodeGetChild(parent, index) == child) {
      continue;
    }
    if (YGNodeGetOwner(child) == parent) {
      YGNodeSetChildren(parent, children.data(), children.size());
      return;
    }
    YGNodeInsertChild(parent, child, index);
  }
}

void YogaSlot::set_children(std::span<const YGNodeRef> children) {
  unbind();
  leave_grid();
  leave_custom();
  adopt(content(), children);
}

void YogaSlot::leave_custom() {
  if (custom_ == nullptr) {
    return;
  }
  YGNodeSetMeasureFunc(node_.get(), nullptr);
  custom_.reset();
}

auto YogaSlot::custom(YGDirection direction) -> CustomArrangement& {
  unbind();
  leave_grid();
  const YGNodeRef node = node_.get();
  if (custom_ == nullptr) {
    if (YGNodeGetChildCount(node) > 0) {
      YGNodeRemoveAllChildren(node);
    }
    custom_ = std::make_unique<CustomArrangement>(config_, *this, direction);
    YGNodeSetDirtiedFunc(custom_->host(), &YogaSlot::host_dirtied);
    YGNodeSetMeasureFunc(node, &YogaSlot::measure_custom);
    YGNodeMarkDirty(node);
  }
  return *custom_;
}

auto YogaSlot::arrangement() noexcept
    -> std::optional<std::reference_wrapper<CustomArrangement>> {
  if (custom_ == nullptr) {
    return std::nullopt;
  }
  return std::ref(*custom_);
}

auto YogaSlot::arrangement() const noexcept
    -> std::optional<std::reference_wrapper<const CustomArrangement>> {
  if (custom_ == nullptr) {
    return std::nullopt;
  }
  return std::cref(*custom_);
}

auto YogaSlot::measure_custom(YGNodeConstRef node, float width,
                              YGMeasureMode width_mode, float height,
                              YGMeasureMode height_mode) -> YGSize {
  auto& slot = *static_cast<YogaSlot*>(YGNodeGetContext(node));
  if (slot.custom_ == nullptr) {
    return YGSize{.width = 0.0F, .height = 0.0F};
  }
  return slot.custom_->measure(width, width_mode, height, height_mode);
}

void YogaSlot::leave_grid() {
  if (grid_ == nullptr) {
    return;
  }
  YGNodeSetMeasureFunc(node_.get(), nullptr);
  grid_.reset();
}

auto YogaSlot::grid(YGDirection direction) -> GridArrangement& {
  unbind();
  leave_custom();
  const YGNodeRef node = node_.get();
  if (grid_ == nullptr) {
    if (YGNodeGetChildCount(node) > 0) {
      YGNodeRemoveAllChildren(node);
    }
    grid_ = std::make_unique<GridArrangement>(config_, *this, direction);
    YGNodeSetDirtiedFunc(grid_->host(), &YogaSlot::host_dirtied);
    YGNodeSetMeasureFunc(node, &YogaSlot::measure_grid);
    YGNodeMarkDirty(node);
  }
  return *grid_;
}

void YogaSlot::relay_grid() {
  if (grid_ == nullptr) {
    return;
  }
  grid_->invalidate();
  YGNodeMarkDirty(node_.get());
}

auto YogaSlot::measure_grid(YGNodeConstRef node, float width,
                            YGMeasureMode width_mode, float height,
                            YGMeasureMode height_mode) -> YGSize {
  const auto& slot = *static_cast<const YogaSlot*>(YGNodeGetContext(node));
  if (slot.grid_ == nullptr) {
    return YGSize{.width = 0.0F, .height = 0.0F};
  }
  return slot.grid_->measure(width, width_mode, height, height_mode);
}

auto YogaSlot::inner_length(bool vertical) const -> float {
  const YGNodeRef node = node_.get();
  return vertical ? YGNodeLayoutGetWidth(node) -
                        YGNodeLayoutGetPadding(node, YGEdgeLeft) -
                        YGNodeLayoutGetPadding(node, YGEdgeRight) -
                        YGNodeLayoutGetBorder(node, YGEdgeLeft) -
                        YGNodeLayoutGetBorder(node, YGEdgeRight)
                  : YGNodeLayoutGetHeight(node) -
                        YGNodeLayoutGetPadding(node, YGEdgeTop) -
                        YGNodeLayoutGetPadding(node, YGEdgeBottom) -
                        YGNodeLayoutGetBorder(node, YGEdgeTop) -
                        YGNodeLayoutGetBorder(node, YGEdgeBottom);
}

void YogaSlot::unbind() {
  if (!host_.valid()) {
    return;
  }
  YGNodeSetMeasureFunc(node_.get(), nullptr);
  host_.reset();
  scratch_.reset();
  naturals_.clear();
  arrangement_.reset();
  laid_ = false;
}

void YogaSlot::bound(std::span<const YGNodeRef> children,
                     const Bound& arrangement) {
  leave_grid();
  leave_custom();
  if (!host_.valid()) {
    host_ = NodeHandle{YGNodeNewWithConfig(config_->get())};
    YGNodeSetContext(host_.get(), this);
    YGNodeSetDirtiedFunc(host_.get(), &YogaSlot::host_dirtied);
  }
  const YGNodeRef host = host_.get();
  if (arrangement_ != arrangement) {
    YGNodeStyleSetFlexDirection(host, arrangement.vertical
                                          ? YGFlexDirectionColumn
                                          : YGFlexDirectionRow);
    YGNodeStyleSetAlignItems(host, arrangement.align);
    YGNodeStyleSetGap(host, arrangement.vertical ? YGGutterRow : YGGutterColumn,
                      arrangement.gap);
    YGNodeStyleSetDirection(host, arrangement.direction);
    arrangement_ = arrangement;
  }
  const YGNodeRef node = node_.get();
  if (YGNodeGetChildCount(node) > 0) {
    YGNodeRemoveAllChildren(node);
  }
  if (!YGNodeHasMeasureFunc(node)) {
    YGNodeSetMeasureFunc(node, &YogaSlot::measure_bound);
    laid_ = false;
  }
  std::erase_if(naturals_, [&children](const auto& entry) {
    return std::ranges::find(children, entry.first) == children.end();
  });
  measure_fresh(children);
  adopt(host, children);
  if (laid_ && !YGNodeIsDirty(host)) {
    return;
  }
  natural_cross_ = 0.0F;
  float natural_main = 0.0F;
  for (const auto& [child, natural] : naturals_) {
    natural_cross_ = std::max(natural_cross_, natural[0]);
    natural_main += natural[1];
  }
  if (laid_) {
    lay_host(cross_);
  } else {
    main_ = natural_main +
            (arrangement.gap *
             static_cast<float>(std::max<std::size_t>(children.size(), 1) - 1));
  }
  YGNodeMarkDirty(node);
}

void YogaSlot::measure_fresh(std::span<const YGNodeRef> children) {
  const bool vertical = arrangement_->vertical;
  if (!scratch_.valid()) {
    scratch_ = NodeHandle{YGNodeNewWithConfig(config_->get())};
    YGNodeStyleSetAlignItems(scratch_.get(), YGAlignFlexStart);
  }
  const YGNodeRef scratch = scratch_.get();
  YGNodeStyleSetFlexDirection(
      scratch, vertical ? YGFlexDirectionColumn : YGFlexDirectionRow);
  std::size_t placed = 0;
  for (const YGNodeRef child : children) {
    const bool known = naturals_.contains(child) && !YGNodeIsDirty(child) &&
                       !std::isnan(YGNodeLayoutGetWidth(child));
    if (known) {
      continue;
    }
    if (YGNodeGetOwner(child) == nullptr) {
      YGNodeCalculateLayout(child, YGUndefined, YGUndefined,
                            arrangement_->direction);
      naturals_.insert_or_assign(child, natural_of(child));
      continue;
    }
    YGNodeRemoveChild(YGNodeGetOwner(child), child);
    YGNodeInsertChild(scratch, child, placed++);
  }
  if (placed == 0) {
    return;
  }
  YGNodeCalculateLayout(scratch, YGUndefined, YGUndefined,
                        arrangement_->direction);
  for (std::size_t index = 0; index < placed; ++index) {
    const YGNodeRef child = YGNodeGetChild(scratch, index);
    naturals_.insert_or_assign(child, natural_of(child));
  }
  YGNodeRemoveAllChildren(scratch);
}

auto YogaSlot::natural_of(YGNodeRef child) const -> std::array<float, 2> {
  const float width = YGNodeLayoutGetWidth(child) +
                      YGNodeLayoutGetMargin(child, YGEdgeLeft) +
                      YGNodeLayoutGetMargin(child, YGEdgeRight);
  const float height = YGNodeLayoutGetHeight(child) +
                       YGNodeLayoutGetMargin(child, YGEdgeTop) +
                       YGNodeLayoutGetMargin(child, YGEdgeBottom);
  return arrangement_->vertical ? std::array{width, height}
                                : std::array{height, width};
}

void YogaSlot::lay_host(float cross) {
  const YGNodeRef host = host_.get();
  const bool vertical = arrangement_->vertical;
  YGNodeCalculateLayout(host, vertical ? cross : YGUndefined,
                        vertical ? YGUndefined : cross,
                        arrangement_->direction);
  cross_ = cross;
  laid_ = true;
  main_ = vertical ? YGNodeLayoutGetHeight(host) : YGNodeLayoutGetWidth(host);
}

auto YogaSlot::settle() -> bool {
  const YGNodeRef node = node_.get();
  if (custom_ != nullptr) {
    custom_->settle(std::max(inner_length(true), 0.0F),
                    std::max(inner_length(false), 0.0F));
    return false;
  }
  if (grid_ != nullptr) {
    const float across = inner_length(grid_->vertical());
    const float along = inner_length(!grid_->vertical());
    if (!grid_->settle(std::max(across, 0.0F), std::max(along, 0.0F))) {
      return false;
    }
    YGNodeMarkDirty(node);
    return true;
  }
  if (!host_.valid()) {
    return false;
  }
  const bool vertical = arrangement_->vertical;
  const float cross = inner_length(vertical);
  if (laid_ && cross == cross_ && !YGNodeIsDirty(host_.get())) {
    return false;
  }
  const float before = main_;
  lay_host(std::max(cross, 0.0F));
  if (main_ == before) {
    return false;
  }
  YGNodeMarkDirty(node);
  return true;
}

auto YogaSlot::anchor(YGNodeRef root) noexcept -> YGNodeRef {
  const void* context = YGNodeGetContext(root);
  if (context == nullptr) {
    return nullptr;
  }
  const auto& slot = *static_cast<const YogaSlot*>(context);
  const bool hosted = slot.host_.get() == root ||
                      (slot.grid_ != nullptr && slot.grid_->host() == root) ||
                      (slot.custom_ != nullptr && slot.custom_->host() == root);
  return hosted ? slot.node_.get() : nullptr;
}

void YogaSlot::host_dirtied(YGNodeConstRef host) {
  const auto& slot = *static_cast<const YogaSlot*>(YGNodeGetContext(host));
  const YGNodeRef node = slot.node_.get();
  if (YGNodeHasMeasureFunc(node) && !YGNodeIsDirty(node)) {
    YGNodeMarkDirty(node);
  }
}

auto YogaSlot::measure_bound(YGNodeConstRef node, float width,
                             YGMeasureMode width_mode, float height,
                             YGMeasureMode height_mode) -> YGSize {
  const auto& slot = *static_cast<const YogaSlot*>(YGNodeGetContext(node));
  if (!slot.arrangement_.has_value()) {
    return YGSize{.width = 0.0F, .height = 0.0F};
  }
  const bool vertical = slot.arrangement_->vertical;
  const float offered = vertical ? width : height;
  const YGMeasureMode mode = vertical ? width_mode : height_mode;
  float cross = slot.natural_cross_;
  if (mode == YGMeasureModeExactly) {
    cross = offered;
  } else if (mode == YGMeasureModeAtMost) {
    cross = std::min(offered, slot.natural_cross_);
  }
  if (vertical) {
    return YGSize{.width = cross, .height = slot.main_};
  }
  return YGSize{.width = slot.main_, .height = cross};
}

}
