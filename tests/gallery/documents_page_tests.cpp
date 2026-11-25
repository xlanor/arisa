#include <arisa/components/text.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/persistent_system_overlays.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/visibility.hpp>
#include <arisa/provider/diagnostics/allocation_meter.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/headless/memory_storage.hpp>
#include <arisa/runtime/application_options.hpp>
#include <arisa/structure/new_document_button.hpp>

#include <documents_page.hpp>
#include <gallery_fixture.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

namespace arisa::runtime {

class GalleryDocumentsTest : public GalleryFixture {
 protected:
  using Page = examples::DocumentsPage;

  class Host final : public composition::Component {
   public:
    static constexpr composition::StateKey<bool> shown{"documents-host", true};

    auto operator==(const Host&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      return Page{context.state(shown)};
    }
  };
};

TEST_F(GalleryDocumentsTest, ThePageEditsNotesOnTheCardAndStaysIdle) {
  provider::headless::HeadlessProviderFactory providers;
  load_fonts(providers);
  providers.surface().resize({.width = provider::SurfaceWidth{1280},
                              .height = provider::SurfaceHeight{720}});
  provider::headless::MemoryStorage card;
  auto created =
      Application::create(providers, Host{},
                          ApplicationOptions{.executors = std::nullopt,
                                             .fonts = {},
                                             .atlas = {},
                                             .interaction = {},
                                             .files = card});
  ASSERT_TRUE(created.has_value());
  Application application = std::move(*created);
  run(providers, application, 30);
  for (const char* title : {"Notes", "Files", "Data flow", "System overlays"}) {
    EXPECT_TRUE(shows(application, title)) << title;
  }
  ASSERT_TRUE(application.root().activate(
      composition::ElementKey{std::string{Page::Demos::tick_button}}));
  run(providers, application, 5);
  EXPECT_TRUE(shows(application, "1"));
  EXPECT_TRUE(shows(application, "7"));
  std::uint64_t used = 0;
  for (int index = 0; index < 30; ++index) {
    ASSERT_TRUE(providers.clock().advance(frame_step).has_value());
    const std::uint64_t before = provider::AllocationMeter::count().value_or(0);
    ASSERT_TRUE(application.frame().has_value());
    used += provider::AllocationMeter::count().value_or(0) - before;
  }
  if (provider::AllocationMeter::counting()) {
    EXPECT_EQ(used, 0U);
  }
}

TEST_F(GalleryDocumentsTest, TheOverlaysToggleInsideTheCoverHidesTheHints) {
  provider::headless::HeadlessProviderFactory providers;
  load_fonts(providers);
  providers.surface().resize({.width = provider::SurfaceWidth{1280},
                              .height = provider::SurfaceHeight{720}});
  auto created = Application::create(providers, Page::Opener{});
  ASSERT_TRUE(created.has_value());
  Application application = std::move(*created);
  run(providers, application, 5);
  ASSERT_TRUE(application.root().activate(
      composition::ElementKey{std::string{Page::Opener::button}}));
  run(providers, application, 30);
  ASSERT_TRUE(application.root().activate(
      composition::ElementKey{std::string{Page::Demos::overlays_toggle}}));
  run(providers, application, 5);
  EXPECT_EQ(application.root().tree().root().preference(
                composition::PersistentSystemOverlays::key),
            std::optional{composition::Visibility::hidden});
}

}
