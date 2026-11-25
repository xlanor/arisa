#include <arisa/components/text.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/node_id.hpp>
#include <arisa/provider/gpu/image.hpp>
#include <arisa/provider/gpu/shape_vertex.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/headless/null_gpu.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/rendering/painter.hpp>
#include <arisa/structure/list_cell.hpp>

#include <gallery_fixture.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::runtime {

class GalleryListEditTest : public GalleryFixture {
 protected:
  struct Row {
    std::string label;
    composition::Rect cell;
    composition::Rect text;
    bool painted{false};
  };

  static auto text_of(const composition::Node& node)
      -> std::optional<std::string> {
    std::optional<std::string> found;
    walk(node, [&found](const composition::Node& each) {
      const auto text = each.content_as<components::Text::Content>();
      if (!found.has_value() && text.has_value()) {
        found = std::string{text->get().text()};
      }
    });
    return found;
  }

  static auto focused_label(Application& application)
      -> std::optional<std::string> {
    const auto focused = application.interaction().focused();
    if (!focused.has_value()) {
      return std::nullopt;
    }
    const composition::Tree& tree = application.root().tree();
    std::optional<std::string> found;
    walk(tree.root(), [&](const composition::Node& node) {
      const auto text = node.content_as<components::Text::Content>();
      if (!text.has_value() ||
          (found.has_value() &&
           std::ranges::contains(fruits, std::string_view{*found}))) {
        return;
      }
      std::optional<composition::NodeId> at = node.id();
      while (at.has_value()) {
        if (*at == *focused) {
          found = std::string{text->get().text()};
          return;
        }
        const auto above = tree.node(*at);
        at = above.has_value() ? above->get().parent() : std::nullopt;
      }
    });
    return found;
  }

  static auto reach(provider::headless::HeadlessProviderFactory& providers,
                    Application& application, std::string_view label) -> bool {
    provider::Button next = provider::Button::dpad_right;
    for (int hop = 0; hop < 60; ++hop) {
      const auto here = focused_label(application);
      if (here == label) {
        return true;
      }
      const bool fruit = here.has_value() &&
                         std::ranges::contains(fruits, std::string_view{*here});
      const auto before = application.interaction().focused();
      press(providers, application, fruit ? provider::Button::dpad_up : next);
      run(providers, application, 12);
      if (!fruit && application.interaction().focused() == before) {
        next = next == provider::Button::dpad_right
                   ? provider::Button::dpad_up
                   : provider::Button::dpad_right;
      } else if (!fruit) {
        next = provider::Button::dpad_right;
      }
    }
    return false;
  }

  static auto rows(Application& application) -> std::vector<Row> {
    std::vector<Row> found;
    walk(application.root().tree().root(),
         [&found](const composition::Node& node) {
           if (!node.content_as<structure::ListCell::Content>().has_value() ||
               !node.presented().has_value()) {
             return;
           }
           std::optional<Row> row;
           walk(node, [&row, &node](const composition::Node& each) {
             const auto text = each.content_as<components::Text::Content>();
             if (row.has_value() || !text.has_value() ||
                 !std::ranges::contains(fruits, text->get().text())) {
               return;
             }
             row = Row{.label = std::string{text->get().text()},
                       .cell = node.presented()->bounds,
                       .text = each.presented().has_value()
                                   ? each.presented()->bounds
                                   : composition::Rect{},
                       .painted = each.presented().has_value() &&
                                  each.presented()->visible &&
                                  each.presented()->opacity > 0.9F};
           });
           if (row.has_value()) {
             found.push_back(*row);
           }
         });
    return found;
  }

