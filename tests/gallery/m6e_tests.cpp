#include <arisa/appearance/theme_file.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/system_symbols.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/navigation_split_view_visibility.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/node_id.hpp>
#include <arisa/provider/diagnostics/provider.hpp>
#include <arisa/provider/gpu/draw_data.hpp>
#include <arisa/provider/gpu/shape_vertex.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/headless/null_gpu.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/runtime/frame_timings.hpp>
#include <arisa/structure/form.hpp>
#include <arisa/structure/list.hpp>
#include <arisa/structure/navigation_split_view.hpp>
#include <arisa/structure/section.hpp>
#include <arisa/structure/tab_view.hpp>
#include <arisa/theme/dynamic_type_size.hpp>
#include <arisa/theme/theme.hpp>

#include <feed_page.hpp>
#include <gallery_fixture.hpp>
#include <launch_page.hpp>
#include <launch_screen.hpp>
#include <navigation_page.hpp>
#include <presentations_page.hpp>
#include <theming_page.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::runtime {

class GalleryM6eTest : public GalleryFixture {
 protected:
  class Host final : public composition::Component {
   public:
    using Build = std::function<composition::Element()>;

    explicit Host(const Build* build) noexcept : build_(build) {}

    auto operator==(const Host&) const -> bool = default;

    auto body(composition::Context&) const -> composition::Element override {
      return (*build_)();
    }

   private:
    const Build* build_;
  };

  static auto launch(provider::headless::HeadlessProviderFactory& providers,
                     examples::LaunchScreen::Ending ending) -> Application {
    load_fonts(providers);
    auto application = Application::create(
        providers,
        examples::GalleryScreen{
            examples::GalleryPage::typography, theme::Seed{}, std::nullopt,
            std::nullopt, examples::LaunchScreen::Style{.ending = ending}},
        ApplicationOptions{
            .executors = std::nullopt,
            .fonts = examples::GalleryFonts::load(providers.assets()),
            .atlas = {},
            .interaction = {},
            .appearance = std::nullopt,
            .launch_colour =
                theme::Colour::hex(examples::LaunchScreen::wallpaper[5])});
    EXPECT_TRUE(application.has_value());
    return std::move(*application);
  }

  static auto hosted(provider::headless::HeadlessProviderFactory& providers,
                     const Host::Build* build) -> Application {
    load_fonts(providers);
    auto application = Application::create(
        providers, Host{build},
        ApplicationOptions{
            .executors = std::nullopt,
            .fonts = examples::GalleryFonts::load(providers.assets()),
            .atlas = {},
            .interaction = {},
            .appearance = std::nullopt});
    EXPECT_TRUE(application.has_value());
    Application started = std::move(*application);
    run(providers, started, 3);
    return started;
  }

  static void until(provider::headless::HeadlessProviderFactory& providers,
                    Application& application, std::chrono::milliseconds when,
                    std::chrono::milliseconds& now) {
    while (now < when) {
      ASSERT_TRUE(providers.clock().advance(frame_step).has_value());
      ASSERT_TRUE(application.frame().has_value());
      now += frame_step;
    }
  }

  struct Stroke {
    std::size_t vertices{0};
    std::uint32_t alpha{0};
  };

  static auto stroke(provider::headless::HeadlessProviderFactory& providers)
      -> Stroke {
    Stroke found;
    const auto record = providers.gpu().last_shapes();
    std::vector<bool> seen(record.vertices.size(), false);
    for (const provider::DrawCommand& command : record.commands) {
      const auto indices = std::span{record.indices}.subspan(
          command.index_offset, command.index_count);
      for (const provider::Index index : indices) {
        const std::size_t at = index + command.vertex_offset;
        if (at >= record.vertices.size() || seen[at]) {
          continue;
        }
        seen[at] = true;
        const provider::ShapeVertex& vertex = record.vertices[at];
        const bool stroked = vertex.kind == provider::ShapeKind::path ||
                             vertex.kind == provider::ShapeKind::gradient;
        if (!stroked || vertex.local[1] <= 0.0F) {
          continue;
        }
        if (vertex.local[1] >= std::max(vertex.shape[2], command.trim[0]) &&
            vertex.local[1] <= std::min(vertex.shape[3], command.trim[1])) {
          ++found.vertices;
        }
        found.alpha = std::max(found.alpha, vertex.colour >> 24U);
      }
    }
    return found;
  }

