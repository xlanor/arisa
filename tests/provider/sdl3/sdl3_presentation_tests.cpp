#include <arisa/conformance/conformance_suite.hpp>
#include <arisa/conformance/gpu_suite.hpp>
#include <arisa/provider/sdl3/sdl3_factory.hpp>
#include <arisa/provider/sdl3/sdl3_gpu.hpp>
#include <arisa/provider/sdl3/sdl3_input.hpp>
#include <arisa/provider/sdl3/sdl3_offscreen_surface.hpp>
#include <arisa/provider/sdl3/sdl3_window_surface.hpp>
#include <arisa/provider/standard/directory_assets.hpp>
#include <arisa/provider/standard/std_threading.hpp>
#include <arisa/support/scoped_callback.hpp>

#include "sdl3_test.hpp"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gpu.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <print>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::provider::sdl3 {

class Sdl3PresentationTest : public Sdl3Test {
 protected:
  static constexpr SurfaceSize small{.width = SurfaceWidth{64},
                                     .height = SurfaceHeight{48}};

  void SetUp() override {
    auto created = Sdl3EventPump::create(threading_);
    ASSERT_TRUE(created.has_value());
    pump_.emplace(std::move(*created));
  }

  auto offscreen_gpu(const Sdl3OffscreenSurface& surface)
      -> std::optional<Sdl3Gpu> {
    auto gpu = Sdl3Gpu::create(surface);
    if (!gpu.has_value()) {
      return std::nullopt;
    }
    return std::move(*gpu);
  }

  static void push_finger(SDL_EventType type, SDL_FingerID finger, float x,
                          float y) {
    SDL_Event event{};
    event.type = type;
    event.tfinger.touchID = 1;
    event.tfinger.fingerID = finger;
    event.tfinger.x = x;
    event.tfinger.y = y;
    ASSERT_TRUE(SDL_PushEvent(&event));
  }

  standard::StdThreadingProvider threading_;
  std::optional<Sdl3EventPump> pump_;
  standard::DirectoryAssets fixtures_{ARISA_TEST_FIXTURES};
};

TEST_F(Sdl3PresentationTest, OffscreenSurfaceHasAFixedSize) {
  EXPECT_FALSE(Sdl3OffscreenSurface::create(*pump_, {}).has_value());
  auto surface = Sdl3OffscreenSurface::create(
      *pump_, {.width = SurfaceWidth{1920}, .height = SurfaceHeight{1080}});
  ASSERT_TRUE(surface.has_value());
  EXPECT_EQ(surface->size().height.value(), 1080U);
  EXPECT_FLOAT_EQ(surface->scale().value(), 1.5F);
  EXPECT_FALSE(surface->poll().has_value());
}

TEST_F(Sdl3PresentationTest, WindowSurfaceReportsPixelsOrFailsCleanly) {
  const auto empty = Sdl3WindowSurface::create(*pump_, "arisa test", {});
  ASSERT_FALSE(empty.has_value());
  EXPECT_EQ(empty.error().code(), ProviderErrorCode::invalid_argument);
  auto surface = Sdl3WindowSurface::create(*pump_, "arisa test", small);
  if (!surface.has_value()) {
    EXPECT_EQ(surface.error().code(), ProviderErrorCode::platform_failure);
    return;
  }
  EXPECT_NE(surface->window(), nullptr);
  EXPECT_FALSE(surface->size().empty());
  while (surface->poll().has_value()) {
  }
  EXPECT_GT(surface->scale().value(), 0.0F);
}

TEST_F(Sdl3PresentationTest, InputTranslatesButtonsAndKeys) {
  EXPECT_EQ(Sdl3Input::translate_button(SDL_GAMEPAD_BUTTON_EAST), Button::a);
  EXPECT_EQ(Sdl3Input::translate_button(SDL_GAMEPAD_BUTTON_SOUTH), Button::b);
  EXPECT_EQ(Sdl3Input::translate_button(SDL_GAMEPAD_BUTTON_START),
            Button::plus);
  EXPECT_EQ(Sdl3Input::translate_button(SDL_GAMEPAD_BUTTON_GUIDE),
            std::nullopt);
  EXPECT_EQ(Sdl3Input::translate_key(SDLK_LEFT), Key::left);
  EXPECT_EQ(Sdl3Input::translate_key(SDLK_LCTRL), Key::left_ctrl);
  EXPECT_EQ(Sdl3Input::translate_key(SDLK_F1), Key::f1);
  EXPECT_EQ(Sdl3Input::translate_key(SDLK_A), Key::a);
  EXPECT_EQ(Sdl3Input::translate_key(SDLK_F13), std::nullopt);
  EXPECT_FLOAT_EQ(Sdl3Input::normalise_axis(32767), 1.0F);
  EXPECT_FLOAT_EQ(Sdl3Input::normalise_axis(-32768), -1.0F);
}

