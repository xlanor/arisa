#include <arisa/components/button.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/layer.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/provider/headless/headless_factory.hpp>

#include <scroll_fixture.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <format>
#include <string>
#include <utility>
#include <vector>

namespace arisa::runtime {

using namespace std::chrono_literals;
using composition::Element;

class DragDropTest : public ScrollFixture {
 protected:
  class Grid final : public composition::Component {
   public:
    static constexpr composition::StateKey<std::string> dropped{"dropped",
                                                                "none"};
    static constexpr composition::StateKey<int> hovering{"hovering", 0};

    auto operator==(const Grid&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto landed = context.state(dropped);
      const auto over = context.state(hovering);
      return components::VStack(
          components::HStack(button("source").draggable(
                                 std::string{"tile"},
                                 [] { return components::Text("preview"); }),
                             button("other")),
          components::VStack(
              components::Text(landed.get()),
              components::Text(std::format("over {}", over.get())))
              .frame({.width = 300.0F, .height = 120.0F})
              .focusable()
              .drop_destination(
                  std::type_identity<std::string>{},
                  [landed](std::vector<std::string> items, composition::Point) {
                    landed.set(items.front());
                    return true;
                  },
                  [over](bool inside) { over.set(inside ? 1 : 0); })
              .id("slot"));
    }
  };

  static auto preview_shown(Application& application) -> bool {
    return shows(application, "preview");
  }
};

TEST_F(DragDropTest, TouchLiftsDragsAndDrops) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, Grid{});
  const provider::PixelPoint from = centre(application, "source");
  const provider::PixelPoint to = centre(application, "slot");
  touch_down(providers, application, from);
  step(providers, application, 550ms);
  frames(application, 2);
  EXPECT_TRUE(application.interaction().drag().has_value());
  for (int index = 1; index <= 6; ++index) {
    const float progress = static_cast<float>(index) / 6.0F;
    touch_move(
        providers, application,
        provider::PixelPoint{.x = from.x + ((to.x - from.x) * progress),
                             .y = from.y + ((to.y - from.y) * progress)});
  }
  frames(application, 2);
  EXPECT_TRUE(preview_shown(application));
  EXPECT_TRUE(shows(application, "over 1"));
  touch_up(providers, application, to);
  frames(application, 3);
  EXPECT_TRUE(shows(application, "tile"));
  EXPECT_TRUE(shows(application, "over 0"));
  EXPECT_FALSE(application.interaction().drag().has_value());
  EXPECT_FALSE(preview_shown(application));
}

TEST_F(DragDropTest, MovingBeforeTheLiftScrollsOrTapsInstead) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, Grid{});
  const provider::PixelPoint from = centre(application, "source");
  drag(providers, application, from,
       provider::PixelPoint{.x = from.x, .y = from.y + 80.0F}, 4);
  frames(application, 2);
  EXPECT_FALSE(application.interaction().drag().has_value());
  EXPECT_TRUE(shows(application, "none"));
}

TEST_F(DragDropTest, ControllerPicksUpAndPutsDown) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, Grid{});
  EXPECT_EQ(focused_key(application), "source");
  const auto& before = application.interaction().hints();
  EXPECT_TRUE(
      std::ranges::any_of(before, [](const composition::CommandHint& hint) {
        return hint.button == provider::Button::x && hint.label == "Move";
      }));
  press(providers, application, provider::Button::x);
  EXPECT_TRUE(application.interaction().drag().has_value());
  EXPECT_TRUE(preview_shown(application));
  press(providers, application, provider::Button::dpad_down);
  EXPECT_EQ(focused_key(application), "slot");
  EXPECT_TRUE(shows(application, "over 1"));
  const auto& during = application.interaction().hints();
  EXPECT_TRUE(
      std::ranges::any_of(during, [](const composition::CommandHint& hint) {
        return hint.button == provider::Button::x && hint.label == "Drop";
      }));
  EXPECT_TRUE(
      std::ranges::any_of(during, [](const composition::CommandHint& hint) {
        return hint.button == provider::Button::b && hint.label == "Cancel";
      }));
  press(providers, application, provider::Button::a);
  frames(application, 2);
  EXPECT_TRUE(shows(application, "tile"));
  EXPECT_FALSE(application.interaction().drag().has_value());
}

TEST_F(DragDropTest, BackCancelsAControllerDrag) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, Grid{});
  press(providers, application, provider::Button::x);
  press(providers, application, provider::Button::dpad_down);
  press(providers, application, provider::Button::b);
  frames(application, 2);
  EXPECT_FALSE(application.interaction().drag().has_value());
  EXPECT_TRUE(shows(application, "none"));
  EXPECT_TRUE(shows(application, "over 0"));
}

}
