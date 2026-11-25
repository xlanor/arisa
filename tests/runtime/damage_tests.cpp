#include <arisa/components/scroll_view.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/controls/progress_view.hpp>
#include <arisa/provider/gpu/draw_data.hpp>
#include <arisa/provider/gpu/image.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/headless/null_gpu.hpp>
#include <arisa/provider/input/provider.hpp>
#include <arisa/provider/surface/surface_scale.hpp>
#include <arisa/provider/surface/surface_size.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/shape.hpp>

#include <scroll_fixture.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace arisa::runtime {

using composition::Element;

class DamageTest : public ScrollFixture {
 protected:
  static constexpr provider::SurfaceSize extent{
      .width = provider::SurfaceWidth{320},
      .height = provider::SurfaceHeight{180}};

  static auto launch(provider::headless::HeadlessProviderFactory& providers,
                     Element content, std::uint32_t images, bool partial)
      -> Application {
    providers.surface().resize(extent);
    providers.surface().set_scale(provider::SurfaceScale::unit());
    providers.gpu().swapchain(images, partial);
    providers.gpu().rasterize(extent);
    return start(providers, std::move(content));
  }

  static void advance(
      provider::headless::HeadlessProviderFactory& providers,
      Application& application,
      std::optional<provider::InputFrame> input = std::nullopt) {
    if (input.has_value()) {
      providers.input().push(std::move(*input));
    }
    ASSERT_TRUE(providers.clock().advance(frame_step).has_value());
    ASSERT_TRUE(application.frame().has_value());
  }

  static auto stripes() -> Element {
    std::vector<Element> rows;
    constexpr std::array<std::uint32_t, 4> colours{0xD03030, 0x30A040, 0x3050D0,
                                                   0xE0C020};
    for (std::size_t index = 0; index < 48; ++index) {
      rows.push_back(
          components::ZStack()
              .frame({.width = 320.0F, .height = 8.0F})
              .background(theme::Colour::hex(colours[index % colours.size()])));
    }
    return components::ScrollView(components::VStack(std::move(rows)))
        .frame({.width = 320.0F, .height = 180.0F})
        .id("stripes");
  }

  static auto scene(bool glass) -> Element {
    if (glass) {
      return components::ZStack(
          stripes(), controls::ProgressView().id("under"),
          controls::ProgressView().offset(0.0F, 52.0F).id("beside"),
          components::ZStack()
              .frame({.width = 200.0F, .height = 60.0F})
              .glass_effect(theme::Glass::regular(), theme::Shape::rectangle())
              .id("panel"));
    }
    return components::ZStack(
        stripes(),
        components::ZStack()
            .frame({.width = 200.0F, .height = 60.0F})
            .background(theme::Colour::hex(0x202020))
            .id("panel"),
        controls::ProgressView().offset(120.0F, 60.0F).id("spinner"));
  }

  struct Changed {
    bool any{false};
    float left{0.0F};
    float top{0.0F};
    float right{0.0F};
    float bottom{0.0F};
  };

  using Record = provider::headless::NullGpu::ShapeRecord;

  static auto quads(const Record& record)
      -> std::vector<std::array<provider::ShapeVertex, 4>> {
    std::vector<std::array<provider::ShapeVertex, 4>> found;
    for (std::size_t base = 0; base + 3 < record.vertices.size(); base += 4) {
      found.push_back({record.vertices[base], record.vertices[base + 1],
                       record.vertices[base + 2], record.vertices[base + 3]});
    }
    return found;
  }

