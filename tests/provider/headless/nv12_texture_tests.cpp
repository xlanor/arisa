#include <arisa/provider/gpu/external_texture.hpp>
#include <arisa/provider/gpu/texture.hpp>
#include <arisa/provider/gpu/yuv_conversion.hpp>
#include <arisa/provider/headless/null_gpu.hpp>
#include <arisa/provider/headless/software_raster.hpp>
#include <arisa/support/scoped_callback.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace arisa::provider::headless {

class Nv12TextureTest : public ::testing::Test {
 protected:
  using Canvas = SoftwareRaster::Canvas;
  using Code = std::array<int, 3>;
  using Seen = std::array<float, 3>;

  static constexpr float code_tolerance = 2.5F;
  static constexpr float bar = 0.75F * 255.0F;
  static constexpr TextureId luma_id{1};
  static constexpr TextureId chroma_id{2};

  struct Bar {
    Code yuv;
    Code rgb;
  };

  static constexpr std::array<Code, 8> bar_rgb{{{1, 1, 1},
                                                {1, 1, 0},
                                                {0, 1, 1},
                                                {0, 1, 0},
                                                {1, 0, 1},
                                                {1, 0, 0},
                                                {0, 0, 1},
                                                {0, 0, 0}}};

  static constexpr std::array<Code, 8> bt601_limited_bars{{{180, 128, 128},
                                                           {162, 44, 142},
                                                           {131, 156, 44},
                                                           {112, 72, 58},
                                                           {84, 184, 198},
                                                           {65, 100, 212},
                                                           {35, 212, 114},
                                                           {16, 128, 128}}};

  static constexpr std::array<Code, 8> bt709_limited_bars{{{180, 128, 128},
                                                           {168, 44, 136},
                                                           {145, 147, 44},
                                                           {133, 63, 52},
                                                           {63, 193, 204},
                                                           {51, 109, 212},
                                                           {28, 212, 120},
                                                           {16, 128, 128}}};

  static auto plane(std::uint32_t width, std::uint32_t height,
                    TextureFormat format, std::span<const std::uint8_t> codes)
      -> std::shared_ptr<Canvas> {
    auto made = std::make_shared<Canvas>(width, height);
    std::vector<std::byte> bytes(codes.size());
    std::ranges::transform(codes, bytes.begin(),
                           [](std::uint8_t code) { return std::byte{code}; });
    made->upload(
        format, TextureRegion{.x = 0, .y = 0, .width = width, .height = height},
        PixelRows{bytes});
    return made;
  }

  static auto flat_luma(std::uint32_t width, std::uint32_t height, int code)
      -> std::shared_ptr<Canvas> {
    const std::vector<std::uint8_t> codes(
        static_cast<std::size_t>(width) * height,
        static_cast<std::uint8_t>(code));
    return plane(width, height, TextureFormat::r8, codes);
  }

  static auto flat_chroma(std::uint32_t width, std::uint32_t height, int cb,
                          int cr) -> std::shared_ptr<Canvas> {
    std::vector<std::uint8_t> codes;
    for (std::uint32_t index = 0; index < width * height; ++index) {
      codes.push_back(static_cast<std::uint8_t>(cb));
      codes.push_back(static_cast<std::uint8_t>(cr));
    }
    return plane(width, height, TextureFormat::rg8, codes);
  }

