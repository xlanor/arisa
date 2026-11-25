#include <layout_fixture.hpp>

#include <arisa/components/button.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/motion/angle.hpp>
#include <arisa/provider/gpu/image.hpp>
#include <arisa/provider/headless/null_gpu.hpp>
#include <arisa/rendering/painter.hpp>
#include <arisa/theme/color_scheme.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/stroke.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <ranges>
#include <string_view>
#include <vector>

namespace arisa::rendering {

class PainterTest : public ::testing::Test, protected layout::LayoutFixture {
 protected:
  struct Painted {
    Painter::Statistics statistics;
    provider::headless::NullGpu::ShapeRecord record;
  };

  static auto paint(provider::headless::NullGpu& gpu, Painter& painter,
                    composition::Tree& target, layout::Viewport viewport,
                    Painter::TypesetterRef setter = std::nullopt,
                    Painter::Highlights highlights = {}) -> Painted {
    auto context = layout::LayoutContext::create();
    EXPECT_TRUE(context.has_value());
    EXPECT_TRUE(context->update(target, viewport, setter).has_value());
    auto frame = gpu.begin_frame();
    EXPECT_TRUE(frame.has_value());
    auto statistics =
        painter.paint(target, *frame,
                      Painter::Target{.pixels = viewport.pixels,
                                      .scale = viewport.scale,
                                      .time = std::chrono::nanoseconds{0}},
                      setter, highlights);
    EXPECT_TRUE(statistics.has_value());
    EXPECT_TRUE(frame->present().has_value());
    return Painted{.statistics = statistics.value_or(Painter::Statistics{}),
                   .record = gpu.last_shapes()};
  }

  static auto of_kind(const provider::headless::NullGpu::ShapeRecord& record,
                      provider::ShapeKind kind)
      -> std::vector<provider::ShapeVertex> {
    std::vector<provider::ShapeVertex> found;
    std::ranges::copy_if(record.vertices, std::back_inserter(found),
                         [kind](const provider::ShapeVertex& vertex) {
                           return vertex.kind == kind;
                         });
    return found;
  }

  static auto alpha(const provider::ShapeVertex& vertex) -> std::uint32_t {
    return vertex.colour >> 24U;
  }

