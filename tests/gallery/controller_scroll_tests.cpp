#include <arisa/composition/layer.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/node_id.hpp>
#include <arisa/composition/overlay_layer.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/structure/navigation_split_view.hpp>
#include <arisa/structure/scaffold.hpp>
#include <arisa/structure/tab_view.hpp>
#include <presentations_page.hpp>

#include <arisa/composition/navigation_split_view_visibility.hpp>
#include <arisa/theme/dynamic_type_size.hpp>
#include <arisa/theme/metrics.hpp>
#include <arisa/theme/space.hpp>
#include <gallery_fixture.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace arisa::runtime {

class GalleryControllerScrollTest
    : public GalleryFixture,
      public ::testing::WithParamInterface<examples::GalleryPage> {
 protected:
  static constexpr int page_presses = 160;

  static auto innermost(Application& application, composition::NodeId id)
      -> std::optional<composition::NodeId> {
    const composition::Tree& tree = application.root().tree();
    std::optional<composition::NodeId> at = id;
    while (at.has_value()) {
      const auto node = tree.node(*at);
      if (!node.has_value()) {
        return std::nullopt;
      }
      if (*at != id &&
          node->get().interactions().scrolling.container.has_value()) {
        return at;
      }
      at = node->get().parent();
    }
    return std::nullopt;
  }

  static auto targets(Application& application)
      -> std::vector<composition::NodeId> {
    std::vector<composition::NodeId> found;
    for (const composition::NodeId id : focusables(application)) {
      if (!in_page(application, id)) {
        continue;
      }
      const auto container = innermost(application, id);
      if (container.has_value() && !std::ranges::contains(found, *container) &&
          within(application, *container, structure::Scaffold::content_key)) {
        found.push_back(*container);
      }
    }
    return found;
  }

  static auto inside(Application& application, composition::NodeId container)
      -> bool {
    const auto focused = application.interaction().focused();
    return focused.has_value() && innermost(application, *focused) == container;
  }

  static auto nested_in(Application& application, composition::NodeId container)
      -> bool {
    const auto focused = application.interaction().focused();
    std::optional<composition::NodeId> at =
        focused.has_value() ? innermost(application, *focused) : std::nullopt;
    while (at.has_value() && *at != container) {
      at = innermost(application, *at);
    }
    return at.has_value();
  }

  static auto enter(provider::headless::HeadlessProviderFactory& providers,
                    Application& application, composition::NodeId container)
      -> bool {
    for (int hop = 0; hop < 3; ++hop) {
      if (inside(application, container)) {
        return true;
      }
      const auto before = application.interaction().focused();
      press(providers, application, provider::Button::dpad_right);
      run(providers, application, 12);
      if (application.interaction().focused() == before) {
        press(providers, application, provider::Button::dpad_down);
        run(providers, application, 12);
      }
    }
    for (const provider::Button button :
         {provider::Button::dpad_down, provider::Button::dpad_up}) {
      for (int hop = 0; hop < 30 && !inside(application, container); ++hop) {
        press(providers, application, button);
        run(providers, application, 12);
      }
    }
    return inside(application, container);
  }

  struct Travel {
    float offset{0.0F};
    float limit{0.0F};
  };

  static auto travel(Application& application, composition::NodeId container)
      -> Travel {
    const auto scroller =
        application.interaction().scrolls().scroller(container);
    if (!scroller.has_value()) {
      return {};
    }
    return Travel{.offset = scroller->get().offset().y,
                  .limit = scroller->get().limit().y};
  }

  static auto reachable_in_view(Application& application,
                                composition::NodeId container) -> bool {
    const composition::Tree& tree = application.root().tree();
    const auto box = tree.node(container);
    if (!box.has_value() || !box->get().presented().has_value()) {
      return false;
    }
    const composition::Rect& area = box->get().presented()->bounds;
    for (const composition::NodeId id : focusables(application)) {
      const auto node = tree.node(id);
      if (innermost(application, id) != container || !node.has_value() ||
          !node->get().presented().has_value()) {
        continue;
      }
      const composition::Rect& shown = node->get().presented()->bounds;
      if (shown.y >= area.y && shown.y + shown.height <= area.y + area.height) {
        return true;
      }
    }
    return false;
  }

  static auto focus_visible_in(Application& application,
                               composition::NodeId container) -> bool {
    const composition::Tree& tree = application.root().tree();
    const auto focused = application.interaction().focused();
    const auto box = tree.node(container);
    if (!focused.has_value() || !box.has_value() ||
        !box->get().presented().has_value()) {
      return false;
    }
    const auto row = tree.node(*focused);
    if (!row.has_value() || !row->get().presented().has_value()) {
      return false;
    }
    const composition::Rect& area = box->get().presented()->bounds;
    const composition::Rect& shown = row->get().presented()->bounds;
    return shown.y + shown.height > area.y && shown.y < area.y + area.height;
  }
};