TEST_F(Sdl3PresentationTest, InputTracksFingersKeysAndPointer) {
  auto input = Sdl3Input::create(*pump_);
  ASSERT_TRUE(input.has_value());
  static_cast<void>(input->sample());
  push_finger(SDL_EVENT_FINGER_DOWN, 5, 0.5F, 0.5F);
  SDL_Event key{};
  key.type = SDL_EVENT_KEY_DOWN;
  key.key.key = SDLK_TAB;
  key.key.down = true;
  ASSERT_TRUE(SDL_PushEvent(&key));
  SDL_Event motion{};
  motion.type = SDL_EVENT_MOUSE_MOTION;
  motion.motion.which = 1;
  motion.motion.x = 12.0F;
  motion.motion.y = 34.0F;
  ASSERT_TRUE(SDL_PushEvent(&motion));
  const InputFrame began = input->sample();
  ASSERT_EQ(began.touches.size(), 1U);
  EXPECT_EQ(began.touches[0].id, TouchId{5});
  EXPECT_EQ(began.touches[0].phase, TouchPhase::began);
  EXPECT_EQ(began.keys, (std::vector{KeyEvent{.key = Key::tab, .down = true}}));
  ASSERT_TRUE(began.pointer.has_value());
  EXPECT_FLOAT_EQ(began.pointer->position.x, 12.0F);
  EXPECT_FLOAT_EQ(began.pointer->position.y, 34.0F);
  const InputFrame held = input->sample();
  ASSERT_EQ(held.touches.size(), 1U);
  EXPECT_EQ(held.touches[0].phase, TouchPhase::stationary);
  EXPECT_TRUE(held.keys.empty());
  push_finger(SDL_EVENT_FINGER_UP, 5, 0.5F, 0.5F);
  const InputFrame ended = input->sample();
  ASSERT_EQ(ended.touches.size(), 1U);
  EXPECT_EQ(ended.touches[0].phase, TouchPhase::ended);
  EXPECT_TRUE(input->sample().touches.empty());
}

TEST_F(Sdl3PresentationTest, GpuDrawsAndCapturesOffscreen) {
  auto surface = Sdl3OffscreenSurface::create(*pump_, small);
  ASSERT_TRUE(surface.has_value());
  auto gpu = offscreen_gpu(*surface);
  if (!gpu.has_value()) {
    GTEST_SKIP() << "no SDL_GPU device under the offscreen driver";
  }
  std::println("SDL_GPU driver: {}", gpu->driver());
  auto frame = gpu->begin_frame();
  ASSERT_TRUE(frame.has_value());
  frame->clear(Colour{.red = 0.0F, .green = 1.0F, .blue = 0.0F, .alpha = 1.0F});
  ASSERT_TRUE(frame->present().has_value());
  const auto image = gpu->capture();
  ASSERT_TRUE(image.has_value());
  EXPECT_EQ(image->width(), 64U);
  EXPECT_EQ(image->height(), 48U);
  EXPECT_TRUE(
      conformance::GpuSuite::matches(*image->pixel(10, 10), {0, 255, 0, 255}));
}

TEST_F(Sdl3PresentationTest, GpuDiscardsUnpresentedFrames) {
  auto surface = Sdl3OffscreenSurface::create(*pump_, small);
  ASSERT_TRUE(surface.has_value());
  auto gpu = offscreen_gpu(*surface);
  if (!gpu.has_value()) {
    GTEST_SKIP() << "no SDL_GPU device under the offscreen driver";
  }
  {
    auto first = gpu->begin_frame();
    ASSERT_TRUE(first.has_value());
    first->clear(Colour{.red = 1.0F});
    ASSERT_TRUE(first->present().has_value());
  }
  {
    auto discarded = gpu->begin_frame();
    ASSERT_TRUE(discarded.has_value());
    discarded->clear(Colour{.blue = 1.0F});
  }
  const auto image = gpu->capture();
  ASSERT_TRUE(image.has_value());
  EXPECT_TRUE(
      conformance::GpuSuite::matches(*image->pixel(5, 5), {255, 0, 0, 255}));
}