  static void expect_settled(const std::vector<Row>& found) {
    ASSERT_FALSE(found.empty());
    for (std::size_t index = 0; index < found.size(); ++index) {
      const Row& row = found[index];
      EXPECT_TRUE(row.painted) << row.label;
      EXPECT_GE(row.text.x, row.cell.x - 0.5F) << row.label;
      EXPECT_GE(row.text.y, row.cell.y - 0.5F) << row.label;
      EXPECT_LE(row.text.y + row.text.height,
                row.cell.y + row.cell.height + 0.5F)
          << row.label;
      EXPECT_NEAR(row.cell.x, found.front().cell.x, 0.5F) << row.label;
      EXPECT_NEAR(row.cell.width, found.front().cell.width, 0.5F) << row.label;
      if (index > 0) {
        const Row& above = found[index - 1];
        EXPECT_GE(row.cell.y, above.cell.y + above.cell.height - 0.5F)
            << above.label << " overlaps " << row.label;
      }
    }
  }

  struct Quad {
    std::size_t order{0};
    provider::ShapeKind kind{provider::ShapeKind::fill};
    float left{0.0F};
    float top{0.0F};
    float right{0.0F};
    float bottom{0.0F};
    provider::ShapeVertex corner;
    std::array<float, 2> local_step{};
  };

  static auto quads(const provider::headless::NullGpu::ShapeRecord& record)
      -> std::vector<Quad> {
    std::vector<Quad> found;
    for (std::size_t base = 0; base + 3 < record.vertices.size(); base += 4) {
      const provider::ShapeVertex& first = record.vertices[base];
      const provider::ShapeVertex& opposite = record.vertices[base + 2];
      const float width = opposite.position[0] - first.position[0];
      const float height = opposite.position[1] - first.position[1];
      found.push_back(Quad{
          .order = base / 4,
          .kind = first.kind,
          .left = std::min(first.position[0], opposite.position[0]),
          .top = std::min(first.position[1], opposite.position[1]),
          .right = std::max(first.position[0], opposite.position[0]),
          .bottom = std::max(first.position[1], opposite.position[1]),
          .corner = first,
          .local_step = {
              width == 0.0F ? 0.0F
                            : (opposite.local[0] - first.local[0]) / width,
              height == 0.0F ? 0.0F
                             : (opposite.local[1] - first.local[1]) / height}});
    }
    return found;
  }

  static auto distance(const Quad& quad, float x, float y) -> float {
    const float lx = quad.corner.local[0] +
                     ((x - quad.corner.position[0]) * quad.local_step[0]);
    const float ly = quad.corner.local[1] +
                     ((y - quad.corner.position[1]) * quad.local_step[1]);
    const float radius = quad.corner.shape[provider::ShapeVertex::radius];
    const float smoothing = quad.corner.shape[provider::ShapeVertex::smoothing];
    const float hx = quad.corner.half_size[0];
    const float hy = quad.corner.half_size[1];
    const float extent =
        std::min(radius * (1.0F + (0.6F * smoothing)), std::min(hx, hy));
    const float qx = std::abs(lx) - hx + extent;
    const float qy = std::abs(ly) - hy + extent;
    const float outside = std::hypot(std::max(qx, 0.0F), std::max(qy, 0.0F));
    return std::min(std::max(qx, qy), 0.0F) + outside - extent;
  }

  static auto covers(const Quad& quad, float x, float y) -> bool {
    if (x < quad.left || x > quad.right || y < quad.top || y > quad.bottom) {
      return false;
    }
    const float stroke = quad.corner.shape[provider::ShapeVertex::stroke];
    switch (quad.kind) {
      case provider::ShapeKind::glyph:
      case provider::ShapeKind::glyph_gradient:
      case provider::ShapeKind::image:
      case provider::ShapeKind::yuv:
        return true;
      case provider::ShapeKind::fill:
      case provider::ShapeKind::gradient_fill:
        return distance(quad, x, y) < 0.0F &&
               (quad.corner.colour >> 24U) > 128U;
      case provider::ShapeKind::stroke: {
        const float d = distance(quad, x, y);
        return d < 0.0F && d > -stroke;
      }
      case provider::ShapeKind::material:
      case provider::ShapeKind::glass:
        return distance(quad, x, y) < 0.0F;
      case provider::ShapeKind::glass_union:
        return true;
      case provider::ShapeKind::layer:
      case provider::ShapeKind::shadow:
      case provider::ShapeKind::path:
      case provider::ShapeKind::gradient:
      case provider::ShapeKind::mesh:
      case provider::ShapeKind::blur_down:
      case provider::ShapeKind::blur_up:
      case provider::ShapeKind::glass_path:
      case provider::ShapeKind::filter:
      case provider::ShapeKind::blend:
      case provider::ShapeKind::mask:
      case provider::ShapeKind::gaussian:
      case provider::ShapeKind::shader:
      case provider::ShapeKind::inner_shadow:
        return false;
    }
    return false;
  }

