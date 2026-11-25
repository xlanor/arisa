#include <arisa/provider/gpu/shape_vertex.hpp>
#include <arisa/provider/headless/null_gpu.hpp>
#include <arisa/structure/scaffold.hpp>
#include <arisa/theme/color_scheme.hpp>

#include <gallery_fixture.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace arisa::runtime {

class GalleryToolbarFocusTest
    : public GalleryFixture,
      public ::testing::WithParamInterface<theme::ColorScheme> {
 protected:
  struct Box {
    float left{0.0F};
    float top{0.0F};
    float right{0.0F};
    float bottom{0.0F};

    auto contains(const Box& other) const noexcept -> bool {
      return other.left >= left - 0.5F && other.top >= top - 0.5F &&
             other.right <= right + 0.5F && other.bottom <= bottom + 0.5F;
    }

    auto overlaps(const Box& other) const noexcept -> bool {
      return other.left < right && other.right > left && other.top < bottom &&
             other.bottom > top;
    }
  };

  struct Probe {
    std::size_t glyphs{0};
    std::uint32_t alpha{0};
    float contrast{0.0F};
    std::size_t covered{0};
  };

  static auto box_of(const composition::Rect& rect) -> Box {
    return Box{.left = rect.x,
               .top = rect.y,
               .right = rect.x + rect.width,
               .bottom = rect.y + rect.height};
  }

  static auto quad(const provider::headless::NullGpu::ShapeRecord& record,
                   std::size_t base) -> Box {
    const provider::ShapeVertex& first = record.vertices[base];
    const provider::ShapeVertex& opposite = record.vertices[base + 2];
    return Box{.left = std::min(first.position[0], opposite.position[0]),
               .top = std::min(first.position[1], opposite.position[1]),
               .right = std::max(first.position[0], opposite.position[0]),
               .bottom = std::max(first.position[1], opposite.position[1])};
  }

  static auto luminance(std::uint32_t colour) -> float {
    const auto channel = [colour](unsigned shift) {
      return static_cast<float>((colour >> shift) & 0xFFU) / 255.0F;
    };
    return (0.2126F * channel(0U)) + (0.7152F * channel(8U)) +
           (0.0722F * channel(16U));
  }

  static auto toggle(Application& application) -> std::optional<Box> {
    const auto id =
        node_of(application, structure::Scaffold::sidebar_toggle_key);
    if (!id.has_value()) {
      return std::nullopt;
    }
    const auto node = application.root().tree().node(*id);
    if (!node.has_value() || !node->get().presented().has_value()) {
      return std::nullopt;
    }
    return box_of(node->get().presented()->bounds);
  }

  static auto on_toggle(Application& application) -> bool {
    const auto focused = application.interaction().focused();
    return focused.has_value() &&
           within(application, *focused,
                  structure::Scaffold::sidebar_toggle_key);
  }

  static void focus_toggle(
      provider::headless::HeadlessProviderFactory& providers,
      Application& application) {
    for (int hop = 0; hop < 40 && !on_toggle(application); ++hop) {
      press(providers, application, provider::Button::dpad_up);
      run(providers, application, 2);
    }
  }

  static auto probe(provider::headless::HeadlessProviderFactory& providers,
                    const Box& around) -> Probe {
    const provider::headless::NullGpu::ShapeRecord record =
        providers.gpu().last_shapes();
    Probe found;
    std::optional<std::size_t> first_glyph;
    std::uint32_t ink = 0;
    for (std::size_t base = 0; base + 3 < record.vertices.size(); base += 4) {
      const provider::ShapeVertex& vertex = record.vertices[base];
      if (vertex.kind != provider::ShapeKind::glyph ||
          !around.contains(quad(record, base))) {
        continue;
      }
      ++found.glyphs;
      const std::uint32_t alpha = vertex.colour >> 24U;
      if (alpha > found.alpha) {
        found.alpha = alpha;
        ink = vertex.colour;
      }
      if (!first_glyph.has_value()) {
        first_glyph = base;
      }
    }
    if (!first_glyph.has_value()) {
      return found;
    }
    const Box glyph = quad(record, *first_glyph);
    const std::optional<provider::Colour> clear = providers.gpu().last_clear();
    float beneath = clear.has_value()
                        ? (0.2126F * clear->red) + (0.7152F * clear->green) +
                              (0.0722F * clear->blue)
                        : 0.0F;
    for (std::size_t base = 0; base + 3 < record.vertices.size(); base += 4) {
      const provider::ShapeVertex& vertex = record.vertices[base];
      const bool solid = vertex.kind == provider::ShapeKind::fill;
      if (!solid || (vertex.colour >> 24U) < 128U ||
          !quad(record, base).contains(glyph)) {
        continue;
      }
      if (base < *first_glyph) {
        beneath = luminance(vertex.colour);
      } else if (quad(record, base).overlaps(glyph)) {
        ++found.covered;
      }
    }
    found.contrast = std::abs(luminance(ink) - beneath);
    return found;
  }

  static void cycle(provider::headless::HeadlessProviderFactory& providers,
                    Application& application);

  static void expect_visible(
      provider::headless::HeadlessProviderFactory& providers,
      Application& application, const std::string& state) {
    ASSERT_TRUE(on_toggle(application)) << state;
    const std::optional<Box> around = toggle(application);
    ASSERT_TRUE(around.has_value()) << state;
    const Probe found = probe(providers, *around);
    EXPECT_GT(found.glyphs, 0U) << state;
    EXPECT_GE(found.alpha, 200U) << state;
    EXPECT_GT(found.contrast, 0.3F) << state;
    EXPECT_EQ(found.covered, 0U) << state;
  }
};

TEST_P(GalleryToolbarFocusTest, FocusedToggleShowsItsGlyphOpenAndClosed) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start_gallery(providers, examples::GalleryPage::controls, GetParam());
  cycle(providers, application);
}

TEST_P(GalleryToolbarFocusTest, FocusedToggleShowsItsGlyphInTheGlassPack) {
  provider::headless::HeadlessProviderFactory providers;
  AppearanceOptions options{
      .policy = examples::GalleryScreen::policy(GetParam()),
      .storage = std::nullopt};
  options.policy.defaults.theme = "arisa-theme-glass";
  Application application = start_gallery(
      providers, examples::GalleryPage::controls, GetParam(),
      provider::headless::ScriptedSurface::default_size, std::move(options));
  cycle(providers, application);
}

void GalleryToolbarFocusTest::cycle(
    provider::headless::HeadlessProviderFactory& providers,
    Application& application) {
  focus_toggle(providers, application);
  run(providers, application, 30);
  expect_visible(providers, application, "open");
  press(providers, application, provider::Button::a);
  run(providers, application, 60);
  expect_visible(providers, application, "closed");
  press(providers, application, provider::Button::a);
  run(providers, application, 60);
  expect_visible(providers, application, "reopened");
}

INSTANTIATE_TEST_SUITE_P(
    Schemes, GalleryToolbarFocusTest,
    ::testing::Values(theme::ColorScheme::light, theme::ColorScheme::dark),
    [](const auto& scheme) {
      return std::string{scheme.param == theme::ColorScheme::light ? "light"
                                                                   : "dark"};
    });

}
