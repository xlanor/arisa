#include <animation.hpp>
#include <sample_model.hpp>
#include <sample_screen.hpp>
#include <sdl_sample_renderer.hpp>

#include <arisa/composition/element_key.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/provider/gpu/image.hpp>
#include <arisa/provider/sdl3/sdl3_factory.hpp>
#include <arisa/provider/sdl3/sdl3_surface_mode.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/root.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <utility>

namespace arisa::examples {

class SampleTest : public ::testing::Test {
 protected:
  static constexpr provider::SurfaceSize size{
      .width = provider::SurfaceWidth{1280},
      .height = provider::SurfaceHeight{720}};
  static constexpr std::chrono::milliseconds settle{150};
  static constexpr std::chrono::milliseconds idle_window{300};
  static constexpr float corner_inset = 32.0F;
  static constexpr composition::Rect open_stage{
      .x = 1000.0F, .y = 80.0F, .width = 240.0F, .height = 560.0F};

  void SetUp() override {
    auto factory = provider::sdl3::Sdl3ProviderFactory::create(
        provider::Providers{}, provider::sdl3::OffscreenMode{.size = size});
    if (!factory.has_value()) {
      GTEST_SKIP() << "no SDL offscreen factory: "
                   << factory.error().description();
    }
    factory_.emplace(std::move(*factory));
    const auto gpu = factory_->stock_gpu();
    if (!gpu.has_value()) {
      GTEST_SKIP() << "no SDL_GPU device under the offscreen driver";
    }
    auto renderer = SdlSampleRenderer::create(gpu->get(), animation_);
    ASSERT_TRUE(renderer.has_value()) << renderer.error();
    renderer_.emplace(std::move(*renderer));
    model_.emplace(animation_, *renderer_);
    auto application =
        runtime::Application::create(*factory_, SampleScreen{*model_});
    ASSERT_TRUE(application.has_value()) << application.error().description();
    application_.emplace(std::move(*application));
  }

  auto now() -> std::chrono::nanoseconds {
    return factory_->clock().now().since_origin();
  }

  auto run_for(std::chrono::milliseconds duration) -> std::uint32_t {
    const std::chrono::nanoseconds until = now() + duration;
    std::uint32_t presented = 0;
    const auto ran = application_->run_until([&](runtime::Root&) {
      presented += application_->last_frame().presented ? 1U : 0U;
      return now() >= until;
    });
    EXPECT_TRUE(ran.has_value());
    return presented;
  }

  auto capture() -> provider::Image {
    auto image = factory_->gpu().capture();
    EXPECT_TRUE(image.has_value());
    return std::move(*image);
  }

  auto bounds(std::string_view key) const -> composition::Rect {
    for (const auto& node : application_->introspection().nodes()) {
      if (node.key.find(key) != std::string::npos) {
        return node.bounds;
      }
    }
    ADD_FAILURE() << "no node keyed " << key;
    return {};
  }

  static auto inset(composition::Rect rect, float by) -> composition::Rect {
    return composition::Rect{
        .x = rect.x + by,
        .y = rect.y + by,
        .width = std::max(rect.width - (2.0F * by), 1.0F),
        .height = std::max(rect.height - (2.0F * by), 1.0F)};
  }

  static auto difference(const provider::Image& before,
                         const provider::Image& after, composition::Rect area)
      -> double {
    const auto left = static_cast<std::uint32_t>(std::max(area.x, 0.0F));
    const auto top = static_cast<std::uint32_t>(std::max(area.y, 0.0F));
    const std::uint32_t right = std::min(
        before.width(), static_cast<std::uint32_t>(area.x + area.width));
    const std::uint32_t bottom = std::min(
        before.height(), static_cast<std::uint32_t>(area.y + area.height));
    std::uint64_t total = 0;
    std::uint64_t samples = 0;
    for (std::uint32_t y = top; y < bottom; ++y) {
      for (std::uint32_t x = left; x < right; ++x) {
        const auto first = before.pixel(x, y);
        const auto second = after.pixel(x, y);
        for (std::size_t channel = 0; channel < 3; ++channel) {
          total += static_cast<std::uint64_t>(
              std::abs(static_cast<int>((*first)[channel]) -
                       static_cast<int>((*second)[channel])));
        }
        samples += 3;
      }
    }
    return samples == 0
               ? 0.0
               : static_cast<double>(total) / static_cast<double>(samples);
  }

