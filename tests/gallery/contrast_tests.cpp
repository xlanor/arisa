#include <arisa/provider/diagnostics/provider.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/theme/color_scheme.hpp>

#include <gallery_fixture.hpp>

#include <gtest/gtest.h>

namespace arisa::runtime {

class GalleryContrastTest
    : public GalleryFixture,
      public ::testing::WithParamInterface<theme::ColorScheme> {};

TEST_P(GalleryContrastTest, EveryPagePassesTheHandheldContrastCheck) {
  for (const examples::GalleryPage page : examples::GalleryPages::all) {
    provider::headless::HeadlessProviderFactory providers;
    Application application = start_gallery(providers, page, GetParam());
    run(providers, application, 8);
    for (const provider::Diagnostic& each :
         providers.diagnostics().snapshot()) {
      if (each.category == "contrast") {
        ADD_FAILURE() << examples::GalleryPages::name(page) << ": "
                      << each.message;
      }
    }
  }
}

INSTANTIATE_TEST_SUITE_P(Schemes, GalleryContrastTest,
                         ::testing::Values(theme::ColorScheme::light,
                                           theme::ColorScheme::dark));

}
