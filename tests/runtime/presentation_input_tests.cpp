#include <arisa/components/button.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/user_interface_size_class.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/layer.hpp>
#include <arisa/composition/presentation_detent.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/presentation/help.hpp>
#include <arisa/presentation/inspector.hpp>
#include <arisa/presentation/presentation_layer.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/structure/navigation_split_view.hpp>

#include <scroll_fixture.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <format>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace arisa::runtime {

using namespace std::chrono_literals;
using composition::Element;
using composition::PresentationDetent;

class PresentationInputTest : public ScrollFixture {
 protected:
  struct Scene {
    std::function<Element(composition::Context&)> build;
  };

  class Stage final : public composition::Component {
   public:
    explicit Stage(Scene* scene) noexcept : scene_(scene) {}

    auto operator==(const Stage&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      return scene_->build(context);
    }

   private:
    Scene* scene_;
  };

  static constexpr composition::StateKey<bool> flag_key{"flag", false};

  static void present(provider::headless::HeadlessProviderFactory& providers,
                      Application& application) {
    frames(application, 2);
    for (int index = 0; index < 40; ++index) {
      step(providers, application, 32ms);
    }
  }

  static auto lines(int count) -> Element {
    std::vector<Element> rows;
    for (int index = 0; index < count; ++index) {
      rows.push_back(
          components::Text(std::format("row {}", index))
              .frame({.height = 44.0F, .max_width = composition::infinity}));
    }
    return components::VStack(std::move(rows));
  }

  static auto sheet_scene(std::optional<composition::StateHandle<bool>>& shown)
      -> Scene {
    return Scene{[&shown](composition::Context& context) {
      const auto flag = context.state(flag_key);
      shown = flag;
      return button("open").sheet(flag, [] {
        return components::ScrollView(lines(40))
            .frame({.width = 400.0F, .max_height = composition::infinity})
            .id("scroll")
            .presentation_detents(
                {PresentationDetent::medium, PresentationDetent::large});
      });
    }};
  }
};

TEST_F(PresentationInputTest, DraggingDownFromTheTopOfASheetScrollDismisses) {
  provider::headless::HeadlessProviderFactory providers;
  std::optional<composition::StateHandle<bool>> shown;
  Scene scene = sheet_scene(shown);
  Application application = start(providers, Stage{&scene});
  shown->set(true);
  present(providers, application);
  const provider::PixelPoint inside = centre(application, "scroll");
  drag(providers, application, inside,
       provider::PixelPoint{.x = inside.x, .y = inside.y + 320.0F}, 8);
  present(providers, application);
  EXPECT_FALSE(shown->get());
}

TEST_F(PresentationInputTest, DraggingUpAtMediumGrowsTheSheetBeforeScrolling) {
  provider::headless::HeadlessProviderFactory providers;
  std::optional<composition::StateHandle<bool>> shown;
  Scene scene = sheet_scene(shown);
  Application application = start(providers, Stage{&scene});
  shown->set(true);
  present(providers, application);
  const composition::Rect medium = presented_of(application, "scroll");
  const provider::PixelPoint inside = centre(application, "scroll");
  drag(providers, application, inside,
       provider::PixelPoint{.x = inside.x, .y = inside.y - 200.0F}, 8);
  present(providers, application);
  EXPECT_TRUE(shown->get());
  EXPECT_GT(presented_of(application, "scroll").height, medium.height + 100.0F);
  EXPECT_FLOAT_EQ(offset(application, "scroll").y, 0.0F);
}