  static auto rings(provider::headless::HeadlessProviderFactory& providers,
                    const composition::Rect& around, float scale)
      -> std::size_t {
    const auto record = providers.gpu().last_shapes();
    std::size_t found = 0;
    for (std::size_t base = 0; base + 3 < record.vertices.size(); base += 4) {
      const provider::ShapeVertex& first = record.vertices[base];
      const provider::ShapeVertex& opposite = record.vertices[base + 2];
      if (first.kind != provider::ShapeKind::stroke) {
        continue;
      }
      const float left = first.position[0] / scale;
      const float top = first.position[1] / scale;
      const float right = opposite.position[0] / scale;
      const float bottom = opposite.position[1] / scale;
      if (left < around.x - 1.0F && top < around.y - 1.0F &&
          right > around.x + around.width + 1.0F &&
          bottom > around.y + around.height + 1.0F &&
          right - left < around.width + 24.0F) {
        ++found;
      }
    }
    return found;
  }

  static auto focused_frame(Application& application)
      -> std::optional<composition::Rect> {
    const auto focused = application.interaction().focused();
    if (!focused.has_value()) {
      return std::nullopt;
    }
    const auto node = application.root().tree().node(*focused);
    if (!node.has_value() || !node->get().presented().has_value()) {
      return std::nullopt;
    }
    return node->get().presented()->bounds;
  }

  static auto inside_key(Application& application, composition::NodeId id,
                         std::string_view key) -> bool {
    return within(application, id, key);
  }
};

TEST_F(GalleryM6eTest, LaunchWritingFollowsThePlanBeatsAndFadesIntoTheApp) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      launch(providers, examples::LaunchScreen::Ending::fade);
  std::chrono::milliseconds now{0};
  std::vector<Stroke> beats;
  for (const int moment : {500, 1500, 2700, 3500, 4500, 5500}) {
    until(providers, application, std::chrono::milliseconds{moment}, now);
    beats.push_back(stroke(providers));
  }
  EXPECT_GT(beats[0].vertices, 0U);
  EXPECT_LT(beats[0].vertices, beats[1].vertices);
  EXPECT_LT(beats[1].vertices, beats[2].vertices);
  EXPECT_EQ(beats[2].vertices, beats[3].vertices);
  EXPECT_EQ(beats[3].alpha, 255U);
  EXPECT_LT(beats[4].alpha, 250U);
  EXPECT_EQ(beats[5].vertices, 0U);
  EXPECT_FALSE(
      node_of(application, examples::LaunchScreen::root_view).has_value());
  const auto focused = application.interaction().focused();
  ASSERT_TRUE(focused.has_value());
  EXPECT_FALSE(
      inside_key(application, *focused, examples::LaunchScreen::root_view));
}

TEST_F(GalleryM6eTest, LaunchWritingTailChaseErasesBeforeTheFade) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      launch(providers, examples::LaunchScreen::Ending::erase);
  std::chrono::milliseconds now{0};
  until(providers, application, std::chrono::milliseconds{3500}, now);
  const Stroke held = stroke(providers);
  until(providers, application, std::chrono::milliseconds{5000}, now);
  const Stroke erasing = stroke(providers);
  until(providers, application, std::chrono::milliseconds{5600}, now);
  const Stroke late = stroke(providers);
  EXPECT_LT(erasing.vertices, held.vertices);
  EXPECT_LT(late.vertices, erasing.vertices);
  EXPECT_EQ(erasing.alpha, 255U);
}

TEST_F(GalleryM6eTest, AnyButtonSkipsTheLaunchWriting) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      launch(providers, examples::LaunchScreen::Ending::fade);
  std::chrono::milliseconds now{0};
  until(providers, application, std::chrono::milliseconds{1000}, now);
  ASSERT_TRUE(
      node_of(application, examples::LaunchScreen::root_view).has_value());
  press(providers, application, provider::Button::y);
  until(providers, application, std::chrono::milliseconds{1500}, now);
  EXPECT_FALSE(
      node_of(application, examples::LaunchScreen::root_view).has_value());
  const auto focused = application.interaction().focused();
  ASSERT_TRUE(focused.has_value());
  EXPECT_FALSE(
      inside_key(application, *focused, examples::LaunchScreen::root_view));
}

