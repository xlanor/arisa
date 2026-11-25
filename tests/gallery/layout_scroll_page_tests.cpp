#include <arisa/components/text.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/provider/diagnostics/allocation_meter.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>

#include <gallery_fixture.hpp>
#include <layout_scroll_page.hpp>

#include <gtest/gtest.h>
#include <stb_image.h>
#include <stb_image_write.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::runtime {

class GalleryLayoutScrollTest : public GalleryFixture {
 protected:
  using Page = examples::LayoutScrollPage;

  class Host final : public composition::Component {
   public:
    static constexpr composition::StateKey<bool> shown{"layout-scroll-host",
                                                       true};

    auto operator==(const Host&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      return Page{context.state(shown)};
    }
  };

  static constexpr float tolerance = 0.5F;

  static auto open(provider::headless::HeadlessProviderFactory& providers,
                   bool docked, bool rasterized = false,
                   std::uint32_t handheld_height = 720) -> Application {
    load_fonts(providers);
    const provider::SurfaceSize size =
        docked ? provider::SurfaceSize{.width = provider::SurfaceWidth{1920},
                                       .height = provider::SurfaceHeight{1080}}
               : provider::SurfaceSize{
                     .width = provider::SurfaceWidth{1280},
                     .height = provider::SurfaceHeight{handheld_height}};
    providers.surface().resize(size);
    if (docked) {
      providers.surface().set_scale(
          provider::SurfaceScale::create(1.5F).value_or(
              provider::SurfaceScale::unit()));
      providers.surface().set_display_mode(provider::DisplayMode::docked);
    } else {
      providers.surface().set_display_mode(provider::DisplayMode::handheld);
    }
    if (rasterized) {
      providers.gpu().rasterize(size);
    }
    Application application = start(providers, Host{});
    run(providers, application, 30);
    return application;
  }

  static auto inside(Application& application, std::string_view container)
      -> bool {
    const auto focused = application.interaction().focused();
    return focused.has_value() && within(application, *focused, container);
  }

  static auto walk(provider::headless::HeadlessProviderFactory& providers,
                   Application& application, std::string_view container,
                   provider::Button direction = provider::Button::dpad_down)
      -> bool {
    for (int step = 0; step < 80; ++step) {
      if (inside(application, container)) {
        return true;
      }
      press(providers, application, direction);
      run(providers, application, 2);
    }
    return inside(application, container);
  }

  static auto find_text(const composition::Node& node, std::string_view wanted)
      -> std::optional<composition::Rect> {
    if (const auto text = node.content_as<components::Text::Content>();
        text.has_value() && text->get().text() == wanted) {
      return node.presented().has_value() ? node.presented()->bounds
                                          : node.frame();
    }
    for (const composition::Node& child : node.children()) {
      if (const std::optional<composition::Rect> found =
              find_text(child, wanted)) {
        return found;
      }
    }
    return std::nullopt;
  }

  static auto at(provider::headless::HeadlessProviderFactory& providers,
                 Application& application, float x, float y)
      -> provider::PixelPoint {
    const float points = application.root().tree().root().frame().width;
    const float factor =
        points > 0.0F
            ? static_cast<float>(providers.surface().size().width.value()) /
                  points
            : 1.0F;
    return provider::PixelPoint{.x = x * factor, .y = y * factor};
  }

  static auto middle(provider::headless::HeadlessProviderFactory& providers,
                     Application& application, composition::Rect rect)
      -> provider::PixelPoint {
    return at(providers, application, rect.x + (rect.width * 0.5F),
              rect.y + (rect.height * 0.5F));
  }

  static auto tap_text(provider::headless::HeadlessProviderFactory& providers,
                       Application& application, std::string_view text)
      -> bool {
    const std::optional<composition::Rect> found =
        find_text(application.root().tree().root(), text);
    if (!found.has_value()) {
      return false;
    }
    const provider::PixelPoint point = middle(providers, application, *found);
    touch_down(providers, application, point);
    touch_up(providers, application, point);
    run(providers, application, 30);
    return true;
  }

  static void reveal(provider::headless::HeadlessProviderFactory& providers,
                     Application& application, std::string_view key) {
    const composition::Rect page = frame_of(application, Page::close_button);
    for (int attempt = 0; attempt < 6; ++attempt) {
      const composition::Rect shown = presented_of(application, key);
      const float bottom =
          application.root().tree().root().frame().height - 8.0F;
      if (shown.y >= 0.0F && shown.y + shown.height <= bottom) {
        return;
      }
      const provider::PixelPoint from =
          at(providers, application, page.x * 0.5F, 520.0F);
      const provider::PixelPoint to =
          at(providers, application, page.x * 0.5F, 60.0F);
      drag(providers, application, from, to, 8, false);
      run(providers, application, 8);
      touch_up(providers, application, to);
      run(providers, application, 12);
    }
  }