  static auto differences(const Record& before, const Record& after)
      -> Changed {
    const auto old_quads = quads(before);
    const auto new_quads = quads(after);
    Changed changed;
    const auto include =
        [&changed](const std::array<provider::ShapeVertex, 4>& quad) {
          for (const provider::ShapeVertex& vertex : quad) {
            if (!changed.any) {
              changed = Changed{.any = true,
                                .left = vertex.position[0],
                                .top = vertex.position[1],
                                .right = vertex.position[0],
                                .bottom = vertex.position[1]};
            }
            changed.left = std::min(changed.left, vertex.position[0]);
            changed.top = std::min(changed.top, vertex.position[1]);
            changed.right = std::max(changed.right, vertex.position[0]);
            changed.bottom = std::max(changed.bottom, vertex.position[1]);
          }
        };
    for (const auto& quad : old_quads) {
      if (std::ranges::find(new_quads, quad) == new_quads.end()) {
        include(quad);
      }
    }
    for (const auto& quad : new_quads) {
      if (std::ranges::find(old_quads, quad) == old_quads.end()) {
        include(quad);
      }
    }
    return changed;
  }

  static auto drag(float y, provider::TouchPhase phase)
      -> provider::InputFrame {
    return touching(provider::PixelPoint{.x = 40.0F, .y = y}, phase);
  }

  class Twin {
   public:
    Twin(bool glass, std::uint32_t images)
        : partial_app_(launch(partial_, scene(glass), images, true)),
          whole_app_(launch(whole_, scene(glass), 1, false)) {}

    void step(std::optional<provider::InputFrame> input = std::nullopt) {
      advance(partial_, partial_app_, input);
      advance(whole_, whole_app_, std::move(input));
      if (partial_.gpu().last_damage().has_value()) {
        ++partial_frames_;
      }
      const auto seen = partial_.gpu().capture();
      const auto expected = whole_.gpu().capture();
      ASSERT_TRUE(seen.has_value());
      ASSERT_TRUE(expected.has_value());
      if (!std::ranges::equal(seen->pixels(), expected->pixels())) {
        ++mismatched_;
      }
    }

    void scroll(float distance) {
      constexpr int steps = 8;
      step(drag(150.0F, provider::TouchPhase::began));
      for (int index = 1; index <= steps; ++index) {
        step(drag(150.0F - (distance * static_cast<float>(index) /
                            static_cast<float>(steps)),
                  provider::TouchPhase::moved));
      }
      step(drag(150.0F - distance, provider::TouchPhase::ended));
      for (int index = 0; index < 20; ++index) {
        step();
      }
    }

    auto image() -> provider::Image { return *partial_.gpu().capture(); }

    auto mismatched() const noexcept -> int { return mismatched_; }

    auto partial_frames() const noexcept -> int { return partial_frames_; }

   private:
    provider::headless::HeadlessProviderFactory partial_;
    provider::headless::HeadlessProviderFactory whole_;
    Application partial_app_;
    Application whole_app_;
    int partial_frames_{0};
    int mismatched_{0};
  };
};

TEST_F(DamageTest, SpinnerFrameDamagesOnlyTheSpinner) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = launch(providers, scene(false), 1, true);
  for (int index = 0; index < 8; ++index) {
    advance(providers, application);
  }
  const auto node = node_named(application, "spinner");
  ASSERT_TRUE(node.has_value());
  ASSERT_TRUE(node->get().presented().has_value());
  const composition::Rect spinner = node->get().presented()->bounds;
  constexpr float reach = 20.0F;
  constexpr float slack = 2.0F;
  auto previous = providers.gpu().last_shapes();
  int checked = 0;
  for (int index = 0; index < 30; ++index) {
    const std::size_t uploads = providers.gpu().vertex_uploads();
    advance(providers, application);
    const FrameTimings& timings = application.last_frame();
    if (!timings.presented || timings.laid_out) {
      continue;
    }
    const auto record = providers.gpu().last_shapes();
    const Changed changed = differences(previous, record);
    previous = record;
    ++checked;
    EXPECT_EQ(providers.gpu().vertex_uploads() - uploads,
              timings.uploaded_vertices);
    const std::optional<provider::ClipRect> damage =
        providers.gpu().last_damage();
    ASSERT_TRUE(damage.has_value());
    ASSERT_TRUE(changed.any);
    EXPECT_GE(static_cast<float>(damage->x), changed.left - slack);
    EXPECT_GE(static_cast<float>(damage->y), changed.top - slack);
    EXPECT_LE(static_cast<float>(damage->x + damage->width),
              changed.right + slack);
    EXPECT_LE(static_cast<float>(damage->y + damage->height),
              changed.bottom + slack);
    EXPECT_GE(changed.left, spinner.x - reach);
    EXPECT_GE(changed.top, spinner.y - reach);
    EXPECT_LE(changed.right, spinner.x + spinner.width + reach);
    EXPECT_LE(changed.bottom, spinner.y + spinner.height + reach);
    EXPECT_EQ(timings.damaged_pixels,
              static_cast<std::uint64_t>(damage->width) * damage->height);
    EXPECT_LT(timings.damaged_pixels,
              static_cast<std::uint64_t>(extent.width.value()) *
                  extent.height.value() / 8);
    const auto inside = std::ranges::count_if(
        record.vertices, [&](const provider::ShapeVertex& vertex) {
          return vertex.position[0] >= spinner.x - reach &&
                 vertex.position[0] <= spinner.x + spinner.width + reach &&
                 vertex.position[1] >= spinner.y - reach &&
                 vertex.position[1] <= spinner.y + spinner.height + reach;
        });
    EXPECT_GT(timings.uploaded_vertices, 0U);
    EXPECT_LE(timings.uploaded_vertices, static_cast<std::uint32_t>(inside));
    EXPECT_LT(timings.uploaded_vertices, record.vertices.size() / 2);
  }
  EXPECT_GT(checked, 10);
}

