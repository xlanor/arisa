#include <arisa/provider/gpu/image.hpp>
#include <arisa/provider/gpu/shape_vertex.hpp>
#include <arisa/provider/headless/null_gpu.hpp>
#include <arisa/provider/surface/surface_size.hpp>
#include <arisa/theme/color_scheme.hpp>

#include <gallery_fixture.hpp>
#include <presentations_page.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <optional>
#include <print>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace arisa::runtime {

class GallerySheetMaterialTest : public GalleryFixture,
                                 public ::testing::WithParamInterface<int> {
 protected:
  static constexpr float width = 640.0F;
  static constexpr float height = 360.0F;
  static constexpr int regular_choice = 1;
  static constexpr int clear_choice = 2;
  static constexpr provider::SurfaceSize extent{
      .width = provider::SurfaceWidth{640},
      .height = provider::SurfaceHeight{360}};
  static constexpr float density = 0.5F;
  static constexpr float sheet_left = width * 0.19F;
  static constexpr float sheet_right = width * 0.81F;
  static constexpr float sheet_top = height * 0.06F;
  static constexpr float sheet_bottom = height * 0.94F;
  static constexpr float content_depth = 124.0F;
  static constexpr float corner = 20.0F;
  static constexpr float rim = 3.0F;
  static constexpr std::chrono::milliseconds stride{100};

  struct Luma {
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::vector<float> values;

    auto at(std::uint32_t x, std::uint32_t y) const -> float {
      return values[(static_cast<std::size_t>(y) * width) + x];
    }
  };

  struct Region {
    std::uint32_t left{0};
    std::uint32_t top{0};
    std::uint32_t right{0};
    std::uint32_t bottom{0};
  };

  static auto start(provider::headless::HeadlessProviderFactory& providers,
                    bool glass, bool partial) -> Application {
    providers.gpu().rasterize(extent);
    if (partial) {
      providers.gpu().swapchain(3, true);
    }
    AppearanceOptions options{
        .policy = examples::GalleryScreen::policy(theme::ColorScheme::light),
        .storage = std::nullopt};
    if (glass) {
      options.policy.defaults.theme = "arisa-theme-glass";
    }
    options.policy.defaults.ui_scale = appearance::UiScales{
        .handheld = 1.25F, .docked = 1.25F, .desktop = 1.25F};
    return start_gallery(providers, examples::GalleryPage::presentations,
                         theme::ColorScheme::light, extent, std::move(options),
                         density);
  }

  static auto activate(Application& application, std::string_view key) -> bool {
    runtime::Root& root = application.root();
    const auto found = root.tree().find(composition::ElementKey{key});
    if (!found.has_value()) {
      return false;
    }
    if (root.activate(*found)) {
      return true;
    }
    const auto node = root.tree().node(*found);
    if (node.has_value() && activate_within(root, node->get())) {
      return true;
    }
    std::optional<composition::NodeId> above =
        node.has_value() ? node->get().parent() : std::nullopt;
    while (above.has_value()) {
      if (root.activate(*above)) {
        return true;
      }
      const auto parent = root.tree().node(*above);
      above = parent.has_value() ? parent->get().parent() : std::nullopt;
    }
    return false;
  }

  static auto activate_within(runtime::Root& root,
                              const composition::Node& node) -> bool {
    return std::ranges::any_of(
        node.children(), [&root](const composition::Node& child) {
          return root.activate(child.id()) || activate_within(root, child);
        });
  }

  static void present(provider::headless::HeadlessProviderFactory& providers,
                      Application& application, int choice) {
    ASSERT_TRUE(
        activate(application, examples::PresentationsPage::sheet_button));
    if (choice == 0) {
      leap(providers, application, 10);
      return;
    }
    leap(providers, application, 1);
    ASSERT_TRUE(
        activate(application, examples::PresentationsPage::background_picker));
    leap(providers, application, 2);
    ASSERT_TRUE(
        activate(application, examples::PresentationsPage::option_key(choice)));
    leap(providers, application, 9);
  }

  static void leap(provider::headless::HeadlessProviderFactory& providers,
                   Application& application, int count) {
    for (int index = 0; index < count; ++index) {
      ASSERT_TRUE(providers.clock().advance(stride).has_value());
      ASSERT_TRUE(application.frame().has_value());
    }
  }

  static auto luma(provider::headless::HeadlessProviderFactory& providers)
      -> Luma {
    const auto image = providers.gpu().capture();
    EXPECT_TRUE(image.has_value());
    Luma result;
    if (!image.has_value()) {
      return result;
    }
    result.width = image->width();
    result.height = image->height();
    result.values.reserve(static_cast<std::size_t>(result.width) *
                          result.height);
    const auto bytes = image->pixels();
    for (std::size_t index = 0; index + 3 < bytes.size();
         index += provider::Image::channels) {
      const auto channel = [&bytes, index](std::size_t offset) {
        return static_cast<float>(
                   std::to_integer<std::uint8_t>(bytes[index + offset])) /
               255.0F;
      };
      result.values.push_back((0.2126F * channel(0)) + (0.7152F * channel(1)) +
                              (0.0722F * channel(2)));
    }
    return result;
  }

  static auto blurred(const Luma& source, int radius) -> Luma {
    Luma across = source;
    Luma result = source;
    const auto columns = static_cast<int>(source.width);
    const auto rows = static_cast<int>(source.height);
    for (int y = 0; y < rows; ++y) {
      for (int x = 0; x < columns; ++x) {
        float sum = 0.0F;
        int count = 0;
        for (int dx = -radius; dx <= radius; ++dx) {
          const int at = std::clamp(x + dx, 0, columns - 1);
          sum += source.at(static_cast<std::uint32_t>(at),
                           static_cast<std::uint32_t>(y));
          ++count;
        }
        across.values[(static_cast<std::size_t>(y) * source.width) +
                      static_cast<std::size_t>(x)] =
            sum / static_cast<float>(count);
      }
    }
    for (int y = 0; y < rows; ++y) {
      for (int x = 0; x < columns; ++x) {
        float sum = 0.0F;
        int count = 0;
        for (int dy = -radius; dy <= radius; ++dy) {
          const int at = std::clamp(y + dy, 0, rows - 1);
          sum += across.at(static_cast<std::uint32_t>(x),
                           static_cast<std::uint32_t>(at));
          ++count;
        }
        result.values[(static_cast<std::size_t>(y) * source.width) +
                      static_cast<std::size_t>(x)] =
            sum / static_cast<float>(count);
      }
    }
    return result;
  }

  static auto interior() -> Region {
    return Region{.left = static_cast<std::uint32_t>(sheet_left + rim),
                  .top = static_cast<std::uint32_t>(sheet_top + content_depth),
                  .right = static_cast<std::uint32_t>(sheet_right - rim),
                  .bottom = static_cast<std::uint32_t>(sheet_bottom - corner)};
  }

  static auto floor() -> Region {
    return Region{.left = static_cast<std::uint32_t>(sheet_left + corner),
                  .top = static_cast<std::uint32_t>(sheet_bottom - corner),
                  .right = static_cast<std::uint32_t>(sheet_right - corner),
                  .bottom = static_cast<std::uint32_t>(sheet_bottom - rim)};
  }

  static auto columns_of(const Luma& shown, const Region& region)
      -> std::vector<float> {
    std::vector<float> means;
    for (std::uint32_t x = region.left; x < region.right; ++x) {
      float sum = 0.0F;
      for (std::uint32_t y = region.top; y < region.bottom; ++y) {
        sum += shown.at(x, y);
      }
      means.push_back(sum / static_cast<float>(region.bottom - region.top));
    }
    return means;
  }

  static auto rows_of(const Luma& shown, const Region& region)
      -> std::vector<float> {
    std::vector<float> means;
    for (std::uint32_t y = region.top; y < region.bottom; ++y) {
      float sum = 0.0F;
      for (std::uint32_t x = region.left; x < region.right; ++x) {
        sum += shown.at(x, y);
      }
      means.push_back(sum / static_cast<float>(region.right - region.left));
    }
    return means;
  }

  static auto spike(const std::vector<float>& line) -> float {
    constexpr std::size_t reach = 3;
    float worst = 0.0F;
    for (std::size_t at = reach; at + reach < line.size(); ++at) {
      const float around = (line[at - reach] + line[at + reach]) * 0.5F;
      worst = std::max(worst, std::abs(line[at] - around));
    }
    return worst;
  }

  static auto worst_seam(const Luma& shown) -> float {
    const Region inside = interior();
    const Region across{
        .left = static_cast<std::uint32_t>(sheet_left + corner),
        .top = inside.top,
        .right = static_cast<std::uint32_t>(sheet_right - corner),
        .bottom = floor().bottom};
    return std::max(spike(columns_of(shown, inside)),
                    spike(rows_of(shown, across)));
  }

  static auto block_range(const Luma& image, const Region& region,
                          std::uint32_t x, std::uint32_t y) -> float {
    constexpr std::uint32_t block = 8;
    float low = 1.0F;
    float high = 0.0F;
    for (std::uint32_t row = y; row < std::min(y + block, region.bottom);
         ++row) {
      for (std::uint32_t column = x; column < std::min(x + block, region.right);
           ++column) {
        low = std::min(low, image.at(column, row));
        high = std::max(high, image.at(column, row));
      }
    }
    return high - low;
  }

  static auto worst_excess(const Luma& shown, const Luma& reference,
                           const Region& region) -> float {
    float worst = 0.0F;
    for (std::uint32_t y = region.top; y < region.bottom; y += 8) {
      for (std::uint32_t x = region.left; x < region.right; x += 8) {
        worst = std::max(worst, block_range(shown, region, x, y) -
                                    block_range(reference, region, x, y));
      }
    }
    return worst;
  }

  static auto worst_contrast(const Luma& shown, const Region& region) -> float {
    float worst = 0.0F;
    for (std::uint32_t y = region.top; y < region.bottom; y += 8) {
      for (std::uint32_t x = region.left; x < region.right; x += 8) {
        worst = std::max(worst, block_range(shown, region, x, y));
      }
    }
    return worst;
  }

  static auto sheet_surface(
      provider::headless::HeadlessProviderFactory& providers)
      -> std::optional<std::tuple<provider::ShapeKind, std::uint32_t, float>> {
    const auto record = providers.gpu().last_shapes();
    for (std::size_t base = 0; base + 3 < record.vertices.size(); base += 4) {
      const provider::ShapeVertex& vertex = record.vertices[base];
      const float shape_width = vertex.half_size[0] * 2.0F;
      const float shape_height = vertex.half_size[1] * 2.0F;
      if (std::abs(shape_width - (sheet_right - sheet_left)) < 3.0F &&
          std::abs(shape_height - (sheet_bottom - sheet_top)) < 3.0F &&
          (vertex.kind == provider::ShapeKind::glass ||
           vertex.kind == provider::ShapeKind::material ||
           vertex.kind == provider::ShapeKind::fill)) {
        return std::tuple{vertex.kind, vertex.colour, vertex.uv[1]};
      }
    }
    return std::nullopt;
  }
};