  std::optional<provider::sdl3::Sdl3ProviderFactory> factory_;
  Animation animation_;
  std::optional<SdlSampleRenderer> renderer_;
  std::optional<SampleModel> model_;
  std::optional<runtime::Application> application_;
};

TEST_F(SampleTest, TheHookAnimatesUnderAStillUiAndGlassRefractsTheSurface) {
  run_for(settle);
  const provider::Image before = capture();
  const std::uint32_t presented = run_for(idle_window);
  const provider::Image after = capture();
  const double stage = difference(before, after, open_stage);
  const double sidebar = difference(
      before, after, inset(bounds(SampleScreen::sidebar), corner_inset));
  const double card_glass = difference(
      before, after, inset(bounds(SampleScreen::card_caption), 4.0F));
  const double picture = difference(
      before, after,
      composition::Rect{.x = bounds(SampleScreen::picture_caption).x,
                        .y = bounds(SampleScreen::picture_caption).y - 100.0F,
                        .width = bounds(SampleScreen::picture_caption).width,
                        .height = 80.0F});
  std::println(
      "sample over {} ms: {} presents, stage {:.2f}, sidebar {:.2f}, "
      "card glass {:.2f}, picture {:.2f}",
      idle_window.count(), presented, stage, sidebar, card_glass, picture);
  EXPECT_GT(presented, 2U);
  EXPECT_GT(stage, 1.0);
  EXPECT_EQ(sidebar, 0.0);
  EXPECT_GT(card_glass, 0.05);
  EXPECT_GT(picture, 1.0);
}

TEST_F(SampleTest, PausingStopsPresentingAndPlayingResumes) {
  run_for(settle);
  ASSERT_TRUE(application_->root().activate(
      composition::ElementKey{SampleScreen::play_button}));
  run_for(settle);
  EXPECT_FALSE(model_->playing().get());
  const provider::Image paused = capture();
  EXPECT_EQ(run_for(idle_window), 0U);
  EXPECT_EQ(difference(paused, capture(), open_stage), 0.0);
  ASSERT_TRUE(application_->root().activate(
      composition::ElementKey{SampleScreen::play_button}));
  EXPECT_GT(run_for(idle_window), 2U);
  EXPECT_GT(difference(paused, capture(), open_stage), 1.0);
}

TEST_F(SampleTest, FullScreenHidesTheUiAndReleasingRestoresIt) {
  run_for(settle);
  const provider::Image shared = capture();
  const std::uint32_t shared_quads = application_->last_frame().quads;
  const composition::Rect sidebar =
      inset(bounds(SampleScreen::sidebar), corner_inset);
  const auto focused = application_->focused();
  model_->set_full_screen(true);
  run_for(settle);
  EXPECT_TRUE(application_->last_frame().exclusive);
  EXPECT_EQ(application_->last_frame().quads, 0U);
  const provider::Image exclusive = capture();
  EXPECT_GT(difference(shared, exclusive, sidebar), 10.0);
  model_->set_full_screen(false);
  run_for(settle);
  EXPECT_FALSE(application_->last_frame().exclusive);
  EXPECT_EQ(application_->focused(), focused);
  const provider::Image restored = capture();
  EXPECT_EQ(difference(shared, restored, sidebar), 0.0);
  std::println("sample quads shared {} exclusive 0", shared_quads);
  EXPECT_GT(shared_quads, 0U);
}

}
