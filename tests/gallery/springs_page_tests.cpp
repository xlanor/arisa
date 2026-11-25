#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/provider/diagnostics/allocation_meter.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/runtime/frame_timings.hpp>

#include <gallery_fixture.hpp>
#include <springs_page.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <string_view>

namespace arisa::runtime {

class GallerySpringsTest : public GalleryFixture {
 protected:
  using Page = examples::SpringsPage;

  class Host final : public composition::Component {
   public:
    static constexpr composition::StateKey<bool> shown{"springs-host", true};

    auto operator==(const Host&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      return Page{context.state(shown)};
    }
  };

  static auto open(provider::headless::HeadlessProviderFactory& providers)
      -> Application {
    load_fonts(providers);
    const provider::SurfaceSize size{.width = provider::SurfaceWidth{1280},
                                     .height = provider::SurfaceHeight{720}};
    providers.surface().resize(size);
    Application application = start(providers, Host{});
    run(providers, application, 30);
    return application;
  }

  static void activate(provider::headless::HeadlessProviderFactory& providers,
                       Application& application, std::string_view key) {
    ASSERT_TRUE(
        application.root().activate(composition::ElementKey{std::string{key}}));
    run(providers, application, 2);
  }

  static auto settled_allocations(
      provider::headless::HeadlessProviderFactory& providers,
      Application& application) -> std::uint64_t {
    std::uint64_t used = 0;
    for (int index = 0; index < 30; ++index) {
      EXPECT_TRUE(providers.clock().advance(frame_step).has_value());
      const std::uint64_t before =
          provider::AllocationMeter::count().value_or(0);
      EXPECT_TRUE(application.frame().has_value());
      used += provider::AllocationMeter::count().value_or(0) - before;
    }
    return used;
  }
};

TEST_F(GallerySpringsTest, ThePageShowsEveryDemo) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  for (const char* title :
       {"Initial velocity", "Unit curves", "Custom transitions",
        "Per-transition animation", "Binding animation", "Gradient fills",
        "Thrown", "Circular in out", "Inverse ease in", "Develop", "Flip",
        "Modifier", "Own animation", "Animated binding", "Gradient"}) {
    EXPECT_TRUE(shows(application, title)) << title;
  }
}

TEST_F(GallerySpringsTest, EveryDemoAnimatesThenSettlesWithoutAllocating) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  for (const std::string_view key :
       {Page::launch_button, Page::curves_button, Page::transitions_button,
        Page::attached_button, Page::binding_toggle, Page::gradient_button}) {
    activate(providers, application, key);
    const bool on_screen =
        key != Page::binding_toggle && key != Page::gradient_button;
    EXPECT_TRUE(!on_screen || application.last_frame().animating) << key;
    run(providers, application, 150);
    EXPECT_FALSE(application.last_frame().animating) << key;
    const std::uint64_t used = settled_allocations(providers, application);
    if (provider::AllocationMeter::counting()) {
      EXPECT_EQ(used, 0U) << key;
    }
  }
  activate(providers, application, Page::transitions_button);
  run(providers, application, 150);
  EXPECT_TRUE(shows(application, "Develop"));
}

}
