#include <arisa/appearance/theme_file.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/navigation_split_view_visibility.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/provider/gpu/image.hpp>
#include <arisa/provider/gpu/shape_vertex.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/provider/surface/surface_scale.hpp>
#include <arisa/provider/surface/surface_size.hpp>
#include <arisa/structure/appearance_settings_section.hpp>
#include <arisa/structure/navigation_split_view.hpp>
#include <arisa/structure/scaffold.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/material.hpp>
#include <arisa/theme/metrics.hpp>
#include <arisa/theme/oklch.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/theme.hpp>

#include <colours_page.hpp>
#include <gallery_fixture.hpp>
#include <gallery_wallpaper.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace arisa::runtime {

using GlassPackCase =
    std::tuple<composition::NavigationSplitViewVisibility, float>;

class GalleryGlassPackTest
    : public GalleryFixture,
      public ::testing::WithParamInterface<GlassPackCase> {
 protected:
  static auto start(provider::headless::HeadlessProviderFactory& providers,
                    examples::GalleryPage page,
                    composition::NavigationSplitViewVisibility sidebar,
                    float scale) -> Application {
    AppearanceOptions options{
        .policy = examples::GalleryScreen::policy(theme::ColorScheme::light),
        .storage = std::nullopt};
    options.policy.defaults.theme = "arisa-theme-glass";
    options.policy.defaults.sidebar = sidebar;
    return start_gallery(providers, page, theme::ColorScheme::light,
                         provider::headless::ScriptedSurface::default_size,
                         std::move(options), scale);
  }

  static auto bounds(const composition::Node& node) -> composition::Rect {
    return node.presented().has_value() ? node.presented()->bounds
                                        : node.frame();
  }

  static auto glass_nodes(const composition::Node& root)
      -> std::vector<composition::Rect> {
    std::vector<composition::Rect> found;
    walk(root, [&found](const composition::Node& node) {
      if (node.modifiers().glass.has_value() && node.presented().has_value() &&
          node.presented()->visible) {
        found.push_back(node.presented()->bounds);
      }
    });
    return found;
  }

  static auto widest_glass_draw(
      provider::headless::HeadlessProviderFactory& providers) -> float {
    const auto record = providers.gpu().last_shapes();
    float widest = 0.0F;
    for (std::size_t base = 0; base + 3 < record.vertices.size(); base += 4) {
      const provider::ShapeVertex& first = record.vertices[base];
      if (first.kind != provider::ShapeKind::glass &&
          first.kind != provider::ShapeKind::glass_union &&
          first.kind != provider::ShapeKind::material) {
        continue;
      }
      float left = first.position[0];
      float right = first.position[0];
      for (std::size_t corner = 1; corner < 4; ++corner) {
        left = std::min(left, record.vertices[base + corner].position[0]);
        right = std::max(right, record.vertices[base + corner].position[0]);
      }
      widest = std::max(widest, right - left);
    }
    return widest;
  }
};