TEST_F(Sdl3PresentationTest, GpuRunsFrameHooksAroundTheUi) {
  auto surface = Sdl3OffscreenSurface::create(*pump_, small);
  ASSERT_TRUE(surface.has_value());
  auto gpu = offscreen_gpu(*surface);
  if (!gpu.has_value()) {
    GTEST_SKIP() << "no SDL_GPU device under the offscreen driver";
  }
  std::vector<std::string_view> order;
  std::optional<Sdl3Gpu::FrameTarget> seen;
  std::optional<FrameInfo> described;
  const FrameHook after = gpu->add_frame_hook(
      FrameStage::after_ui,
      [&order](const Sdl3Gpu::FrameTarget&, const FrameInfo&) {
        order.emplace_back("after");
      });
  const FrameHook before = gpu->add_frame_hook(
      FrameStage::before_ui,
      [&order, &seen, &described](const Sdl3Gpu::FrameTarget& target,
                                  const FrameInfo& info) {
        order.emplace_back("before");
        seen = target;
        described = info;
      });
  const FrameHook second = gpu->add_frame_hook(
      FrameStage::before_ui,
      [&order](const Sdl3Gpu::FrameTarget&, const FrameInfo&) {
        order.emplace_back("second");
      });
  auto frame = gpu->begin_frame();
  ASSERT_TRUE(frame.has_value());
  frame->plan(FramePlan{.predicted_present = std::chrono::milliseconds{5},
                        .replay = true});
  frame->clear(Colour{});
  ASSERT_TRUE(frame->present().has_value());
  EXPECT_EQ(order,
            (std::vector<std::string_view>{"before", "second", "after"}));
  ASSERT_TRUE(seen.has_value());
  EXPECT_EQ(seen->width, 64U);
  EXPECT_EQ(seen->height, 48U);
  EXPECT_EQ(seen->format, gpu->target_format());
  EXPECT_NE(seen->commands, nullptr);
  ASSERT_TRUE(described.has_value());
  EXPECT_EQ(described->serial, GpuSerial{1});
  EXPECT_EQ(described->extent.width.value(), 64U);
  EXPECT_TRUE(described->replay);
  EXPECT_EQ(described->predicted_present, std::chrono::milliseconds{5});
}

TEST_F(Sdl3PresentationTest, BeforeUiContentSurvivesTheUiPass) {
  auto surface = Sdl3OffscreenSurface::create(*pump_, small);
  ASSERT_TRUE(surface.has_value());
  auto gpu = offscreen_gpu(*surface);
  if (!gpu.has_value()) {
    GTEST_SKIP() << "no SDL_GPU device under the offscreen driver";
  }
  const FrameHook under = gpu->add_frame_hook(
      FrameStage::before_ui,
      [](const Sdl3Gpu::FrameTarget& target, const FrameInfo&) {
        SDL_GPUColorTargetInfo info{};
        info.texture = target.texture;
        info.clear_color =
            SDL_FColor{.r = 0.0F, .g = 0.0F, .b = 1.0F, .a = 1.0F};
        info.load_op = SDL_GPU_LOADOP_CLEAR;
        info.store_op = SDL_GPU_STOREOP_STORE;
        SDL_EndGPURenderPass(
            SDL_BeginGPURenderPass(target.commands, &info, 1, nullptr));
      });
  {
    auto frame = gpu->begin_frame();
    ASSERT_TRUE(frame.has_value());
    frame->clear(Colour{.red = 1.0F});
    ASSERT_TRUE(frame->present().has_value());
  }
  const auto image = gpu->capture();
  ASSERT_TRUE(image.has_value());
  EXPECT_TRUE(
      conformance::GpuSuite::matches(*image->pixel(5, 5), {0, 0, 255, 255}));
}