  static auto painter(provider::headless::NullGpu& gpu) -> Painter {
    auto created = Painter::create(gpu);
    EXPECT_TRUE(created.has_value());
    return std::move(*created);
  }
};

TEST_F(PainterTest, WritesGeometryOnceIntoOneAllocation) {
  provider::headless::NullGpu gpu;
  Painter target_painter = painter(gpu);
  const auto layout =
      tree(components::VStack(box("a", 100.0F, 40.0F), box("b", 60.0F, 20.0F))
               .padding(8.0F)
               .background(theme::ColourRole::surface_raised,
                           theme::RoundedRectangle(
                               theme::radius::card,
                               theme::RoundedCornerStyle::continuous))
               .border(theme::ColourRole::separator, theme::Stroke::hairline())
               .shadow(theme::Elevation::medium));
  const std::size_t allocations = gpu.geometry_allocations();
  const Painted painted = paint(gpu, target_painter, *layout, handheld);
  EXPECT_EQ(gpu.geometry_allocations(), allocations + 1);
  EXPECT_EQ(painted.statistics.quads, 22U);
  EXPECT_EQ(painted.record.vertices.size(), 88U);
  EXPECT_EQ(painted.record.indices.size(), 132U);
  EXPECT_EQ(painted.statistics.shadow_misses + painted.statistics.shadow_hits,
            2U);
  EXPECT_EQ(gpu.geometry_bytes(), (88U * sizeof(provider::ShapeVertex)) +
                                      (132U * sizeof(provider::Index)));
  EXPECT_EQ(painted.statistics.commands, 1U);
  ASSERT_EQ(painted.record.commands.size(), 1U);
  EXPECT_EQ(painted.record.commands.front().index_count, 132U);
  EXPECT_TRUE(of_kind(painted.record, provider::ShapeKind::shadow).empty());
  EXPECT_EQ(of_kind(painted.record, provider::ShapeKind::glyph).size(), 72U);
  EXPECT_EQ(of_kind(painted.record, provider::ShapeKind::stroke).size(), 4U);
}

TEST_F(PainterTest, ContinuousCornersCarrySmoothingInTheVertex) {
  provider::headless::NullGpu gpu;
  Painter target_painter = painter(gpu);
  const auto layout = tree(components::VStack(
      components::VStack()
          .frame({.width = 100.0F, .height = 100.0F})
          .background(theme::ColourRole::accent,
                      theme::Shape::rounded(20.0F, true)),
      components::VStack()
          .frame({.width = 100.0F, .height = 100.0F})
          .background(theme::ColourRole::accent,
                      theme::Shape::rounded(20.0F, false)),
      components::VStack()
          .frame({.width = 100.0F, .height = 40.0F})
          .background(theme::ColourRole::accent, theme::Shape::capsule())));
  const Painted handheld_paint = paint(gpu, target_painter, *layout, handheld);
  const auto fills = of_kind(handheld_paint.record, provider::ShapeKind::fill);
  ASSERT_EQ(fills.size(), 12U);
  EXPECT_FLOAT_EQ(fills[0].shape[provider::ShapeVertex::radius], 20.0F);
  EXPECT_FLOAT_EQ(fills[0].shape[provider::ShapeVertex::smoothing],
                  Painter::continuous_smoothing);
  EXPECT_FLOAT_EQ(fills[4].shape[provider::ShapeVertex::smoothing], 0.0F);
  EXPECT_FLOAT_EQ(fills[8].shape[provider::ShapeVertex::radius], 20.0F);
  EXPECT_FLOAT_EQ(fills[0].half_size[0], 50.0F);
  const Painted docked_paint = paint(gpu, target_painter, *layout, docked());
  const auto docked_fills =
      of_kind(docked_paint.record, provider::ShapeKind::fill);
  ASSERT_EQ(docked_fills.size(), 12U);
  EXPECT_FLOAT_EQ(docked_fills[0].shape[provider::ShapeVertex::radius], 30.0F);
  EXPECT_FLOAT_EQ(docked_fills[0].half_size[0], 75.0F);
}

TEST_F(PainterTest, HairlinesSnapToWholeDevicePixelsAtEveryScale) {
  provider::headless::NullGpu gpu;
  Painter target_painter = painter(gpu);
  const auto layout = tree(
      components::VStack()
          .frame({.width = 101.0F, .height = 33.0F})
          .border(theme::ColourRole::separator, theme::Stroke::hairline()));
  for (const layout::Viewport viewport : {handheld, docked()}) {
    const Painted painted = paint(gpu, target_painter, *layout, viewport);
    const auto strokes = of_kind(painted.record, provider::ShapeKind::stroke);
    ASSERT_EQ(strokes.size(), 4U);
    for (const provider::ShapeVertex& vertex : strokes) {
      EXPECT_FLOAT_EQ(vertex.shape[provider::ShapeVertex::stroke], 1.0F);
      EXPECT_FLOAT_EQ(vertex.position[0], std::round(vertex.position[0]));
      EXPECT_FLOAT_EQ(vertex.position[1], std::round(vertex.position[1]));
      EXPECT_FLOAT_EQ(vertex.half_size[0] * 2.0F,
                      std::round(vertex.half_size[0] * 2.0F));
    }
  }
}

TEST_F(PainterTest, ColourSchemeOverrideFlipsResolvedColours) {
  provider::headless::NullGpu gpu;
  Painter target_painter = painter(gpu);
  const auto filled = [&](theme::ColorScheme scheme) {
    const auto layout =
        tree(components::VStack()
                 .frame({.width = 50.0F, .height = 50.0F})
                 .background(theme::ColourRole::surface_raised)
                 .environment(components::Environment::colour_scheme,
                              std::optional<theme::ColorScheme>{scheme}));
    const auto fills =
        of_kind(paint(gpu, target_painter, *layout, handheld).record,
                provider::ShapeKind::fill);
    EXPECT_EQ(fills.size(), 4U);
    return fills.empty() ? 0U : fills.front().colour;
  };
  const std::uint32_t light = filled(theme::ColorScheme::light);
  const std::uint32_t dark = filled(theme::ColorScheme::dark);
  EXPECT_NE(light, dark);
  EXPECT_EQ(light & 0xFFU, 255U);
  EXPECT_LT(dark & 0xFFU, 100U);
}

TEST_F(PainterTest, OpacityMultipliesVertexAlphaDownTheTree) {
  provider::headless::NullGpu gpu;
  Painter target_painter = painter(gpu);
  const auto layout =
      tree(components::VStack(components::VStack()
                                  .frame({.width = 50.0F, .height = 50.0F})
                                  .background(theme::Colour::hex(0xFF0000))
                                  .opacity(0.5F))
               .opacity(0.5F));
  const Painted painted = paint(gpu, target_painter, *layout, handheld);
  const auto fills = of_kind(painted.record, provider::ShapeKind::fill);
  ASSERT_EQ(fills.size(), 4U);
  EXPECT_EQ(alpha(fills[0]), 64U);
  EXPECT_EQ(fills[0].colour & 0xFFU, 255U);
}

TEST_F(PainterTest, ClipNarrowsTheScissorOfDescendants) {
  provider::headless::NullGpu gpu;
  Painter target_painter = painter(gpu);
  const auto layout = tree(components::VStack(box("inner", 300.0F, 300.0F))
                               .frame({.width = 100.0F, .height = 100.0F})
                               .background(theme::ColourRole::surface)
                               .clip_shape(theme::Rectangle())
                               .id("clipper"));
  const Painted painted = paint(gpu, target_painter, *layout, handheld);
  ASSERT_EQ(painted.record.commands.size(), 2U);
  EXPECT_EQ(painted.record.commands[0].clip,
            (provider::ClipRect{.x = 0, .y = 0, .width = 1280, .height = 720}));
  const composition::Rect clipper = frame(*layout, "clipper");
  EXPECT_EQ(painted.record.commands[1].clip,
            (provider::ClipRect{.x = static_cast<std::uint32_t>(clipper.x),
                                .y = static_cast<std::uint32_t>(clipper.y),
                                .width = 100,
                                .height = 100}));
}

TEST_F(PainterTest, ButtonsDrawAFilledAccentShapeAndGlyphs) {
  provider::headless::NullGpu gpu;
  Painter target_painter = painter(gpu);
  const auto layout = tree(components::Button("Continue", [] {}).id("go"));
  text::Typesetter& setter = typesetter();
  const Painted idle = paint(gpu, target_painter, *layout, handheld, setter);
  const auto fills = of_kind(idle.record, provider::ShapeKind::fill);
  ASSERT_EQ(fills.size(), 4U);
  EXPECT_GT(fills[0].shape[provider::ShapeVertex::radius], 0.0F);
  EXPECT_FALSE(of_kind(idle.record, provider::ShapeKind::glyph).empty());
  const auto node = layout->find(composition::ElementKey{"go"});
  const Painted pressed =
      paint(gpu, target_painter, *layout, handheld, setter,
            Painter::Highlights{.focused = node, .pressed = node});
  const auto pressed_fills = of_kind(pressed.record, provider::ShapeKind::fill);
  ASSERT_EQ(pressed_fills.size(), 4U);
  EXPECT_NE(pressed_fills[0].colour, fills[0].colour);
  EXPECT_EQ(of_kind(pressed.record, provider::ShapeKind::stroke).size(), 4U);
  EXPECT_GE(pressed.record.commands.size(), 1U);
  EXPECT_TRUE(std::ranges::all_of(
      pressed.record.commands, [&](const provider::DrawCommand& command) {
        return command.texture ==
               target_painter.atlas().pages().front().texture();
      }));
}

TEST_F(PainterTest, TransformsAreAppliedWhileWritingGeometryOnce) {
  provider::headless::NullGpu gpu;
  Painter target_painter = painter(gpu);
  const auto layout = tree(
      components::VStack(components::VStack()
                             .frame({.width = 100.0F, .height = 40.0F})
                             .background(theme::ColourRole::accent,
                                         theme::Shape::rounded(8.0F, false))
                             .scale_effect(2.0F)
                             .rotation_effect(motion::Angle::degrees(90.0))));
  const std::size_t allocations = gpu.geometry_allocations();
  const Painted painted = paint(gpu, target_painter, *layout, handheld);
  EXPECT_EQ(gpu.geometry_allocations(), allocations + 1);
  const auto fills = of_kind(painted.record, provider::ShapeKind::fill);
  ASSERT_EQ(fills.size(), 4U);
  EXPECT_NEAR(fills[0].position[0], 682.0F, 1.0e-3F);
  EXPECT_NEAR(fills[0].position[1], 258.0F, 1.0e-3F);
  EXPECT_NEAR(fills[2].position[0], 598.0F, 1.0e-3F);
  EXPECT_NEAR(fills[2].position[1], 462.0F, 1.0e-3F);
  EXPECT_NEAR(fills[0].local[0], -102.0F, 1.0e-3F);
  EXPECT_NEAR(fills[0].local[1], -42.0F, 1.0e-3F);
  EXPECT_NEAR(fills[0].half_size[0], 100.0F, 1.0e-3F);
  EXPECT_NEAR(fills[0].half_size[1], 40.0F, 1.0e-3F);
  EXPECT_NEAR(fills[0].shape[provider::ShapeVertex::radius], 16.0F, 1.0e-3F);
  EXPECT_EQ(painted.record.vertices.size(), 4U);
  EXPECT_FALSE(painted.statistics.animating);
}

TEST_F(PainterTest, OffsetsMoveGlyphsAndShapesTogether) {
  provider::headless::NullGpu gpu;
  Painter target_painter = painter(gpu);
  text::Typesetter& setter = typesetter();
  const auto still = tree(components::Button("Go", [] {}));
  const auto moved =
      tree(components::Button("Go", [] {}).offset(30.0F, -10.0F));
  const Painted before = paint(gpu, target_painter, *still, handheld, setter);
  const Painted after = paint(gpu, target_painter, *moved, handheld, setter);
  ASSERT_EQ(before.record.vertices.size(), after.record.vertices.size());
  ASSERT_FALSE(before.record.vertices.empty());
  for (std::size_t index = 0; index < before.record.vertices.size(); ++index) {
    EXPECT_NEAR(after.record.vertices[index].position[0],
                before.record.vertices[index].position[0] + 30.0F, 1.0e-3F);
    EXPECT_NEAR(after.record.vertices[index].position[1],
                before.record.vertices[index].position[1] - 10.0F, 1.0e-3F);
    EXPECT_EQ(after.record.vertices[index].local,
              before.record.vertices[index].local);
  }
}

}