  static auto render(const Canvas& luma, const Canvas& chroma,
                     YuvConversion conversion, std::uint32_t width,
                     std::uint32_t height,
                     SamplerFilter filter = SamplerFilter::linear) -> Canvas {
    Canvas target{width, height};
    target.fill(
        Colour{.red = 0.0F, .green = 0.0F, .blue = 0.0F, .alpha = 1.0F});
    const auto right = static_cast<float>(width);
    const auto bottom = static_cast<float>(height);
    const ShapeVertex base{.colour = 0xFFFFFFFFU,
                           .kind = ShapeKind::yuv,
                           .effect = conversion.levels(),
                           .detail = conversion.weights()};
    std::array<ShapeVertex, 4> vertices{base, base, base, base};
    vertices[0].position = {0.0F, 0.0F};
    vertices[0].uv = {0.0F, 0.0F};
    vertices[1].position = {right, 0.0F};
    vertices[1].uv = {1.0F, 0.0F};
    vertices[2].position = {right, bottom};
    vertices[2].uv = {1.0F, 1.0F};
    vertices[3].position = {0.0F, bottom};
    vertices[3].uv = {0.0F, 1.0F};
    const std::array<Index, 6> indices{0, 1, 2, 2, 3, 0};
    const std::array<DrawCommand, 1> commands{
        DrawCommand{.texture = luma_id,
                    .clip = {.x = 0, .y = 0, .width = width, .height = height},
                    .index_offset = 0,
                    .index_count = 6,
                    .vertex_offset = 0,
                    .detail = chroma_id}};
    SoftwareRaster::draw(
        target,
        PassRegion{.x = 0.0F, .y = 0.0F, .width = right, .height = bottom},
        vertices, indices, commands, [&](TextureId id) {
          return SoftwareRaster::Source{
              .canvas = id == luma_id ? &luma : &chroma, .filter = filter};
        });
    return target;
  }

  static auto seen_at(const Canvas& canvas, std::int64_t x, std::int64_t y)
      -> Seen {
    const SoftwareRaster::Rgba texel = canvas.texel(x, y);
    return {texel[0] * 255.0F, texel[1] * 255.0F, texel[2] * 255.0F};
  }

  static auto decode(ColourMatrix matrix, ColourRange range, const Code& yuv)
      -> Seen {
    const Canvas shown =
        render(*flat_luma(2, 2, yuv[0]), *flat_chroma(1, 1, yuv[1], yuv[2]),
               YuvConversion{matrix, range}, 2, 2);
    return seen_at(shown, 0, 0);
  }

  static auto encoded(ColourMatrix matrix, ColourRange range, const Code& rgb)
      -> Code {
    const std::array<float, 2> published =
        matrix == ColourMatrix::bt709 ? std::array<float, 2>{0.2126F, 0.0722F}
        : matrix == ColourMatrix::bt601
            ? std::array<float, 2>{0.299F, 0.114F}
            : std::array<float, 2>{0.2627F, 0.0593F};
    const float red = 0.75F * static_cast<float>(rgb[0]);
    const float green = 0.75F * static_cast<float>(rgb[1]);
    const float blue = 0.75F * static_cast<float>(rgb[2]);
    const float kr = published[0];
    const float kb = published[1];
    const float y = (kr * red) + ((1.0F - kr - kb) * green) + (kb * blue);
    const float pb = (blue - y) / (2.0F * (1.0F - kb));
    const float pr = (red - y) / (2.0F * (1.0F - kr));
    const bool full = range == ColourRange::full;
    const auto quantise = [](float value) {
      return static_cast<int>(std::lround(std::clamp(value, 0.0F, 255.0F)));
    };
    return {quantise(full ? y * 255.0F : 16.0F + (y * 219.0F)),
            quantise(128.0F + (pb * (full ? 255.0F : 224.0F))),
            quantise(128.0F + (pr * (full ? 255.0F : 224.0F)))};
  }

  static void expect_bars(ColourMatrix matrix, ColourRange range,
                          std::span<const Code, 8> table) {
    for (std::size_t index = 0; index < table.size(); ++index) {
      const Seen seen = decode(matrix, range, table[index]);
      for (std::size_t channel = 0; channel < 3; ++channel) {
        EXPECT_NEAR(seen[channel],
                    static_cast<float>(bar_rgb[index][channel]) * bar,
                    code_tolerance)
            << "bar " << index << " channel " << channel;
      }
    }
  }

  static auto encoded_bars(ColourMatrix matrix, ColourRange range)
      -> std::array<Code, 8> {
    std::array<Code, 8> table{};
    std::ranges::transform(bar_rgb, table.begin(), [&](const Code& rgb) {
      return encoded(matrix, range, rgb);
    });
    return table;
  }