TEST_P(GallerySheetMaterialTest, SheetHidesThePageWithoutArtifacts) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, true, false);
  leap(providers, application, 1);
  const Luma page = luma(providers);
  present(providers, application, GetParam());
  const Luma shown = luma(providers);
  const Luma reference = blurred(page, 4);
  if (GetParam() == clear_choice) {
    EXPECT_GT(worst_contrast(shown, interior()), 0.10F);
    return;
  }
  EXPECT_LT(worst_seam(shown), 0.006F) << "choice " << GetParam();
  for (const Region& region : {interior(), floor()}) {
    EXPECT_LT(worst_contrast(shown, region), 0.10F) << "choice " << GetParam();
    EXPECT_LT(worst_excess(shown, reference, region), 0.06F)
        << "choice " << GetParam();
  }
}

TEST_P(GallerySheetMaterialTest, PartialRepaintMatchesAFullRepaint) {
  provider::headless::HeadlessProviderFactory partial;
  provider::headless::HeadlessProviderFactory full;
  Application patched = start(partial, true, true);
  Application whole = start(full, true, false);
  present(partial, patched, GetParam());
  present(full, whole, GetParam());
  const Luma left = luma(partial);
  const Luma right = luma(full);
  ASSERT_EQ(left.values.size(), right.values.size());
  float worst = 0.0F;
  for (std::size_t index = 0; index < left.values.size(); ++index) {
    worst = std::max(worst, std::abs(left.values[index] - right.values[index]));
  }
  EXPECT_LT(worst, 2.5F / 255.0F) << "choice " << GetParam();
}

