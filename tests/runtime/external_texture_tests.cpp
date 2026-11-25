#include <arisa/components/image.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/provider/gpu/draw_data.hpp>
#include <arisa/provider/gpu/external_texture.hpp>
#include <arisa/provider/gpu/image.hpp>
#include <arisa/provider/gpu/texture.hpp>
#include <arisa/provider/gpu/yuv_conversion.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/headless/null_gpu.hpp>
#include <arisa/provider/headless/software_raster.hpp>
#include <arisa/provider/surface/surface_scale.hpp>
#include <arisa/provider/surface/surface_size.hpp>
#include <arisa/provider/threading/thread_role.hpp>
#include <arisa/provider/threading/thread_spec.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/application_options.hpp>
#include <arisa/theme/colour.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <stop_token>
#include <utility>
#include <vector>

namespace arisa::runtime {

using namespace std::chrono_literals;

class ExternalTextureTest : public ::testing::Test {
 protected:
  using NullGpu = provider::headless::NullGpu;
  using Canvas = provider::headless::SoftwareRaster::Canvas;
  using Factory = provider::headless::HeadlessProviderFactory;

  enum class Scene : std::uint8_t { bare, glass, moving };

  enum class Planes : std::uint8_t { rgba, nv12 };

  static constexpr std::uint32_t width = 240;
  static constexpr std::uint32_t height = 120;
  static constexpr std::uint32_t video_width = 64;
  static constexpr std::uint32_t video_height = 48;
  static constexpr provider::ClipRect video{
      .x = 88, .y = 36, .width = video_width, .height = video_height};
  static constexpr int settle_limit = 60;

  class Stage final : public composition::Component {
   public:
    Stage(provider::TextureId texture, Scene scene,
          const composition::Observable<bool>& moved) noexcept
        : texture_(texture), scene_(scene), moved_(moved) {}

    auto operator==(const Stage& other) const -> bool {
      return texture_ == other.texture_ && scene_ == other.scene_ &&
             &moved_.get() == &other.moved_.get();
    }

    auto body(composition::Context& context) const
        -> composition::Element override {
      const bool moved = context.read(moved_.get());
      composition::Element shown = components::Image::External(texture_).frame(
          {.width = static_cast<float>(video_width),
           .height = static_cast<float>(video_height)});
      composition::Element marker =
          components::VStack()
              .frame({.width = 16.0F, .height = 16.0F})
              .background(theme::Colour::hex(0x2050C0))
              .offset(moved ? -60.0F : -100.0F, -40.0F);
      if (scene_ == Scene::glass) {
        return components::ZStack(std::move(shown),
                                  components::VStack()
                                      .frame({.width = 96.0F, .height = 24.0F})
                                      .glass_effect(),
                                  components::VStack()
                                      .frame({.width = 24.0F, .height = 24.0F})
                                      .glass_effect()
                                      .offset(-100.0F, -40.0F));
      }
      return components::ZStack(std::move(shown), std::move(marker));
    }

   private:
    provider::TextureId texture_;
    Scene scene_;
    std::reference_wrapper<const composition::Observable<bool>> moved_;
  };

  class Player {
   public:
    Player(Scene scene, std::uint32_t images, bool partial,
           bool full_walk = false, Planes planes = Planes::rgba)
        : image_(std::make_shared<Canvas>(video_width, video_height)),
          chroma_(std::make_shared<Canvas>(video_width / 2, video_height / 2)),
          planes_(planes) {
      const provider::SurfaceSize extent{
          .width = provider::SurfaceWidth{width},
          .height = provider::SurfaceHeight{height}};
      providers_.surface().resize(extent);
      providers_.surface().set_scale(provider::SurfaceScale::unit());
      providers_.gpu().rasterize(extent);
      providers_.gpu().swapchain(images, partial);
      shade(0);
      const std::array<std::shared_ptr<Canvas>, 2> both{image_, chroma_};
      auto imported =
          planes == Planes::nv12
              ? providers_.gpu().import_texture(
                    both,
                    provider::ImportDescription{
                        .width = video_width,
                        .height = video_height,
                        .format = provider::TextureFormat::nv12,
                        .filter = provider::SamplerFilter::linear,
                        .premultiplied = false,
                        .colour_matrix = provider::ColourMatrix::bt709,
                        .range = provider::ColourRange::limited})
              : providers_.gpu().import_texture(
                    image_, provider::ImportDescription{
                                .width = video_width, .height = video_height});
      EXPECT_TRUE(imported.has_value());
      texture_.emplace(std::move(*imported));
      auto created =
          Application::create(providers_, Stage{texture_->id(), scene, moved_},
                              ApplicationOptions{.executors = std::nullopt,
                                                 .fonts = {},
                                                 .atlas = {},
                                                 .interaction = {},
                                                 .appearance = std::nullopt,
                                                 .debug_full_walk = full_walk});
      EXPECT_TRUE(created.has_value());
      application_.emplace(std::move(*created));
    }

