#include <arisa/conformance/gpu_suite.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::conformance {

using provider::ProviderErrorCode;

void GpuSuite::run(provider::ProviderFactory& factory, Report& report) {
  provider::GpuProvider& gpu = factory.gpu();
  if (!factory.supplies(provider::Capability::gpu)) {
    check_absent(gpu, report);
    return;
  }
  check_texture_lifecycle(gpu, report);
  check_texture_rejects(gpu, report);
  check_frame_discard(gpu, report);
  const provider::SurfaceSize surface = factory.surface().size();
  const provider::SurfaceSize size =
      surface.empty()
          ? provider::SurfaceSize{.width = provider::SurfaceWidth{64},
                                  .height = provider::SurfaceHeight{64}}
          : surface;
  check_frame_present(gpu, size, report);
  check_unwritten_texels(gpu, size, report);
}

auto GpuSuite::matches(std::array<std::uint8_t, 4> actual,
                       std::array<std::uint8_t, 4> expected) noexcept -> bool {
  return std::ranges::equal(
      actual, expected, [](std::uint8_t left, std::uint8_t right) {
        return std::abs(static_cast<int>(left) - static_cast<int>(right)) <=
               tolerance;
      });
}

void GpuSuite::check_texture_lifecycle(provider::GpuProvider& gpu,
                                       Report& report) {
  bool passed = false;
  {
    auto texture = gpu.create_texture(provider::TextureDescription{
        .width = 2, .height = 2, .format = provider::TextureFormat::rgba8});
    if (texture.has_value() && texture->valid()) {
      const std::vector<std::byte> pixels(2 * 2 * 4, std::byte{0xff});
      const std::vector<std::byte> row(2 * 4, std::byte{0x80});
      passed = gpu.update_texture(*texture,
                                  provider::TextureRegion{
                                      .x = 0, .y = 0, .width = 2, .height = 2},
                                  pixels)
                   .has_value() &&
               gpu.update_texture(*texture,
                                  provider::TextureRegion{
                                      .x = 0, .y = 1, .width = 2, .height = 1},
                                  row)
                   .has_value();
    }
  }
  report.record("gpu.texture_lifecycle", passed,
                "create_texture() or update_texture() failed on a valid "
                "texture and region");
}

void GpuSuite::check_texture_rejects(provider::GpuProvider& gpu,
                                     Report& report) {
  const auto rejected = [](const auto& result) {
    return !result.has_value() &&
           result.error().code() == ProviderErrorCode::invalid_argument;
  };
  const bool empty_rejected = rejected(gpu.create_texture(
      provider::TextureDescription{.width = 0, .height = 4}));
  bool region_rejected = false;
  auto texture = gpu.create_texture(provider::TextureDescription{
      .width = 4, .height = 4, .format = provider::TextureFormat::alpha8});
  if (texture.has_value()) {
    const std::vector<std::byte> pixels(4 * 4, std::byte{0});
    region_rejected =
        rejected(gpu.update_texture(
            *texture,
            provider::TextureRegion{.x = 2, .y = 2, .width = 4, .height = 4},
            pixels)) &&
        rejected(gpu.update_texture(
            *texture,
            provider::TextureRegion{.x = 0, .y = 0, .width = 4, .height = 4},
            std::span{pixels}.first(3)));
  }
  report.record("gpu.texture_rejects", empty_rejected && region_rejected,
                "an empty texture, an out-of-bounds region or a short upload "
                "was not rejected with invalid_argument");
}

void GpuSuite::check_frame_discard(provider::GpuProvider& gpu, Report& report) {
  bool passed = false;
  {
    auto discarded = gpu.begin_frame();
    if (discarded.has_value()) {
      discarded->clear(clear_colour);
      passed = discarded->active();
    }
  }
  auto next = gpu.begin_frame();
  passed = passed && next.has_value();
  report.record("gpu.frame_discard", passed,
                "a frame could not be begun again after discarding one");
}

