#include <arisa/components/text.hpp>
#include <arisa/composition/layer.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/provider/input/provider.hpp>

#include <gallery_fixture.hpp>
#include <presentations_page.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>

namespace arisa::runtime {

class GalleryPresentationPageTest : public GalleryFixture {
 protected:
  static auto modal_layers(Application& application) -> std::size_t {
    std::size_t found = 0;
    walk(application.root().tree().root(),
         [&found](const composition::Node& node) {
           const auto layer = node.content_as<composition::Layer::Content>();
           if (layer.has_value() &&
               layer->get().input() != composition::OverlayInput::passthrough) {
             ++found;
           }
         });
    return found;
  }

  static auto activate_within(runtime::Root& root,
                              const composition::Node& node) -> bool {
    return std::ranges::any_of(
        node.children(), [&root](const composition::Node& child) {
          return root.activate(child.id()) || activate_within(root, child);
        });
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
    return node.has_value() && activate_within(root, node->get());
  }

  static void back(provider::headless::HeadlessProviderFactory& providers,
                   Application& application) {
    provider::InputFrame down;
    down.pads[0] = provider::PadState{.connected = true,
                                      .buttons = {provider::Button::b},
                                      .left = {},
                                      .right = {}};
    provider::InputFrame up;
    up.pads[0] = provider::PadState{
        .connected = true, .buttons = {}, .left = {}, .right = {}};
    providers.input().push(down);
    providers.input().push(up);
    run(providers, application, 3);
  }
};

TEST_F(GalleryPresentationPageTest, EachNewPresentationOpensAndClosesByBack) {
  constexpr std::array keys{
      examples::PresentationsPage::options_button,
      examples::PresentationsPage::load_button,
      examples::PresentationsPage::erase_button,
      examples::PresentationsPage::edit_button,
      examples::PresentationsPage::cover_item_button,
      examples::PresentationsPage::popover_item_button,
  };
  for (const std::string_view key : keys) {
    provider::headless::HeadlessProviderFactory providers;
    Application application =
        start_gallery(providers, examples::GalleryPage::presentations);
    const std::size_t before = modal_layers(application);
    ASSERT_TRUE(activate(application, key)) << key;
    run(providers, application, 30);
    EXPECT_EQ(modal_layers(application), before + 1) << key;
    back(providers, application);
    run(providers, application, 30);
    EXPECT_EQ(modal_layers(application), before) << key;
  }
}

TEST_F(GalleryPresentationPageTest, ImperativeOverlayLeavesWhenItsTaskEnds) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start_gallery(providers, examples::GalleryPage::presentations);
  ASSERT_TRUE(activate(application, examples::PresentationsPage::flash_button));
  run(providers, application, 10);
  EXPECT_EQ(application.root().tree().overlays(), 1U);
  run(providers, application, 160);
  EXPECT_EQ(application.root().tree().overlays(), 0U);
}

TEST_F(GalleryPresentationPageTest, InspectorTogglesItsColumn) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start_gallery(providers, examples::GalleryPage::presentations);
  ASSERT_TRUE(
      activate(application, examples::PresentationsPage::inspector_button));
  run(providers, application, 30);
  EXPECT_TRUE(shows(application, "Size 4.2 GB"));
  ASSERT_TRUE(
      activate(application, examples::PresentationsPage::inspector_button));
  run(providers, application, 30);
  EXPECT_FALSE(shows(application, "Size 4.2 GB"));
}

}