INSTANTIATE_TEST_SUITE_P(
    Backgrounds, GallerySheetMaterialTest,
    ::testing::Range(0, examples::PresentationsPage::choices));

class GalleryPresentationBackgroundTest : public GallerySheetMaterialTest {};

TEST_F(GalleryPresentationBackgroundTest, EveryChoiceChangesTheSurface) {
  std::set<std::tuple<int, std::uint32_t, float>> seen;
  std::optional<std::tuple<provider::ShapeKind, std::uint32_t, float>> themed;
  for (int choice = 0; choice < examples::PresentationsPage::choices;
       ++choice) {
    provider::headless::HeadlessProviderFactory providers;
    AppearanceOptions options{
        .policy = examples::GalleryScreen::policy(theme::ColorScheme::light),
        .storage = std::nullopt};
    options.policy.defaults.theme = "arisa-theme-glass";
    Application application = start_gallery(
        providers, examples::GalleryPage::presentations,
        theme::ColorScheme::light, extent, std::move(options), density);
    present(providers, application, choice);
    const auto surface = sheet_surface(providers);
    ASSERT_TRUE(surface.has_value()) << "choice " << choice;
    const auto [kind, colour, saturation] = *surface;
    if (choice == 0) {
      EXPECT_EQ(kind, provider::ShapeKind::glass);
      EXPECT_GT(saturation, 0.0F);
      themed = *surface;
    } else if (choice == regular_choice) {
      EXPECT_EQ(themed, surface) << "the glass theme's sheet is regular glass";
      continue;
    } else if (choice < 4) {
      EXPECT_EQ(kind, provider::ShapeKind::glass) << "choice " << choice;
    } else {
      EXPECT_EQ(kind, provider::ShapeKind::material) << "choice " << choice;
    }
    EXPECT_TRUE(seen.emplace(static_cast<int>(kind), colour, saturation).second)
        << "choice " << choice << " drew the same surface as another choice";
  }
}