  static void present(NullGpu& gpu, const NullGpu::Imported& texture) {
    auto frame = gpu.begin_frame();
    ASSERT_TRUE(frame.has_value());
    auto geometry = frame->allocate_shapes(4, 6);
    ASSERT_TRUE(geometry.has_value());
    const std::array<Index, 6> indices{0, 1, 2, 0, 2, 3};
    std::ranges::copy(indices, geometry->indices.begin());
    const auto published = gpu.publications()->get().find(texture.id());
    ASSERT_TRUE(published.has_value());
    const std::array<DrawCommand, 1> commands{
        DrawCommand{.texture = texture.id(),
                    .clip = {.x = 0, .y = 0, .width = 8, .height = 8},
                    .index_offset = 0,
                    .index_count = 6,
                    .vertex_offset = 0,
                    .detail = published->chroma}};
    ASSERT_TRUE(
        frame->draw(ShapeDrawData{.geometry = *geometry, .commands = commands})
            .has_value());
    ASSERT_TRUE(frame->present().has_value());
  }

  static auto nv12(std::uint32_t width, std::uint32_t height,
                   ColourMatrix matrix = ColourMatrix::bt709,
                   ColourRange range = ColourRange::limited)
      -> ImportDescription {
    return ImportDescription{.width = width,
                             .height = height,
                             .format = TextureFormat::nv12,
                             .filter = SamplerFilter::linear,
                             .premultiplied = false,
                             .colour_matrix = matrix,
                             .range = range};
  }

  static auto planes(std::uint32_t width, std::uint32_t height)
      -> std::array<std::shared_ptr<Canvas>, 2> {
    return {flat_luma(width, height, 126),
            flat_chroma((width + 1) / 2, (height + 1) / 2, 128, 128)};
  }
};

TEST_F(Nv12TextureTest, CoefficientsMatchThePublishedDecodeMatrices) {
  const std::array<std::pair<ColourMatrix, YuvConversion::Packed>, 3> published{
      {{ColourMatrix::bt601, {1.402F, 0.344136F, 0.714136F, 1.772F}},
       {ColourMatrix::bt709, {1.5748F, 0.187324F, 0.468124F, 1.8556F}},
       {ColourMatrix::bt2020, {1.4746F, 0.164553F, 0.571353F, 1.8814F}}}};
  for (const auto& [matrix, expected] : published) {
    const YuvConversion::Packed weights =
        YuvConversion{matrix, ColourRange::limited}.weights();
    for (std::size_t index = 0; index < weights.size(); ++index) {
      EXPECT_NEAR(weights[index], expected[index], 2.0e-4F)
          << static_cast<int>(matrix) << " " << index;
    }
  }
}

TEST_F(Nv12TextureTest, BlackWhiteAndGreyDecodeForEveryMatrixAndRange) {
  for (const ColourMatrix matrix :
       {ColourMatrix::bt601, ColourMatrix::bt709, ColourMatrix::bt2020}) {
    for (const ColourRange range : {ColourRange::limited, ColourRange::full}) {
      const bool full = range == ColourRange::full;
      const std::array<std::pair<Code, float>, 3> cases{
          {{{full ? 0 : 16, 128, 128}, 0.0F},
           {{full ? 255 : 235, 128, 128}, 255.0F},
           {{full ? 128 : 126, 128, 128}, full ? 128.0F : 128.08F}}};
      for (const auto& [yuv, grey] : cases) {
        const Seen seen = decode(matrix, range, yuv);
        for (const float channel : seen) {
          EXPECT_NEAR(channel, grey, 1.0F)
              << static_cast<int>(matrix) << " " << static_cast<int>(range)
              << " y " << yuv[0];
        }
      }
    }
  }
}

TEST_F(Nv12TextureTest, PublishedLimitedRangeColourBarsDecode) {
  expect_bars(ColourMatrix::bt601, ColourRange::limited, bt601_limited_bars);
  expect_bars(ColourMatrix::bt709, ColourRange::limited, bt709_limited_bars);
}