TEST_P(GalleryGlassPackTest, SidebarFloatsInsetOverTheDetail) {
  const auto [sidebar, scale] = GetParam();
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers, examples::GalleryPage::typography, sidebar, scale);
  run(providers, application, 20);
  const composition::Tree& tree = application.root().tree();
  const composition::Rect window = tree.root().frame();
  const auto panel = tree.find(
      composition::ElementKey{structure::NavigationSplitView::sidebar_id});
  const auto detail = tree.find(
      composition::ElementKey{structure::NavigationSplitView::detail_id});
  ASSERT_TRUE(panel.has_value());
  ASSERT_TRUE(detail.has_value());
  const composition::Rect shown = bounds(tree.node(*detail)->get());
  EXPECT_LE(shown.x, 0.5F);
  EXPECT_GE(shown.x + shown.width, window.width - 0.5F);
  std::vector<composition::Rect> sidebar_glass;
  walk(tree.node(*panel)->get(), [&](const composition::Node& node) {
    if (node.modifiers().glass.has_value() && node.presented().has_value() &&
        node.presented()->visible && node.presented()->opacity > 0.01F) {
      sidebar_glass.push_back(node.presented()->bounds);
    }
  });
  if (sidebar == composition::NavigationSplitViewVisibility::detail_only) {
    EXPECT_TRUE(sidebar_glass.empty());
    return;
  }
  ASSERT_EQ(sidebar_glass.size(), 1U);
  const composition::Rect glass = sidebar_glass.front();
  const float inset = theme::metrics::list.sidebar_panel_inset;
  EXPECT_NEAR(glass.x, inset, 0.75F);
  EXPECT_NEAR(glass.y, inset, 0.75F);
  EXPECT_NEAR(glass.y + glass.height, window.height - inset, 0.75F);
  EXPECT_GT(glass.x + glass.width, shown.x);
  const auto leading = tree.find(
      composition::ElementKey{structure::Scaffold::leading_group_key});
  ASSERT_TRUE(leading.has_value());
  const composition::Rect group = bounds(tree.node(*leading)->get());
  EXPECT_GE(group.x, glass.x - 0.75F);
  EXPECT_GE(group.y, glass.y - 0.75F);
  EXPECT_LE(group.x + group.width, glass.x + glass.width + 0.75F);
  EXPECT_LE(group.y + group.height, glass.y + glass.height + 0.75F);
  const auto toggle = tree.find(
      composition::ElementKey{structure::Scaffold::sidebar_toggle_key});
  ASSERT_TRUE(toggle.has_value());
  const composition::Rect button = bounds(tree.node(*toggle)->get());
  EXPECT_GE(button.x, glass.x);
  EXPECT_LE(button.x + button.width, glass.x + glass.width);
  std::size_t rows = 0;
  for (const composition::NodeId id : focusables(application)) {
    if (!within(application, id, structure::NavigationSplitView::sidebar_id)) {
      continue;
    }
    const composition::Node& row = tree.node(id)->get();
    if (!visible(row, window.width, window.height)) {
      continue;
    }
    ++rows;
    EXPECT_GE(bounds(row).y, group.y + group.height - 0.5F);
  }
  EXPECT_GT(rows, 0U);
}

TEST_P(GalleryGlassPackTest, NoGlassSpansTheWindowWidth) {
  const auto [sidebar, scale] = GetParam();
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers, examples::GalleryPage::controls, sidebar, scale);
  run(providers, application, 20);
  const composition::Rect window = application.root().tree().root().frame();
  for (const composition::Rect& each :
       glass_nodes(application.root().tree().root())) {
    EXPECT_LT(each.width, window.width * 0.9F);
  }
  const auto pixels = static_cast<float>(
      provider::headless::ScriptedSurface::default_size.width.value());
  EXPECT_LT(widest_glass_draw(providers), pixels * 0.9F);
  const composition::Tree& tree = application.root().tree();
  for (const std::string_view key : {structure::Scaffold::leading_group_key,
                                     structure::Scaffold::trailing_group_key,
                                     structure::Scaffold::hint_group_key}) {
    EXPECT_TRUE(tree.find(composition::ElementKey{key}).has_value()) << key;
  }
}