TEST_F(Sdl3PresentationTest, PartialTextureUpdatesKeepTheRestOfTheTexture) {
  auto surface = Sdl3OffscreenSurface::create(*pump_, small);
  ASSERT_TRUE(surface.has_value());
  auto gpu = offscreen_gpu(*surface);
  if (!gpu.has_value()) {
    GTEST_SKIP() << "no SDL_GPU device under the offscreen driver";
  }
  auto texture = gpu->create_texture(
      {.width = 2, .height = 1, .format = TextureFormat::rgba8});
  ASSERT_TRUE(texture.has_value());
  const std::array red{std::byte{0xff}, std::byte{0x00}, std::byte{0x00},
                       std::byte{0xff}};
  const std::array green{std::byte{0x00}, std::byte{0xff}, std::byte{0x00},
                         std::byte{0xff}};
  ASSERT_TRUE(gpu->update_texture(
                     *texture, {.x = 0, .y = 0, .width = 1, .height = 1}, red)
                  .has_value());
  const std::uint32_t white = Vertex::rgba(255, 255, 255, 255);
  const std::array vertices{
      Vertex{.position = {0.0F, 0.0F}, .uv = {0.0F, 0.0F}, .colour = white},
      Vertex{.position = {64.0F, 0.0F}, .uv = {1.0F, 0.0F}, .colour = white},
      Vertex{.position = {64.0F, 48.0F}, .uv = {1.0F, 1.0F}, .colour = white},
      Vertex{.position = {0.0F, 48.0F}, .uv = {0.0F, 1.0F}, .colour = white},
  };
  const std::array<Index, 6> indices{0, 1, 2, 0, 2, 3};
  const std::array commands{
      DrawCommand{.texture = texture->id(),
                  .clip = ClipRect{.x = 0, .y = 0, .width = 64, .height = 48},
                  .index_offset = 0,
                  .index_count = 6,
                  .vertex_offset = 0}};
  for (int pass = 0; pass < 2; ++pass) {
    if (pass == 1) {
      ASSERT_TRUE(gpu->update_texture(*texture,
                                      {.x = 1, .y = 0, .width = 1, .height = 1},
                                      green)
                      .has_value());
    }
    auto frame = gpu->begin_frame();
    ASSERT_TRUE(frame.has_value());
    frame->clear(Colour{.alpha = 1.0F});
    auto geometry = frame->allocate_geometry(vertices.size(), indices.size());
    ASSERT_TRUE(geometry.has_value());
    std::ranges::copy(vertices, geometry->vertices.begin());
    std::ranges::copy(indices, geometry->indices.begin());
    ASSERT_TRUE(
        frame->draw(DrawData{.geometry = *geometry, .commands = commands})
            .has_value());
    ASSERT_TRUE(frame->present().has_value());
  }
  const auto image = gpu->capture();
  ASSERT_TRUE(image.has_value());
  EXPECT_TRUE(
      conformance::GpuSuite::matches(*image->pixel(16, 24), {255, 0, 0, 255}));
  EXPECT_TRUE(
      conformance::GpuSuite::matches(*image->pixel(48, 24), {0, 255, 0, 255}));
}

