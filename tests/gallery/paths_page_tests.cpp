#include <arisa/composition/element.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/presented_clip.hpp>
#include <arisa/provider/headless/headless_factory.hpp>

#include <gallery_fixture.hpp>
#include <paths_page.hpp>

#include <gtest/gtest.h>

namespace arisa::runtime {

class GalleryPathsTest : public GalleryFixture {};

TEST_F(GalleryPathsTest, TheMeshTileIsClippedToItsRoundedRectangle) {
  provider::headless::HeadlessProviderFactory providers;
  load_fonts(providers);
  Application application = start(providers, examples::PathsPage{});
  run(providers, application, 10);
  const auto mesh = node_named(application, examples::PathsPage::mesh_view);
  ASSERT_TRUE(mesh.has_value());
  const auto& presented = mesh->get().presented();
  ASSERT_TRUE(presented.has_value());
  ASSERT_TRUE(presented->clip_shape.has_value());
  const composition::Rect bounds = presented->bounds;
  EXPECT_NEAR(presented->clip_shape->radii[0], 14.0F, 0.01F);
  EXPECT_FALSE(presented->clip_shape->contains(
      {.x = bounds.x + 1.0F, .y = bounds.y + 1.0F}));
  EXPECT_TRUE(presented->clip_shape->contains(
      {.x = bounds.x + (bounds.width * 0.5F),
       .y = bounds.y + (bounds.height * 0.5F)}));
  EXPECT_TRUE(presented->covers(
      {.x = bounds.x + (bounds.width * 0.5F), .y = bounds.y + 2.0F}));
  EXPECT_FALSE(presented->covers({.x = bounds.x + 1.0F, .y = bounds.y + 1.0F}));
}

}