TEST_P(GalleryGlassPackTest, ScrolledSidebarRowsStayBelowTheLeadingGroup) {
  const auto [sidebar, scale] = GetParam();
  if (sidebar == composition::NavigationSplitViewVisibility::detail_only) {
    return;
  }
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers, examples::GalleryPage::typography, sidebar, scale);
  run(providers, application, 20);
  for (int step = 0; step < 8; ++step) {
    const auto focused = application.interaction().focused();
    if (focused.has_value() &&
        within(application, *focused,
               structure::NavigationSplitView::sidebar_id)) {
      break;
    }
    press(providers, application, provider::Button::dpad_left);
    run(providers, application, 20);
  }
  for (int step = 0; step < 24; ++step) {
    press(providers, application, provider::Button::dpad_down);
    run(providers, application, 12);
  }
  run(providers, application, 30);
  const composition::Tree& tree = application.root().tree();
  const auto leading = tree.find(
      composition::ElementKey{structure::Scaffold::leading_group_key});
  const auto panel = tree.find(
      composition::ElementKey{structure::NavigationSplitView::sidebar_id});
  ASSERT_TRUE(leading.has_value());
  ASSERT_TRUE(panel.has_value());
  const composition::Rect group = bounds(tree.node(*leading)->get());
  std::size_t tucked = 0;
  std::size_t shown = 0;
  walk(tree.node(*panel)->get(), [&](const composition::Node& node) {
    if (!node.content_as<components::Text::Content>().has_value() &&
        !node.content_as<components::Image::Content>().has_value()) {
      return;
    }
    if (!node.presented().has_value()) {
      return;
    }
    composition::Rect glyph = node.presented()->bounds;
    const bool under = glyph.y < group.y + group.height - 0.5F;
    if (!node.presented()->visible) {
      tucked += under ? 1U : 0U;
      return;
    }
    if (const auto& clip = node.presented()->clip) {
      const float top = std::max(glyph.y, clip->y);
      const float bottom =
          std::min(glyph.y + glyph.height, clip->y + clip->height);
      glyph.y = top;
      glyph.height = std::max(bottom - top, 0.0F);
    }
    if (glyph.height <= 0.0F) {
      tucked += under ? 1U : 0U;
      return;
    }
    ++shown;
    if (glyph.x + glyph.width <= group.x || glyph.x >= group.x + group.width) {
      return;
    }
    EXPECT_FALSE(glyph.y < group.y + group.height - 0.5F &&
                 glyph.y + glyph.height > group.y + 0.5F)
        << "glyph at " << glyph.x << "," << glyph.y << " " << glyph.width << "x"
        << glyph.height << " over the group ending at "
        << group.y + group.height;
  });
  EXPECT_GT(shown, 0U);
  if (scale > 1.0F) {
    EXPECT_GT(tucked, 0U);
  }
}

INSTANTIATE_TEST_SUITE_P(
    Visibilities, GalleryGlassPackTest,
    ::testing::Combine(
        ::testing::Values(
            composition::NavigationSplitViewVisibility::all,
            composition::NavigationSplitViewVisibility::icons_only,
            composition::NavigationSplitViewVisibility::detail_only),
        ::testing::Values(1.0F, 1.25F)),
    [](const ::testing::TestParamInfo<GlassPackCase>& shown) {
      const composition::NavigationSplitViewVisibility sidebar =
          std::get<0>(shown.param);
      const float scale = std::get<1>(shown.param);
      const std::string name =
          sidebar == composition::NavigationSplitViewVisibility::all
              ? "all"
              : (sidebar ==
                         composition::NavigationSplitViewVisibility::icons_only
                     ? "icons_only"
                     : "detail_only");
      return name + (scale > 1.0F ? "_125" : "_100");
    });

using OpaqueSlabCase = std::tuple<examples::GalleryPage, theme::ColorScheme>;

class GalleryOpaqueSlabTest
    : public GalleryFixture,
      public ::testing::WithParamInterface<OpaqueSlabCase> {};

