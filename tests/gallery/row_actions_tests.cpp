#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>

#include <gallery_fixture.hpp>
#include <row_actions_page.hpp>

#include <gtest/gtest.h>

#include <string>

namespace arisa::runtime {

class GalleryRowActionsTest : public GalleryFixture {
 protected:
  class Host final : public composition::Component {
   public:
    static constexpr composition::StateKey<bool> shown{"row-actions-host",
                                                       true};

    auto operator==(const Host&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      return examples::RowActionsPage{context.state(shown)};
    }
  };

  static auto has(Application& application, std::string_view key) -> bool {
    return application.root()
        .tree()
        .find(composition::ElementKey{std::string{key}})
        .has_value();
  }
};

TEST_F(GalleryRowActionsTest, SavesListOffersEveryRowActionAndBatchDelete) {
  provider::headless::HeadlessProviderFactory providers;
  load_fonts(providers);
  Application application = start(providers, Host{});
  run(providers, application, 20);
  EXPECT_TRUE(shows(application, "Breath 1"));
  EXPECT_TRUE(shows(application, "1"));
  EXPECT_FALSE(has(application, "arisa.list.select"));
  tap(providers, application,
      centre(application, examples::RowActionsPage::edit_button));
  run(providers, application, 20);
  EXPECT_TRUE(has(application, "arisa.list.select"));
  EXPECT_TRUE(shows(application, "Done"));
}

}
