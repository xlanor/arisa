#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/layer.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/provider/gpu/image.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/surface/surface_scale.hpp>
#include <arisa/provider/surface/surface_size.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/shape.hpp>

#include <focus_fixture.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <utility>

namespace arisa::runtime {

using namespace std::chrono_literals;

class PresentationFocusRingTest : public FocusFixture {
 protected:
  static constexpr std::uint32_t side = 480;
  static constexpr composition::StateKey<bool> open{"open", true};

  class Bar final : public composition::Component {
   public:
    explicit Bar(bool focusable) noexcept : focusable_(focusable) {}

    auto operator==(const Bar&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      const auto shown = context.state(open);
      const bool focusable = focusable_;
      return components::ZStack(
                 components::HStack(
                     components::Button([] {},
                                        components::ZStack().frame(
                                            {.width = 80.0F, .height = 24.0F}))
                         .button_style(components::ButtonStyle::plain())
                         .id("anchor")
                         .popover(
                             shown,
                             [focusable] {
                               return components::Button(
                                          [] {},
                                          components::ZStack()
                                              .frame({.width = 120.0F,
                                                      .height = 40.0F})
                                              .background(
                                                  theme::Colour::hex(0xF0F0F0)))
                                   .button_style(
                                       components::ButtonStyle::plain())
                                   .focusable(focusable)
                                   .id("inside");
                             }))
                     .padding(theme::Insets::symmetric(theme::Space{20.0F},
                                                       theme::Space{6.0F}))
                     .background(theme::Colour::hex(0x404040),
                                 theme::Shape::capsule()))
          .frame({.width = static_cast<float>(side),
                  .height = static_cast<float>(side),
                  .alignment = composition::Alignment::top})
          .background(theme::Colour::hex(0x000000));
    }

   private:
    bool focusable_;
  };

  struct Capture {
    provider::Image image;
    composition::Rect panel;
  };

  static auto capture(bool focusable) -> Capture {
    provider::headless::HeadlessProviderFactory providers;
    const provider::SurfaceSize extent{.width = provider::SurfaceWidth{side},
                                       .height = provider::SurfaceHeight{side}};
    providers.surface().resize(extent);
    providers.surface().set_scale(*provider::SurfaceScale::create(1.0F));
    providers.gpu().rasterize(extent);
    Application application = start(providers, Bar{focusable});
    for (int index = 0; index < 40; ++index) {
      EXPECT_TRUE(providers.clock().advance(32ms).has_value());
      frames(application, 1);
    }
    composition::Rect panel{};
    const auto search = [&panel](const auto& self,
                                 const composition::Node& node) -> void {
      if (node.content_as<composition::Layer::Content>().has_value() &&
          node.child_count() == 2) {
        panel = (*std::next(node.children().begin())).presented()->bounds;
      }
      for (const composition::Node& child : node.children()) {
        self(self, child);
      }
    };
    search(search, application.root().tree().root());
    auto image = providers.gpu().capture();
    EXPECT_TRUE(image.has_value());
    return Capture{.image = std::move(*image), .panel = panel};
  }
};

TEST_F(PresentationFocusRingTest,
       RingInAPopoverFromACapsuleBarStaysInThePanel) {
  const Capture ringed = capture(true);
  const Capture plain = capture(false);
  ASSERT_GT(ringed.panel.width, 0.0F);
  int changed = 0;
  int escaped = 0;
  for (std::uint32_t y = 0; y < ringed.image.height(); ++y) {
    for (std::uint32_t x = 0; x < ringed.image.width(); ++x) {
      const auto a = ringed.image.pixel(x, y);
      const auto b = plain.image.pixel(x, y);
      if (a == b) {
        continue;
      }
      ++changed;
      const float px = static_cast<float>(x) + 0.5F;
      const float py = static_cast<float>(y) + 0.5F;
      if (px < ringed.panel.x - 1.0F ||
          px > ringed.panel.x + ringed.panel.width + 1.0F ||
          py < ringed.panel.y - 1.0F ||
          py > ringed.panel.y + ringed.panel.height + 1.0F) {
        ++escaped;
      }
    }
  }
  EXPECT_GT(changed, 40);
  EXPECT_EQ(escaped, 0);
}

}