TEST_F(Sdl3PresentationTest, ImportedTexturesSampleWithTheirFilterAndRelease) {
  auto surface = Sdl3OffscreenSurface::create(*pump_, small);
  ASSERT_TRUE(surface.has_value());
  auto gpu = offscreen_gpu(*surface);
  if (!gpu.has_value()) {
    GTEST_SKIP() << "no SDL_GPU device under the offscreen driver";
  }
  SDL_GPUDevice* const device = gpu->device();
  SDL_GPUTextureCreateInfo info{};
  info.type = SDL_GPU_TEXTURETYPE_2D;
  info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
  info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
  info.width = 2;
  info.height = 1;
  info.layer_count_or_depth = 1;
  info.num_levels = 1;
  SDL_GPUTexture* const owned = SDL_CreateGPUTexture(device, &info);
  ASSERT_NE(owned, nullptr);
  SDL_GPUTransferBufferCreateInfo staging_info{};
  staging_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  staging_info.size = 8;
  SDL_GPUTransferBuffer* const staging =
      SDL_CreateGPUTransferBuffer(device, &staging_info);
  ASSERT_NE(staging, nullptr);
  void* const mapped = SDL_MapGPUTransferBuffer(device, staging, false);
  ASSERT_NE(mapped, nullptr);
  const std::array<std::byte, 8> texels{
      std::byte{0xff}, std::byte{0x00}, std::byte{0x00}, std::byte{0xff},
      std::byte{0x00}, std::byte{0x00}, std::byte{0xff}, std::byte{0xff}};
  std::ranges::copy(texels, static_cast<std::byte*>(mapped));
  SDL_UnmapGPUTransferBuffer(device, staging);
  SDL_GPUCommandBuffer* const upload = SDL_AcquireGPUCommandBuffer(device);
  ASSERT_NE(upload, nullptr);
  SDL_GPUCopyPass* const copy = SDL_BeginGPUCopyPass(upload);
  SDL_GPUTextureTransferInfo source{};
  source.transfer_buffer = staging;
  source.pixels_per_row = 2;
  source.rows_per_layer = 1;
  SDL_GPUTextureRegion destination{};
  destination.texture = owned;
  destination.w = 2;
  destination.h = 1;
  destination.d = 1;
  SDL_UploadToGPUTexture(copy, &source, &destination, false);
  SDL_EndGPUCopyPass(copy);
  ASSERT_TRUE(SDL_SubmitGPUCommandBuffer(upload));
  SDL_ReleaseGPUTransferBuffer(device, staging);
  const std::uint32_t white = Vertex::rgba(255, 255, 255, 255);
  const std::array vertices{
      Vertex{.position = {0.0F, 0.0F}, .uv = {0.0F, 0.0F}, .colour = white},
      Vertex{.position = {64.0F, 0.0F}, .uv = {1.0F, 0.0F}, .colour = white},
      Vertex{.position = {64.0F, 48.0F}, .uv = {1.0F, 1.0F}, .colour = white},
      Vertex{.position = {0.0F, 48.0F}, .uv = {0.0F, 1.0F}, .colour = white},
  };
  const std::array<Index, 6> indices{0, 1, 2, 0, 2, 3};
  const auto show = [&](TextureId texture) {
    const std::array commands{
        DrawCommand{.texture = texture,
                    .clip = ClipRect{.x = 0, .y = 0, .width = 64, .height = 48},
                    .index_offset = 0,
                    .index_count = 6,
                    .vertex_offset = 0}};
    auto frame = gpu->begin_frame();
    ASSERT_TRUE(frame.has_value());
    frame->clear(Colour{.alpha = 1.0F});
    auto geometry = frame->allocate_geometry(vertices.size(), indices.size());
    ASSERT_TRUE(geometry.has_value());
    std::ranges::copy(vertices, geometry->vertices.begin());
    std::ranges::copy(indices, geometry->indices.begin());
    ASSERT_TRUE(
        frame->draw(DrawData{.geometry = *geometry, .commands = commands})
            .has_value());
    ASSERT_TRUE(frame->present().has_value());
  };
  bool released = false;
  {
    auto nearest = gpu->import_texture(
        owned,
        ImportDescription{.width = 2,
                          .height = 1,
                          .format = TextureFormat::rgba8,
                          .filter = SamplerFilter::nearest,
                          .premultiplied = false},
        support::ScopedCallback{[&released]() noexcept { released = true; }});
    ASSERT_TRUE(nearest.has_value());
    auto linear =
        gpu->import_texture(owned, ImportDescription{.width = 2, .height = 1});
    ASSERT_TRUE(linear.has_value());
    EXPECT_FALSE(
        gpu->import_texture(nullptr, ImportDescription{.width = 2, .height = 1})
            .has_value());
    show(nearest->id());
    const auto sharp = gpu->capture();
    ASSERT_TRUE(sharp.has_value());
    EXPECT_TRUE(conformance::GpuSuite::matches(*sharp->pixel(30, 24),
                                               {255, 0, 0, 255}));
    EXPECT_TRUE(conformance::GpuSuite::matches(*sharp->pixel(33, 24),
                                               {0, 0, 255, 255}));
    show(linear->id());
    const auto soft = gpu->capture();
    ASSERT_TRUE(soft.has_value());
    const auto blended = *soft->pixel(30, 24);
    EXPECT_GT(blended[0], 64);
    EXPECT_GT(blended[2], 64);
    show(linear->id());
    show(linear->id());
    EXPECT_FALSE(nearest->in_use());
  }
  EXPECT_TRUE(released);
  SDL_ReleaseGPUTexture(device, owned);
}