TEST_F(GalleryM6eTest, LaunchPageReplaysTheLaunchWriting) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start_gallery(providers, examples::GalleryPage::launch);
  EXPECT_FALSE(
      node_of(application, examples::LaunchScreen::root_view).has_value());
  ASSERT_TRUE(application.root().activate(
      composition::ElementKey{examples::LaunchPage::erase_button}));
  run(providers, application, 3);
  EXPECT_TRUE(
      node_of(application, examples::LaunchScreen::root_view).has_value());
}

TEST_F(GalleryM6eTest, FirstFrameIsTheLaunchWallpaperAtFullOpacity) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      launch(providers, examples::LaunchScreen::Ending::fade);
  ASSERT_TRUE(application.frame().has_value());
  std::size_t mesh = 0;
  std::uint32_t faintest = 255;
  for (const provider::ShapeVertex& vertex :
       providers.gpu().last_shapes().vertices) {
    if (vertex.kind == provider::ShapeKind::mesh) {
      ++mesh;
      faintest = std::min(faintest, vertex.colour >> 24U);
    }
  }
  EXPECT_EQ(providers.gpu().frames_presented(), 1U);
  EXPECT_GT(mesh, 0U);
  EXPECT_EQ(faintest, 255U);
  const auto clear = providers.gpu().last_clear();
  ASSERT_TRUE(clear.has_value());
  const theme::Colour navy =
      theme::Colour::hex(examples::LaunchScreen::wallpaper[5]);
  EXPECT_NEAR(clear->red, navy.red, 0.01F);
  EXPECT_NEAR(clear->blue, navy.blue, 0.01F);
  EXPECT_LT(clear->green, 0.9F);
}

TEST_F(GalleryM6eTest, FirstFrameHasTheFinalScaleWithoutAZoom) {
  provider::headless::HeadlessProviderFactory providers;
  const Host::Build build = [] {
    return composition::Element{components::Text("still")};
  };
  load_fonts(providers);
  providers.surface().set_scale(provider::SurfaceScale::create(1.25F).value_or(
      provider::SurfaceScale::unit()));
  auto created = Application::create(
      providers, Host{&build},
      ApplicationOptions{
          .executors = std::nullopt,
          .fonts = examples::GalleryFonts::load(providers.assets()),
          .atlas = {},
          .interaction = {},
          .appearance = std::nullopt});
  ASSERT_TRUE(created.has_value());
  ASSERT_TRUE(created->frame().has_value());
  EXPECT_TRUE(created->last_frame().presented);
  EXPECT_FALSE(created->last_frame().animating);
}

TEST_F(GalleryM6eTest, WriteOnCostsNoTessellationAndPresentsEveryFrame) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      launch(providers, examples::LaunchScreen::Ending::fade);
  std::chrono::milliseconds now{0};
  until(providers, application, std::chrono::milliseconds{500}, now);
  const std::size_t uploads = providers.gpu().uploads();
  int frames = 0;
  int presented = 0;
  std::uint32_t tessellations = 0;
  std::uint32_t meshes = 0;
  while (now < std::chrono::milliseconds{2300}) {
    ASSERT_TRUE(providers.clock().advance(frame_step).has_value());
    ASSERT_TRUE(application.frame().has_value());
    now += frame_step;
    const FrameTimings& timings = application.last_frame();
    ++frames;
    presented += timings.presented ? 1 : 0;
    tessellations += timings.tessellations;
    meshes += timings.mesh_builds;
  }
  EXPECT_EQ(tessellations, 0U);
  EXPECT_EQ(presented, frames);
  EXPECT_LE(meshes, static_cast<std::uint32_t>((frames / 4) + 2));
  EXPECT_EQ(providers.gpu().uploads(), uploads);
}

TEST_F(GalleryM6eTest, FeedPagesThroughTheFakeSourceAsItScrolls) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start_gallery(providers, examples::GalleryPage::feed);
  run(providers, application, 10);
  const auto count = [&application] {
    return count_keyed(application.root().tree().root(), "");
  };
  const auto feed = node_of(application, examples::FeedPage::feed_view);
  ASSERT_TRUE(feed.has_value());
  const std::size_t initial = count();
  press(providers, application, provider::Button::dpad_right);
  run(providers, application, 20);
  for (int index = 0; index < 30; ++index) {
    press(providers, application, provider::Button::zr);
    run(providers, application, 8);
  }
  run(providers, application, 20);
  EXPECT_GE(count(), initial / 2);
  EXPECT_FALSE(
      node_of(application, examples::FeedPage::loading_row).has_value());
  const auto scroller = application.interaction().scrolls().scroller(*feed);
  ASSERT_TRUE(scroller.has_value());
  const float limit = scroller->get().metrics().content.height -
                      scroller->get().metrics().viewport.height;
  EXPECT_GT(limit, static_cast<float>(examples::FeedPage::page_size *
                                      (examples::FeedPage::page_count - 1)) *
                       30.0F);
}

