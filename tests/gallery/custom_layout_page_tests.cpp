#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/provider/diagnostics/allocation_meter.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>

#include <custom_layout_page.hpp>
#include <gallery_fixture.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <string_view>

namespace arisa::runtime {

class GalleryCustomLayoutTest : public GalleryFixture {
 protected:
  using Page = examples::CustomLayoutPage;

  class Host final : public composition::Component {
   public:
    static constexpr composition::StateKey<bool> shown{"custom-layout-host",
                                                       true};

    auto operator==(const Host&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      return Page{context.state(shown)};
    }
  };

  static auto open(provider::headless::HeadlessProviderFactory& providers,
                   bool docked) -> Application {
    load_fonts(providers);
    if (docked) {
      providers.surface().resize(
          provider::SurfaceSize{.width = provider::SurfaceWidth{1920},
                                .height = provider::SurfaceHeight{1080}});
      providers.surface().set_scale(
          provider::SurfaceScale::create(1.5F).value_or(
              provider::SurfaceScale::unit()));
      providers.surface().set_display_mode(provider::DisplayMode::docked);
    } else {
      providers.surface().resize(
          provider::SurfaceSize{.width = provider::SurfaceWidth{1280},
                                .height = provider::SurfaceHeight{720}});
      providers.surface().set_display_mode(provider::DisplayMode::handheld);
    }
    Application application = start(providers, Host{});
    run(providers, application, 20);
    return application;
  }

  static auto withheld(Application& application, std::string_view key) -> bool {
    const auto node = node_named(application, key);
    if (!node.has_value()) {
      return false;
    }
    return application.root().tree().presentation(node->get()).withheld();
  }
};

TEST_F(GalleryCustomLayoutTest, TagsFlowIntoRows) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers, false);
  const composition::Rect flow = frame_of(application, Page::flow_view);
  const composition::Rect first = frame_of(application, Page::tag_key(0));
  const composition::Rect last =
      frame_of(application, Page::tag_key(Page::tag_names.size() - 1));
  EXPECT_NEAR(first.x, flow.x, 0.5F);
  EXPECT_GT(last.y, first.y);
  for (std::size_t index = 0; index < Page::tag_names.size(); ++index) {
    const composition::Rect tag = frame_of(application, Page::tag_key(index));
    EXPECT_LE(tag.x + tag.width, flow.x + flow.width + 0.5F) << index;
  }
}

TEST_F(GalleryCustomLayoutTest, FocusMovesThroughTheTagsInReadingOrder) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers, false);
  for (int step = 0; step < 4 && focused_key(application) != Page::tag_key(0);
       ++step) {
    press(providers, application, provider::Button::dpad_down);
  }
  ASSERT_EQ(focused_key(application), Page::tag_key(0));
  press(providers, application, provider::Button::dpad_right);
  EXPECT_EQ(focused_key(application), Page::tag_key(1));
  press(providers, application, provider::Button::dpad_right);
  EXPECT_EQ(focused_key(application), Page::tag_key(2));
}

TEST_F(GalleryCustomLayoutTest, CardsStackHandheldAndRowDocked) {
  provider::headless::HeadlessProviderFactory handheld_providers;
  Application handheld = open(handheld_providers, false);
  const composition::Rect top = frame_of(handheld, Page::card_key(0));
  const composition::Rect below = frame_of(handheld, Page::card_key(1));
  EXPECT_GE(below.y, top.y + top.height);
  provider::headless::HeadlessProviderFactory docked_providers;
  Application docked = open(docked_providers, true);
  const composition::Rect left = frame_of(docked, Page::card_key(0));
  const composition::Rect right = frame_of(docked, Page::card_key(1));
  EXPECT_NEAR(left.y, right.y, 0.5F);
  EXPECT_GE(right.x, left.x + left.width);
}

TEST_F(GalleryCustomLayoutTest, SwitchingAnimatesAndKeepsTheCards) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers, false);
  const auto before = node_of(application, Page::card_key(1));
  const composition::Rect stacked = frame_of(application, Page::card_key(1));
  ASSERT_TRUE(application.root().activate(
      composition::ElementKey{Page::switch_button}));
  run(providers, application, 3);
  const composition::Rect moving = presented_of(application, Page::card_key(1));
  run(providers, application, 60);
  const composition::Rect row = frame_of(application, Page::card_key(1));
  EXPECT_EQ(node_of(application, Page::card_key(1)), before);
  EXPECT_LT(row.y, stacked.y);
  EXPECT_GT(row.x, stacked.x);
  EXPECT_LT(moving.y, stacked.y - 0.5F);
  EXPECT_GT(moving.y, row.y + 0.5F);
}

TEST_F(GalleryCustomLayoutTest, TitlesPickTheVariantThatFits) {
  for (const bool docked : {false, true}) {
    provider::headless::HeadlessProviderFactory providers;
    Application application = open(providers, docked);
    EXPECT_FALSE(
        withheld(application, Page::variant_key(Page::wide_view, "long")))
        << docked;
    EXPECT_TRUE(
        withheld(application, Page::variant_key(Page::wide_view, "short")))
        << docked;
    EXPECT_TRUE(
        withheld(application, Page::variant_key(Page::narrow_view, "long")))
        << docked;
    EXPECT_FALSE(
        withheld(application, Page::variant_key(Page::narrow_view, "short")))
        << docked;
  }
}

TEST_F(GalleryCustomLayoutTest, FormFieldsShareTheCustomGuide) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers, false);
  const float first = frame_of(application, Page::field_key(0)).x;
  for (std::size_t index = 1; index < Page::field_names.size(); ++index) {
    EXPECT_NEAR(frame_of(application, Page::field_key(index)).x, first, 0.5F)
        << index;
  }
}

TEST_F(GalleryCustomLayoutTest, SettledFramesLayOutAndAllocateNothing) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers, false);
  run(providers, application, 60);
  std::uint64_t used = 0;
  int laid = 0;
  for (int index = 0; index < 120; ++index) {
    ASSERT_TRUE(providers.clock().advance(frame_step).has_value());
    const std::uint64_t before = provider::AllocationMeter::count().value_or(0);
    ASSERT_TRUE(application.frame().has_value());
    used += provider::AllocationMeter::count().value_or(0) - before;
    laid += application.last_frame().laid_out ? 1 : 0;
  }
  EXPECT_EQ(laid, 0);
  if (provider::AllocationMeter::counting()) {
    EXPECT_EQ(used, 0U);
  }
}

}