TEST_F(Sdl3PresentationTest, Nv12PlanesImportAndConvertWhileSampling) {
  auto surface = Sdl3OffscreenSurface::create(*pump_, small);
  ASSERT_TRUE(surface.has_value());
  auto gpu = offscreen_gpu(*surface);
  if (!gpu.has_value()) {
    GTEST_SKIP() << "no SDL_GPU device under the offscreen driver";
  }
  SDL_GPUDevice* const device = gpu->device();
  const auto made = [device](SDL_GPUTextureFormat format, std::uint32_t width,
                             std::uint32_t height,
                             std::span<const std::byte> bytes) {
    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = format;
    info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    info.width = width;
    info.height = height;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    SDL_GPUTexture* const texture = SDL_CreateGPUTexture(device, &info);
    if (texture == nullptr) {
      return texture;
    }
    SDL_GPUTransferBufferCreateInfo staging_info{};
    staging_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    staging_info.size = static_cast<std::uint32_t>(bytes.size());
    SDL_GPUTransferBuffer* const staging =
        SDL_CreateGPUTransferBuffer(device, &staging_info);
    void* const mapped = SDL_MapGPUTransferBuffer(device, staging, false);
    std::ranges::copy(bytes, static_cast<std::byte*>(mapped));
    SDL_UnmapGPUTransferBuffer(device, staging);
    SDL_GPUCommandBuffer* const upload = SDL_AcquireGPUCommandBuffer(device);
    SDL_GPUCopyPass* const copy = SDL_BeginGPUCopyPass(upload);
    SDL_GPUTextureTransferInfo source{};
    source.transfer_buffer = staging;
    source.pixels_per_row = width;
    source.rows_per_layer = height;
    SDL_GPUTextureRegion destination{};
    destination.texture = texture;
    destination.w = width;
    destination.h = height;
    destination.d = 1;
    SDL_UploadToGPUTexture(copy, &source, &destination, false);
    SDL_EndGPUCopyPass(copy);
    static_cast<void>(SDL_SubmitGPUCommandBuffer(upload));
    SDL_ReleaseGPUTransferBuffer(device, staging);
    return texture;
  };
  std::array<std::byte, 8> luma{};
  std::ranges::fill(luma, std::byte{168});
  const std::array<std::byte, 4> chroma{std::byte{44}, std::byte{136},
                                        std::byte{44}, std::byte{136}};
  SDL_GPUTexture* const luma_plane =
      made(SDL_GPU_TEXTUREFORMAT_R8_UNORM, 4, 2, luma);
  SDL_GPUTexture* const chroma_plane =
      made(SDL_GPU_TEXTUREFORMAT_R8G8_UNORM, 2, 1, chroma);
  ASSERT_NE(luma_plane, nullptr);
  ASSERT_NE(chroma_plane, nullptr);
  const ImportDescription description{.width = 4,
                                      .height = 2,
                                      .format = TextureFormat::nv12,
                                      .filter = SamplerFilter::linear,
                                      .premultiplied = false,
                                      .colour_matrix = ColourMatrix::bt709,
                                      .range = ColourRange::limited};
  int released = 0;
  {
    const std::array<SDL_GPUTexture*, 2> planes{luma_plane, chroma_plane};
    const std::array<SDL_GPUTexture*, 2> missing{luma_plane, nullptr};
    EXPECT_FALSE(gpu->import_texture(missing, description).has_value());
    EXPECT_FALSE(gpu->import_texture(luma_plane, description).has_value());
    auto imported = gpu->import_texture(
        planes, description,
        support::ScopedCallback{[&released]() noexcept { ++released; }});
    ASSERT_TRUE(imported.has_value());
    ASSERT_TRUE(gpu->publications().has_value());
    const auto published = gpu->publications()->get().find(imported->id());
    ASSERT_TRUE(published.has_value());
    ASSERT_TRUE(published->planar());
    const ShapeVertex base{.colour = 0xFFFFFFFFU,
                           .kind = ShapeKind::yuv,
                           .effect = published->conversion.levels(),
                           .detail = published->conversion.weights()};
    std::array<ShapeVertex, 4> vertices{base, base, base, base};
    vertices[0].position = {0.0F, 0.0F};
    vertices[0].uv = {0.0F, 0.0F};
    vertices[1].position = {64.0F, 0.0F};
    vertices[1].uv = {1.0F, 0.0F};
    vertices[2].position = {64.0F, 48.0F};
    vertices[2].uv = {1.0F, 1.0F};
    vertices[3].position = {0.0F, 48.0F};
    vertices[3].uv = {0.0F, 1.0F};
    const std::array<Index, 6> indices{0, 1, 2, 0, 2, 3};
    const std::array commands{
        DrawCommand{.texture = imported->id(),
                    .clip = ClipRect{.x = 0, .y = 0, .width = 64, .height = 48},
                    .index_offset = 0,
                    .index_count = 6,
                    .vertex_offset = 0,
                    .detail = published->chroma}};
    imported->publish();
    auto frame = gpu->begin_frame();
    ASSERT_TRUE(frame.has_value());
    frame->clear(Colour{.alpha = 1.0F});
    auto geometry = frame->allocate_shapes(vertices.size(), indices.size());
    ASSERT_TRUE(geometry.has_value());
    std::ranges::copy(vertices, geometry->vertices.begin());
    std::ranges::copy(indices, geometry->indices.begin());
    ASSERT_TRUE(
        frame->draw(ShapeDrawData{.geometry = *geometry, .commands = commands})
            .has_value());
    ASSERT_TRUE(frame->present().has_value());
    const auto image = gpu->capture();
    ASSERT_TRUE(image.has_value());
    const auto yellow = *image->pixel(32, 24);
    EXPECT_NEAR(yellow[0], 191, 3);
    EXPECT_NEAR(yellow[1], 191, 3);
    EXPECT_NEAR(yellow[2], 0, 3);
  }
  for (int frame = 0; frame < 4 && released == 0; ++frame) {
    auto idle = gpu->begin_frame();
    ASSERT_TRUE(idle.has_value());
    ASSERT_TRUE(idle->present().has_value());
  }
  EXPECT_EQ(released, 1);
  SDL_ReleaseGPUTexture(device, luma_plane);
  SDL_ReleaseGPUTexture(device, chroma_plane);
}