TEST_F(GalleryM6eTest, InheritedFontAndNestedDynamicTypeClamp) {
  provider::headless::HeadlessProviderFactory providers;
  const Host::Build build = [] {
    return composition::Element{components::VStack(
        components::VStack(components::Text("Header").id("inherited"))
            .font(theme::Font::large_title),
        components::Text("Header").id("plain"),
        components::VStack(
            components::Text("Header").font(theme::Font::body).id("own"))
            .font(theme::Font::large_title),
        components::VStack(
            components::Text("Header")
                .dynamic_type_size(theme::DynamicTypeSize::accessibility5)
                .id("clamped"))
            .dynamic_type_size(
                theme::DynamicTypeRange{theme::DynamicTypeSize::small,
                                        theme::DynamicTypeSize::large}))};
  };
  Application application = hosted(providers, &build);
  EXPECT_GT(frame_of(application, "inherited").height,
            frame_of(application, "plain").height + 4.0F);
  EXPECT_NEAR(frame_of(application, "own").height,
              frame_of(application, "plain").height, 0.5F);
  EXPECT_NEAR(frame_of(application, "clamped").height,
              frame_of(application, "plain").height, 0.5F);
}

TEST_F(GalleryM6eTest, SectionHeadersUseSwiftUIHeaderFonts) {
  provider::headless::HeadlessProviderFactory providers;
  const Host::Build build = [] {
    return composition::Element{components::VStack(
        composition::Element{structure::Form(structure::Section(
                                 "Grouped", components::Text("row")))}
            .frame({.height = 160.0F})
            .id("form"),
        composition::Element{structure::List(structure::Section(
                                 "Plain", components::Text("row")))}
            .list_style(structure::ListStyle::plain())
            .frame({.height = 160.0F})
            .id("list"))};
  };
  Application application = hosted(providers, &build);
  std::optional<float> grouped;
  std::optional<float> plain;
  walk(application.root().tree().root(), [&](const composition::Node& node) {
    if (const auto text = node.content_as<components::Text::Content>()) {
      if (text->get().text() == "Grouped") {
        grouped = node.frame().height;
      } else if (text->get().text() == "Plain") {
        plain = node.frame().height;
      }
    }
  });
  ASSERT_TRUE(grouped.has_value());
  ASSERT_TRUE(plain.has_value());
  EXPECT_GT(*grouped, *plain);
}

TEST_F(GalleryM6eTest, ThemeFilesCarryMotionTokens) {
  const auto parsed = appearance::ThemeFile::parse(
      R"(schema = 1
extends = "default"

[motion.focus]
response = 0.25
damping_ratio = 0.6
)",
      "motion.toml", [](std::string_view) { return theme::Theme::defaults(); });
  ASSERT_TRUE(parsed.has_value());
  EXPECT_NEAR(parsed->motion().focus.response, 0.25, 1.0e-6);
  EXPECT_NEAR(parsed->motion().focus.damping_ratio, 0.6, 1.0e-6);
  const auto wrong = appearance::ThemeFile::parse(
      "schema = 1\n[motion.focus]\nbounce = 1\n", "wrong.toml",
      [](std::string_view) { return theme::Theme::defaults(); });
  EXPECT_FALSE(wrong.has_value());
}

TEST_F(GalleryM6eTest, SidebarRowsShowTheCapsuleWithoutARing) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start_gallery(providers, examples::GalleryPage::controls);
  const auto focused = application.interaction().focused();
  ASSERT_TRUE(focused.has_value());
  ASSERT_TRUE(within(application, *focused, "arisa.tab.sidebar"));
  run(providers, application, 4);
  const auto shown = focused_frame(application);
  ASSERT_TRUE(shown.has_value());
  EXPECT_EQ(rings(providers, *shown, 1.0F), 0U);
  press(providers, application, provider::Button::dpad_right);
  run(providers, application, 30);
  const auto content = application.interaction().focused();
  ASSERT_TRUE(content.has_value());
  EXPECT_FALSE(within(application, *content, "arisa.tab.sidebar"));
  const auto control = focused_frame(application);
  ASSERT_TRUE(control.has_value());
  EXPECT_GT(rings(providers, *control, 1.0F), 0U);
}

