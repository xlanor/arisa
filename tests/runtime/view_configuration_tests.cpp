#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/provider/surface/surface_scale.hpp>
#include <arisa/provider/surface/surface_size.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/theme/colour.hpp>

#include <scroll_fixture.hpp>

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <utility>

namespace arisa::runtime {

using namespace std::chrono_literals;
using provider::Button;

class ViewConfigurationTest : public ScrollFixture {
 protected:
  static auto tile(std::string key) -> composition::Element {
    return components::Button(key, [] {})
        .frame({.width = 120.0F, .height = 40.0F})
        .id(std::move(key));
  }

  static auto column(bool hide) -> composition::Element {
    composition::Element middle = tile("middle");
    if (hide) {
      middle = std::move(middle).hidden();
    }
    return components::VStack(tile("top"), std::move(middle), tile("bottom"));
  }

  static auto opacity_of(Application& application, std::string_view key)
      -> float {
    const auto node = node_named(application, key);
    if (!node.has_value()) {
      return -1.0F;
    }
    const auto& presented = node->get().presented();
    EXPECT_TRUE(presented.has_value());
    return presented.has_value() ? presented->opacity : -1.0F;
  }

  static auto pixel_of(composition::Element content, std::uint32_t x,
                       std::uint32_t y) -> std::array<int, 3> {
    provider::headless::HeadlessProviderFactory providers;
    const provider::SurfaceSize extent{.width = provider::SurfaceWidth{320},
                                       .height = provider::SurfaceHeight{200}};
    providers.surface().resize(extent);
    providers.surface().set_scale(provider::SurfaceScale::unit());
    providers.gpu().rasterize(extent);
    auto created = Application::create(providers, std::move(content));
    EXPECT_TRUE(created.has_value());
    if (!created.has_value()) {
      return {};
    }
    for (int index = 0; index < 30; ++index) {
      EXPECT_TRUE(providers.clock().advance(16ms).has_value());
      EXPECT_TRUE(created->frame().has_value());
    }
    const auto image = providers.gpu().capture();
    EXPECT_TRUE(image.has_value());
    if (!image.has_value()) {
      return {};
    }
    const auto found = image->pixel(x, y);
    EXPECT_TRUE(found.has_value());
    if (!found.has_value()) {
      return {};
    }
    return {(*found)[0], (*found)[1], (*found)[2]};
  }
};

TEST_F(ViewConfigurationTest, HiddenKeepsItsSpaceButDrawsAndFocusesNothing) {
  provider::headless::HeadlessProviderFactory providers;
  Application shown = start(providers, column(false));
  const composition::Rect bottom = frame_of(shown, "bottom");
  provider::headless::HeadlessProviderFactory other;
  Application hidden = start(other, column(true));
  EXPECT_EQ(frame_of(hidden, "bottom"), bottom);
  EXPECT_EQ(frame_of(hidden, "middle"), frame_of(shown, "middle"));
  const auto concealed = node_named(hidden, "middle");
  ASSERT_TRUE(concealed.has_value());
  const auto& presented = concealed->get().presented();
  ASSERT_TRUE(presented.has_value());
  EXPECT_FALSE(presented->visible);
  EXPECT_EQ(focused_key(hidden), "top");
  press(other, hidden, Button::dpad_down);
  EXPECT_EQ(focused_key(hidden), "bottom");
  const composition::Rect middle = frame_of(hidden, "middle");
  tap(other, hidden,
      provider::PixelPoint{.x = middle.x + (middle.width * 0.5F),
                           .y = middle.y + (middle.height * 0.5F)});
  EXPECT_EQ(focused_key(hidden), "bottom");
}

TEST_F(ViewConfigurationTest, TintRecoloursTheAccentOfControlsBelowIt) {
  const auto prominent = [](std::optional<theme::Colour> tint) {
    composition::Element button =
        components::Button("Tinted", [] {})
            .button_style(components::ButtonStyle::bordered_prominent())
            .frame({.width = 300.0F, .height = 180.0F});
    if (tint.has_value()) {
      button = std::move(button).tint(*tint);
    }
    return button;
  };
  const std::array<int, 3> plain = pixel_of(prominent(std::nullopt), 20, 100);
  const std::array<int, 3> tinted =
      pixel_of(prominent(theme::Colour::hex(0xE02020)), 20, 100);
  EXPECT_NE(plain, tinted);
  EXPECT_GT(tinted[0], 150);
  EXPECT_LT(tinted[1], 90);
  EXPECT_LT(tinted[2], 90);
}

TEST_F(ViewConfigurationTest, ADisabledAutomaticButtonIsDimmedLikeSwiftUI) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers,
            components::HStack(tile("enabled"), tile("disabled").disabled()));
  settle(providers, application);
  EXPECT_FLOAT_EQ(opacity_of(application, "enabled"), 1.0F);
  EXPECT_NEAR(opacity_of(application, "disabled"),
              components::ButtonStyle::disabled_opacity, 1.0e-3F);
}

TEST_F(ViewConfigurationTest, DisabledDoesNotDimViewsThatAreNotControls) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers,
            components::HStack(components::Text("Plain").id("plain"),
                               components::Text("Off").disabled().id("off")));
  settle(providers, application);
  EXPECT_FLOAT_EQ(opacity_of(application, "plain"), 1.0F);
  EXPECT_FLOAT_EQ(opacity_of(application, "off"), 1.0F);
}

}