TEST_F(Nv12TextureTest, JpegFullRangePrimariesDecode) {
  const std::array<Bar, 3> jfif{{{{76, 85, 255}, {255, 0, 0}},
                                 {{150, 44, 21}, {0, 255, 0}},
                                 {{29, 255, 107}, {0, 0, 255}}}};
  for (const Bar& primary : jfif) {
    const Seen seen =
        decode(ColourMatrix::bt601, ColourRange::full, primary.yuv);
    for (std::size_t channel = 0; channel < 3; ++channel) {
      EXPECT_NEAR(seen[channel], static_cast<float>(primary.rgb[channel]),
                  code_tolerance)
          << primary.yuv[0] << " " << channel;
    }
  }
}

TEST_F(Nv12TextureTest, EncodedColourBarsDecodeForTheRemainingMatrices) {
  for (const auto& [matrix, range] :
       {std::pair{ColourMatrix::bt601, ColourRange::full},
        std::pair{ColourMatrix::bt709, ColourRange::full},
        std::pair{ColourMatrix::bt2020, ColourRange::limited},
        std::pair{ColourMatrix::bt2020, ColourRange::full}}) {
    const std::array<Code, 8> table = encoded_bars(matrix, range);
    expect_bars(matrix, range, table);
  }
  const std::array<Code, 8> bt709 =
      encoded_bars(ColourMatrix::bt709, ColourRange::limited);
  for (std::size_t index = 0; index < bt709.size(); ++index) {
    for (std::size_t channel = 0; channel < 3; ++channel) {
      EXPECT_NEAR(bt709[index][channel], bt709_limited_bars[index][channel], 1)
          << index << " " << channel;
    }
  }
}

TEST_F(Nv12TextureTest, TheWrongMatrixOrRangeIsVisiblyWrong) {
  const Seen right =
      decode(ColourMatrix::bt709, ColourRange::limited, bt709_limited_bars[3]);
  const Seen wrong_matrix =
      decode(ColourMatrix::bt601, ColourRange::limited, bt709_limited_bars[3]);
  float worst = 0.0F;
  for (std::size_t channel = 0; channel < 3; ++channel) {
    worst = std::max(worst, std::abs(right[channel] - wrong_matrix[channel]));
  }
  EXPECT_GT(worst, 10.0F);
  const Seen wrong_range =
      decode(ColourMatrix::bt709, ColourRange::full, {235, 128, 128});
  EXPECT_NEAR(wrong_range[0], 235.0F, 1.0F);
}

TEST_F(Nv12TextureTest, ChromaIsLeftSitedHorizontallyAndCentredVertically) {
  const auto luma = flat_luma(4, 2, 126);
  const std::array<std::uint8_t, 4> across{128, 170, 128, 128};
  const auto wide = plane(2, 1, TextureFormat::rg8, across);
  const YuvConversion conversion{ColourMatrix::bt709, ColourRange::limited};
  const Canvas shown = render(*luma, *wide, conversion, 4, 2);
  const float grey = seen_at(shown, 3, 0)[0];
  const float peak = seen_at(shown, 0, 0)[0] - grey;
  EXPECT_GT(peak, 50.0F);
  EXPECT_NEAR(seen_at(shown, 1, 0)[0] - grey, peak * 0.5F, code_tolerance);
  EXPECT_NEAR(seen_at(shown, 2, 0)[0] - grey, 0.0F, code_tolerance);
  EXPECT_NEAR(seen_at(shown, 0, 1)[0], seen_at(shown, 0, 0)[0], 0.5F);

  const auto tall_luma = flat_luma(2, 4, 126);
  const std::array<std::uint8_t, 4> down{128, 170, 128, 128};
  const auto tall = plane(1, 2, TextureFormat::rg8, down);
  const Canvas stacked = render(*tall_luma, *tall, conversion, 2, 4);
  const float base = seen_at(stacked, 0, 3)[0];
  const float top = seen_at(stacked, 0, 0)[0] - base;
  EXPECT_GT(top, 50.0F);
  EXPECT_NEAR(seen_at(stacked, 0, 1)[0] - base, top * 0.75F, code_tolerance);
  EXPECT_NEAR(seen_at(stacked, 0, 2)[0] - base, top * 0.25F, code_tolerance);
}

