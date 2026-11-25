#include <arisa/provider/headless/software_raster.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace arisa::provider::headless {

class SoftwareRasterTest : public ::testing::Test {
 protected:
  struct Capsule {
    std::array<float, 2> centre{};
    std::array<float, 2> half_size{};
  };

  static constexpr std::uint32_t width = 200;
  static constexpr std::uint32_t height = 80;
  static constexpr float spacing = 20.0F;
  static constexpr std::uint32_t table_width = 50;

  static auto render(std::span<const Capsule> capsules, float reach = spacing)
      -> SoftwareRaster::Canvas {
    SoftwareRaster::Canvas atlas{4, 4};
    atlas.fill(Colour{.red = 1.0F, .green = 1.0F, .blue = 1.0F, .alpha = 1.0F});
    std::array<float, table_width> values{};
    values[0] = static_cast<float>(capsules.size());
    values[1] = reach;
    std::size_t at = 2;
    for (const Capsule& capsule : capsules) {
      const std::array<float, 6> member{
          capsule.centre[0],
          capsule.centre[1],
          capsule.half_size[0],
          capsule.half_size[1],
          std::min(capsule.half_size[0], capsule.half_size[1]),
          0.0F};
      std::ranges::copy(member,
                        values.begin() + static_cast<std::ptrdiff_t>(at));
      at += member.size();
    }
    SoftwareRaster::Canvas table{table_width, 1};
    table.upload(
        TextureFormat::rgba8,
        TextureRegion{.x = 0, .y = 0, .width = table_width, .height = 1},
        PixelRows{std::as_bytes(std::span{values})});
    SoftwareRaster::Canvas target{width, height};
    target.fill(
        Colour{.red = 0.0F, .green = 0.0F, .blue = 0.0F, .alpha = 1.0F});
    const ShapeVertex base{.half_size = {width * 0.5F, height * 0.5F},
                           .shape = {reach, 16.0F, 0.0F, 1.0F},
                           .colour = 0x00FFFFFFU,
                           .kind = ShapeKind::glass_union,
                           .effect = {0.0F, 0.0F, static_cast<float>(width),
                                      static_cast<float>(height)},
                           .detail = {0.0F, 0.0F, 0.0F, 0.0F}};
    std::array<ShapeVertex, 4> vertices{base, base, base, base};
    const std::array<std::array<float, 2>, 4> corners{
        {{0.0F, 0.0F},
         {static_cast<float>(width), 0.0F},
         {static_cast<float>(width), static_cast<float>(height)},
         {0.0F, static_cast<float>(height)}}};
    for (std::size_t corner = 0; corner < corners.size(); ++corner) {
      vertices[corner].position = corners[corner];
      vertices[corner].local = {corners[corner][0] - (width * 0.5F),
                                corners[corner][1] - (height * 0.5F)};
    }
    const std::array<Index, 6> indices{0, 1, 2, 2, 3, 0};
    const std::array<DrawCommand, 1> commands{
        DrawCommand{.texture = TextureId{1},
                    .clip = {.x = 0, .y = 0, .width = width, .height = height},
                    .index_offset = 0,
                    .index_count = 6,
                    .vertex_offset = 0,
                    .detail = TextureId{2}}};
    SoftwareRaster::draw(target,
                         PassRegion{.x = 0.0F,
                                    .y = 0.0F,
                                    .width = static_cast<float>(width),
                                    .height = static_cast<float>(height)},
                         vertices, indices, commands,
                         [&](TextureId id) -> SoftwareRaster::Source {
                           return SoftwareRaster::Source{
                               .canvas = id == TextureId{1} ? &atlas : &table,
                               .filter = SamplerFilter::linear};
                         });
    return target;
  }

  static auto coverage(const SoftwareRaster::Canvas& canvas, std::uint32_t x,
                       std::uint32_t y) -> float {
    return canvas.texel(x, y)[0];
  }

