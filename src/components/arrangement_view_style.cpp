#include <arisa/components/any_layout.hpp>
#include <arisa/components/arrangement_view.hpp>
#include <arisa/components/arrangement_view_style.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/geometry_reader.hpp>
#include <arisa/components/layout_subviews.hpp>
#include <arisa/components/proposed_view_size.hpp>
#include <arisa/composition/geometry_proxy.hpp>
#include <arisa/composition/overlay_arrangement_edges.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/composition/split_arrangement_sizing.hpp>

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

namespace arisa::components {

class ArrangementViewStyle::SplitLayout {
 public:
  static constexpr float default_ratio = 0.5F;

  explicit SplitLayout(composition::Axis axis) noexcept : axis_(axis) {}

  auto size_that_fits(ProposedViewSize proposal,
                      const LayoutSubviews& subviews) const
      -> composition::Size {
    static_cast<void>(subviews);
    return proposal.replacing_unspecified_dimensions();
  }

  void place_subviews(composition::Rect bounds, ProposedViewSize proposal,
                      const LayoutSubviews& subviews) const {
    static_cast<void>(proposal);
    if (subviews.size() < 2) {
      for (const LayoutSubview& subview : subviews) {
        subview.place(composition::Point{.x = bounds.x, .y = bounds.y},
                      motion::UnitPoint::top_leading,
                      ProposedViewSize{bounds.width, bounds.height});
      }
      return;
    }
    const bool horizontal = axis_ == composition::Axis::horizontal;
    const float length = horizontal ? bounds.width : bounds.height;
    const float cross = horizontal ? bounds.height : bounds.width;
    const float second = secondary_length(subviews, length, cross);
    const float first = length - second;
    const bool mirrored =
        horizontal && subviews.layout_direction() ==
                          composition::LayoutDirection::right_to_left;
    const float primary_offset = mirrored ? second : 0.0F;
    const float secondary_offset = mirrored ? 0.0F : first;
    place(subviews[0], bounds, primary_offset, first, cross);
    place(subviews[1], bounds, secondary_offset, second, cross);
  }

  friend auto operator==(const SplitLayout&, const SplitLayout&)
      -> bool = default;

 private:
  auto limits(const LayoutSubview& subview, float length, float cross) const
      -> composition::SplitArrangementSizing::Bounds {
    const composition::SplitArrangementSizing& sizing =
        subview[composition::SplitArrangementSizing::key];
    if (sizing.fixed(axis_)) {
      const ProposedViewSize asked =
          axis_ == composition::Axis::horizontal
              ? ProposedViewSize{std::nullopt, cross}
              : ProposedViewSize{cross, std::nullopt};
      const composition::Size fitted = subview.size_that_fits(asked);
      const float ideal =
          axis_ == composition::Axis::horizontal ? fitted.width : fitted.height;
      return composition::SplitArrangementSizing::Bounds{
          .minimum = ideal, .ideal = ideal, .maximum = ideal};
    }
    const composition::SplitArrangementSizing::Bounds& ratio =
        sizing.ratio(axis_);
    const composition::SplitArrangementSizing::Bounds& size =
        sizing.size(axis_);
    const auto scaled = [length](std::optional<float> value) {
      return value.transform([length](float part) { return part * length; });
    };
    const std::optional<float> ratio_minimum = scaled(ratio.minimum);
    const std::optional<float> ratio_maximum = scaled(ratio.maximum);
    std::optional<float> minimum = size.minimum;
    if (ratio_minimum.has_value()) {
      minimum = std::max(minimum.value_or(*ratio_minimum), *ratio_minimum);
    }
    std::optional<float> maximum = size.maximum;
    if (ratio_maximum.has_value()) {
      maximum = std::min(maximum.value_or(*ratio_maximum), *ratio_maximum);
    }
    return composition::SplitArrangementSizing::Bounds{
        .minimum = minimum,
        .ideal = size.ideal.has_value() ? size.ideal : scaled(ratio.ideal),
        .maximum = maximum};
  }