TEST_F(Nv12TextureTest, NearestPlanesKeepCodesWhereLinearBlends) {
  const std::array<std::uint8_t, 2> edge{16, 235};
  const auto luma = plane(2, 1, TextureFormat::r8, edge);
  const auto chroma = flat_chroma(1, 1, 128, 128);
  const YuvConversion conversion{ColourMatrix::bt709, ColourRange::limited};
  const Canvas nearest =
      render(*luma, *chroma, conversion, 8, 1, SamplerFilter::nearest);
  const Canvas linear =
      render(*luma, *chroma, conversion, 8, 1, SamplerFilter::linear);
  for (std::int64_t x = 0; x < 8; ++x) {
    const float value = seen_at(nearest, x, 0)[0];
    EXPECT_TRUE(value < 1.0F || value > 254.0F) << x << " " << value;
  }
  EXPECT_GT(seen_at(linear, 3, 0)[0], 20.0F);
  EXPECT_LT(seen_at(linear, 3, 0)[0], 235.0F);

  const auto flat = flat_luma(4, 1, 126);
  const std::array<std::uint8_t, 4> across{128, 240, 128, 16};
  const auto split = plane(2, 1, TextureFormat::rg8, across);
  const Canvas kept =
      render(*flat, *split, conversion, 4, 1, SamplerFilter::nearest);
  const Canvas blended =
      render(*flat, *split, conversion, 4, 1, SamplerFilter::linear);
  const float high = seen_at(kept, 0, 0)[0];
  const float low = seen_at(kept, 3, 0)[0];
  for (std::int64_t x = 0; x < 4; ++x) {
    const float value = seen_at(kept, x, 0)[0];
    EXPECT_TRUE(std::abs(value - high) < 0.5F || std::abs(value - low) < 0.5F)
        << x << " " << value;
  }
  const float middle = seen_at(blended, 1, 0)[0];
  EXPECT_GT(std::min(std::abs(middle - high), std::abs(middle - low)), 20.0F);
}

TEST_F(Nv12TextureTest, ImportRegistersTwoPlanesUnderOneId) {
  NullGpu gpu;
  const auto made = planes(8, 6);
  auto imported = gpu.import_texture(
      made, nv12(8, 6, ColourMatrix::bt601, ColourRange::full));
  ASSERT_TRUE(imported.has_value());
  EXPECT_EQ(gpu.imports(), 1U);
  EXPECT_EQ(gpu.uploads(), 0U);
  EXPECT_EQ(gpu.live_textures(), 2U);
  ASSERT_TRUE(gpu.publications().has_value());
  EXPECT_EQ(gpu.publications()->get().live(), 1U);
  const auto published = gpu.publications()->get().find(imported->id());
  ASSERT_TRUE(published.has_value());
  EXPECT_TRUE(published->planar());
  EXPECT_NE(published->chroma, imported->id());
  EXPECT_EQ(published->conversion,
            (YuvConversion{ColourMatrix::bt601, ColourRange::full}));
  EXPECT_FALSE(gpu.publications()->get().find(published->chroma).has_value());
  imported->publish();
  EXPECT_EQ(gpu.publications()->get().find(imported->id())->generation, 1U);
}

