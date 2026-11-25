#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/provider/assets/asset_path.hpp>
#include <arisa/provider/diagnostics/allocation_meter.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>

#include <gallery_fixture.hpp>
#include <rich_text_page.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::runtime {

class GalleryRichTextTest : public GalleryFixture {
 protected:
  using Page = examples::RichTextPage;

  class Host final : public composition::Component {
   public:
    static constexpr composition::StateKey<bool> shown{"rich-text-host", true};

    auto operator==(const Host&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      return Page{context.state(shown)};
    }
  };

  static void families(provider::headless::HeadlessProviderFactory& providers) {
    for (const std::string_view file :
         {std::string_view{"arisa/fonts/InterItalic-Regular.ttf"},
          std::string_view{"arisa/fonts/InterItalic-Bold.ttf"},
          std::string_view{"arisa/fonts/JetBrainsMono-Regular.ttf"},
          std::string_view{"arisa/fonts/NotoSerif-Regular.ttf"},
          std::string_view{"arisa/fonts/Nunito-Regular.ttf"}}) {
      std::ifstream stream{std::filesystem::path{ARISA_SOURCE_ASSETS} / file,
                           std::ios::binary};
      const std::vector<char> raw{std::istreambuf_iterator<char>{stream},
                                  std::istreambuf_iterator<char>{}};
      std::vector<std::byte> bytes(raw.size());
      std::ranges::transform(raw, bytes.begin(),
                             [](char value) { return std::byte(value); });
      ASSERT_FALSE(bytes.empty()) << file;
      providers.assets().add(*provider::AssetPath::create(file),
                             std::move(bytes));
    }
  }

  static auto open(provider::headless::HeadlessProviderFactory& providers)
      -> Application {
    load_fonts(providers);
    families(providers);
    providers.surface().resize(
        provider::SurfaceSize{.width = provider::SurfaceWidth{1280},
                              .height = provider::SurfaceHeight{720}});
    providers.surface().set_display_mode(provider::DisplayMode::handheld);
    Application application = start(providers, Host{});
    run(providers, application, 20);
    return application;
  }
};

TEST_F(GalleryRichTextTest, LogLinesShareOneMonospacedWidth) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  const float first = frame_of(application, Page::log_key(0)).width;
  for (std::size_t index = 1; index < Page::log_lines.size(); ++index) {
    const float expected =
        first * static_cast<float>(Page::log_lines.at(index).size()) /
        static_cast<float>(Page::log_lines.at(0).size());
    EXPECT_NEAR(frame_of(application, Page::log_key(index)).width, expected,
                1.0F)
        << index;
  }
}

TEST_F(GalleryRichTextTest, FacesHaveTheirOwnWidths) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  const float condensed =
      frame_of(application, Page::face_key("condensed")).width;
  const float expanded =
      frame_of(application, Page::face_key("expanded")).width;
  EXPECT_GT(expanded, condensed * 1.3F);
  const float standard =
      frame_of(application, Page::face_key("standard")).height;
  EXPECT_NEAR(frame_of(application, Page::face_key("secondary")).height,
              standard, 0.5F);
}

TEST_F(GalleryRichTextTest, ReleaseNoteLinksAreReachableByPad) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  for (int step = 0; step < 6 && focused_key(application) != Page::notes_view;
       ++step) {
    press(providers, application, provider::Button::dpad_down);
  }
  ASSERT_EQ(focused_key(application), Page::notes_view);
  const auto node = node_named(application, Page::notes_view);
  ASSERT_TRUE(node.has_value());
  EXPECT_EQ(node->get().link_frames().size(), 2U);
  press(providers, application, provider::Button::dpad_right);
  EXPECT_EQ(node->get().selected_link(), 1U);
  press(providers, application, provider::Button::a);
  ASSERT_EQ(providers.browser().opened().size(), 1U);
  EXPECT_EQ(providers.browser().opened().back().value(),
            "https://example.com/guide");
}

TEST_F(GalleryRichTextTest, SettledFramesLayOutAndAllocateNothing) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
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