  static auto edge_width(const std::vector<float>& ramp) -> float {
    const auto crossing = [&ramp](float level) -> std::optional<float> {
      for (std::size_t index = 1; index < ramp.size(); ++index) {
        if (ramp[index - 1] < level && ramp[index] >= level) {
          const float t =
              (level - ramp[index - 1]) / (ramp[index] - ramp[index - 1]);
          return static_cast<float>(index - 1) + t;
        }
      }
      return std::nullopt;
    };
    const std::optional<float> low = crossing(0.1F);
    const std::optional<float> high = crossing(0.9F);
    EXPECT_TRUE(low.has_value() && high.has_value());
    if (!low.has_value() || !high.has_value()) {
      return 1.0e3F;
    }
    return *high - *low;
  }

  static auto row(const SoftwareRaster::Canvas& canvas, std::uint32_t y,
                  std::uint32_t from, std::uint32_t to) -> std::vector<float> {
    std::vector<float> values;
    for (std::uint32_t x = from; x < to; ++x) {
      values.push_back(coverage(canvas, x, y));
    }
    return values;
  }

  static auto column(const SoftwareRaster::Canvas& canvas, std::uint32_t x,
                     std::uint32_t from, std::uint32_t to)
      -> std::vector<float> {
    std::vector<float> values;
    for (std::uint32_t y = from; y < to; ++y) {
      values.push_back(coverage(canvas, x, y));
    }
    return values;
  }

  static auto top_boundary(const SoftwareRaster::Canvas& canvas,
                           std::uint32_t x) -> float {
    const std::vector<float> values = column(canvas, x, 0, height / 2);
    for (std::size_t index = 1; index < values.size(); ++index) {
      if (values[index - 1] < 0.5F && values[index] >= 0.5F) {
        const float t =
            (0.5F - values[index - 1]) / (values[index] - values[index - 1]);
        return static_cast<float>(index - 1) + t;
      }
    }
    return static_cast<float>(height);
  }
};

TEST_F(SoftwareRasterTest, MergedOutlineIsAsSharpAsALoneCapsule) {
  const std::array<Capsule, 1> lone{
      Capsule{.centre = {60.0F, 40.0F}, .half_size = {30.0F, 15.0F}}};
  const std::array<Capsule, 2> pair{
      Capsule{.centre = {60.0F, 40.0F}, .half_size = {30.0F, 15.0F}},
      Capsule{.centre = {126.0F, 40.0F}, .half_size = {30.0F, 15.0F}}};
  const SoftwareRaster::Canvas single = render(lone);
  const SoftwareRaster::Canvas merged = render(pair);
  const float alone = edge_width(row(single, 40, 20, 40));
  EXPECT_LE(alone, 1.5F);
  EXPECT_LE(edge_width(row(merged, 40, 20, 40)), 1.5F);
  EXPECT_NEAR(edge_width(row(merged, 40, 20, 40)), alone, 0.2F);
  EXPECT_LE(edge_width(column(merged, 60, 15, 35)), 1.5F);
  EXPECT_LE(edge_width(column(merged, 93, 15, 40)), 1.75F);
}

TEST_F(SoftwareRasterTest, NeckBetweenNearCapsulesIsFilledAndSmooth) {
  const std::array<Capsule, 2> pair{
      Capsule{.centre = {60.0F, 40.0F}, .half_size = {30.0F, 15.0F}},
      Capsule{.centre = {126.0F, 40.0F}, .half_size = {30.0F, 15.0F}}};
  const SoftwareRaster::Canvas merged = render(pair);
  EXPECT_GT(coverage(merged, 93, 40), 0.99F);
  const std::vector<float> across = column(merged, 93, 5, 41);
  for (std::size_t index = 1; index < across.size(); ++index) {
    EXPECT_GE(across[index] + 1.0e-3F, across[index - 1]) << index;
  }
  std::vector<float> boundary;
  for (std::uint32_t x = 34; x <= 152; ++x) {
    boundary.push_back(top_boundary(merged, x));
  }
  for (std::size_t x = 60; x < 93; ++x) {
    EXPECT_GE(boundary[x + 1 - 34] + 1.0e-3F, boundary[x - 34]) << x;
  }
  for (std::size_t x = 93; x < 126; ++x) {
    EXPECT_LE(boundary[x + 1 - 34], boundary[x - 34] + 1.0e-3F) << x;
  }
  EXPECT_GT(boundary[93 - 34], boundary[60 - 34] + 1.0F);
}

