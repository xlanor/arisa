#pragma once

#include <arisa/composition/alignment.hpp>
#include <arisa/composition/anchor.hpp>
#include <arisa/composition/container_corners.hpp>
#include <arisa/composition/content_margin_guide.hpp>
#include <arisa/composition/coordinate_space.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/layout_direction_behavior.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/composition/reserved_region.hpp>
#include <arisa/composition/size.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace arisa::composition {

class GeometryProxy {
 public:
  struct NamedSpace {
    ElementKey name;
    Rect frame;

    friend auto operator==(const NamedSpace&, const NamedSpace&)
        -> bool = default;
  };

  struct ScrollViews {
    std::optional<Rect> horizontal;
    std::optional<Rect> vertical;

    friend auto operator==(const ScrollViews&, const ScrollViews&)
        -> bool = default;
  };

  struct Surroundings {
    theme::Edges safe_area;
    std::optional<ContainerCorners> container;

    friend auto operator==(const Surroundings&, const Surroundings&)
        -> bool = default;
  };

  GeometryProxy() noexcept = default;

  using Spaces = std::shared_ptr<const std::vector<NamedSpace>>;

  GeometryProxy(Rect global, std::optional<Rect> scroll_view,
                std::vector<NamedSpace> spaces = {})
      : global_(global),
        scroll_view_(scroll_view),
        spaces_(spaces.empty()
                    ? nullptr
                    : std::make_shared<const std::vector<NamedSpace>>(
                          std::move(spaces))) {}

  GeometryProxy(Rect global, std::optional<Rect> scroll_view,
                Spaces spaces) noexcept
      : global_(global),
        scroll_view_(scroll_view),
        spaces_(std::move(spaces)) {}

  using Regions = std::shared_ptr<const std::vector<ReservedRegion>>;

  GeometryProxy(Rect global, std::optional<Rect> scroll_view, Spaces spaces,
                Regions regions, LayoutDirection direction) noexcept
      : global_(global),
        scroll_view_(scroll_view),
        spaces_(std::move(spaces)),
        regions_(std::move(regions)),
        direction_(direction) {}

  GeometryProxy(Rect global, std::optional<Rect> scroll_view, ScrollViews along,
                Spaces spaces, Regions regions,
                LayoutDirection direction) noexcept
      : global_(global),
        scroll_view_(scroll_view),
        along_(along),
        spaces_(std::move(spaces)),
        regions_(std::move(regions)),
        direction_(direction) {}

  GeometryProxy(Rect global, std::optional<Rect> scroll_view, ScrollViews along,
                Spaces spaces, Regions regions, LayoutDirection direction,
                Surroundings surroundings) noexcept
      : global_(global),
        scroll_view_(scroll_view),
        along_(along),
        spaces_(std::move(spaces)),
        regions_(std::move(regions)),
        direction_(direction),
        surroundings_(surroundings) {}

  auto size() const noexcept -> Size {
    return Size{.width = global_.width, .height = global_.height};
  }

  auto frame(const CoordinateSpace& space) const noexcept -> Rect {
    switch (space.kind()) {
      case CoordinateSpace::Kind::local:
        return Rect{.x = 0.0F,
                    .y = 0.0F,
                    .width = global_.width,
                    .height = global_.height};
      case CoordinateSpace::Kind::global:
        return global_;
      case CoordinateSpace::Kind::scroll_view:
        if (const std::optional<Rect> found = scroll_view_of(space)) {
          return relative_to(*found);
        }
        return global_;
      case CoordinateSpace::Kind::named:
        if (const NamedSpace* found = named(space)) {
          return relative_to(found->frame);
        }
        return global_;
    }
    return global_;
  }

  auto bounds(const CoordinateSpace& space) const noexcept
      -> std::optional<Rect> {
    if (space.kind() == CoordinateSpace::Kind::scroll_view) {
      return scroll_view_of(space);
    }
    if (const NamedSpace* found = named(space)) {
      return Rect{.x = found->frame.x - global_.x,
                  .y = found->frame.y - global_.y,
                  .width = found->frame.width,
                  .height = found->frame.height};
    }
    return std::nullopt;
  }

  auto reserved_regions(ReservedRegion::Kind kind,
                        ReservedRegion::QueryOptions options = {},
                        LayoutDirectionBehavior layout_direction_behavior =
                            LayoutDirectionBehavior::mirrors()) const
      -> std::vector<ReservedRegion> {
    std::vector<ReservedRegion> found;
    if (regions_ == nullptr) {
      return found;
    }
    const bool inactive =
        options.contains(ReservedRegion::QueryOptions::include_inactive);
    const bool mirrored = layout_direction_behavior.mirrored_in(direction_);
    for (const ReservedRegion& region : *regions_) {
      if (region.kind() != kind || (!region.is_active() && !inactive)) {
        continue;
      }
      const Rect global = region.frame();
      if (global.x >= global_.x + global_.width ||
          global.x + global.width <= global_.x ||
          global.y >= global_.y + global_.height ||
          global.y + global.height <= global_.y) {
        continue;
      }
      Rect local{.x = global.x - global_.x,
                 .y = global.y - global_.y,
                 .width = global.width,
                 .height = global.height};
      theme::Edges margins = region.margins();
      if (mirrored) {
        local.x = global_.width - (local.x + local.width);
        margins = theme::Edges{.top = margins.top,
                               .leading = margins.trailing,
                               .bottom = margins.bottom,
                               .trailing = margins.leading};
      }
      found.push_back(region.moved_to(local, margins));
    }
    return found;
  }