TEST_F(GalleryPresentationBackgroundTest, DefaultThemeKeepsItsSheetFill) {
  provider::headless::HeadlessProviderFactory providers;
  AppearanceOptions options{
      .policy = examples::GalleryScreen::policy(theme::ColorScheme::light),
      .storage = std::nullopt};
  Application application = start_gallery(
      providers, examples::GalleryPage::presentations,
      theme::ColorScheme::light, extent, std::move(options), density);
  present(providers, application, 0);
  const auto surface = sheet_surface(providers);
  ASSERT_TRUE(surface.has_value());
  EXPECT_EQ(std::get<0>(*surface), provider::ShapeKind::fill);
}

}

namespace arisa::runtime {

class GalleryPresentationBudgetTest : public GallerySheetMaterialTest {
 protected:
#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
  static constexpr int slowdown = 6;
#else
  static constexpr int slowdown = 1;
#endif
  static constexpr std::uint32_t backdrop_budget = 5;
  static constexpr std::chrono::microseconds mean_cpu_budget{2000 * slowdown};
  static constexpr std::chrono::microseconds worst_cpu_budget{12000 * slowdown};

  struct Cost {
    int frames{0};
    int laid_out{0};
    std::uint32_t backdrops{0};
    std::uint32_t blurs{0};
    std::chrono::nanoseconds cpu{0};
    std::chrono::nanoseconds worst{0};