TEST_F(SoftwareRasterTest, WaistIsShallowAtAContainerGapAndDeepensWithIt) {
  constexpr float container = 30.0F;
  float previous = 0.0F;
  for (const float gap : {2.0F, 5.0F, 7.5F, 12.0F, 18.0F, 24.0F}) {
    const std::array<Capsule, 2> pair{
        Capsule{.centre = {60.0F, 40.0F}, .half_size = {30.0F, 15.0F}},
        Capsule{.centre = {120.0F + gap, 40.0F}, .half_size = {30.0F, 15.0F}}};
    const SoftwareRaster::Canvas merged = render(pair, container);
    const auto middle = static_cast<std::uint32_t>(90.0F + (gap * 0.5F));
    float lowest = 0.0F;
    for (std::uint32_t x = middle - 2; x <= middle + 2; ++x) {
      lowest = std::max(lowest, top_boundary(merged, x));
    }
    const float depth = (lowest - top_boundary(merged, 60)) / 30.0F;
    if (gap == 7.5F) {
      EXPECT_GE(depth, 0.10F);
      EXPECT_LE(depth, 0.20F);
    }
    EXPECT_GT(depth, previous) << "gap " << gap;
    EXPECT_LT(depth, 0.5F) << "gap " << gap;
    previous = depth;
  }
}

TEST_F(SoftwareRasterTest, ContainerOutlineCurvesWithoutKinks) {
  const std::array<Capsule, 2> pair{
      Capsule{.centre = {60.0F, 40.0F}, .half_size = {30.0F, 15.0F}},
      Capsule{.centre = {127.5F, 40.0F}, .half_size = {30.0F, 15.0F}}};
  const SoftwareRaster::Canvas merged = render(pair, 30.0F);
  std::vector<float> boundary;
  for (std::uint32_t x = 60; x <= 128; ++x) {
    boundary.push_back(top_boundary(merged, x));
  }
  int flips = 0;
  float sign = 0.0F;
  float sharpest = 0.0F;
  constexpr std::size_t stride = 3;
  for (std::size_t index = stride; index + stride < boundary.size(); ++index) {
    const float bend = boundary[index + stride] - (2.0F * boundary[index]) +
                       boundary[index - stride];
    sharpest = std::max(sharpest, std::abs(bend));
    if (std::abs(bend) < 0.3F) {
      continue;
    }
    const float now = bend > 0.0F ? 1.0F : -1.0F;
    if (sign != 0.0F && now != sign) {
      ++flips;
    }
    sign = now;
  }
  EXPECT_LE(flips, 2);
  EXPECT_LT(sharpest, 1.5F);
}

TEST_F(SoftwareRasterTest, MergedUnionNeverLeavesTheMembersSlab) {
  for (const float gap : {0.0F, 2.0F, 6.0F, 12.0F, 18.0F}) {
    const std::array<Capsule, 2> pair{
        Capsule{.centre = {60.0F, 40.0F}, .half_size = {30.0F, 15.0F}},
        Capsule{.centre = {120.0F + gap, 40.0F}, .half_size = {30.0F, 15.0F}}};
    const SoftwareRaster::Canvas merged = render(pair);
    for (std::uint32_t x = 0; x < width; ++x) {
      for (std::uint32_t y = 0; y < height; ++y) {
        if (y < 24 || y > 55) {
          EXPECT_LT(coverage(merged, x, y), 0.02F)
              << "gap " << gap << " at " << x << "," << y;
        }
      }
    }
  }
}

TEST_F(SoftwareRasterTest, CapsulesFartherThanTheSpacingStaySeparate) {
  const std::array<Capsule, 2> pair{
      Capsule{.centre = {60.0F, 40.0F}, .half_size = {30.0F, 15.0F}},
      Capsule{.centre = {150.0F, 40.0F}, .half_size = {30.0F, 15.0F}}};
  const SoftwareRaster::Canvas apart = render(pair);
  EXPECT_LT(coverage(apart, 105, 40), 0.01F);
  EXPECT_GT(coverage(apart, 60, 40), 0.99F);
  EXPECT_GT(coverage(apart, 150, 40), 0.99F);
}

}