  auto secondary_length(const LayoutSubviews& subviews, float length,
                        float cross) const -> float {
    const composition::SplitArrangementSizing::Bounds first =
        limits(subviews[0], length, cross);
    const composition::SplitArrangementSizing::Bounds second =
        limits(subviews[1], length, cross);
    float chosen = length * default_ratio;
    if (second.ideal.has_value()) {
      chosen = *second.ideal;
    } else if (first.ideal.has_value()) {
      chosen = length - *first.ideal;
    }
    if (first.minimum.has_value()) {
      chosen = std::min(chosen, length - *first.minimum);
    }
    if (first.maximum.has_value()) {
      chosen = std::max(chosen, length - *first.maximum);
    }
    if (second.maximum.has_value()) {
      chosen = std::min(chosen, *second.maximum);
    }
    if (second.minimum.has_value()) {
      chosen = std::max(chosen, *second.minimum);
    }
    return std::clamp(chosen, 0.0F, std::max(length, 0.0F));
  }

  void place(const LayoutSubview& subview, composition::Rect bounds,
             float offset, float length, float cross) const {
    const bool horizontal = axis_ == composition::Axis::horizontal;
    subview.place(
        composition::Point{.x = horizontal ? bounds.x + offset : bounds.x,
                           .y = horizontal ? bounds.y : bounds.y + offset},
        motion::UnitPoint::top_leading,
        horizontal ? ProposedViewSize{length, cross}
                   : ProposedViewSize{cross, length});
  }

  composition::Axis axis_;
};

class ArrangementViewStyle::OverlayLayout {
 public:
  explicit OverlayLayout(composition::Axis axis) noexcept : axis_(axis) {}

  auto size_that_fits(ProposedViewSize proposal,
                      const LayoutSubviews& subviews) const
      -> composition::Size {
    static_cast<void>(subviews);
    return proposal.replacing_unspecified_dimensions();
  }

  void place_subviews(composition::Rect bounds, ProposedViewSize proposal,
                      const LayoutSubviews& subviews) const {
    static_cast<void>(proposal);
    if (subviews.empty()) {
      return;
    }
    subviews[0].place(composition::Point{.x = bounds.x, .y = bounds.y},
                      motion::UnitPoint::top_leading,
                      ProposedViewSize{bounds.width, bounds.height});
    if (subviews.size() < 2) {
      return;
    }
    const LayoutSubview& floating = subviews[1];
    const composition::OverlayArrangementEdges& edges =
        floating[composition::OverlayArrangementEdges::key];
    if (axis_ == composition::Axis::vertical) {
      const composition::Size fitted =
          floating.size_that_fits(ProposedViewSize{bounds.width, std::nullopt});
      const float height = std::clamp(fitted.height, 0.0F, bounds.height);
      const bool top =
          edges.vertical().value_or(composition::VerticalEdge::bottom) ==
          composition::VerticalEdge::top;
      floating.place(
          composition::Point{
              .x = bounds.x,
              .y = top ? bounds.y : bounds.y + bounds.height - height},
          motion::UnitPoint::top_leading,
          ProposedViewSize{bounds.width, height});
      return;
    }
    const composition::Size fitted =
        floating.size_that_fits(ProposedViewSize{std::nullopt, bounds.height});
    const float width = std::clamp(fitted.width, 0.0F, bounds.width);
    const bool leading =
        edges.horizontal().value_or(composition::HorizontalEdge::trailing) ==
        composition::HorizontalEdge::leading;
    const bool mirrored = subviews.layout_direction() ==
                          composition::LayoutDirection::right_to_left;
    const bool left = leading != mirrored;
    floating.place(
        composition::Point{
            .x = left ? bounds.x : bounds.x + bounds.width - width,
            .y = bounds.y},
        motion::UnitPoint::top_leading, ProposedViewSize{width, bounds.height});
  }

  friend auto operator==(const OverlayLayout&, const OverlayLayout&)
      -> bool = default;