namespace arisa::rendering {

TEST_F(PainterTest, TranslatedGlyphsLandOnWholePixelsAtBothScales) {
  for (const layout::Viewport viewport : {handheld, docked()}) {
    provider::headless::NullGpu gpu;
    Painter target_painter = painter(gpu);
    const auto layout = tree(
        components::VStack(components::Text("Soft text").offset(0.37F, 0.61F)));
    text::Typesetter& setter = typesetter();
    const Painted painted =
        paint(gpu, target_painter, *layout, viewport, setter);
    const auto glyphs = of_kind(painted.record, provider::ShapeKind::glyph);
    ASSERT_FALSE(glyphs.empty());
    for (const provider::ShapeVertex& vertex : glyphs) {
      EXPECT_NEAR(vertex.position[1], std::round(vertex.position[1]), 1.0e-3F);
    }
  }
}

TEST_F(PainterTest, ShadowsReuseOneCachedPenumbra) {
  provider::headless::NullGpu gpu;
  Painter target_painter = painter(gpu);
  std::vector<composition::Element> cards;
  for (int index = 0; index < 4; ++index) {
    cards.push_back(components::VStack()
                        .frame({.width = 120.0F, .height = 80.0F})
                        .background(theme::ColourRole::surface_raised,
                                    theme::RoundedRectangle(14.0F))
                        .shadow(theme::Elevation::low));
  }
  const auto layout = tree(components::VStack(std::move(cards)));
  const Painted first = paint(gpu, target_painter, *layout, handheld);
  EXPECT_EQ(first.statistics.shadow_misses, 2U);
  EXPECT_EQ(first.statistics.shadow_hits, 6U);
  const Painted second = paint(gpu, target_painter, *layout, handheld);
  EXPECT_EQ(second.statistics.shadow_misses, 0U);
  EXPECT_EQ(second.statistics.shadow_hits, 8U);
  EXPECT_TRUE(of_kind(second.record, provider::ShapeKind::shadow).empty());
}

}