  static void stick(provider::headless::HeadlessProviderFactory& providers,
                    Application& application, provider::Stick right,
                    std::chrono::milliseconds duration) {
    provider::InputFrame held;
    held.pads[0] = provider::PadState{
        .connected = true, .buttons = {}, .left = {}, .right = right};
    hold(providers, application, held, duration);
    run(providers, application, 60);
  }
};

TEST_F(GalleryLayoutScrollTest, EveryTileShows) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers, false);
  for (const std::string_view title :
       {"Lazy stack", "Safe area", "Negative space", "Arrangement",
        "Carousel",   "Paging",    "Margins",        "Keyboard",
        "Header",     "Ignores",   "Respects",       "Aligned",
        "Padding",    "Spacing",   "Automatic",      "Split",
        "Overlay",    "Content",   "Indicators",     "Immediately",
        "Never"}) {
    EXPECT_TRUE(shows(application, title)) << title;
  }
}

TEST_F(GalleryLayoutScrollTest, LazyStackKeepsItsStaticHeaderAndFooter) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers, false);
  EXPECT_TRUE(node_of(application, Page::lazy_header).has_value());
  EXPECT_TRUE(node_of(application, Page::lazy_footer).has_value());
  EXPECT_TRUE(node_of(application, Page::lazy_row_key(0)).has_value());
  EXPECT_FALSE(node_of(application, Page::lazy_row_key(Page::lazy_rows - 1))
                   .has_value());
  const composition::Rect header = frame_of(application, Page::lazy_header);
  const composition::Rect first = frame_of(application, Page::lazy_row_key(0));
  EXPECT_NEAR(first.y, header.y + header.height + 4.0F, tolerance);
  const composition::Rect viewport = presented_of(application, Page::lazy_view);
  for (int stroke = 0; stroke < 6; ++stroke) {
    scroll_by(providers, application, middle(providers, application, viewport),
              160.0F);
  }
  ASSERT_TRUE(node_of(application, Page::lazy_row_key(Page::lazy_rows - 1))
                  .has_value());
  const composition::Rect last =
      frame_of(application, Page::lazy_row_key(Page::lazy_rows - 1));
  const composition::Rect footer = frame_of(application, Page::lazy_footer);
  EXPECT_NEAR(footer.y, last.y + last.height + 4.0F, tolerance);
  EXPECT_FALSE(node_of(application, Page::lazy_row_key(0)).has_value());
}

TEST_F(GalleryLayoutScrollTest, SafeAreaRegionsAreIgnoredRespectedAndAligned) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers, false);
  const auto offset_in = [&application](std::string_view fill,
                                        std::string_view region) {
    const composition::Rect inner = frame_of(application, fill);
    const composition::Rect outer = frame_of(application, region);
    return composition::Rect{.x = inner.x - outer.x,
                             .y = inner.y - outer.y,
                             .width = inner.width,
                             .height = inner.height};
  };
  const composition::Rect ignored =
      offset_in(Page::ignored_fill, Page::ignored_region);
  EXPECT_NEAR(ignored.y, 0.0F, tolerance);
  EXPECT_NEAR(ignored.height, Page::region_height, tolerance);
  const composition::Rect respected =
      offset_in(Page::respected_fill, Page::respected_region);
  EXPECT_NEAR(respected.y, Page::region_inset, tolerance);
  EXPECT_NEAR(respected.height,
              Page::region_height - (2.0F * Page::region_inset), tolerance);
  const composition::Rect aligned =
      offset_in(Page::aligned_fill, Page::aligned_region);
  EXPECT_NEAR(aligned.y, Page::region_inset * 0.5F, tolerance);
}

TEST_F(GalleryLayoutScrollTest, NegativePaddingAndSpacingOverlap) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers, false);
  const composition::Rect first = frame_of(application, Page::chip_key(0));
  const composition::Rect second = frame_of(application, Page::chip_key(1));
  const composition::Rect third = frame_of(application, Page::chip_key(2));
  const composition::Rect drawn = frame_of(application, Page::overlapped_chip);
  EXPECT_NEAR(second.x, first.x + first.width + Page::chip_overlap, tolerance);
  EXPECT_NEAR(third.x, second.x + second.width + Page::chip_overlap, tolerance);
  EXPECT_NEAR(drawn.x, second.x, tolerance);
  EXPECT_NEAR(drawn.width, second.width, tolerance);
  for (int index = 1; index < Page::avatar_count; ++index) {
    const composition::Rect before =
        frame_of(application, Page::avatar_key(index - 1));
    const composition::Rect after =
        frame_of(application, Page::avatar_key(index));
    EXPECT_NEAR(after.x - before.x, Page::avatar_size + Page::avatar_spacing,
                tolerance)
        << index;
  }
}