void GpuSuite::check_frame_present(provider::GpuProvider& gpu,
                                   provider::SurfaceSize size, Report& report) {
  auto white = gpu.create_texture(provider::TextureDescription{
      .width = 1, .height = 1, .format = provider::TextureFormat::rgba8});
  bool presented = false;
  if (white.has_value()) {
    const std::vector<std::byte> pixel(4, std::byte{0xff});
    presented = gpu.update_texture(*white,
                                   provider::TextureRegion{
                                       .x = 0, .y = 0, .width = 1, .height = 1},
                                   pixel)
                    .has_value();
  }
  const float width = static_cast<float>(size.width.value());
  const float height = static_cast<float>(size.height.value());
  const std::uint32_t colour = provider::Vertex::rgba(
      quad_rgba[0], quad_rgba[1], quad_rgba[2], quad_rgba[3]);
  const std::array quad_vertices{
      provider::Vertex{.position = {width / 4.0F, height / 4.0F},
                       .uv = {0.0F, 0.0F},
                       .colour = colour},
      provider::Vertex{.position = {width * 3.0F / 4.0F, height / 4.0F},
                       .uv = {1.0F, 0.0F},
                       .colour = colour},
      provider::Vertex{.position = {width * 3.0F / 4.0F, height * 3.0F / 4.0F},
                       .uv = {1.0F, 1.0F},
                       .colour = colour},
      provider::Vertex{.position = {width / 4.0F, height * 3.0F / 4.0F},
                       .uv = {0.0F, 1.0F},
                       .colour = colour},
  };
  const std::array<provider::Index, 6> quad_indices{0, 1, 2, 0, 2, 3};
  if (presented) {
    const std::array commands{provider::DrawCommand{
        .texture = white->id(),
        .clip = provider::ClipRect{.x = 0,
                                   .y = 0,
                                   .width = size.width.value(),
                                   .height = size.height.value()},
        .index_offset = 0,
        .index_count = 6,
        .vertex_offset = 0}};
    auto frame = gpu.begin_frame();
    presented = frame.has_value();
    if (presented) {
      frame->clear(clear_colour);
      auto geometry =
          frame->allocate_geometry(quad_vertices.size(), quad_indices.size());
      presented = geometry.has_value() &&
                  geometry->vertices.size() == quad_vertices.size() &&
                  geometry->indices.size() == quad_indices.size();
      if (presented) {
        std::ranges::copy(quad_vertices, geometry->vertices.begin());
        std::ranges::copy(quad_indices, geometry->indices.begin());
        presented = frame
                        ->draw(provider::DrawData{.geometry = *geometry,
                                                  .commands = commands})
                        .has_value() &&
                    frame->present().has_value() && !frame->active();
      }
    }
  }
  report.record("gpu.frame_present", presented,
                "begin_frame(), clear(), draw() or present() failed for a "
                "textured quad");

  const auto image = gpu.capture();
  if (!image.has_value()) {
    report.record(
        "gpu.capture",
        image.error().code() == ProviderErrorCode::unsupported_capability,
        "capture() failed with an error other than unsupported_capability");
    return;
  }
  const auto centre = image->pixel(image->width() / 2, image->height() / 2);
  const auto corner = image->pixel(1, 1);
  static constexpr std::array<std::uint8_t, 4> clear_rgba{0, 0, 255, 255};
  report.record("gpu.capture",
                image->width() == size.width.value() &&
                    image->height() == size.height.value() &&
                    centre.has_value() && matches(*centre, quad_rgba) &&
                    corner.has_value() && matches(*corner, clear_rgba),
                "the captured image did not show the quad over the clear "
                "colour at the surface size");
}

void GpuSuite::check_unwritten_texels(provider::GpuProvider& gpu,
                                      provider::SurfaceSize size,
                                      Report& report) {
  static constexpr std::string_view name = "gpu.unwritten_texels";
  if (!present_half_written(gpu, size)) {
    report.record(std::string{name}, false,
                  "a half-written texture could not be created, uploaded or "
                  "drawn");
    return;
  }
  const auto image = gpu.capture();
  if (!image.has_value()) {
    report.record(
        std::string{name},
        image.error().code() == ProviderErrorCode::unsupported_capability,
        "capture() failed with an error other than unsupported_capability");
    return;
  }
  const std::uint32_t middle = size.height.value() / 2;
  const auto written = image->pixel(size.width.value() * 3 / 8, middle);
  const auto unwritten = image->pixel(size.width.value() * 5 / 8, middle);
  static constexpr std::array<std::uint8_t, 4> clear_rgba{0, 0, 255, 255};
  report.record(std::string{name},
                image->width() == size.width.value() &&
                    image->height() == size.height.value() &&
                    written.has_value() && matches(*written, quad_rgba) &&
                    unwritten.has_value() && matches(*unwritten, clear_rgba),
                "the unwritten half of a new texture did not read as "
                "transparent black");
}