TEST_F(GalleryM6eTest, NavigationBackPopsInnermostFirst) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start_gallery(providers, examples::GalleryPage::navigation);
  ASSERT_TRUE(application.root().activate(
      composition::ElementKey{examples::NavigationPage::jump_button}));
  run(providers, application, 40);
  for (int depth = 0; depth < 2; ++depth) {
    if (const auto deeper = node_of(
            application, examples::NavigationPage::Detail::deeper_link)) {
      application.root().activate(*deeper);
      run(providers, application, 40);
    }
  }
  const auto& chapters = examples::NavigationPage::path;
  static_cast<void>(chapters);
  for (int pop = 0; pop < 3; ++pop) {
    const auto before = application.interaction().focused();
    ASSERT_TRUE(before.has_value());
    EXPECT_FALSE(within(application, *before, "arisa.tab.sidebar")) << pop;
    press(providers, application, provider::Button::b);
    run(providers, application, 40);
  }
  const auto after = application.interaction().focused();
  ASSERT_TRUE(after.has_value());
  EXPECT_FALSE(within(application, *after, "arisa.tab.sidebar"));
}

TEST_F(GalleryM6eTest, SwitchThemeRepaintsTheCard) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start_gallery(providers, examples::GalleryPage::theming);
  const auto fills = [&providers] {
    std::vector<std::uint32_t> found;
    for (const provider::ShapeVertex& vertex :
         providers.gpu().last_shapes().vertices) {
      if (vertex.kind == provider::ShapeKind::fill) {
        found.push_back(vertex.colour);
      }
    }
    std::ranges::sort(found);
    found.erase(std::unique(found.begin(), found.end()), found.end());
    return found;
  };
  drag(providers, application, provider::PixelPoint{.x = 760.0F, .y = 560.0F},
       provider::PixelPoint{.x = 760.0F, .y = 160.0F}, 12);
  run(providers, application, 90);
  const auto before = fills();
  ASSERT_TRUE(application.root().activate(
      composition::ElementKey{examples::ThemingPage::switch_button}));
  run(providers, application, 60);
  EXPECT_TRUE(shows(application, "Warm"));
  const auto after = fills();
  EXPECT_NE(before, after);
  const theme::Colour orange = theme::Colour::hex(0xFF9500);
  const std::uint32_t orange_packed =
      0xFF000000U |
      (static_cast<std::uint32_t>(std::lround(orange.blue * 255.0F)) << 16U) |
      (static_cast<std::uint32_t>(std::lround(orange.green * 255.0F)) << 8U) |
      static_cast<std::uint32_t>(std::lround(orange.red * 255.0F));
  EXPECT_TRUE(std::ranges::contains(after, orange_packed));
}

TEST_F(GalleryM6eTest, RailLabelFadesAfterItsLingerAndReturnsOnMove) {
  provider::headless::HeadlessProviderFactory providers;
  AppearanceOptions options{
      .policy = examples::GalleryScreen::policy(std::nullopt), .storage = {}};
  options.policy.defaults.sidebar =
      composition::NavigationSplitViewVisibility::icons_only;
  Application application = start_gallery(
      providers, examples::GalleryPage::typography, theme::ColorScheme::light,
      provider::headless::ScriptedSurface::default_size, std::move(options));
  const auto faded = [&application] {
    const auto pill = node_of(application, structure::TabView::rail_label_id);
    if (!pill.has_value()) {
      return std::optional<bool>{};
    }
    bool hidden = false;
    walk(application.root().tree().node(*pill)->get(),
         [&hidden](const composition::Node& node) {
           hidden = hidden || node.modifiers().opacity < 0.01F;
         });
    return std::optional<bool>{hidden};
  };
  run(providers, application, 10);
  ASSERT_TRUE(faded().has_value());
  EXPECT_FALSE(*faded());
  int presented = 0;
  for (int index = 0; index < 60; ++index) {
    ASSERT_TRUE(providers.clock().advance(frame_step).has_value());
    ASSERT_TRUE(application.frame().has_value());
    presented += application.last_frame().presented ? 1 : 0;
  }
  EXPECT_LE(presented, 2);
  run(providers, application, 120);
  ASSERT_TRUE(faded().has_value());
  EXPECT_TRUE(*faded());
  press(providers, application, provider::Button::dpad_down);
  run(providers, application, 4);
  ASSERT_TRUE(faded().has_value());
  EXPECT_FALSE(*faded());
}

}