TEST_F(PresentationInputTest, ScrollingBackToTheTopHandsTheDragToTheSheet) {
  provider::headless::HeadlessProviderFactory providers;
  std::optional<composition::StateHandle<bool>> shown;
  Scene scene = sheet_scene(shown);
  Application application = start(providers, Stage{&scene});
  shown->set(true);
  present(providers, application);
  const provider::PixelPoint inside = centre(application, "scroll");
  drag(providers, application, inside,
       provider::PixelPoint{.x = inside.x, .y = inside.y - 200.0F}, 8);
  present(providers, application);
  const provider::PixelPoint grown = centre(application, "scroll");
  const provider::PixelPoint raised{.x = grown.x, .y = grown.y - 150.0F};
  drag(providers, application, grown, raised, 8, false);
  for (int index = 0; index < 10; ++index) {
    step(providers, application, 32ms);
  }
  touch_up(providers, application, raised);
  present(providers, application);
  ASSERT_GT(offset(application, "scroll").y, 50.0F);
  const provider::PixelPoint lowered{.x = grown.x, .y = grown.y + 500.0F};
  drag(providers, application, grown, lowered, 20, false);
  EXPECT_LE(offset(application, "scroll").y, 0.0F);
  EXPECT_NEAR(offset(application, "scroll").y, 0.0F, 4.0F);
  EXPECT_GT(presented_of(application, "scroll").y, grown.y);
  touch_up(providers, application, lowered);
  present(providers, application);
  EXPECT_FALSE(shown->get());
  EXPECT_FALSE(node_of(application, "scroll").has_value());
}

TEST_F(PresentationInputTest, TheInspectorColumnDragsBetweenItsBounds) {
  provider::headless::HeadlessProviderFactory providers;
  std::optional<composition::StateHandle<bool>> shown;
  Scene scene{[&shown](composition::Context& context) {
    const auto flag = context.state(flag_key);
    shown = flag;
    return button("content").inspector(flag, [] {
      return button("details").inspector_column_width(200.0F, 270.0F, 360.0F);
    });
  }};
  Application application = start(providers, Stage{&scene});
  shown->set(true);
  present(providers, application);
  const composition::Rect screen = application.root().tree().root().frame();
  const float start = presented_of(application, "details").x;
  const float inset = start - (screen.width - 270.0F);
  const provider::PixelPoint grip{.x = screen.width - 270.0F + 2.0F,
                                  .y = 40.0F};
  drag(providers, application, grip,
       provider::PixelPoint{.x = grip.x - 60.0F, .y = grip.y}, 6);
  present(providers, application);
  EXPECT_NEAR(presented_of(application, "details").x, start - 60.0F, 4.0F);
  const provider::PixelPoint again{.x = screen.width - 330.0F + 2.0F,
                                   .y = 40.0F};
  drag(providers, application, again,
       provider::PixelPoint{.x = again.x - 300.0F, .y = again.y}, 6);
  present(providers, application);
  EXPECT_NEAR(presented_of(application, "details").x,
              screen.width - 360.0F + inset, 4.0F);
}

TEST_F(PresentationInputTest, SplitColumnsDragWhenTheyDeclareARange) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers,
      Element{
          structure::NavigationSplitView(
              components::VStack(button("alpha"), button("beta"))
                  .frame({.max_width = composition::infinity})
                  .navigation_split_view_column_width(120.0F, 160.0F, 260.0F)
                  .id("side"),
              components::VStack(button("detail")))}
          .frame({.max_width = composition::infinity,
                  .max_height = composition::infinity})
          .environment(components::Environment::horizontal_size_class,
                       components::UserInterfaceSizeClass::regular));
  present(providers, application);
  const composition::Rect side = presented_of(application, "side");
  const provider::PixelPoint grip{.x = side.x + side.width - 2.0F,
                                  .y = side.y + (side.height * 0.5F)};
  drag(providers, application, grip,
       provider::PixelPoint{.x = grip.x + 50.0F, .y = grip.y}, 6);
  present(providers, application);
  EXPECT_NEAR(presented_of(application, "side").width, side.width + 50.0F,
              4.0F);
}

TEST_F(PresentationInputTest, HelpAppearsUnderAHoveringPointer) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers, components::VStack(button("first"),
                                    button("tipped").help("Saves the file")));
  const provider::PixelPoint over = centre(application, "tipped");
  provider::InputFrame moved;
  moved.pointer = provider::PointerState{
      .position = provider::PixelPoint{.x = over.x - 5.0F, .y = over.y}};
  providers.input().push(moved);
  frames(application, 1);
  provider::InputFrame hovering;
  hovering.pointer = provider::PointerState{.position = over};
  providers.input().push(hovering);
  frames(application, 2);
  EXPECT_FALSE(shows(application, "Saves the file"));
  for (int index = 0; index < 70; ++index) {
    step(providers, application, 20ms);
  }
  EXPECT_TRUE(shows(application, "Saves the file"));
}

}