TEST_F(GalleryLayoutScrollTest,
       AutomaticArrangementOverlaysHandheldAndSplitsDocked) {
  provider::headless::HeadlessProviderFactory handheld_providers;
  Application handheld = open(handheld_providers, false);
  const composition::Rect area = frame_of(handheld, Page::arrangement_view);
  const composition::Rect primary = frame_of(handheld, Page::primary_view);
  const composition::Rect secondary = frame_of(handheld, Page::secondary_view);
  EXPECT_NEAR(primary.width, area.width, tolerance);
  EXPECT_NEAR(primary.height, area.height, tolerance);
  EXPECT_NEAR(secondary.x + secondary.width, area.x + area.width, tolerance);
  EXPECT_LT(secondary.width, area.width * 0.75F);
  provider::headless::HeadlessProviderFactory docked_providers;
  Application docked = open(docked_providers, true);
  const composition::Rect split = frame_of(docked, Page::arrangement_view);
  const composition::Rect left = frame_of(docked, Page::primary_view);
  const composition::Rect right = frame_of(docked, Page::secondary_view);
  EXPECT_NEAR(left.x, split.x, tolerance);
  EXPECT_NEAR(right.x, left.x + left.width, tolerance);
  EXPECT_NEAR(right.width, split.width * 0.5F, 1.0F);
}

TEST_F(GalleryLayoutScrollTest,
       TheControllerStepsTheStyleAndTouchPicksTheRatio) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers, false);
  ASSERT_TRUE(walk(providers, application, Page::share_picker));
  ASSERT_TRUE(walk(providers, application, Page::style_picker,
                   provider::Button::dpad_up));
  press(providers, application, provider::Button::dpad_right);
  run(providers, application, 30);
  const composition::Rect area = frame_of(application, Page::arrangement_view);
  const composition::Rect half = frame_of(application, Page::secondary_view);
  EXPECT_NEAR(half.width, area.width * 0.5F, 1.0F);
  EXPECT_NEAR(half.x, area.x + (area.width * 0.5F), 1.0F);
  ASSERT_TRUE(tap_text(providers, application, "Third"));
  EXPECT_NEAR(frame_of(application, Page::secondary_view).width,
              area.width / 3.0F, 1.0F);
  ASSERT_TRUE(tap_text(providers, application, "Two thirds"));
  EXPECT_NEAR(frame_of(application, Page::secondary_view).width,
              area.width * 2.0F / 3.0F, 1.0F);
  ASSERT_TRUE(tap_text(providers, application, "Overlay"));
  const composition::Rect floating = frame_of(application, Page::primary_view);
  EXPECT_NEAR(floating.width, area.width, tolerance);
  EXPECT_NEAR(frame_of(application, Page::secondary_view).width,
              Page::secondary_ideal, tolerance);
}

TEST_F(GalleryLayoutScrollTest, TheCarouselCentresACardAndPagingMovesOnePage) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers, false);
  const composition::Rect carousel =
      presented_of(application, Page::carousel_view);
  drag(providers, application,
       at(providers, application, carousel.x + carousel.width - 20.0F,
          carousel.y + (carousel.height * 0.5F)),
       at(providers, application, carousel.x + carousel.width - 200.0F,
          carousel.y + (carousel.height * 0.5F)),
       8);
  settle(providers, application);
  const float scrolled = offset(application, Page::carousel_view).x;
  EXPECT_GT(scrolled, 0.0F);
  const float centre = carousel.width * 0.5F;
  bool centred = false;
  for (int index = 0; index < Page::card_count; ++index) {
    const composition::Rect card = frame_of(application, Page::card_key(index));
    const float middle_x = card.x - carousel.x + (card.width * 0.5F) - scrolled;
    centred = centred || std::abs(middle_x - centre) <= 1.0F;
  }
  EXPECT_TRUE(centred) << scrolled;
  const composition::Rect paging = presented_of(application, Page::paging_view);
  drag(providers, application,
       at(providers, application, paging.x + paging.width - 20.0F,
          paging.y + (paging.height * 0.5F)),
       at(providers, application, paging.x + (paging.width * 0.4F),
          paging.y + (paging.height * 0.5F)),
       8);
  settle(providers, application);
  EXPECT_NEAR(offset(application, Page::paging_view).x, paging.width, 1.0F);
}

