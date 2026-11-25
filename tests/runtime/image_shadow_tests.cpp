#include "focus_fixture.hpp"

#include <arisa/components/image.hpp>
#include <arisa/components/text.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/typography.hpp>

#include <gtest/gtest.h>

namespace arisa::runtime {

class ImageShadowTest : public FocusFixture {
 protected:
  static auto silhouettes(const composition::Node& node) -> int {
    int found = node.modifiers().graphics.colour.empty() ? 0 : 1;
    for (const composition::Node& child : node.children()) {
      found += silhouettes(child);
    }
    return found;
  }

  static auto rect_shadows(const composition::Node& node) -> int {
    int found = node.modifiers().drop_shadow.has_value() ? 1 : 0;
    for (const composition::Node& child : node.children()) {
      found += rect_shadows(child);
    }
    return found;
  }

  provider::headless::HeadlessProviderFactory providers_;
};

TEST_F(ImageShadowTest, AnImageShadowIsCastFromTheImagesAlpha) {
  Application application = start(
      providers_,
      components::Image(components::Image::SystemName{"sports_esports"})
          .font(theme::Font::system(80.0F))
          .shadow(theme::ColourValue{theme::Colour{0.0F, 0.0F, 0.0F, 0.4F}},
                  8.0F, 0.0F, 6.0F)
          .id("art"));
  const composition::Node& root = application.root().tree().root();
  EXPECT_EQ(silhouettes(root), 1);
  EXPECT_EQ(rect_shadows(root), 0);
  EXPECT_TRUE(node_of(application, "art").has_value());
}

TEST_F(ImageShadowTest, OtherViewsKeepTheirShapeShadow) {
  Application application = start(
      providers_,
      components::Text("Label").shadow(
          theme::ColourValue{theme::Colour{0.0F, 0.0F, 0.0F, 0.4F}}, 8.0F));
  const composition::Node& root = application.root().tree().root();
  EXPECT_EQ(silhouettes(root), 0);
  EXPECT_EQ(rect_shadows(root), 1);
}

}