TEST_P(GalleryControllerScrollTest, EveryScrollContainerTravelsEndToEnd) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start_gallery(providers, GetParam());
  run(providers, application, 20);
  press(providers, application, provider::Button::dpad_right);
  run(providers, application, 20);
  for (const composition::NodeId container : targets(application)) {
    if (travel(application, container).limit < 1.0F) {
      continue;
    }
    ASSERT_TRUE(enter(providers, application, container))
        << "focus never enters scroll container " << container.value();
    int presses = 0;
    while (presses < page_presses) {
      const Travel now = travel(application, container);
      if (now.offset >= now.limit - 1.0F) {
        break;
      }
      press(providers, application, provider::Button::zr);
      run(providers, application, 8);
      ++presses;
    }
    run(providers, application, 40);
    const Travel bottom = travel(application, container);
    EXPECT_GE(bottom.offset, bottom.limit - 1.0F)
        << "ZR did not reach the end of " << container.value();
    EXPECT_TRUE(!reachable_in_view(application, container) ||
                focus_visible_in(application, container));
    press(providers, application, provider::Button::left_stick);
    run(providers, application, 90);
    EXPECT_LE(travel(application, container).offset, 1.0F);
    EXPECT_TRUE(focus_visible_in(application, container));
    press(providers, application, provider::Button::right_stick);
    run(providers, application, 90);
    const Travel end = travel(application, container);
    EXPECT_GE(end.offset, end.limit - 1.0F);
    press(providers, application, provider::Button::zl);
    run(providers, application, 40);
    EXPECT_LT(travel(application, container).offset, end.offset - 1.0F);
    EXPECT_TRUE(!reachable_in_view(application, container) ||
                focus_visible_in(application, container));
  }
}

TEST_P(GalleryControllerScrollTest, UpAfterTheEndStaysInsideUntilTheTop) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start_gallery(providers, GetParam());
  run(providers, application, 20);
  press(providers, application, provider::Button::dpad_right);
  run(providers, application, 20);
  for (const composition::NodeId container : targets(application)) {
    if (travel(application, container).limit < 1.0F ||
        !enter(providers, application, container)) {
      continue;
    }
    press(providers, application, provider::Button::right_stick);
    run(providers, application, 90);
    bool left = false;
    for (int presses = 0; presses < page_presses; ++presses) {
      if (!nested_in(application, container)) {
        left = true;
        break;
      }
      press(providers, application, provider::Button::dpad_up);
      run(providers, application, 12);
    }
    run(providers, application, 40);
    EXPECT_TRUE(!left || travel(application, container).offset <= 1.0F)
        << examples::GalleryPages::name(GetParam()) << " left container "
        << container.value() << " before reaching the top";
  }
}