  static auto top(const std::vector<Quad>& all, float x, float y)
      -> std::optional<Quad> {
    std::optional<Quad> found;
    for (const Quad& quad : all) {
      if (covers(quad, x, y)) {
        found = quad;
      }
    }
    return found;
  }

  static auto rings(const std::vector<Quad>& all, float width)
      -> std::vector<Quad> {
    std::vector<Quad> found;
    std::ranges::copy_if(
        all, std::back_inserter(found), [width](const Quad& q) {
          return q.kind == provider::ShapeKind::stroke &&
                 std::abs(q.corner.shape[provider::ShapeVertex::stroke] -
                          width) < 0.01F;
        });
    return found;
  }

  static auto label_frame(Application& application, std::string_view label)
      -> std::optional<composition::Rect> {
    std::optional<composition::Rect> found;
    walk(application.root().tree().root(), [&](const composition::Node& node) {
      const auto text = node.content_as<components::Text::Content>();
      if (text.has_value() && text->get().text() == label &&
          node.presented().has_value()) {
        found = node.presented()->bounds;
      }
    });
    return found;
  }

  static auto card_around(const std::vector<Quad>& all, composition::Rect label,
                          std::size_t before) -> std::optional<Quad> {
    std::optional<Quad> found;
    for (const Quad& quad : all) {
      if (quad.order >= before || quad.kind != provider::ShapeKind::fill ||
          (quad.corner.colour >> 24U) < 200U) {
        continue;
      }
      if (quad.left <= label.x && quad.right >= label.x + label.width &&
          quad.top <= label.y && quad.bottom >= label.y + label.height &&
          quad.right - quad.left < 1200.0F) {
        found = quad;
      }
    }
    return found;
  }
  static constexpr int minimum_ink = 20;

  static auto inked(const provider::Image& image, composition::Rect label)
      -> int {
    constexpr int contrast = 96;
    const auto paper = image.pixel(
        static_cast<std::uint32_t>(std::max(label.x - 3.0F, 0.0F)),
        static_cast<std::uint32_t>(label.y + (label.height * 0.5F)));
    if (!paper.has_value()) {
      return 0;
    }
    int count = 0;
    const auto left = static_cast<std::uint32_t>(std::max(label.x, 0.0F));
    const auto top = static_cast<std::uint32_t>(std::max(label.y, 0.0F));
    const auto right = static_cast<std::uint32_t>(label.x + label.width);
    const auto bottom = static_cast<std::uint32_t>(label.y + label.height);
    for (std::uint32_t y = top; y < bottom; ++y) {
      for (std::uint32_t x = left; x < right; ++x) {
        const auto seen = image.pixel(x, y);
        if (!seen.has_value()) {
          continue;
        }
        int difference = 0;
        for (std::size_t channel = 0; channel < 3; ++channel) {
          difference += std::abs(static_cast<int>((*seen)[channel]) -
                                 static_cast<int>((*paper)[channel]));
        }
        count += difference > contrast ? 1 : 0;
      }
    }
    return count;
  }

