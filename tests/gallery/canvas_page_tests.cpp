#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/provider/headless/headless_factory.hpp>

#include <canvas_page.hpp>
#include <gallery_fixture.hpp>

#include <gtest/gtest.h>

namespace arisa::runtime {

class GalleryCanvasTest : public GalleryFixture {
 protected:
  class Host final : public composition::Component {
   public:
    static constexpr composition::StateKey<bool> shown{"canvas-host", true};

    auto operator==(const Host&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      return examples::CanvasPage{context.state(shown)};
    }
  };
};

TEST_F(GalleryCanvasTest, ThePageShowsEveryDemo) {
  provider::headless::HeadlessProviderFactory providers;
  load_fonts(providers);
  const provider::SurfaceSize size{.width = provider::SurfaceWidth{1280},
                                   .height = provider::SurfaceHeight{720}};
  providers.surface().resize(size);
  providers.gpu().rasterize(size);
  Application application = start(providers, Host{});
  run(providers, application, 30);
  for (const char* title :
       {"Path booleans", "Circles and strokes", "Porter-Duff", "Union",
        "Fill then stroke", "Source atop"}) {
    EXPECT_TRUE(shows(application, title)) << title;
  }
}

}