namespace arisa::runtime {

TEST_F(GalleryM6eTest, HiddenSidebarHasNoFocusTargetsAndLeftEdgeRevealsIt) {
  provider::headless::HeadlessProviderFactory providers;
  AppearanceOptions options{
      .policy = examples::GalleryScreen::policy(std::nullopt), .storage = {}};
  options.policy.defaults.sidebar =
      composition::NavigationSplitViewVisibility::detail_only;
  Application application = start_gallery(
      providers, examples::GalleryPage::counter, theme::ColorScheme::light,
      provider::headless::ScriptedSurface::default_size, std::move(options));
  run(providers, application, 20);
  for (const composition::NodeId id : focusables(application)) {
    EXPECT_FALSE(within(application, id, "arisa.tab.sidebar"));
    const auto node = application.root().tree().node(id);
    ASSERT_TRUE(node.has_value());
    if (node->get().presented().has_value() &&
        node->get().presented()->visible) {
      EXPECT_GT(node->get().presented()->bounds.width, 0.0F);
    }
  }
  const auto sidebar_width = [&application] {
    const auto detail =
        node_of(application, structure::NavigationSplitView::detail_id);
    if (!detail.has_value()) {
      return 0.0F;
    }
    return application.root().tree().node(*detail)->get().frame().x;
  };
  EXPECT_LE(sidebar_width(), 1.0F);
  press(providers, application, provider::Button::dpad_down);
  run(providers, application, 20);
  const auto inside = application.interaction().focused();
  ASSERT_TRUE(inside.has_value());
  EXPECT_TRUE(
      within(application, *inside, structure::NavigationSplitView::detail_id));
  for (int press_count = 0; press_count < 8; ++press_count) {
    press(providers, application, provider::Button::dpad_left);
    run(providers, application, 30);
  }
  EXPECT_GT(sidebar_width(), 40.0F);
}

}

namespace arisa::runtime {

TEST_F(GalleryM6eTest, EveryGalleryAndScaffoldSymbolIsInTheIconFont) {
  for (const examples::GalleryPage page : examples::GalleryPages::all) {
    EXPECT_TRUE(components::SystemSymbols::codepoint(
                    examples::GalleryPages::symbol(page))
                    .has_value())
        << examples::GalleryPages::name(page);
  }
  EXPECT_TRUE(components::SystemSymbols::codepoint("left_panel_open"));
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start_gallery(providers, examples::GalleryPage::icons);
  run(providers, application, 10);
  for (const provider::Diagnostic& each : providers.diagnostics().snapshot()) {
    EXPECT_NE(each.category, "symbol") << each.message;
  }
}

TEST_F(GalleryM6eTest, UnknownSymbolReportsADiagnosticAndDrawsAPlaceholder) {
  provider::headless::HeadlessProviderFactory providers;
  const Host::Build build = [] {
    return composition::Element{components::Image::system_name("no_such_symbol")
                                    .font(theme::Font::title3)};
  };
  Application application = hosted(providers, &build);
  std::size_t reported = 0;
  for (const provider::Diagnostic& each : providers.diagnostics().snapshot()) {
    reported += each.category == "symbol" ? 1U : 0U;
  }
  EXPECT_EQ(reported, 1U);
  std::size_t dashed = 0;
  for (const provider::ShapeVertex& vertex :
       providers.gpu().last_shapes().vertices) {
    dashed += vertex.kind == provider::ShapeKind::path ? 1U : 0U;
  }
  EXPECT_GT(dashed, 0U);
}

TEST_F(GalleryM6eTest, BackDismissesASheetBeforeLeavingThePage) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start_gallery(providers, examples::GalleryPage::presentations);
  const auto before = application.interaction().focused();
  ASSERT_TRUE(application.root().activate(
      composition::ElementKey{examples::PresentationsPage::sheet_button}));
  run(providers, application, 40);
  ASSERT_TRUE(node_of(application, examples::PresentationsPage::done_button)
                  .has_value());
  press(providers, application, provider::Button::b);
  run(providers, application, 40);
  EXPECT_FALSE(node_of(application, examples::PresentationsPage::done_button)
                   .has_value());
  EXPECT_TRUE(node_of(application, examples::PresentationsPage::sheet_button)
                  .has_value());
  EXPECT_EQ(application.interaction().focused(), before);
}

}