    void add(const FrameTimings& timings, std::chrono::nanoseconds spent) {
      ++frames;
      laid_out += timings.laid_out ? 1 : 0;
      backdrops = std::max(backdrops, timings.backdrops);
      blurs += timings.blurs;
      cpu += spent;
      worst = std::max(worst, spent);
    }
  };

  static auto launch(provider::headless::HeadlessProviderFactory& providers)
      -> Application {
    AppearanceOptions options{
        .policy = examples::GalleryScreen::policy(theme::ColorScheme::light),
        .storage = std::nullopt};
    options.policy.defaults.theme = "arisa-theme-glass";
    options.policy.defaults.ui_scale = appearance::UiScales{
        .handheld = 1.25F, .docked = 1.25F, .desktop = 1.25F};
    return start_gallery(
        providers, examples::GalleryPage::presentations,
        theme::ColorScheme::light,
        provider::SurfaceSize{.width = provider::SurfaceWidth{1280},
                              .height = provider::SurfaceHeight{720}},
        std::move(options));
  }

  static void step(provider::headless::HeadlessProviderFactory& providers,
                   Application& application, Cost& cost, int count) {
    for (int index = 0; index < count; ++index) {
      ASSERT_TRUE(providers.clock().advance(frame_step).has_value());
      const std::clock_t before = std::clock();
      ASSERT_TRUE(application.frame().has_value());
      const std::clock_t after = std::clock();
      cost.add(application.last_frame(),
               std::chrono::microseconds{(after - before) * 1'000'000 /
                                         CLOCKS_PER_SEC});
    }
  }

  static void within_budget(const Cost& cost) {
    ASSERT_GT(cost.frames, 0);
    EXPECT_LE(cost.backdrops, backdrop_budget) << "choice " << GetParam();
    EXPECT_LE(cost.cpu / cost.frames, mean_cpu_budget)
        << "choice " << GetParam();
    EXPECT_LE(cost.worst, worst_cpu_budget) << "choice " << GetParam();
  }

  static void pick(provider::headless::HeadlessProviderFactory& providers,
                   Application& application, Cost& cost,
                   std::string_view opener) {
    ASSERT_TRUE(activate(application, opener));
    step(providers, application, cost, 40);
    ASSERT_TRUE(
        activate(application, examples::PresentationsPage::background_picker));
    step(providers, application, cost, 30);
    ASSERT_TRUE(activate(application,
                         examples::PresentationsPage::option_key(GetParam())));
    step(providers, application, cost, 30);
    ASSERT_TRUE(
        activate(application, examples::PresentationsPage::background_picker));
    step(providers, application, cost, 60);
  }
};

TEST_P(GalleryPresentationBudgetTest, SheetWithItsPickerStaysWithinBudget) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = launch(providers);
  Cost cost;
  pick(providers, application, cost, examples::PresentationsPage::sheet_button);
  within_budget(cost);
}

TEST_P(GalleryPresentationBudgetTest, PopoverWithItsPickerStaysWithinBudget) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = launch(providers);
  Cost cost;
  pick(providers, application, cost,
       examples::PresentationsPage::popover_button);
  within_budget(cost);
}

INSTANTIATE_TEST_SUITE_P(
    Backgrounds, GalleryPresentationBudgetTest,
    ::testing::Range(0, examples::PresentationsPage::choices));

}