  static void expect_lifted_label(theme::ColorScheme scheme,
                                  std::string_view row) {
    provider::headless::HeadlessProviderFactory providers;
    Application application =
        start_gallery(providers, examples::GalleryPage::lists, scheme);
    run(providers, application, 60);
    ASSERT_TRUE(reach(providers, application, row));
    const auto check = [&providers, &application,
                        row](std::string_view moment) {
      const auto label = label_frame(application, row);
      ASSERT_TRUE(label.has_value()) << moment;
      const auto image = providers.gpu().capture();
      ASSERT_TRUE(image.has_value()) << moment;
      EXPECT_GE(inked(*image, *label), minimum_ink) << moment;
    };
    providers.gpu().rasterize(
        provider::headless::ScriptedSurface::default_size);
    press(providers, application, structure::ListCell::move_button);
    run(providers, application, 3);
    check("lifting");
    run(providers, application, 30);
    check("lifted");
    press(providers, application, provider::Button::dpad_down);
    run(providers, application, 3);
    check("moving");
    run(providers, application, 30);
    check("moved");
  }

  static void expect_ring_on_top(std::string_view row) {
    provider::headless::HeadlessProviderFactory providers;
    Application application =
        start_gallery(providers, examples::GalleryPage::lists);
    run(providers, application, 60);
    ASSERT_TRUE(reach(providers, application, row));
    run(providers, application, 30);
    const auto label = label_frame(application, row);
    ASSERT_TRUE(label.has_value());
    const std::vector<Quad> all = quads(providers.gpu().last_shapes());
    const std::vector<Quad> found =
        rings(all, std::round(rendering::Painter::focus_ring_width));
    ASSERT_EQ(found.size(), 1U);
    const Quad& ring = found.front();
    const auto card = card_around(all, *label, all.size());
    ASSERT_TRUE(card.has_value());
    const float outset = rendering::Painter::focus_ring_gap +
                         rendering::Painter::focus_ring_width;
    EXPECT_NEAR(ring.left, card->left, outset + 1.0F);
    EXPECT_NEAR(ring.right, card->right, outset + 1.0F);
    EXPECT_LE(ring.top, label->y);
    EXPECT_GE(ring.bottom, label->y + label->height);
    const float middle = label->y + (label->height * 0.5F);
    const float side = rendering::Painter::focus_ring_width * 0.5F;
    for (const auto& [x, y] : std::vector<std::array<float, 2>>{
             {ring.left + 1.0F + side, middle},
             {ring.right - 1.0F - side, middle},
             {label->x, ring.top + 1.0F + side},
             {label->x, ring.bottom - 1.0F - side}}) {
      const auto shown = top(all, x, y);
      ASSERT_TRUE(shown.has_value()) << x << "," << y;
      EXPECT_EQ(shown->order, ring.order) << x << "," << y;
    }
  }

  static constexpr std::array<std::string_view, 5> fruits{
      "Apple", "Banana", "Cherry", "Damson", "Elderberry"};
};

TEST_F(GalleryListEditTest, MovedCancelledAndDeletedRowsSettleAndPaint) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start_gallery(providers, examples::GalleryPage::lists);
  run(providers, application, 60);
  ASSERT_TRUE(reach(providers, application, "Apple"));
  press(providers, application, structure::ListCell::move_button);
  run(providers, application, 20);
  press(providers, application, provider::Button::dpad_down);
  run(providers, application, 30);
  press(providers, application, provider::Button::a);
  run(providers, application, 60);
  expect_settled(rows(application));
  press(providers, application, structure::ListCell::move_button);
  run(providers, application, 20);
  press(providers, application, provider::Button::b);
  run(providers, application, 60);
  expect_settled(rows(application));
  press(providers, application, provider::Button::minus);
  run(providers, application, 60);
  const std::vector<Row> found = rows(application);
  EXPECT_EQ(found.size(), fruits.size() - 1);
  expect_settled(found);
}