    void shade(int frame) {
      const float level = static_cast<float>(frame % 7) / 6.0F;
      if (planes_ == Planes::nv12) {
        image_->fill(provider::Colour{.red = luma_code(level) / 255.0F,
                                      .green = 0.0F,
                                      .blue = 0.0F,
                                      .alpha = 1.0F});
        chroma_->fill(provider::Colour{.red = blue_code / 255.0F,
                                       .green = red_code / 255.0F,
                                       .blue = 0.0F,
                                       .alpha = 1.0F});
        return;
      }
      image_->fill(provider::Colour{
          .red = level, .green = 1.0F - level, .blue = 0.25F, .alpha = 1.0F});
    }

    void publish(int frame) {
      shade(frame);
      texture_->publish();
    }

    void frame() {
      ASSERT_TRUE(providers_.clock().advance(16ms).has_value());
      ASSERT_TRUE(application_->frame().has_value());
    }

    void settle() {
      for (int index = 0; index < settle_limit; ++index) {
        frame();
        if (!application_->last_frame().presented) {
          return;
        }
      }
      ADD_FAILURE() << "the player never went idle";
    }

    void move() {
      motion::with_animation(motion::Animation::linear(1.0),
                             [this] { moved_.set(true); });
    }

    auto gpu() -> NullGpu& { return providers_.gpu(); }

    auto providers() -> Factory& { return providers_; }

    auto application() -> Application& { return *application_; }

    auto texture() -> NullGpu::Imported& { return *texture_; }

    auto sampled() -> bool {
      const auto shapes = providers_.gpu().last_shapes();
      return std::ranges::any_of(shapes.commands,
                                 [this](const provider::DrawCommand& command) {
                                   return command.texture == texture_->id();
                                 });
    }

   private:
    Factory providers_;
    composition::Observable<bool> moved_{false};
    std::shared_ptr<Canvas> image_;
    std::shared_ptr<Canvas> chroma_;
    Planes planes_;
    std::optional<NullGpu::Imported> texture_;
    std::optional<Application> application_;
  };

  static constexpr float blue_code = 90.0F;
  static constexpr float red_code = 170.0F;

  static auto luma_code(float level) noexcept -> float {
    return std::round(16.0F + (level * 219.0F));
  }

  static auto colour_at(const provider::Image& image, std::uint32_t x,
                        std::uint32_t y) -> std::array<int, 3> {
    const auto found = image.pixel(x, y);
    EXPECT_TRUE(found.has_value());
    if (!found.has_value()) {
      return {};
    }
    return {(*found)[0], (*found)[1], (*found)[2]};
  }