auto GpuSuite::present_half_written(provider::GpuProvider& gpu,
                                    provider::SurfaceSize size) -> bool {
  auto texture = gpu.create_texture(
      provider::TextureDescription{.width = 2,
                                   .height = 2,
                                   .format = provider::TextureFormat::rgba8,
                                   .usage = provider::TextureUsage::sampled,
                                   .filter = provider::SamplerFilter::nearest});
  if (!texture.has_value()) {
    return false;
  }
  const std::vector<std::byte> column{
      std::byte{quad_rgba[0]}, std::byte{quad_rgba[1]}, std::byte{quad_rgba[2]},
      std::byte{quad_rgba[3]}, std::byte{quad_rgba[0]}, std::byte{quad_rgba[1]},
      std::byte{quad_rgba[2]}, std::byte{quad_rgba[3]}};
  if (!gpu.update_texture(
              *texture,
              provider::TextureRegion{.x = 0, .y = 0, .width = 1, .height = 2},
              column)
           .has_value()) {
    return false;
  }
  auto frame = gpu.begin_frame();
  if (!frame.has_value()) {
    return false;
  }
  frame->clear(clear_colour);
  auto geometry = frame->allocate_shapes(4, 6);
  if (!geometry.has_value() || geometry->vertices.size() != 4 ||
      geometry->indices.size() != 6) {
    return false;
  }
  const float width = static_cast<float>(size.width.value());
  const float height = static_cast<float>(size.height.value());
  const provider::ShapeVertex corner{.colour = 0xFFFFFFFFU,
                                     .kind = provider::ShapeKind::image};
  const std::array<std::array<float, 2>, 4> positions{
      {{width / 4.0F, height / 4.0F},
       {width * 3.0F / 4.0F, height / 4.0F},
       {width * 3.0F / 4.0F, height * 3.0F / 4.0F},
       {width / 4.0F, height * 3.0F / 4.0F}}};
  const std::array<std::array<float, 2>, 4> uvs{
      {{0.0F, 0.0F}, {1.0F, 0.0F}, {1.0F, 1.0F}, {0.0F, 1.0F}}};
  for (std::size_t index = 0; index < positions.size(); ++index) {
    provider::ShapeVertex& vertex = geometry->vertices[index];
    vertex = corner;
    vertex.position = positions[index];
    vertex.uv = uvs[index];
  }
  const std::array<provider::Index, 6> indices{0, 1, 2, 0, 2, 3};
  std::ranges::copy(indices, geometry->indices.begin());
  const std::array commands{provider::DrawCommand{
      .texture = texture->id(),
      .clip = provider::ClipRect{.x = 0,
                                 .y = 0,
                                 .width = size.width.value(),
                                 .height = size.height.value()},
      .index_offset = 0,
      .index_count = 6,
      .vertex_offset = 0}};
  return frame
             ->draw(provider::ShapeDrawData{.geometry = *geometry,
                                            .commands = commands})
             .has_value() &&
         frame->present().has_value() && !frame->active();
}

void GpuSuite::check_absent(provider::GpuProvider& gpu, Report& report) {
  const auto unsupported = [](const auto& result) {
    return !result.has_value() &&
           result.error().code() == ProviderErrorCode::unsupported_capability;
  };
  report.record("gpu.absent",
                unsupported(gpu.create_texture(
                    provider::TextureDescription{.width = 1, .height = 1})) &&
                    unsupported(gpu.begin_frame()) &&
                    unsupported(gpu.capture()),
                "an unsupplied gpu provider did not return "
                "unsupported_capability");
}

}