TEST_F(Sdl3PresentationTest, Bgra8TexturesSampleInTheirOwnChannelOrder) {
  auto surface = Sdl3OffscreenSurface::create(*pump_, small);
  ASSERT_TRUE(surface.has_value());
  auto gpu = offscreen_gpu(*surface);
  if (!gpu.has_value()) {
    GTEST_SKIP() << "no SDL_GPU device under the offscreen driver";
  }
  for (const TextureFormat format :
       {TextureFormat::bgra8, TextureFormat::rgb565}) {
    auto texture =
        gpu->create_texture({.width = 1, .height = 1, .format = format});
    ASSERT_TRUE(texture.has_value());
    const std::array<std::byte, 4> bgra{std::byte{0xff}, std::byte{0x00},
                                        std::byte{0x00}, std::byte{0xff}};
    const std::array<std::byte, 2> rgb565{std::byte{0x1f}, std::byte{0x00}};
    ASSERT_TRUE(
        gpu->update_texture(*texture, {.x = 0, .y = 0, .width = 1, .height = 1},
                            format == TextureFormat::bgra8 ? PixelRows{bgra}
                                                           : PixelRows{rgb565})
            .has_value());
    const std::uint32_t white = Vertex::rgba(255, 255, 255, 255);
    const std::array vertices{
        Vertex{.position = {0.0F, 0.0F}, .uv = {0.0F, 0.0F}, .colour = white},
        Vertex{.position = {64.0F, 0.0F}, .uv = {1.0F, 0.0F}, .colour = white},
        Vertex{.position = {64.0F, 48.0F}, .uv = {1.0F, 1.0F}, .colour = white},
        Vertex{.position = {0.0F, 48.0F}, .uv = {0.0F, 1.0F}, .colour = white},
    };
    const std::array<Index, 6> indices{0, 1, 2, 0, 2, 3};
    const std::array commands{
        DrawCommand{.texture = texture->id(),
                    .clip = ClipRect{.x = 0, .y = 0, .width = 64, .height = 48},
                    .index_offset = 0,
                    .index_count = 6,
                    .vertex_offset = 0}};
    auto frame = gpu->begin_frame();
    ASSERT_TRUE(frame.has_value());
    frame->clear(Colour{.alpha = 1.0F});
    auto geometry = frame->allocate_geometry(vertices.size(), indices.size());
    ASSERT_TRUE(geometry.has_value());
    std::ranges::copy(vertices, geometry->vertices.begin());
    std::ranges::copy(indices, geometry->indices.begin());
    ASSERT_TRUE(
        frame->draw(DrawData{.geometry = *geometry, .commands = commands})
            .has_value());
    ASSERT_TRUE(frame->present().has_value());
    const auto image = gpu->capture();
    ASSERT_TRUE(image.has_value());
    EXPECT_TRUE(
        conformance::GpuSuite::matches(*image->pixel(10, 10), {0, 0, 255, 255}))
        << static_cast<int>(format);
  }
}