TEST_F(DamageTest, ScrollingReplaysRowsWithoutUploadingThem) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = launch(providers, stripes(), 1, true);
  for (int index = 0; index < 6; ++index) {
    advance(providers, application);
  }
  constexpr std::uint32_t entering = 24;
  int checked = 0;
  const auto check = [&] {
    const FrameTimings& timings = application.last_frame();
    if (!timings.presented || timings.laid_out) {
      return;
    }
    ++checked;
    EXPECT_LE(timings.uploaded_vertices, entering);
  };
  advance(providers, application, drag(150.0F, provider::TouchPhase::began));
  for (int index = 1; index <= 8; ++index) {
    advance(providers, application,
            drag(150.0F - (10.0F * static_cast<float>(index)),
                 provider::TouchPhase::moved));
    check();
  }
  advance(providers, application, drag(70.0F, provider::TouchPhase::ended));
  for (int index = 0; index < 20; ++index) {
    advance(providers, application);
    check();
  }
  EXPECT_GT(checked, 10);
}

class SwapchainDamageTest
    : public DamageTest,
      public ::testing::WithParamInterface<std::uint32_t> {};

TEST_P(SwapchainDamageTest, EverySwapchainImageMatchesAFullRepaint) {
  Twin twin{false, GetParam()};
  for (int index = 0; index < 6; ++index) {
    twin.step();
  }
  twin.scroll(90.0F);
  twin.scroll(-40.0F);
  EXPECT_EQ(twin.mismatched(), 0);
  EXPECT_GT(twin.partial_frames(), 10);
}

TEST_P(SwapchainDamageTest, GlassOverAChangingBackdropStillUpdates) {
  Twin twin{true, GetParam()};
  for (int index = 0; index < 6; ++index) {
    twin.step();
  }
  const provider::Image before = twin.image();
  std::size_t changes = 0;
  for (int index = 0; index < 40; ++index) {
    twin.step();
    const provider::Image after = twin.image();
    bool moved = false;
    for (std::uint32_t y = 78; y <= 102; y += 4) {
      for (std::uint32_t x = 148; x <= 172; x += 4) {
        moved = moved || after.pixel(x, y) != before.pixel(x, y);
      }
    }
    if (moved) {
      ++changes;
    }
  }
  twin.scroll(30.0F);
  EXPECT_EQ(twin.mismatched(), 0);
  EXPECT_GT(twin.partial_frames(), 10);
  EXPECT_GT(changes, 0U);
}

INSTANTIATE_TEST_SUITE_P(Images, SwapchainDamageTest,
                         ::testing::Values(1U, 2U, 3U));

}
