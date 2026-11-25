#include <glass_scene.hpp>

#include <arisa/components/alignment.hpp>
#include <arisa/components/shape_view.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/graphics/path.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/graphics/stroke_style.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/shape.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <vector>

namespace arisa::examples {

auto GlassScene::filled(graphics::Path path, std::uint32_t colour)
    -> composition::Element {
  return components::ShapeView(std::move(path))
      .fill(theme::Colour::hex(colour));
}

auto GlassScene::patterned(const Item& item, float spacing, int tiles)
    -> std::array<graphics::Path, 2> {
  std::array<graphics::Path, 2> paths;
  for (int tile = 0; tile < tiles; ++tile) {
    const float shift = static_cast<float>(tile) * spacing;
    if (const auto* bars = std::get_if<Bars>(&item)) {
      const float stride = bars->period * 0.5F;
      for (int index = 0; static_cast<float>(index) * stride < bars->rect.width;
           ++index) {
        paths.at(static_cast<std::size_t>(index % 2))
            .add_rect({.x = shift + bars->rect.x +
                            (static_cast<float>(index) * stride),
                       .y = bars->rect.y,
                       .width = stride,
                       .height = bars->rect.height});
      }
    } else if (const auto* checks = std::get_if<Checks>(&item)) {
      const auto columns =
          static_cast<int>(checks->rect.width / checks->cell + 0.5F);
      const auto rows =
          static_cast<int>(checks->rect.height / checks->cell + 0.5F);
      for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
          paths.at(static_cast<std::size_t>((row + column + 1) % 2))
              .add_rect({.x = shift + checks->rect.x +
                              (static_cast<float>(column) * checks->cell),
                         .y = checks->rect.y +
                              (static_cast<float>(row) * checks->cell),
                         .width = checks->cell,
                         .height = checks->cell});
        }
      }
    }
  }
  return paths;
}

auto GlassScene::drawn(const Item& item, float spacing, int tiles)
    -> composition::Element {
  return std::visit(
      [&item, spacing, tiles](const auto& each) -> composition::Element {
        using Kind = std::decay_t<decltype(each)>;
        if constexpr (std::is_same_v<Kind, Bars> ||
                      std::is_same_v<Kind, Checks>) {
          auto [first, second] = patterned(item, spacing, tiles);
          return components::ZStack(
              components::ZStackOptions{.alignment =
                                            components::Alignment::top_leading},
              filled(std::move(first), each.first),
              filled(std::move(second), each.second));
        } else {
          graphics::Path path;
          for (int tile = 0; tile < tiles; ++tile) {
            const float shift = static_cast<float>(tile) * spacing;
            if constexpr (std::is_same_v<Kind, Stroke>) {
              path.move({.x = each.from.x + shift, .y = each.from.y});
              path.add_line({.x = each.to.x + shift, .y = each.to.y});
            } else {
              composition::Rect rect = each.rect;
              rect.x += shift;
              if constexpr (std::is_same_v<Kind, Box>) {
                path.add_rounded_rect(
                    rect, {.width = each.radius, .height = each.radius});
              } else if (rect.width == rect.height) {
                path.add_ellipse(rect);
              } else {
                path.add_rounded_rect(
                    rect,
                    {.width = rect.height * 0.5F, .height = rect.height * 0.5F},
                    theme::RoundedCornerStyle::circular);
              }
            }
          }
          if constexpr (std::is_same_v<Kind, Stroke>) {
            return components::ShapeView(std::move(path))
                .stroke(theme::Colour::hex(each.colour),
                        graphics::StrokeStyle{
                            .line_width = each.width,
                            .line_cap = graphics::LineCap::round});
          } else {
            return filled(std::move(path), each.colour);
          }
        }
      },
      item);
}

auto GlassScene::layered() -> std::vector<composition::Element> {
  const int tiles = static_cast<int>(width / period) + 1;
  std::vector<composition::Element> layers;
  layers.reserve(spans.size() + motifs.size());
  for (const Item& item : spans) {
    layers.push_back(drawn(item, 0.0F, 1));
  }
  for (const Item& item : motifs) {
    layers.push_back(drawn(item, period, tiles));
  }
  return layers;
}

auto GlassScene::body(composition::Context&) const -> composition::Element {
  return components::ZStack(
             components::ZStackOptions{.alignment =
                                           components::Alignment::top_leading},
             layered())
      .frame({.max_width = composition::infinity,
              .max_height = composition::infinity,
              .alignment = components::Alignment::top_leading});
}

}