TEST_P(GalleryControllerScrollTest,
       LastFocusedItemIsFullyVisibleAtHandheldScale) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start_gallery(
      providers, GetParam(), theme::ColorScheme::light,
      provider::headless::ScriptedSurface::default_size, std::nullopt, 1.25F);
  run(providers, application, 20);
  press(providers, application, provider::Button::dpad_right);
  run(providers, application, 20);
  std::optional<composition::NodeId> previous;
  for (int presses = 0; presses < page_presses; ++presses) {
    const auto focused = application.interaction().focused();
    if (focused == previous) {
      break;
    }
    previous = focused;
    press(providers, application, provider::Button::dpad_down);
    run(providers, application, 16);
  }
  run(providers, application, 60);
  const auto focused = application.interaction().focused();
  ASSERT_TRUE(focused.has_value());
  if (!in_page(application, *focused)) {
    return;
  }
  const composition::Tree& tree = application.root().tree();
  const auto content =
      tree.find(composition::ElementKey{structure::Scaffold::content_key});
  ASSERT_TRUE(content.has_value());
  const auto area = tree.node(*content)->get().presented();
  const auto shown = tree.node(*focused)->get().presented();
  ASSERT_TRUE(area.has_value());
  ASSERT_TRUE(shown.has_value());
  const float outset = theme::metrics::focus_ring.outset;
  EXPECT_GE(shown->bounds.y - outset, area->bounds.y - 1.0F)
      << examples::GalleryPages::name(GetParam());
  EXPECT_LE(shown->bounds.y + shown->bounds.height + outset,
            area->bounds.y + area->bounds.height + 1.0F)
      << examples::GalleryPages::name(GetParam());
}

TEST_F(GalleryControllerScrollTest, RightStickScrollsTheListAndFocusFollows) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start_gallery(providers, examples::GalleryPage::scroll);
  run(providers, application, 20);
  press(providers, application, provider::Button::dpad_right);
  run(providers, application, 20);
  const auto containers = targets(application);
  ASSERT_FALSE(containers.empty());
  const composition::NodeId container = *std::ranges::max_element(
      containers, {}, [&application](composition::NodeId id) {
        return travel(application, id).limit;
      });
  ASSERT_TRUE(enter(providers, application, container));
  const float before = travel(application, container).offset;
  provider::InputFrame tilted = pad({});
  tilted.pads[0].right = provider::Stick{.x = 0.0F, .y = -1.0F};
  hold(providers, application, tilted, std::chrono::milliseconds{1500});

  run(providers, application, 30);
  EXPECT_GT(travel(application, container).offset, before + 200.0F);
  EXPECT_TRUE(focus_visible_in(application, container));
}

INSTANTIATE_TEST_SUITE_P(
    Pages, GalleryControllerScrollTest,
    ::testing::ValuesIn(examples::GalleryPages::all.begin(),
                        examples::GalleryPages::all.end()),
    [](const ::testing::TestParamInfo<examples::GalleryPage>& shown) {
      return std::string{examples::GalleryPages::name(shown.param)};
    });

}

namespace arisa::runtime {

class GallerySidebarScrollTest : public GalleryFixture,
                                 public ::testing::WithParamInterface<
                                     std::tuple<theme::DynamicTypeSize, bool>> {
 protected:
  static auto options(theme::DynamicTypeSize size, bool rail)
      -> AppearanceOptions {
    AppearanceOptions chosen{
        .policy = examples::GalleryScreen::policy(theme::ColorScheme::light),
        .storage = std::nullopt};
    chosen.policy.defaults.dynamic_type_size = size;
    if (rail) {
      chosen.policy.defaults.sidebar =
          composition::NavigationSplitViewVisibility::icons_only;
    }
    return chosen;
  }

  static auto visible(Application& application)
      -> std::optional<composition::Rect> {
    const auto sidebar = node_of(application, "arisa.tab.sidebar");
    if (!sidebar.has_value()) {
      return std::nullopt;
    }
    const composition::Tree& tree = application.root().tree();
    const auto node = tree.node(*sidebar);
    if (!node.has_value() || !node->get().presented().has_value()) {
      return std::nullopt;
    }
    composition::Rect area = node->get().presented()->bounds;
    const theme::Edges& safe = node->get().safe_area();
    area.y += safe.top;
    area.height -= safe.vertical();
    if (const auto& clip = node->get().presented()->clip) {
      const float top = std::max(area.y, clip->y);
      const float bottom =
          std::min(area.y + area.height, clip->y + clip->height);
      area.y = top;
      area.height = bottom - top;
    }
    return area;
  }
};

TEST_P(GallerySidebarScrollTest, EveryEntryIsReachableAndFullyVisible) {
  const auto [size, rail] = GetParam();
  provider::headless::HeadlessProviderFactory providers;
  Application application = start_gallery(
      providers, examples::GalleryPage::typography, theme::ColorScheme::light,
      provider::headless::ScriptedSurface::default_size, options(size, rail),
      1.25F);
  run(providers, application, 20);
  std::vector<composition::NodeId> seen;
  for (std::size_t step = 0; step < examples::GalleryPages::all.size() + 4;
       ++step) {
    const auto focused = application.interaction().focused();
    ASSERT_TRUE(focused.has_value());
    const auto area = visible(application);
    ASSERT_TRUE(area.has_value());
    const auto node = application.root().tree().node(*focused);
    ASSERT_TRUE(node.has_value() && node->get().presented().has_value());
    const composition::Rect& shown = node->get().presented()->bounds;
    EXPECT_GE(shown.y, area->y - 1.0F) << "step " << step;
    EXPECT_LE(shown.y + shown.height, area->y + area->height + 1.0F)
        << "step " << step;
    if (!std::ranges::contains(seen, *focused)) {
      seen.push_back(*focused);
    }
    press(providers, application, provider::Button::dpad_down);
    run(providers, application, 30);
  }
  EXPECT_GE(seen.size(), examples::GalleryPages::all.size());
}

INSTANTIATE_TEST_SUITE_P(
    Sizes, GallerySidebarScrollTest,
    ::testing::Combine(
        ::testing::Values(theme::DynamicTypeSize::large,
                          theme::DynamicTypeSize::accessibility3),
        ::testing::Bool()));

}