namespace arisa::rendering {

TEST_F(PainterTest, RepaintingAnUnchangedTreeRecordsNothing) {
  provider::headless::NullGpu gpu;
  Painter target_painter = painter(gpu);
  const auto layout = tree(components::VStack(
      components::VStack(box("a", 100.0F, 40.0F), box("b", 60.0F, 20.0F)),
      components::VStack(box("c", 80.0F, 30.0F), box("d", 40.0F, 10.0F))
          .padding(8.0F)
          .shadow(theme::Elevation::medium)));
  const Painted first = paint(gpu, target_painter, *layout, handheld);
  const Painted settled = paint(gpu, target_painter, *layout, handheld);
  const Painted idle = paint(gpu, target_painter, *layout, handheld);
  EXPECT_GE(first.statistics.recorded, 8U);
  EXPECT_EQ(first.statistics.spliced, 0U);
  EXPECT_EQ(idle.statistics.recorded, 0U);
  EXPECT_EQ(idle.statistics.spliced, 1U);
  EXPECT_EQ(idle.statistics.damaged_pixels, 0U);
  EXPECT_EQ(idle.record.vertices, settled.record.vertices);
  EXPECT_EQ(idle.record.commands, settled.record.commands);
}

TEST_F(PainterTest, AFocusMoveRecordsOnlyTheNodesItTouches) {
  const auto layout = tree(components::VStack(
      components::VStack(box("a", 100.0F, 40.0F), box("b", 60.0F, 20.0F)),
      components::VStack(box("c", 80.0F, 30.0F), box("d", 40.0F, 10.0F),
                         box("e", 40.0F, 10.0F))));
  const auto focused = [&layout](std::string_view key) {
    return layout->find(composition::ElementKey{key});
  };
  const auto depth = [&layout, &focused](std::string_view key) {
    return layout->node(*focused(key))->get().depth();
  };
  const Painted moved = [&] {
    provider::headless::NullGpu gpu;
    Painter target_painter = painter(gpu);
    static_cast<void>(paint(gpu, target_painter, *layout, handheld));
    static_cast<void>(paint(gpu, target_painter, *layout, handheld));
    const Painted ringed =
        paint(gpu, target_painter, *layout, handheld, std::nullopt,
              Painter::Highlights{.focused = focused("d")});
    EXPECT_EQ(ringed.statistics.recorded, 1U) << depth("d");
    const Painted again =
        paint(gpu, target_painter, *layout, handheld, std::nullopt,
              Painter::Highlights{.focused = focused("d")});
    EXPECT_EQ(again.statistics.recorded, 0U) << depth("d");
    return paint(gpu, target_painter, *layout, handheld, std::nullopt,
                 Painter::Highlights{.focused = focused("a")});
  }();
  EXPECT_EQ(moved.statistics.recorded, 2U);
  EXPECT_GT(moved.statistics.spliced, 0U);
  provider::headless::NullGpu reference_gpu;
  Painter reference = painter(reference_gpu);
  reference.debug_full_walk(true);
  const Painted walked =
      paint(reference_gpu, reference, *layout, handheld, std::nullopt,
            Painter::Highlights{.focused = focused("a")});
  EXPECT_EQ(moved.record.vertices, walked.record.vertices);
  EXPECT_EQ(moved.record.indices, walked.record.indices);
}

TEST_F(PainterTest, ShadowPatchesLeaveNoSeamOffThePixelGrid) {
  constexpr layout::Viewport stage{
      .pixels = provider::SurfaceSize{.width = provider::SurfaceWidth{240},
                                      .height = provider::SurfaceHeight{200}},
      .scale = provider::SurfaceScale::unit()};
  provider::headless::NullGpu gpu;
  gpu.rasterize(stage.pixels);
  Painter target_painter = painter(gpu);
  const auto layout =
      tree(components::VStack(
               components::VStack()
                   .frame({.width = 120.0F, .height = 80.0F})
                   .background(theme::ColourValue{theme::Colour::hex(0xFFFFFF)},
                               theme::RoundedRectangle(14.0F))
                   .shadow(theme::ColourValue{theme::Colour::hex(0x000000)},
                           10.0F, 0.25F, 6.25F))
               .frame({.width = 240.0F, .height = 200.0F})
               .background(theme::ColourValue{theme::Colour::hex(0xFFFFFF)}));
  paint(gpu, target_painter, *layout, stage);
  const auto image = gpu.capture();
  ASSERT_TRUE(image.has_value());
  const auto luma = [&image](std::uint32_t x, std::uint32_t y) {
    const auto found = image->pixel(x, y);
    EXPECT_TRUE(found.has_value());
    return found.has_value() ? static_cast<int>((*found)[1]) : 0;
  };
  const auto smooth = [](int before, int at, int after) {
    return std::abs((2 * at) - before - after) <= 4;
  };
  for (std::uint32_t y = 50; y < 150; ++y) {
    EXPECT_TRUE(smooth(luma(55, y - 1), luma(55, y), luma(55, y + 1)))
        << "row " << y;
  }
  for (std::uint32_t x = 70; x < 170; ++x) {
    EXPECT_TRUE(smooth(luma(x - 1, 150), luma(x, 150), luma(x + 1, 150)))
        << "column " << x;
  }
}

TEST_F(PainterTest, MinifiedSmoothImagesUploadAReducedCopy) {
  std::vector<std::byte> pixels(std::size_t{64} * 64 * 4, std::byte{0xFF});
  const auto image = std::make_shared<const provider::Image>(
      std::move(provider::Image::create(64, 64, std::move(pixels)).value()));
  const auto drawn = [&image](components::Image::Interpolation mode) {
    provider::headless::NullGpu gpu;
    Painter target_painter = painter(gpu);
    const auto layout = tree(components::VStack(
        components::Image(image).resizable().interpolation(mode).frame(
            {.width = 16.0F, .height = 16.0F})));
    paint(gpu, target_painter, *layout, handheld);
    return target_painter.pictures().bytes;
  };
  EXPECT_EQ(drawn(components::Image::Interpolation::high), 16U * 16U * 4U);
  EXPECT_EQ(drawn(components::Image::Interpolation::medium), 16U * 16U * 4U);
  EXPECT_EQ(drawn(components::Image::Interpolation::low), 64U * 64U * 4U);
  EXPECT_EQ(drawn(components::Image::Interpolation::none), 64U * 64U * 4U);
}

TEST_F(PainterTest, ImagesInARoundedClipHoldingGlassCarryTheClipCorners) {
  std::vector<std::byte> pixels(std::size_t{64} * 64 * 4, std::byte{0xFF});
  const auto image = std::make_shared<const provider::Image>(
      std::move(provider::Image::create(64, 64, std::move(pixels)).value()));
  const auto image_radius = [&image](bool glass) {
    provider::headless::NullGpu gpu;
    Painter target_painter = painter(gpu);
    composition::Element badge =
        components::VStack().frame({.width = 20.0F, .height = 20.0F});
    if (glass) {
      badge = std::move(badge).glass_effect(theme::Glass::clear(),
                                            theme::Shape::circle());
    }
    const auto layout = tree(components::VStack(
        components::Image(image)
            .resizable()
            .frame({.width = 100.0F, .height = 100.0F})
            .overlay(composition::Alignment::center, std::move(badge))
            .clip_shape(theme::Shape::rounded(20.0F, false))));
    const Painted painted = paint(gpu, target_painter, *layout, handheld);
    const auto images = of_kind(painted.record, provider::ShapeKind::image);
    return images.empty() ? -1.0F
                          : images.front().shape[provider::ShapeVertex::radius];
  };
  EXPECT_GT(image_radius(true), 0.0F);
  EXPECT_EQ(image_radius(false), 0.0F);
}

}