  auto safe_area_insets() const noexcept -> theme::Edges {
    return surroundings_.safe_area;
  }

  auto container_corner_insets() const noexcept
      -> theme::RectangleCornerInsets {
    return surroundings_.container.has_value()
               ? surroundings_.container->insets(global_)
               : theme::RectangleCornerInsets{};
  }

  auto concentric_corner_radii() const noexcept
      -> std::optional<theme::RectangleCornerRadii> {
    return surroundings_.container.has_value()
               ? surroundings_.container->concentric_radii(global_)
               : theme::RectangleCornerRadii{};
  }

  auto concentric_corner_radii(Rect frame) const noexcept
      -> std::optional<theme::RectangleCornerRadii> {
    const Rect placed{.x = global_.x + frame.x,
                      .y = global_.y + frame.y,
                      .width = frame.width,
                      .height = frame.height};
    return surroundings_.container.has_value()
               ? surroundings_.container->concentric_radii(placed)
               : theme::RectangleCornerRadii{};
  }

  auto content_margins(ContentMarginGuide guide,
                       theme::Edge edges = theme::Edge::all) const noexcept
      -> theme::Edges {
    return guide.insets(edges);
  }

  auto operator[](const Anchor<Rect>& anchor) const noexcept -> Rect {
    return Rect{.x = anchor.global_.x - global_.x,
                .y = anchor.global_.y - global_.y,
                .width = anchor.global_.width,
                .height = anchor.global_.height};
  }

  auto operator[](const Anchor<Point>& anchor) const noexcept -> Point {
    return Point{.x = anchor.global_.x - global_.x,
                 .y = anchor.global_.y - global_.y};
  }

  template <class Value>
  auto operator[](const Anchor<std::vector<Value>>& anchor) const
      -> std::vector<Value> {
    std::vector<Value> local;
    local.reserve(anchor.global_.size());
    for (const Value& global : anchor.global_) {
      Anchor<Value> each;
      each.global_ = global;
      local.push_back((*this)[each]);
    }
    return local;
  }

  friend auto operator==(const GeometryProxy& left, const GeometryProxy& right)
      -> bool {
    if (left.global_ != right.global_ ||
        left.scroll_view_ != right.scroll_view_ ||
        left.along_ != right.along_ || left.direction_ != right.direction_ ||
        left.surroundings_ != right.surroundings_ ||
        !same_regions(left.regions_, right.regions_)) {
      return false;
    }
    if (left.spaces_ == right.spaces_) {
      return true;
    }
    const bool left_empty = left.spaces_ == nullptr || left.spaces_->empty();
    const bool right_empty = right.spaces_ == nullptr || right.spaces_->empty();
    if (left_empty || right_empty) {
      return left_empty == right_empty;
    }
    return *left.spaces_ == *right.spaces_;
  }

 private:
  static auto same_regions(const Regions& left, const Regions& right) -> bool {
    if (left == right) {
      return true;
    }
    const bool left_empty = left == nullptr || left->empty();
    const bool right_empty = right == nullptr || right->empty();
    if (left_empty || right_empty) {
      return left_empty == right_empty;
    }
    return *left == *right;
  }

  auto scroll_view_of(const CoordinateSpace& space) const noexcept
      -> std::optional<Rect> {
    if (!space.axis().has_value()) {
      return scroll_view_;
    }
    return *space.axis() == Axis::horizontal ? along_.horizontal
                                             : along_.vertical;
  }

  auto relative_to(const Rect& origin) const noexcept -> Rect {
    return Rect{.x = global_.x - origin.x,
                .y = global_.y - origin.y,
                .width = global_.width,
                .height = global_.height};
  }

  auto named(const CoordinateSpace& space) const noexcept -> const NamedSpace* {
    if (space.kind() != CoordinateSpace::Kind::named ||
        !space.name().has_value()) {
      return nullptr;
    }
    if (spaces_ == nullptr) {
      return nullptr;
    }
    const auto found =
        std::ranges::find(*spaces_, *space.name(), &NamedSpace::name);
    return found == spaces_->end() ? nullptr : &*found;
  }

  Rect global_;
  std::optional<Rect> scroll_view_;
  ScrollViews along_;
  Spaces spaces_;
  Regions regions_;
  LayoutDirection direction_{LayoutDirection::left_to_right};
  Surroundings surroundings_;
};

}