 private:
  composition::Axis axis_;
};

const composition::EnvironmentKey<ArrangementViewStyle>
    ArrangementViewStyle::key{"arrangement_view_style",
                              ArrangementViewStyle::automatic()};

ArrangementViewStyle::ArrangementViewStyle(
    AutomaticArrangementViewStyle) noexcept
    : ArrangementViewStyle(Kind::automatic, composition::Axes::all) {}

ArrangementViewStyle::ArrangementViewStyle(
    SplitArrangementViewStyle style) noexcept
    : ArrangementViewStyle(Kind::split, style.axes()) {}

ArrangementViewStyle::ArrangementViewStyle(
    OverlayArrangementViewStyle style) noexcept
    : ArrangementViewStyle(Kind::overlay, style.axes()) {}

auto ArrangementViewStyle::automatic() noexcept
    -> AutomaticArrangementViewStyle {
  return AutomaticArrangementViewStyle{};
}

auto ArrangementViewStyle::split() noexcept -> SplitArrangementViewStyle {
  return SplitArrangementViewStyle{};
}

auto ArrangementViewStyle::overlay() noexcept -> OverlayArrangementViewStyle {
  return OverlayArrangementViewStyle{};
}

auto operator==(const ArrangementViewStyle& left,
                const ArrangementViewStyle& right) -> bool {
  if (left.kind_ != right.kind_ || left.axes_ != right.axes_) {
    return false;
  }
  return left.kind_ != ArrangementViewStyle::Kind::custom ||
         left.custom_->equals(*right.custom_);
}

auto ArrangementViewStyle::arranged_axis(composition::Axes allowed,
                                         composition::Size space)
    -> composition::Axis {
  const bool horizontal = allowed.contains(composition::Axis::horizontal);
  const bool vertical = allowed.contains(composition::Axis::vertical);
  if (horizontal != vertical) {
    return horizontal ? composition::Axis::horizontal
                      : composition::Axis::vertical;
  }
  return space.height > space.width ? composition::Axis::vertical
                                    : composition::Axis::horizontal;
}

auto ArrangementViewStyle::make_body(Configuration configuration) const
    -> composition::Element {
  switch (kind_) {
    case Kind::custom:
      return custom_->make_body(std::move(configuration));
    case Kind::split:
      return split_body(std::move(configuration), axes_);
    case Kind::overlay:
      return overlay_body(std::move(configuration), axes_);
    case Kind::automatic:
      break;
  }
  return AutomaticArrangementViewStyle{}.make_body(std::move(configuration));
}

auto ArrangementViewStyle::split_body(Configuration configuration,
                                      composition::Axes allowed)
    -> composition::Element {
  auto primary = std::make_shared<composition::PlaceholderContentView::Source>(
      std::move(configuration.primary));
  auto secondary =
      std::make_shared<composition::PlaceholderContentView::Source>(
          std::move(configuration.secondary));
  return GeometryReader(
      [primary, secondary, allowed](
          const composition::GeometryProxy& proxy) -> composition::Element {
        const composition::Axis axis = arranged_axis(allowed, proxy.size());
        const std::optional<composition::Axis> shown = axis;
        return AnyLayout(SplitLayout{axis})(
            composition::Element{composition::PlaceholderContentView(primary)}
                .environment(Environment::split_arrangement_axis, shown)
                .environment(Environment::overlay_arrangement_z_index, 0),
            composition::Element{composition::PlaceholderContentView(secondary)}
                .environment(Environment::split_arrangement_axis, shown)
                .environment(Environment::overlay_arrangement_z_index, 0));
      });
}

auto ArrangementViewStyle::overlay_body(Configuration configuration,
                                        composition::Axes allowed)
    -> composition::Element {
  auto primary = std::make_shared<composition::PlaceholderContentView::Source>(
      std::move(configuration.primary));
  auto secondary =
      std::make_shared<composition::PlaceholderContentView::Source>(
          std::move(configuration.secondary));
  return GeometryReader(
      [primary, secondary, allowed](
          const composition::GeometryProxy& proxy) -> composition::Element {
        const composition::Axis axis = arranged_axis(allowed, proxy.size());
        return AnyLayout(OverlayLayout{axis})(
            composition::Element{composition::PlaceholderContentView(primary)}
                .environment(Environment::split_arrangement_axis, std::nullopt)
                .environment(Environment::overlay_arrangement_z_index, 0),
            composition::Element{composition::PlaceholderContentView(secondary)}
                .environment(Environment::split_arrangement_axis, std::nullopt)
                .environment(Environment::overlay_arrangement_z_index, 1));
      });
}

auto AutomaticArrangementViewStyle::make_body(
    ArrangementViewStyle::Configuration configuration) const
    -> composition::Element {
  return ArrangementView(std::move(configuration))
      .arrangement_view_style(ArrangementViewStyle::automatic());
}

auto SplitArrangementViewStyle::make_body(
    ArrangementViewStyle::Configuration configuration) const
    -> composition::Element {
  return ArrangementViewStyle{*this}.make_body(std::move(configuration));
}

auto OverlayArrangementViewStyle::make_body(
    ArrangementViewStyle::Configuration configuration) const
    -> composition::Element {
  return ArrangementViewStyle{*this}.make_body(std::move(configuration));
}

}