TEST_P(GalleryOpaqueSlabTest, NothingOpaqueTallerThanACardSitsBehindTheDetail) {
  const auto [page, scheme] = GetParam();
  provider::headless::HeadlessProviderFactory providers;
  AppearanceOptions options{.policy = examples::GalleryScreen::policy(scheme),
                            .storage = std::nullopt};
  options.policy.defaults.theme = "arisa-theme-glass";
  Application application =
      start_gallery(providers, page, scheme,
                    provider::headless::ScriptedSurface::default_size,
                    std::move(options), 1.0F);
  run(providers, application, 30);
  const composition::Tree& tree = application.root().tree();
  const auto detail = tree.find(
      composition::ElementKey{structure::NavigationSplitView::detail_id});
  ASSERT_TRUE(detail.has_value());
  const composition::Node& area = tree.node(*detail)->get();
  const composition::Rect shown =
      area.presented().has_value() ? area.presented()->bounds : area.frame();
  const auto record = providers.gpu().last_shapes();
  for (std::size_t base = 0; base + 3 < record.vertices.size(); base += 4) {
    const provider::ShapeVertex& first = record.vertices[base];
    if (first.kind != provider::ShapeKind::fill ||
        (first.colour >> 24U) < 250U) {
      continue;
    }
    float left = first.position[0];
    float right = first.position[0];
    float top = first.position[1];
    float bottom = first.position[1];
    for (std::size_t corner = 1; corner < 4; ++corner) {
      const auto& at = record.vertices[base + corner].position;
      left = std::min(left, at[0]);
      right = std::max(right, at[0]);
      top = std::min(top, at[1]);
      bottom = std::max(bottom, at[1]);
    }
    const bool behind = right > shown.x && left < shown.x + shown.width &&
                        bottom > shown.y && top < shown.y + shown.height;
    const bool window = right - left >= tree.root().frame().width;
    const bool scrolled =
        top < shown.y - 0.5F || bottom > shown.y + shown.height + 0.5F;
    if (!behind || window || scrolled || right - left < 48.0F) {
      continue;
    }
    EXPECT_LE(bottom - top, shown.height * 0.6F)
        << examples::GalleryPages::name(page) << " fill " << left << "," << top
        << " " << right - left << "x" << bottom - top;
  }
}

INSTANTIATE_TEST_SUITE_P(
    Pages, GalleryOpaqueSlabTest,
    ::testing::Combine(::testing::ValuesIn(examples::GalleryPages::all.begin(),
                                           examples::GalleryPages::all.end()),
                       ::testing::Values(theme::ColorScheme::light,
                                         theme::ColorScheme::dark)),
    [](const ::testing::TestParamInfo<OpaqueSlabCase>& shown) {
      return std::string{
                 examples::GalleryPages::name(std::get<0>(shown.param))} +
             (std::get<1>(shown.param) == theme::ColorScheme::dark ? "_dark"
                                                                   : "_light");
    });

class GalleryPaletteTest : public GalleryFixture,
                           public ::testing::WithParamInterface<bool> {};

TEST_P(GalleryPaletteTest, EveryRoleHasOneCardShowingItsVariants) {
  std::vector<int> seen(theme::colour_role_count, 0);
  for (const auto group : examples::ColoursPage::groups()) {
    for (const examples::ColoursPage::Swatch& entry : group) {
      ++seen[static_cast<std::size_t>(entry.role)];
    }
  }
  for (std::size_t index = 0; index < seen.size(); ++index) {
    EXPECT_EQ(seen[index], 1) << "role " << index;
  }
  provider::headless::HeadlessProviderFactory providers;
  AppearanceOptions options{
      .policy = examples::GalleryScreen::policy(theme::ColorScheme::light),
      .storage = std::nullopt};
  if (GetParam()) {
    options.policy.defaults.theme = "arisa-theme-glass";
  }
  Application application = start_gallery(
      providers, examples::GalleryPage::colours, theme::ColorScheme::light,
      provider::headless::ScriptedSurface::default_size, std::move(options));
  ASSERT_TRUE(application.appearance().has_value());
  application.appearance()->update(
      [](appearance::AppearanceOverrides& overrides) {
        overrides.accent =
            structure::AppearanceSettingsSection::accents[4].colour;
      });
  run(providers, application, 10);
  const theme::Theme& theme = application.appearance()->theme();
  const composition::Tree& tree = application.root().tree();
  for (const auto group : examples::ColoursPage::groups()) {
    for (const examples::ColoursPage::Swatch& entry : group) {
      ASSERT_TRUE(tree.find(composition::ElementKey{
                                examples::ColoursPage::card_key(entry.name)})
                      .has_value())
          << entry.name;
      const theme::AppearanceVariants& expected = theme.variants(entry.role);
      for (const examples::ColoursPage::Variant& variant :
           examples::ColoursPage::variants) {
        const auto chip = tree.find(composition::ElementKey{
            examples::ColoursPage::chip_key(entry.name, variant.name)});
        if (!examples::ColoursPage::shows(expected, variant)) {
          EXPECT_FALSE(chip.has_value()) << entry.name << " " << variant.name;
          continue;
        }
        ASSERT_TRUE(chip.has_value()) << entry.name << " " << variant.name;
        const auto& background = tree.node(*chip)->get().modifiers().background;
        ASSERT_TRUE(background.has_value());
        const std::optional<theme::ColourValue> fill =
            background->fill.colour();
        ASSERT_TRUE(fill.has_value());
        const theme::Colour* literal = fill->literal();
        ASSERT_NE(literal, nullptr);
        EXPECT_EQ(*literal, expected.*variant.value)
            << entry.name << " " << variant.name;
      }
    }
  }
  const theme::Colour accent = theme.variants(theme::ColourRole::accent).light;
  const theme::Colour chosen =
      structure::AppearanceSettingsSection::accents[4].colour;
  EXPECT_NEAR(accent.red, chosen.red, 0.01F);
  EXPECT_NEAR(accent.green, chosen.green, 0.01F);
  EXPECT_NEAR(accent.blue, chosen.blue, 0.01F);
  press(providers, application, provider::Button::dpad_right);
  run(providers, application, 10);
  std::vector<std::string> visited;
  for (int step = 0; step < 12; ++step) {
    const std::string key = focused_key(application);
    if (key.starts_with("colours.") &&
        (visited.empty() || visited.back() != key)) {
      visited.push_back(key);
    }
    press(providers, application, provider::Button::dpad_down);
    run(providers, application, 30);
  }
  EXPECT_GE(visited.size(), 4U);
  const auto focused = application.interaction().focused();
  ASSERT_TRUE(focused.has_value());
  const composition::Rect shown =
      tree.node(*focused)->get().presented()->bounds;
  EXPECT_GE(shown.y, 0.0F);
  EXPECT_LE(shown.y + shown.height, tree.root().frame().height);
}