  static auto contains(provider::ClipRect outer, provider::ClipRect inner)
      -> bool {
    return outer.x <= inner.x && outer.y <= inner.y &&
           outer.x + outer.width >= inner.x + inner.width &&
           outer.y + outer.height >= inner.y + inner.height;
  }
};

TEST_F(ExternalTextureTest, APublishIsAPaintOnlyFrameDamagingOnlyTheVideo) {
  Player player{Scene::bare, 1, true};
  player.settle();
  player.frame();
  EXPECT_FALSE(player.application().last_frame().presented);
  const std::size_t uploads = player.gpu().uploads();
  const std::size_t bytes = player.gpu().uploaded_bytes();
  const std::size_t imports = player.gpu().imports();
  const std::size_t presented = player.gpu().frames_presented();
  const std::size_t passes = player.gpu().passes();
  const auto before = player.gpu().capture();
  ASSERT_TRUE(before.has_value());
  player.publish(3);
  player.frame();
  const FrameTimings& timings = player.application().last_frame();
  EXPECT_TRUE(timings.presented);
  EXPECT_EQ(timings.evaluations, 0U);
  EXPECT_FALSE(timings.laid_out);
  EXPECT_EQ(player.gpu().frames_presented() - presented, 1U);
  EXPECT_EQ(player.gpu().uploads(), uploads);
  EXPECT_EQ(player.gpu().uploaded_bytes(), bytes);
  EXPECT_EQ(player.gpu().imports(), imports);
  EXPECT_EQ(player.gpu().passes(), passes);
  EXPECT_EQ(timings.uploaded_vertices, 0U);
  const auto damage = player.gpu().last_damage();
  ASSERT_TRUE(damage.has_value());
  EXPECT_TRUE(contains(*damage, video));
  EXPECT_TRUE(contains(provider::ClipRect{.x = video.x - 2,
                                          .y = video.y - 2,
                                          .width = video.width + 4,
                                          .height = video.height + 4},
                       *damage));
  EXPECT_TRUE(player.sampled());
  const auto after = player.gpu().capture();
  ASSERT_TRUE(after.has_value());
  EXPECT_NE(colour_at(*after, 120, 60), colour_at(*before, 120, 60));
  EXPECT_EQ(colour_at(*after, 20, 20), colour_at(*before, 20, 20));
  player.frame();
  EXPECT_FALSE(player.application().last_frame().presented);
}

TEST_F(ExternalTextureTest,
       OverlappingGlassRepaintsWithTheVideoAndAFarOneDoesNot) {
  Player player{Scene::glass, 1, true};
  player.settle();
  player.publish(2);
  player.frame();
  ASSERT_TRUE(player.application().last_frame().presented);
  EXPECT_EQ(player.application().last_frame().evaluations, 0U);
  const auto damage = player.gpu().last_damage();
  ASSERT_TRUE(damage.has_value());
  EXPECT_TRUE(contains(*damage, video));
  EXPECT_LT(damage->x, video.x);
  EXPECT_GT(damage->x, 40U);
}

TEST_F(ExternalTextureTest, InUseFollowsTheFramesThatSampledTheTexture) {
  Player player{Scene::bare, 1, true};
  player.settle();
  EXPECT_FALSE(player.texture().in_use());
  player.gpu().hold_completion(2);
  player.publish(1);
  player.frame();
  EXPECT_TRUE(player.texture().in_use());
  player.gpu().hold_completion(0);
  EXPECT_FALSE(player.texture().in_use());
}

TEST_F(ExternalTextureTest, ReplayedFramesKeepSamplingTheExternalTexture) {
  Player player{Scene::moving, 1, false};
  player.settle();
  player.move();
  player.frame();
  player.frame();
  const std::uint64_t replayed = player.application().replayed_frames();
  for (int index = 0; index < 20; ++index) {
    ASSERT_TRUE(player.providers().clock().advance(16ms).has_value());
    player.application().step_animations();
  }
  EXPECT_GT(player.application().replayed_frames(), replayed);
  EXPECT_TRUE(player.sampled());
}

TEST_F(ExternalTextureTest,
       ImportAndPublishFromAnotherThreadWhileTheWorkerReplays) {
  Player player{Scene::moving, 1, false};
  player.settle();
  player.move();
  player.frame();
  player.frame();
  std::atomic<int> imported{0};
  std::atomic<int> failed{0};
  const std::uint64_t replayed = player.application().replayed_frames();
  {
    auto decoder = player.providers().threading().start(
        provider::ThreadSpec{.name = "test-decoder",
                             .role = provider::ThreadRole::app},
        [&](const std::stop_token& stop) {
          while (!stop.stop_requested()) {
            auto ring = player.gpu().import_texture(
                std::make_shared<Canvas>(4U, 4U),
                provider::ImportDescription{.width = 4, .height = 4});
            if (!ring.has_value()) {
              failed.fetch_add(1);
              continue;
            }
            ring->publish();
            static_cast<void>(ring->in_use());
            player.texture().publish();
            imported.fetch_add(1);
          }
        });
    ASSERT_TRUE(decoder.has_value());
    for (int index = 0; index < 30; ++index) {
      ASSERT_TRUE(player.providers().clock().advance(16ms).has_value());
      player.application().step_animations();
      if (index % 5 == 0) {
        player.frame();
      }
    }
  }
  EXPECT_GT(imported.load(), 0);
  EXPECT_EQ(failed.load(), 0);
  EXPECT_GT(player.application().replayed_frames(), replayed);
  player.settle();
  ASSERT_TRUE(player.gpu().publications().has_value());
  EXPECT_EQ(player.gpu().publications()->get().live(), 1U);
}

TEST_F(ExternalTextureTest,
       RetainedPaintMatchesTheFullWalkWhilePublishingEveryFrame) {
  Player retained{Scene::moving, 1, false, false};
  Player walked{Scene::moving, 1, false, true};
  std::map<std::uint64_t, std::uint64_t> pairs;
  int mismatched = 0;
  int compared = 0;
  for (int frame = 0; frame < 40; ++frame) {
    if (frame == 12) {
      retained.move();
      walked.move();
    }
    retained.publish(frame);
    walked.publish(frame);
    retained.frame();
    walked.frame();
    if (!retained.application().last_frame().presented ||
        !walked.application().last_frame().presented) {
      mismatched += retained.application().last_frame().presented !=
                            walked.application().last_frame().presented
                        ? 1
                        : 0;
      continue;
    }
    ++compared;
    const auto seen = retained.gpu().last_shapes();
    const auto expected = walked.gpu().last_shapes();
    bool same = seen.commands.size() == expected.commands.size() &&
                seen.indices == expected.indices &&
                seen.vertices == expected.vertices &&
                retained.gpu().last_damage() == walked.gpu().last_damage();
    for (std::size_t index = 0; same && index < seen.commands.size(); ++index) {
      provider::DrawCommand left = seen.commands[index];
      const provider::DrawCommand& right = expected.commands[index];
      const auto paired =
          pairs.try_emplace(left.texture.value(), right.texture.value());
      same = paired.first->second == right.texture.value();
      left.texture = right.texture;
      left.detail = right.detail;
      same = same && left == right;
    }
    mismatched += same ? 0 : 1;
  }
  EXPECT_EQ(mismatched, 0);
  EXPECT_GT(compared, 30);
}

TEST_F(ExternalTextureTest,
       PartialRepaintMatchesTheWholeRepaintWhilePublishing) {
  Player partial{Scene::glass, 2, true};
  Player whole{Scene::glass, 1, false};
  int mismatched = 0;
  int partial_frames = 0;
  for (int frame = 0; frame < 24; ++frame) {
    partial.publish(frame);
    whole.publish(frame);
    partial.frame();
    whole.frame();
    partial_frames += partial.gpu().last_damage().has_value() ? 1 : 0;
    const auto seen = partial.gpu().capture();
    const auto expected = whole.gpu().capture();
    ASSERT_TRUE(seen.has_value());
    ASSERT_TRUE(expected.has_value());
    if (!std::ranges::equal(seen->pixels(), expected->pixels())) {
      ++mismatched;
    }
  }
  EXPECT_EQ(mismatched, 0);
  EXPECT_GT(partial_frames, 10);
}

TEST_F(ExternalTextureTest,
       AnNv12PublishIsAPaintOnlyFrameDamagingOnlyTheVideo) {
  Player player{Scene::bare, 1, true, false, Planes::nv12};
  player.settle();
  player.frame();
  EXPECT_FALSE(player.application().last_frame().presented);
  const std::size_t uploads = player.gpu().uploads();
  const std::size_t bytes = player.gpu().uploaded_bytes();
  const std::size_t imports = player.gpu().imports();
  player.publish(3);
  player.frame();
  const FrameTimings& timings = player.application().last_frame();
  EXPECT_TRUE(timings.presented);
  EXPECT_EQ(timings.evaluations, 0U);
  EXPECT_FALSE(timings.laid_out);
  EXPECT_EQ(player.gpu().uploads(), uploads);
  EXPECT_EQ(player.gpu().uploaded_bytes(), bytes);
  EXPECT_EQ(player.gpu().imports(), imports);
  EXPECT_EQ(timings.uploaded_vertices, 0U);
  const auto damage = player.gpu().last_damage();
  ASSERT_TRUE(damage.has_value());
  EXPECT_TRUE(contains(*damage, video));
  EXPECT_TRUE(contains(provider::ClipRect{.x = video.x - 2,
                                          .y = video.y - 2,
                                          .width = video.width + 4,
                                          .height = video.height + 4},
                       *damage));
  const auto shapes = player.gpu().last_shapes();
  const auto sampled = std::ranges::find_if(
      shapes.commands, [&player](const provider::DrawCommand& command) {
        return command.texture == player.texture().id();
      });
  ASSERT_NE(sampled, shapes.commands.end());
  const auto published =
      player.gpu().publications()->get().find(player.texture().id());
  ASSERT_TRUE(published.has_value());
  EXPECT_EQ(sampled->detail, published->chroma);
  const auto after = player.gpu().capture();
  ASSERT_TRUE(after.has_value());
  const provider::YuvConversion conversion{provider::ColourMatrix::bt709,
                                           provider::ColourRange::limited};
  const provider::YuvConversion::Rgb expected = conversion.rgb(
      luma_code(3.0F / 6.0F) / 255.0F, blue_code / 255.0F, red_code / 255.0F);
  const std::array<int, 3> seen = colour_at(*after, 120, 60);
  for (std::size_t channel = 0; channel < 3; ++channel) {
    EXPECT_NEAR(seen[channel], expected[channel] * 255.0F, 2.5F) << channel;
  }
  player.frame();
  EXPECT_FALSE(player.application().last_frame().presented);
}

TEST_F(ExternalTextureTest, GlassOverAnNv12VideoSeesConvertedColour) {
  Player player{Scene::glass, 1, true, false, Planes::nv12};
  player.settle();
  player.publish(2);
  player.frame();
  ASSERT_TRUE(player.application().last_frame().presented);
  EXPECT_EQ(player.application().last_frame().evaluations, 0U);
  const auto damage = player.gpu().last_damage();
  ASSERT_TRUE(damage.has_value());
  EXPECT_TRUE(contains(*damage, video));
  EXPECT_LT(damage->x, video.x);
  const auto captured = player.gpu().capture();
  ASSERT_TRUE(captured.has_value());
  const std::array<int, 3> under_glass = colour_at(*captured, 120, 60);
  EXPECT_GT(under_glass[0], under_glass[2] + 20);
  EXPECT_GT(under_glass[0], under_glass[1]);
}

TEST_F(ExternalTextureTest,
       RetainedPaintMatchesTheFullWalkWhilePublishingNv12) {
  Player retained{Scene::moving, 1, false, false, Planes::nv12};
  Player walked{Scene::moving, 1, false, true, Planes::nv12};
  int mismatched = 0;
  int compared = 0;
  for (int frame = 0; frame < 40; ++frame) {
    if (frame == 12) {
      retained.move();
      walked.move();
    }
    retained.publish(frame);
    walked.publish(frame);
    retained.frame();
    walked.frame();
    if (!retained.application().last_frame().presented ||
        !walked.application().last_frame().presented) {
      mismatched += retained.application().last_frame().presented !=
                            walked.application().last_frame().presented
                        ? 1
                        : 0;
      continue;
    }
    ++compared;
    const auto seen = retained.gpu().last_shapes();
    const auto expected = walked.gpu().last_shapes();
    bool same = seen.commands.size() == expected.commands.size() &&
                seen.indices == expected.indices &&
                seen.vertices == expected.vertices &&
                retained.gpu().last_damage() == walked.gpu().last_damage();
    for (std::size_t index = 0; same && index < seen.commands.size(); ++index) {
      provider::DrawCommand left = seen.commands[index];
      const provider::DrawCommand& right = expected.commands[index];
      const bool video_left = left.texture == retained.texture().id();
      const bool video_right = right.texture == walked.texture().id();
      same = video_left == video_right;
      left.texture = right.texture;
      left.detail = right.detail;
      same = same && left == right;
    }
    const auto left_pixels = retained.gpu().capture();
    const auto right_pixels = walked.gpu().capture();
    same = same && left_pixels.has_value() && right_pixels.has_value() &&
           std::ranges::equal(left_pixels->pixels(), right_pixels->pixels());
    mismatched += same ? 0 : 1;
  }
  EXPECT_EQ(mismatched, 0);
  EXPECT_GT(compared, 30);
}

}
