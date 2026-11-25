#include <arisa/components/shape_view.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/alignment.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/graphics/path.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/structure/callout_board.hpp>

#include <algorithm>
#include <cstddef>
#include <format>
#include <utility>

namespace arisa::structure {

CalloutBoard::CalloutBoard(composition::Element art,
                           std::vector<Callout> callouts)
    : CalloutBoard(std::move(art), std::move(callouts), Options{}) {}

CalloutBoard::CalloutBoard(composition::Element art,
                           std::vector<Callout> callouts, Options options)
    : Element(compose(std::move(art), std::move(callouts), options)) {}

auto CalloutBoard::width(const Options& options) noexcept -> float {
  return (2.0F * options.column_width) + (2.0F * options.gap) +
         options.art.width;
}

auto CalloutBoard::height(const std::vector<Side>& sides,
                          const Options& options) noexcept -> float {
  const auto leading =
      static_cast<float>(std::ranges::count(sides, Side::leading));
  const float tallest =
      std::max(leading, static_cast<float>(sides.size()) - leading);
  const float column =
      tallest > 0.0F ? (tallest * (options.chip_height + options.chip_spacing)) -
                           options.chip_spacing
                     : 0.0F;
  return std::max(options.art.height + art_margin, column + column_margin);
}

auto CalloutBoard::leaders(const std::vector<motion::UnitPoint>& anchors,
                           const std::vector<Side>& sides,
                           const Options& options) -> std::vector<Leader> {
  const float board = height(sides, options);
  const float art_left = options.column_width + options.gap;
  const float art_right = art_left + options.art.width;
  const float art_top = (board - options.art.height) * 0.5F;
  std::vector<Leader> drawn(anchors.size());
  for (const Side side : {Side::leading, Side::trailing}) {
    std::vector<std::size_t> placed;
    for (std::size_t index = 0; index < anchors.size(); ++index) {
      if (sides[index] == side) {
        placed.push_back(index);
      }
    }
    std::ranges::stable_sort(placed, [&anchors, side](std::size_t first,
                                                      std::size_t second) {
      if (anchors[first].y != anchors[second].y) {
        return anchors[first].y < anchors[second].y;
      }
      return (side == Side::leading) == (anchors[first].x < anchors[second].x);
    });
    const auto count = static_cast<float>(placed.size());
    const float step =
        placed.size() > 1 ? (board - options.chip_height) / (count - 1.0F)
                          : 0.0F;
    const float top =
        placed.size() > 1 ? 0.0F : (board - options.chip_height) * 0.5F;
    const float from_x = side == Side::leading ? options.column_width
                                               : art_right + options.gap;
    const float elbow_x = side == Side::leading ? from_x + options.elbow
                                                : from_x - options.elbow;
    for (std::size_t order = 0; order < placed.size(); ++order) {
      const std::size_t index = placed[order];
      const float centre = top + (static_cast<float>(order) * step) +
                           (options.chip_height * 0.5F);
      drawn[index] = Leader{
          .from = composition::Point{.x = from_x, .y = centre},
          .elbow = composition::Point{.x = elbow_x, .y = centre},
          .to = composition::Point{
              .x = art_left + (anchors[index].x * options.art.width),
              .y = art_top + (anchors[index].y * options.art.height)}};
    }
  }
  return drawn;
}

auto CalloutBoard::compose(composition::Element art,
                           std::vector<Callout> callouts,
                           const Options& options) -> composition::Element {
  std::vector<motion::UnitPoint> anchors;
  std::vector<Side> sides;
  for (const Callout& callout : callouts) {
    anchors.push_back(callout.anchor);
    sides.push_back(callout.side);
  }
  const std::vector<Leader> lines = leaders(anchors, sides, options);
  const float total = width(options);
  const float board = height(sides, options);
  std::vector<composition::Element> layers;
  layers.reserve(callouts.size() + 2);
  graphics::Path leads;
  graphics::Path dots;
  const float radius = options.dot_diameter * 0.5F;
  for (const Leader& leader : lines) {
    leads.move(leader.from);
    leads.add_line(leader.elbow);
    leads.add_line(leader.to);
    dots.add_path(graphics::Path::ellipse_in(
        composition::Rect{.x = leader.to.x - radius,
                          .y = leader.to.y - radius,
                          .width = options.dot_diameter,
                          .height = options.dot_diameter}));
  }
  layers.push_back(
      std::move(art)
          .frame({.width = options.art.width, .height = options.art.height})
          .offset(options.column_width + options.gap,
                  (board - options.art.height) * 0.5F)
          .id(art_key));
  layers.push_back(
      components::ZStack(
          components::ZStackOptions{.alignment =
                                        composition::Alignment::top_leading},
          composition::Element{components::ShapeView(std::move(leads))
                                   .stroke(graphics::ShapeStyle{options.line},
                                           options.line_width)},
          composition::Element{components::ShapeView(std::move(dots))
                                   .fill(graphics::ShapeStyle{options.line})})
          .frame({.width = total, .height = board})
          .allows_hit_testing(false)
          .accessibility_hidden(true)
          .id(lines_key));
  for (std::size_t index = 0; index < callouts.size(); ++index) {
    const bool left = sides[index] == Side::leading;
    const float top = lines[index].from.y - (options.chip_height * 0.5F);
    layers.push_back(
        std::move(callouts[index].chip)
            .frame({.width = options.column_width,
                    .height = options.chip_height,
                    .alignment = left ? composition::Alignment::trailing
                                      : composition::Alignment::leading})
            .offset(left ? 0.0F : lines[index].from.x, top)
            .id(std::format("{}.{}", chip_key, index)));
  }
  layers.push_back(
      components::ZStack().frame({.width = total, .height = board}));
  return components::ZStack(
             components::ZStackOptions{.alignment =
                                           composition::Alignment::top_leading},
             std::move(layers))
      .frame({.width = total,
              .height = board,
              .alignment = composition::Alignment::top_leading});
}

}