INSTANTIATE_TEST_SUITE_P(Packs, GalleryPaletteTest, ::testing::Bool(),
                         [](const ::testing::TestParamInfo<bool>& shown) {
                           return std::string{shown.param ? "glass"
                                                          : "default"};
                         });

class GalleryGlassTintTest
    : public GalleryFixture,
      public ::testing::WithParamInterface<theme::ColorScheme> {
 protected:
  static constexpr provider::SurfaceSize extent{
      .width = provider::SurfaceWidth{240},
      .height = provider::SurfaceHeight{160}};

  static auto pack() -> theme::Theme {
    std::ifstream stream{std::filesystem::path{ARISA_SOURCE_ASSETS} /
                         "arisa/themes/arisa-theme-glass.toml"};
    const std::string text{std::istreambuf_iterator<char>{stream},
                           std::istreambuf_iterator<char>{}};
    const auto parsed = appearance::ThemeFile::parse(
        text, "arisa-theme-glass.toml",
        [](std::string_view) { return theme::Theme::defaults(); });
    EXPECT_TRUE(parsed.has_value());
    return parsed.value_or(theme::Theme::defaults());
  }

  static auto scene(theme::ColorScheme scheme,
                    std::optional<composition::Element> panel)
      -> composition::Element {
    return composition::Element{
        components::ZStack(
            composition::Element{examples::GalleryWallpaper{
                                     examples::GalleryWallpaper::Look::window}}
                .frame({.width = 240.0F, .height = 160.0F}),
            std::move(panel))}
        .environment(components::Environment::colour_scheme, scheme)
        .theme_override(pack());
  }

  static auto pane() -> composition::Element {
    return components::ZStack().frame({.width = 180.0F, .height = 110.0F});
  }

  static auto paint(theme::ColorScheme scheme,
                    std::optional<composition::Element> panel)
      -> provider::Image {
    provider::headless::HeadlessProviderFactory providers;
    providers.surface().resize(extent);
    providers.surface().set_scale(provider::SurfaceScale::unit());
    providers.gpu().rasterize(extent);
    Application application = start(providers, scene(scheme, std::move(panel)));
    run(providers, application, 2);
    auto image = providers.gpu().capture();
    if (!image.has_value()) {
      ADD_FAILURE() << "capture failed";
      return provider::Image::create(1, 1, std::vector<std::byte>(4)).value();
    }
    return std::move(*image);
  }

  static auto average(const provider::Image& image, std::uint32_t x,
                      std::uint32_t y) -> theme::Colour {
    std::array<float, 3> sum{};
    for (std::uint32_t dy = 0; dy < 5; ++dy) {
      for (std::uint32_t dx = 0; dx < 5; ++dx) {
        const auto found = image.pixel(x + dx - 2, y + dy - 2);
        if (!found.has_value()) {
          continue;
        }
        for (std::size_t channel = 0; channel < 3; ++channel) {
          sum[channel] += static_cast<float>((*found)[channel]) / 255.0F;
        }
      }
    }
    return theme::Colour{sum[0] / 25.0F, sum[1] / 25.0F, sum[2] / 25.0F, 1.0F};
  }

  static auto hue_shift(float from, float to) -> float {
    const float raw = std::fmod(std::abs(from - to), 360.0F);
    return std::min(raw, 360.0F - raw);
  }

  static auto compare(theme::ColorScheme scheme, const provider::Image& plain,
                      const provider::Image& covered, std::string_view name)
      -> std::size_t {
    std::size_t checked = 0;
    for (const std::uint32_t x : {50U, 85U, 120U, 155U, 190U}) {
      for (const std::uint32_t y : {45U, 80U, 115U}) {
        const theme::Oklch before = theme::Oklch::from(average(plain, x, y));
        const theme::Oklch after = theme::Oklch::from(average(covered, x, y));
        if (before.chroma() < 0.02F ||
            after.chroma() < std::max(0.008F, before.chroma() * 0.35F)) {
          continue;
        }
        ++checked;
        EXPECT_LT(hue_shift(before.hue(), after.hue()), 20.0F)
            << name << (scheme == theme::ColorScheme::dark ? " dark" : " light")
            << " at " << x << "," << y << " hue " << before.hue() << " -> "
            << after.hue();
        EXPECT_FALSE(after.hue() >= 150.0F && after.hue() <= 210.0F)
            << name << (scheme == theme::ColorScheme::dark ? " dark" : " light")
            << " at " << x << "," << y << " turned green-cyan, hue "
            << after.hue();
      }
    }
    return checked;
  }
};