TEST_F(GalleryLayoutScrollTest,
       ContentMarginsInsetTheContentOrOnlyTheIndicators) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers, false);
  const composition::Rect content =
      frame_of(application, Page::content_margin_view);
  const composition::Rect content_row =
      frame_of(application, Page::margin_row_key(Page::content_margin_view, 0));
  EXPECT_NEAR(content_row.x - content.x, Page::margin_inset, tolerance);
  EXPECT_NEAR(content_row.y - content.y, Page::margin_inset, tolerance);
  const composition::Rect indicators =
      frame_of(application, Page::indicator_margin_view);
  const composition::Rect indicator_row = frame_of(
      application, Page::margin_row_key(Page::indicator_margin_view, 0));
  EXPECT_NEAR(indicator_row.x - indicators.x, 0.0F, tolerance);
  EXPECT_NEAR(indicator_row.y - indicators.y, 0.0F, tolerance);
}

TEST_F(GalleryLayoutScrollTest, ScrollingDismissesTheKeyboardOnlyWhenAsked) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers, false);
  ASSERT_TRUE(walk(providers, application, Page::immediate_list));
  ASSERT_TRUE(application.root().activate(
      composition::ElementKey{Page::immediate_field}));
  run(providers, application, 4);
  ASSERT_EQ(providers.text_input().live_sessions(), 1U);
  stick(providers, application, provider::Stick{.x = 0.0F, .y = -1.0F},
        std::chrono::milliseconds{300});
  EXPECT_GT(offset(application, Page::immediate_list).y, 0.0F);
  EXPECT_EQ(providers.text_input().live_sessions(), 0U);
  ASSERT_TRUE(
      application.root().activate(composition::ElementKey{Page::never_field}));
  run(providers, application, 4);
  ASSERT_EQ(providers.text_input().live_sessions(), 1U);
  reveal(providers, application, Page::never_list);
  const composition::Rect never = presented_of(application, Page::never_list);
  drag(providers, application,
       at(providers, application, never.x + 40.0F,
          never.y + never.height - 10.0F),
       at(providers, application, never.x + 40.0F, never.y + 10.0F), 8);
  settle(providers, application);
  EXPECT_GT(offset(application, Page::never_list).y, 0.0F);
  EXPECT_EQ(providers.text_input().live_sessions(), 1U);
}

TEST_F(GalleryLayoutScrollTest, TheControllerReachesEveryInteractiveTile) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers, false);
  EXPECT_TRUE(inside(application, Page::close_button));
  EXPECT_TRUE(walk(providers, application, Page::lazy_view));
  EXPECT_TRUE(walk(providers, application, Page::share_picker));
  EXPECT_TRUE(walk(providers, application, Page::style_picker,
                   provider::Button::dpad_up));
  EXPECT_TRUE(walk(providers, application, Page::content_margin_view));
  EXPECT_TRUE(walk(providers, application, Page::indicator_margin_view));
  EXPECT_TRUE(walk(providers, application, Page::immediate_list));
  EXPECT_TRUE(walk(providers, application, Page::never_list));
  EXPECT_TRUE(walk(providers, application, Page::paging_view,
                   provider::Button::dpad_up));
  EXPECT_TRUE(walk(providers, application, Page::carousel_view,
                   provider::Button::dpad_up));
  const float before = offset(application, Page::carousel_view).x;
  for (int step = 0; step < 4; ++step) {
    press(providers, application, provider::Button::dpad_right);
    run(providers, application, 30);
  }
  EXPECT_TRUE(inside(application, Page::carousel_view));
  EXPECT_GT(offset(application, Page::carousel_view).x, before);
}

TEST_F(GalleryLayoutScrollTest, ASettledPageAllocatesNothing) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers, false);
  run(providers, application, 60);
  std::uint64_t used = 0;
  int presented = 0;
  for (int index = 0; index < 120; ++index) {
    EXPECT_TRUE(providers.clock().advance(frame_step).has_value());
    const std::uint64_t before = provider::AllocationMeter::count().value_or(0);
    EXPECT_TRUE(application.frame().has_value());
    used += provider::AllocationMeter::count().value_or(0) - before;
    presented += application.last_frame().presented ? 1 : 0;
  }
  EXPECT_EQ(presented, 0);
  if (provider::AllocationMeter::counting()) {
    EXPECT_EQ(used, 0U);
  }
}

TEST_F(GalleryLayoutScrollTest, HandheldMatchesGolden) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers, false, true);
  run(providers, application, 120);
  matches_golden(providers, "layout_scroll_handheld_1280x720");
}

TEST_F(GalleryLayoutScrollTest, DockedMatchesGolden) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers, true, true);
  run(providers, application, 120);
  matches_golden(providers, "layout_scroll_docked_1920x1080");
}

TEST_F(GalleryLayoutScrollTest, TheWholePageMatchesGolden) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers, false, true, 960);
  ASSERT_TRUE(tap_text(providers, application, "Split"));
  ASSERT_TRUE(tap_text(providers, application, "Two thirds"));
  run(providers, application, 120);
  matches_golden(providers, "layout_scroll_whole_1280x960");
}

}
