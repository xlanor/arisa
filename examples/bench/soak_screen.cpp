#include <soak_screen.hpp>

#include <arisa/theme/seed.hpp>

#include <gallery_page.hpp>
#include <gallery_screen.hpp>

namespace arisa::examples {

auto SoakScreen::body(composition::Context& context) const
    -> composition::Element {
  const auto mode = context.state(showing);
  remote_->mode = mode;
  switch (mode.get()) {
    case Mode::fling:
      return BenchScreen{BenchScreen::Mode::fling, remote_->bench};
    case Mode::sheet:
      return BenchScreen{BenchScreen::Mode::sheet, remote_->bench};
    case Mode::navigation:
      return BenchScreen{BenchScreen::Mode::navigation, remote_->bench};
    case Mode::gallery:
      return GalleryScreen{GalleryPage::typography, theme::Seed{}};
  }
  return GalleryScreen{GalleryPage::typography, theme::Seed{}};
}

}