namespace arisa::runtime {

class GallerySafeAreaTest
    : public GalleryFixture,
      public ::testing::WithParamInterface<std::tuple<
          examples::GalleryPage, float, theme::DynamicTypeSize, bool>> {
 protected:
  static auto bars(Application& application) -> std::pair<float, float> {
    const composition::Tree& tree = application.root().tree();
    const auto header =
        tree.find(composition::ElementKey{structure::Scaffold::header_key});
    const auto footer =
        tree.find(composition::ElementKey{structure::Scaffold::footer_key});
    const float top = header.has_value()
                          ? tree.node(*header)->get().frame().y +
                                tree.node(*header)->get().frame().height
                          : 0.0F;
    const float bottom = footer.has_value()
                             ? tree.node(*footer)->get().frame().y
                             : std::numeric_limits<float>::max();
    return {top, bottom};
  }
};

TEST_P(GallerySafeAreaTest, ContentRestsBetweenTheBarsAndFocusStaysClear) {
  const auto [page, scale, size, floating] = GetParam();
  AppearanceOptions options{
      .policy = examples::GalleryScreen::policy(theme::ColorScheme::light),
      .storage = std::nullopt};
  options.policy.defaults.dynamic_type_size = size;
  if (floating) {
    options.policy.defaults.theme = "arisa-theme-glass";
  }
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start_gallery(providers, page, theme::ColorScheme::light,
                    provider::headless::ScriptedSurface::default_size,
                    std::move(options), scale);
  run(providers, application, 20);
  const auto [top, bottom] = bars(application);
  const composition::Tree& tree = application.root().tree();
  const auto content =
      tree.find(composition::ElementKey{structure::Scaffold::content_key});
  ASSERT_TRUE(content.has_value());
  const composition::Rect area = tree.node(*content)->get().frame();
  if (floating) {
    EXPECT_NEAR(area.y, 0.0F, 1.0F);
  } else {
    EXPECT_NEAR(area.y, top, 1.0F);
    EXPECT_NEAR(area.y + area.height, bottom - 0.5F, 1.0F);
  }
  press(providers, application, provider::Button::dpad_right);
  run(providers, application, 20);
  for (int step = 0; step < 40; ++step) {
    const auto focused = application.interaction().focused();
    if (focused.has_value() &&
        within(application, *focused,
               structure::NavigationSplitView::detail_id)) {
      const auto node = tree.node(*focused);
      if (node.has_value() && node->get().presented().has_value() &&
          node->get().presented()->bounds.height <= bottom - top) {
        const composition::Rect& shown = node->get().presented()->bounds;
        EXPECT_GE(shown.y, top - 1.0F)
            << examples::GalleryPages::name(page) << " step " << step;
        EXPECT_LE(shown.y + shown.height, bottom + 1.0F)
            << examples::GalleryPages::name(page) << " step " << step;
      }
    }
    press(providers, application, provider::Button::dpad_down);
    run(providers, application, 45);
  }
}

INSTANTIATE_TEST_SUITE_P(
    Pages, GallerySafeAreaTest,
    ::testing::Combine(
        ::testing::ValuesIn(examples::GalleryPages::all.begin(),
                            examples::GalleryPages::all.end()),
        ::testing::Values(1.0F, 1.25F),
        ::testing::Values(theme::DynamicTypeSize::large,
                          theme::DynamicTypeSize::accessibility3),
        ::testing::Bool()));

TEST_F(GalleryControllerScrollTest, AStickHeldAtTheLimitPresentsNothing) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start_gallery(
      providers, examples::GalleryPage::typography, theme::ColorScheme::light,
      provider::SurfaceSize{.width = provider::SurfaceWidth{1920},
                            .height = provider::SurfaceHeight{1080}},
      std::nullopt, 1.5F);
  run(providers, application, 60);
  provider::InputFrame tilted = pad({});
  tilted.pads[0].right = provider::Stick{.x = 0.0F, .y = 1.0F};
  for (int frame = 0; frame < 150; ++frame) {
    providers.input().push(tilted);
    run(providers, application, 1);
  }
  int presented = 0;
  std::uint64_t uploaded = 0;
  for (int frame = 0; frame < 60; ++frame) {
    providers.input().push(tilted);
    run(providers, application, 1);
    presented += application.last_frame().presented ? 1 : 0;
    uploaded += application.last_frame().uploaded_bytes;
  }
  EXPECT_EQ(presented, 0) << presented << " frames presented, " << uploaded
                          << " bytes uploaded";
  EXPECT_EQ(uploaded, 0U);
}

