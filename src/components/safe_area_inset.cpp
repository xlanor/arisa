#include <arisa/components/safe_area_inset.hpp>
#include <arisa/components/shape_view.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/graphics/gradient.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/metrics.hpp>
#include <arisa/theme/shape.hpp>

#include <vector>

namespace arisa::components {

auto SafeAreaInset::edge(composition::VerticalEdge edge) noexcept
    -> theme::Edge {
  return edge == composition::VerticalEdge::top ? theme::Edge::top
                                                : theme::Edge::bottom;
}

auto SafeAreaInset::edge(composition::HorizontalEdge edge) noexcept
    -> theme::Edge {
  return edge == composition::HorizontalEdge::leading ? theme::Edge::leading
                                                      : theme::Edge::trailing;
}

auto SafeAreaInset::placed(composition::VerticalEdge edge,
                           composition::HorizontalAlignment alignment) noexcept
    -> composition::Alignment {
  return composition::Alignment{alignment,
                                edge == composition::VerticalEdge::top
                                    ? composition::VerticalAlignment::top
                                    : composition::VerticalAlignment::bottom};
}

auto SafeAreaInset::placed(composition::HorizontalEdge edge,
                           composition::VerticalAlignment alignment) noexcept
    -> composition::Alignment {
  return composition::Alignment{
      edge == composition::HorizontalEdge::leading
          ? composition::HorizontalAlignment::leading
          : composition::HorizontalAlignment::trailing,
      alignment};
}

auto SafeAreaInset::fade(theme::Edge edge) -> composition::Element {
  const theme::ColourValue near =
      theme::ColourValue{theme::ColourRole::background}.opacity(
          theme::metrics::navigation.scroll_edge_opacity);
  const theme::ColourValue far =
      theme::ColourValue{theme::ColourRole::background}.opacity(0.0F);
  motion::UnitPoint start = motion::UnitPoint::top;
  motion::UnitPoint end = motion::UnitPoint::bottom;
  switch (edge) {
    case theme::Edge::bottom:
      start = motion::UnitPoint::bottom;
      end = motion::UnitPoint::top;
      break;
    case theme::Edge::leading:
      start = motion::UnitPoint::leading;
      end = motion::UnitPoint::trailing;
      break;
    case theme::Edge::trailing:
      start = motion::UnitPoint::trailing;
      end = motion::UnitPoint::leading;
      break;
    default:
      break;
  }
  return composition::Element{
      ShapeView(theme::Shape::rectangle())
          .fill(graphics::LinearGradient{
              .gradient = graphics::Gradient{std::vector<theme::ColourValue>{
                  near, far}},
              .start_point = start,
              .end_point = end})}
      .frame({.max_width = composition::infinity,
              .max_height = composition::infinity})
      .allows_hit_testing(false);
}

auto SafeAreaInset::attach(composition::Element base, theme::Edge edge,
                           composition::Alignment alignment, float spacing,
                           composition::Element content, bool bar)
    -> composition::Element {
  const theme::Space gap{spacing};
  theme::Insets pad{};
  bool vertical = true;
  switch (edge) {
    case theme::Edge::top:
      pad.bottom = gap;
      break;
    case theme::Edge::bottom:
      pad.top = gap;
      break;
    case theme::Edge::leading:
      pad.trailing = gap;
      vertical = false;
      break;
    case theme::Edge::trailing:
      pad.leading = gap;
      vertical = false;
      break;
    default:
      break;
  }
  composition::Element placed =
      ZStack(ZStackOptions{.alignment = alignment}, std::move(content))
          .frame(
              vertical
                  ? composition::FrameSpec{.max_width = composition::infinity,
                                           .alignment = alignment}
                  : composition::FrameSpec{.max_height = composition::infinity,
                                           .alignment = alignment})
          .padding(pad);
  if (bar) {
    placed = std::move(placed).background(fade(edge));
  }
  return std::move(base).overlay(alignment,
                                 std::move(placed).safe_area_bar(edge));
}

}