TEST_F(Sdl3PresentationTest, TexturesMayOutliveTheGpu) {
  auto surface = Sdl3OffscreenSurface::create(*pump_, small);
  ASSERT_TRUE(surface.has_value());
  std::optional<Texture> texture;
  std::optional<GpuFrame> frame;
  {
    auto gpu = offscreen_gpu(*surface);
    if (!gpu.has_value()) {
      GTEST_SKIP() << "no SDL_GPU device under the offscreen driver";
    }
    auto created = gpu->create_texture(
        {.width = 4, .height = 4, .format = TextureFormat::alpha8});
    ASSERT_TRUE(created.has_value());
    const std::vector<std::byte> pixels(16, std::byte{0x7f});
    ASSERT_TRUE(gpu->update_texture(*created,
                                    {.x = 0, .y = 0, .width = 4, .height = 4},
                                    pixels)
                    .has_value());
    EXPECT_EQ(gpu->live_textures(), 1U);
    texture.emplace(std::move(*created));
    auto begun = gpu->begin_frame();
    ASSERT_TRUE(begun.has_value());
    frame.emplace(std::move(*begun));
  }
  EXPECT_TRUE(texture->valid());
  const auto presented = frame->present();
  ASSERT_FALSE(presented.has_value());
  EXPECT_EQ(presented.error().code(), ProviderErrorCode::platform_failure);
  texture.reset();
}

TEST_F(Sdl3PresentationTest, OffscreenFactoryPassesConformance) {
  auto factory = Sdl3ProviderFactory::create(Providers{.assets = fixtures_},
                                             OffscreenMode{.size = small});
  ASSERT_TRUE(factory.has_value());
  EXPECT_TRUE(factory->supplies(Capability::surface));
  EXPECT_TRUE(factory->supplies(Capability::input));
  if (!factory->supplies(Capability::gpu)) {
    GTEST_SKIP() << "no SDL_GPU device under the offscreen driver";
  }
  Sdl3ProviderFactory moved{std::move(*factory)};
  const conformance::Report report = conformance::ConformanceSuite::run(moved);
  for (const conformance::CheckResult& result : report) {
    if (!result.passed) {
      std::println("FAILED {}: {}", result.name, result.detail);
    }
  }
  EXPECT_TRUE(report.passed());
  const auto capture =
      std::ranges::find_if(report, [](const conformance::CheckResult& result) {
        return result.name == "gpu.capture";
      });
  ASSERT_NE(capture, report.end());
  EXPECT_TRUE(capture->passed);
  EXPECT_TRUE(moved.stock_gpu().has_value());
}

TEST_F(Sdl3PresentationTest, NoSurfaceModeLeavesGpuAbsent) {
  auto factory = Sdl3ProviderFactory::create(Providers{.assets = fixtures_});
  ASSERT_TRUE(factory.has_value());
  EXPECT_FALSE(factory->supplies(Capability::surface));
  EXPECT_FALSE(factory->supplies(Capability::gpu));
  EXPECT_FALSE(factory->stock_gpu().has_value());
}

}