TEST_F(GalleryGlassTintTest, WindowWallpaperHasNoGreenCyanCell) {
  for (const auto& cells : {examples::GalleryWallpaper::window_light,
                            examples::GalleryWallpaper::window_dark}) {
    for (const std::uint32_t cell : cells) {
      const float hue = theme::Oklch::from(theme::Colour::hex(cell)).hue();
      EXPECT_FALSE(hue >= 150.0F && hue <= 210.0F)
          << std::hex << cell << std::dec << " hue " << hue;
    }
  }
}

TEST_P(GalleryGlassTintTest, GlassAndMaterialsKeepTheWallpaperHue) {
  const theme::ColorScheme scheme = GetParam();
  const provider::Image plain = paint(scheme, std::nullopt);
  std::size_t checked =
      compare(scheme, plain,
              paint(scheme, pane().glass_effect(theme::Glass::regular(),
                                                theme::Shape::rounded(18.0F))),
              "glass");
  for (const std::string_view name : theme::Material::names) {
    const std::optional<theme::Material> material =
        theme::Material::parse(name);
    ASSERT_TRUE(material.has_value());
    checked += compare(scheme, plain,
                       paint(scheme, pane().background(*material)), name);
  }
  EXPECT_GT(checked, 40U);
}

INSTANTIATE_TEST_SUITE_P(
    Schemes, GalleryGlassTintTest,
    ::testing::Values(theme::ColorScheme::light, theme::ColorScheme::dark),
    [](const ::testing::TestParamInfo<theme::ColorScheme>& shown) {
      return std::string{shown.param == theme::ColorScheme::dark ? "dark"
                                                                 : "light"};
    });

}