TEST_F(GalleryListEditTest, EditModeRowStaysCentredAndFocusWrapsTheRow) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start_gallery(providers, examples::GalleryPage::lists);
  run(providers, application, 60);
  ASSERT_TRUE(reach(providers, application, "Apple"));
  for (int hop = 0; hop < 12 && focused_label(application) != "Edit mode";
       ++hop) {
    press(providers, application, provider::Button::dpad_down);
    run(providers, application, 12);
  }
  ASSERT_EQ(focused_label(application), "Edit mode");
  const auto check = [&application] {
    const composition::Tree& tree = application.root().tree();
    const auto focused = application.interaction().focused();
    ASSERT_TRUE(focused.has_value());
    const composition::Node& row = tree.node(*focused)->get();
    ASSERT_TRUE(row.presented().has_value());
    const composition::Rect& ring = row.presented()->bounds;
    std::optional<composition::Rect> label;
    std::optional<composition::Rect> cell;
    walk(tree.root(), [&](const composition::Node& node) {
      const auto text = node.content_as<components::Text::Content>();
      if (text.has_value() && text->get().text() == "Edit mode" &&
          node.presented().has_value()) {
        label = node.presented()->bounds;
      }
    });
    ASSERT_TRUE(label.has_value());
    walk(tree.root(), [&](const composition::Node& node) {
      if (!node.content_as<structure::ListCell::Content>().has_value() ||
          !node.presented().has_value()) {
        return;
      }
      const composition::Rect& bounds = node.presented()->bounds;
      if (bounds.y <= label->y &&
          bounds.y + bounds.height >= label->y + label->height &&
          bounds.x <= label->x && bounds.x + bounds.width >= label->x) {
        cell = bounds;
      }
    });
    const float ring_middle = ring.y + (ring.height * 0.5F);
    const float label_middle = label->y + (label->height * 0.5F);
    EXPECT_NEAR(ring_middle, label_middle, 2.0F);
    EXPECT_GE(ring.width, 400.0F);
    ASSERT_TRUE(cell.has_value());
    EXPECT_GE(ring.height, cell->height - 1.0F);
    EXPECT_NEAR(ring_middle, cell->y + (cell->height * 0.5F), 2.0F);
  };
  check();
  press(providers, application, provider::Button::a);
  run(providers, application, 60);
  check();
  press(providers, application, provider::Button::a);
  run(providers, application, 60);
  check();
}

TEST_F(GalleryListEditTest, FocusRingSurroundsTheListRowAndPaintsOnTop) {
  for (const std::string_view row :
       {std::string_view{"Edit mode"}, std::string_view{"Apple"}}) {
    SCOPED_TRACE(row);
    expect_ring_on_top(row);
  }
}

TEST_F(GalleryListEditTest, LiftedRowKeepsItsLabelAboveTheCardInLight) {
  for (const std::string_view row :
       {std::string_view{"Apple"}, std::string_view{"Cherry"},
        std::string_view{"Elderberry"}}) {
    SCOPED_TRACE(row);
    expect_lifted_label(theme::ColorScheme::light, row);
  }
}

TEST_F(GalleryListEditTest, LiftedRowKeepsItsLabelAboveTheCardInDark) {
  for (const std::string_view row :
       {std::string_view{"Apple"}, std::string_view{"Cherry"},
        std::string_view{"Elderberry"}}) {
    SCOPED_TRACE(row);
    expect_lifted_label(theme::ColorScheme::dark, row);
  }
}

TEST_F(GalleryListEditTest, BackCancelsALiftBeforeLeavingThePage) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start_gallery(providers, examples::GalleryPage::lists);
  run(providers, application, 60);
  ASSERT_TRUE(reach(providers, application, "Apple"));
  press(providers, application, structure::ListCell::move_button);
  run(providers, application, 30);
  EXPECT_TRUE(shows(application, "Drop"));
  press(providers, application, provider::Button::b);
  run(providers, application, 30);
  EXPECT_FALSE(shows(application, "Drop"));
  EXPECT_EQ(focused_label(application), std::optional<std::string>{"Apple"});
  const auto focused = application.interaction().focused();
  ASSERT_TRUE(focused.has_value());
  EXPECT_FALSE(within(application, *focused, "arisa.tab.sidebar"));
}

}