TEST_F(Nv12TextureTest, ImportRejectsMismatchedPlanes) {
  NullGpu gpu;
  const std::array<std::shared_ptr<Canvas>, 2> wide_chroma{
      flat_luma(8, 6, 126), flat_chroma(8, 6, 128, 128)};
  EXPECT_FALSE(gpu.import_texture(wide_chroma, nv12(8, 6)).has_value());
  const std::array<std::shared_ptr<Canvas>, 2> short_luma{
      flat_luma(8, 4, 126), flat_chroma(4, 3, 128, 128)};
  EXPECT_FALSE(gpu.import_texture(short_luma, nv12(8, 6)).has_value());
  const std::array<std::shared_ptr<Canvas>, 2> missing{flat_luma(8, 6, 126),
                                                       nullptr};
  EXPECT_FALSE(gpu.import_texture(missing, nv12(8, 6)).has_value());
  const std::array<std::shared_ptr<Canvas>, 1> one{flat_luma(8, 6, 126)};
  EXPECT_FALSE(gpu.import_texture(one, nv12(8, 6)).has_value());
  EXPECT_FALSE(
      gpu.import_texture(flat_luma(8, 6, 126), nv12(8, 6)).has_value());
  ImportDescription rgba = nv12(8, 6);
  rgba.format = TextureFormat::rgba8;
  EXPECT_FALSE(gpu.import_texture(planes(8, 6), rgba).has_value());
  EXPECT_FALSE(gpu.create_texture(
                      {.width = 8, .height = 6, .format = TextureFormat::nv12})
                   .has_value());
  EXPECT_EQ(gpu.live_textures(), 0U);
  EXPECT_EQ(gpu.imports(), 0U);
  const std::array<std::shared_ptr<Canvas>, 2> odd{flat_luma(7, 5, 126),
                                                   flat_chroma(4, 3, 128, 128)};
  EXPECT_TRUE(gpu.import_texture(odd, nv12(7, 5)).has_value());
}

TEST_F(Nv12TextureTest, InUseAndDeferredReleaseFreeBothPlanesOnce) {
  NullGpu gpu;
  int released = 0;
  std::optional<NullGpu::Imported> imported;
  {
    auto made = gpu.import_texture(
        planes(4, 4), nv12(4, 4),
        support::ScopedCallback{[&released]() noexcept { ++released; }});
    ASSERT_TRUE(made.has_value());
    imported.emplace(std::move(*made));
  }
  EXPECT_FALSE(imported->in_use());
  gpu.hold_completion(3);
  present(gpu, *imported);
  EXPECT_TRUE(imported->in_use());
  imported.reset();
  EXPECT_EQ(released, 0);
  EXPECT_EQ(gpu.live_textures(), 2U);
  EXPECT_EQ(gpu.publications()->get().live(), 1U);
  gpu.hold_completion(0);
  EXPECT_EQ(released, 1);
  EXPECT_EQ(gpu.live_textures(), 0U);
  EXPECT_EQ(gpu.publications()->get().live(), 0U);
}

TEST_F(Nv12TextureTest, PlaneFormatsUploadLikeAnyOtherTexture) {
  NullGpu gpu;
  auto luma = gpu.create_texture(
      {.width = 2, .height = 1, .format = TextureFormat::r8});
  auto chroma = gpu.create_texture(
      {.width = 1, .height = 1, .format = TextureFormat::rg8});
  ASSERT_TRUE(luma.has_value());
  ASSERT_TRUE(chroma.has_value());
  const std::array<std::byte, 2> two{std::byte{16}, std::byte{235}};
  EXPECT_TRUE(
      gpu.update_texture(*luma, {.x = 0, .y = 0, .width = 2, .height = 1}, two)
          .has_value());
  EXPECT_TRUE(gpu.update_texture(*chroma,
                                 {.x = 0, .y = 0, .width = 1, .height = 1}, two)
                  .has_value());
  EXPECT_EQ(gpu.uploaded_bytes(), 4U);
  EXPECT_EQ(Texture::bytes_per_pixel(TextureFormat::nv12), 0U);
  EXPECT_FALSE(Texture::upload_size(
                   {.width = 2, .height = 2, .format = TextureFormat::nv12},
                   {.x = 0, .y = 0, .width = 2, .height = 2})
                   .has_value());
  const auto r8 =
      plane(1, 1, TextureFormat::r8, std::array<std::uint8_t, 1>{200});
  EXPECT_NEAR(r8->texel(0, 0)[0], 200.0F / 255.0F, 1.0e-3F);
  EXPECT_FLOAT_EQ(r8->texel(0, 0)[1], 0.0F);
  EXPECT_FLOAT_EQ(r8->texel(0, 0)[3], 1.0F);
}

}