TEST_F(GalleryFixture, ToastsRestAboveTheHintBar) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start_gallery(providers, examples::GalleryPage::presentations);
  ASSERT_TRUE(application.root().activate(
      composition::ElementKey{examples::PresentationsPage::toast_button}));
  run(providers, application, 40);
  const composition::Tree& tree = application.root().tree();
  const auto footer =
      tree.find(composition::ElementKey{structure::Scaffold::footer_key});
  ASSERT_TRUE(footer.has_value());
  const float limit = tree.node(*footer)->get().frame().y;
  bool found = false;
  walk(tree.root(), [&](const composition::Node& node) {
    if (node.content_as<composition::Layer::Content>().has_value() &&
        node.content_as<composition::Layer::Content>()->get().layer() ==
            composition::OverlayLayer::toast) {
      for (const composition::Node& child : node.children()) {
        found = true;
        EXPECT_LE(child.frame().y + child.frame().height, limit + 0.5F);
      }
    }
  });
  EXPECT_TRUE(found);
}

}

namespace arisa::runtime {

class GallerySidebarRepaintTest : public GalleryFixture,
                                  public ::testing::WithParamInterface<bool> {
 protected:
  struct Census {
    std::size_t glyphs{0};
    std::size_t capsules{0};
  };

  static auto census(provider::headless::HeadlessProviderFactory& providers,
                     Application& application) -> Census {
    const auto sidebar = node_of(application, "arisa.tab.sidebar");
    Census found;
    if (!sidebar.has_value()) {
      return found;
    }
    const auto node = application.root().tree().node(*sidebar);
    const composition::Rect area = node->get().presented()->bounds;
    const float scale = 1.25F;
    const auto record = providers.gpu().last_shapes();
    for (std::size_t base = 0; base + 3 < record.vertices.size(); base += 4) {
      const provider::ShapeVertex& corner = record.vertices[base];
      const float x = corner.position[0] / scale;
      const float y = corner.position[1] / scale;
      if (x < area.x || x > area.x + area.width || y < area.y ||
          y > area.y + area.height) {
        continue;
      }
      if (corner.kind == provider::ShapeKind::glyph) {
        ++found.glyphs;
      } else if (corner.kind == provider::ShapeKind::fill &&
                 (corner.colour >> 24U) > 200U && corner.half_size[0] > 20.0F) {
        ++found.capsules;
      }
    }
    return found;
  }
};

TEST_P(GallerySidebarRepaintTest, RowsAndCapsuleRepaintAfterAScrollRoundTrip) {
  AppearanceOptions options{
      .policy = examples::GalleryScreen::policy(theme::ColorScheme::light),
      .storage = std::nullopt};
  if (GetParam()) {
    options.policy.defaults.sidebar =
        composition::NavigationSplitViewVisibility::icons_only;
  }
  provider::headless::HeadlessProviderFactory providers;
  Application application = start_gallery(
      providers, examples::GalleryPage::typography, theme::ColorScheme::light,
      provider::headless::ScriptedSurface::default_size, std::move(options),
      1.25F);
  run(providers, application, 30);
  const Census before = census(providers, application);
  ASSERT_GT(before.glyphs, 0U);
  ASSERT_GT(before.capsules, 0U);
  const std::size_t count = examples::GalleryPages::all.size();
  for (std::size_t step = 0; step < count; ++step) {
    press(providers, application, provider::Button::dpad_down);
    run(providers, application, 10);
  }
  run(providers, application, 40);
  const Census bottom = census(providers, application);
  EXPECT_GT(bottom.capsules, 0U);
  EXPECT_GT(bottom.glyphs, before.glyphs / 2);
  for (std::size_t step = 0; step < count; ++step) {
    press(providers, application, provider::Button::dpad_up);
    run(providers, application, 10);
  }
  run(providers, application, 40);
  const Census after = census(providers, application);
  EXPECT_GT(after.capsules, 0U);
  EXPECT_GE(after.glyphs + 4U, before.glyphs);
}

TEST_P(GallerySidebarRepaintTest,
       TheCapsuleStaysOnTheSelectedRowWhileScrolled) {
  AppearanceOptions options{
      .policy = examples::GalleryScreen::policy(theme::ColorScheme::light),
      .storage = std::nullopt};
  if (GetParam()) {
    options.policy.defaults.sidebar =
        composition::NavigationSplitViewVisibility::icons_only;
  }
  provider::headless::HeadlessProviderFactory providers;
  Application application = start_gallery(
      providers, examples::GalleryPage::typography, theme::ColorScheme::light,
      provider::headless::ScriptedSurface::default_size, std::move(options),
      1.25F);
  run(providers, application, 30);
  const composition::Tree& tree = application.root().tree();
  const auto aligned = [&application, &tree] {
    const auto focused = application.interaction().focused();
    const auto capsule =
        tree.find(composition::ElementKey{structure::TabView::capsule_id});
    if (!focused.has_value() || !capsule.has_value()) {
      return false;
    }
    const auto row = tree.node(*focused);
    const auto fill = tree.node(*capsule);
    if (!row.has_value() || !fill.has_value() ||
        !row->get().presented().has_value() ||
        !fill->get().presented().has_value()) {
      return false;
    }
    const composition::Rect ring = row->get().presented()->bounds;
    const composition::Rect shown = fill->get().presented()->bounds;
    return std::abs(ring.y - shown.y) <= 1.0F &&
           std::abs(ring.height - shown.height) <= 1.0F;
  };
  EXPECT_TRUE(aligned());
  const std::size_t count = examples::GalleryPages::all.size();
  for (std::size_t step = 0; step < count; ++step) {
    press(providers, application, provider::Button::dpad_down);
    run(providers, application, 10);
  }
  run(providers, application, 40);
  EXPECT_TRUE(aligned());
  for (std::size_t step = 1; step < count; ++step) {
    press(providers, application, provider::Button::dpad_up);
    run(providers, application, 10);
  }
  run(providers, application, 40);
  EXPECT_TRUE(aligned());
}

INSTANTIATE_TEST_SUITE_P(Modes, GallerySidebarRepaintTest, ::testing::Bool());

}
