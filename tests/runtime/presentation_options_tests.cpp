#include <arisa/components/button.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/bindable.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/layer.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/composition/presentation_background_interaction.hpp>
#include <arisa/composition/presentation_detent.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/presentation/inspector.hpp>
#include <arisa/presentation/modal_presentation.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/provider/surface/display_mode.hpp>
#include <arisa/theme/shape.hpp>

#include <scroll_fixture.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <expected>
#include <format>
#include <functional>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace arisa::runtime {

using namespace std::chrono_literals;
using composition::Element;
using composition::PresentationDetent;
using provider::Button;

class PresentationOptionsTest : public ScrollFixture {
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

  class Failure {
   public:
    auto error_description() const -> std::string { return "Load failed"; }

    auto recovery_suggestion() const -> std::string { return "Try again"; }
  };

  struct Model {
    bool shown{false};
    friend auto operator==(const Model&, const Model&) -> bool = default;
  };

  static constexpr float sheet_padding = 24.0F;
  static constexpr composition::StateKey<bool> flag_key{"flag", false};
  static constexpr composition::StateKey<std::optional<int>> item_key{
      "item", std::nullopt};

  static auto body(std::string key) -> Element {
    return components::ZStack()
        .frame({.max_width = composition::infinity,
                .max_height = composition::infinity})
        .id(std::move(key));
  }

  static auto layers(const composition::Node& node) -> std::size_t {
    std::size_t found =
        node.phase() != composition::NodeLifecyclePhase::departing &&
                node.content_as<composition::Layer::Content>().has_value()
            ? 1U
            : 0U;
    for (const composition::Node& child : node.children()) {
      found += layers(child);
    }
    return found;
  }

  static auto grabbers(const composition::Node& node) -> std::size_t {
    const composition::Rect& laid = node.frame();
    std::size_t found =
        node.phase() != composition::NodeLifecyclePhase::departing &&
                std::abs(laid.width -
                         presentation::ModalPresentation::grabber_width) <
                    0.5F &&
                std::abs(laid.height -
                         presentation::ModalPresentation::grabber_height) < 0.5F
            ? 1U
            : 0U;
    for (const composition::Node& child : node.children()) {
      found += grabbers(child);
    }
    return found;
  }

  static auto outlined(const composition::Node& node, const theme::Shape& shape)
      -> bool {
    if (node.modifiers().background.has_value() &&
        node.modifiers().background->shape == shape) {
      return true;
    }
    return std::ranges::any_of(node.children(),
                               [&shape](const composition::Node& child) {
                                 return outlined(child, shape);
                               });
  }

  static auto layer_count(Application& application) -> std::size_t {
    return layers(application.root().tree().root());
  }

  static auto screen(Application& application) -> composition::Rect {
    return application.root().tree().root().frame();
  }

  static void present(provider::headless::HeadlessProviderFactory& providers,
                      Application& application) {
    frames(application, 2);
    for (int index = 0; index < 40; ++index) {
      step(providers, application, 32ms);
    }
  }

  static auto activatable_with_text(const composition::Tree& tree,
                                    const composition::Node& node,
                                    std::string_view text)
      -> std::optional<composition::NodeId> {
    for (const composition::Node& child : node.children()) {
      if (const auto found = activatable_with_text(tree, child, text)) {
        return found;
      }
    }
    const auto shown = node.content_as<components::Text::Content>();
    if (!shown.has_value() || shown->get().text() != text) {
      return std::nullopt;
    }
    std::optional<composition::NodeId> at = node.parent();
    while (at.has_value()) {
      const auto found = tree.node(*at);
      if (!found.has_value()) {
        return std::nullopt;
      }
      if (found->get().content().activatable()) {
        return *at;
      }
      at = found->get().parent();
    }
    return std::nullopt;
  }

  static auto hinted(Application& application, Button button,
                     std::string_view label) -> bool {
    return std::ranges::any_of(
        application.interaction().hints(),
        [button, label](const composition::CommandHint& hint) {
          return hint.button == button && hint.label == label;
        });
  }
};

TEST_F(PresentationOptionsTest, SheetBindsToAModelField) {
  provider::headless::HeadlessProviderFactory providers;
  composition::Observable<Model> model{Model{}};
  Scene scene{[&model](composition::Context& context) {
    return button("open").sheet(
        composition::Bindable{context, model}[&Model::shown],
        [] { return button("inside"); });
  }};
  Application application = start(providers, Stage{&scene});
  EXPECT_EQ(layer_count(application), 0U);
  model.write(&Model::shown, true);
  present(providers, application);
  EXPECT_EQ(layer_count(application), 1U);
  EXPECT_EQ(focused_key(application), "inside");
  press(providers, application, Button::b);
  EXPECT_FALSE(model.get().shown);
  present(providers, application);
  EXPECT_EQ(layer_count(application), 0U);
  EXPECT_EQ(focused_key(application), "open");
}

TEST_F(PresentationOptionsTest,
       DragIndicatorVisibilityOverridesTheDetentCount) {
  for (const auto& [visibility, detents, expected] :
       {std::tuple{composition::Visibility::hidden, 2U, 0U},
        std::tuple{composition::Visibility::automatic, 2U, 1U},
        std::tuple{composition::Visibility::visible, 1U, 1U},
        std::tuple{composition::Visibility::automatic, 1U, 0U}}) {
    provider::headless::HeadlessProviderFactory providers;
    std::optional<composition::StateHandle<bool>> shown;
    Scene scene{[&shown, visibility, detents](composition::Context& context) {
      const auto flag = context.state(flag_key);
      shown = flag;
      return button("open").sheet(flag, [visibility, detents] {
        std::vector<PresentationDetent> chosen{PresentationDetent::medium};
        if (detents > 1) {
          chosen.push_back(PresentationDetent::large);
        }
        return components::VStack(button("inside"), body("body"))
            .presentation_detents(chosen)
            .presentation_drag_indicator(visibility);
      });
    }};
    Application application = start(providers, Stage{&scene});
    shown->set(true);
    present(providers, application);
    EXPECT_EQ(grabbers(application.root().tree().root()), expected) << detents;
  }
}

TEST_F(PresentationOptionsTest, PresentationCornerRadiusRoundsTheSheet) {
  provider::headless::HeadlessProviderFactory providers;
  std::optional<composition::StateHandle<bool>> shown;
  Scene scene{[&shown](composition::Context& context) {
    const auto flag = context.state(flag_key);
    shown = flag;
    return button("open").sheet(flag, [] {
      return components::VStack(button("inside"), body("body"))
          .presentation_corner_radius(30.0F);
    });
  }};
  Application application = start(providers, Stage{&scene});
  shown->set(true);
  present(providers, application);
  EXPECT_TRUE(
      outlined(application.root().tree().root(),
               theme::RoundedRectangle(theme::Radius{30.0F},
                                       theme::RoundedCornerStyle::continuous)));
  EXPECT_FALSE(
      outlined(application.root().tree().root(),
               theme::RoundedRectangle(theme::radius::sheet,
                                       theme::RoundedCornerStyle::continuous)));
}

TEST_F(PresentationOptionsTest, DetentsSizeTheSheetAndTheRightStickResizes) {
  provider::headless::HeadlessProviderFactory providers;
  std::optional<composition::StateHandle<bool>> shown;
  Scene scene{[&shown](composition::Context& context) {
    const auto flag = context.state(flag_key);
    shown = flag;
    return button("open").sheet(flag, [] {
      return components::VStack(button("inside"), body("body"))
          .presentation_detents(
              {PresentationDetent::large, PresentationDetent::medium});
    });
  }};
  Application application = start(providers, Stage{&scene});
  shown->set(true);
  present(providers, application);
  const float height = screen(application).height;
  const float largest = height * presentation::PresentationLayer::sheet_height;
  const composition::Rect medium = presented_of(application, "body");
  EXPECT_NEAR(medium.y + medium.height,
              (height + largest) * 0.5F - sheet_padding, 4.0F);
  EXPECT_TRUE(hinted(application, Button::right_stick, "Resize"));
  press(providers, application, Button::right_stick);
  present(providers, application);
  const composition::Rect large = presented_of(application, "body");
  EXPECT_GT(large.height, medium.height + (largest * 0.4F));
  press(providers, application, Button::right_stick);
  present(providers, application);
  EXPECT_NEAR(presented_of(application, "body").height, medium.height, 1.0F);
}

struct ThirdDetent {
  static auto height(const PresentationDetent::Context& context)
      -> std::optional<float> {
    return context.max_detent_value / 3.0F;
  }
};

struct HiddenDetent {
  static auto height(const PresentationDetent::Context&)
      -> std::optional<float> {
    return std::nullopt;
  }
};

TEST_F(PresentationOptionsTest, CustomDetentsResolveFromTheLargestHeight) {
  constexpr PresentationDetent third =
      PresentationDetent::custom<ThirdDetent>();
  constexpr PresentationDetent hidden =
      PresentationDetent::custom<HiddenDetent>();
  EXPECT_FLOAT_EQ(third.resolve(900.0F), 300.0F);
  EXPECT_TRUE(third.available(900.0F));
  EXPECT_FALSE(hidden.available(900.0F));
  EXPECT_NE(third, hidden);
  EXPECT_EQ(third, PresentationDetent::custom<ThirdDetent>());
  provider::headless::HeadlessProviderFactory providers;
  std::optional<composition::StateHandle<bool>> shown;
  Scene scene{[&shown](composition::Context& context) {
    const auto flag = context.state(flag_key);
    shown = flag;
    return button("open").sheet(flag, [] {
      return components::VStack(button("inside"), body("body"))
          .presentation_detents({PresentationDetent::custom<HiddenDetent>(),
                                 PresentationDetent::custom<ThirdDetent>(),
                                 PresentationDetent::large});
    });
  }};
  Application application = start(providers, Stage{&scene});
  shown->set(true);
  present(providers, application);
  const float largest = screen(application).height *
                        presentation::PresentationLayer::sheet_height;
  const composition::Rect small = presented_of(application, "body");
  EXPECT_LT(small.height, largest / 3.0F);
  press(providers, application, Button::right_stick);
  present(providers, application);
  EXPECT_GT(presented_of(application, "body").height,
            small.height + (largest * 0.5F));
}

TEST_F(PresentationOptionsTest, DetentSelectionBindingFollowsResizing) {
  provider::headless::HeadlessProviderFactory providers;
  composition::Observable<PresentationDetent> chosen{
      PresentationDetent::medium};
  std::optional<composition::StateHandle<bool>> shown;
  Scene scene{[&shown, &chosen](composition::Context& context) {
    const auto flag = context.state(flag_key);
    shown = flag;
    static_cast<void>(chosen.get());
    return button("open").sheet(flag, [&chosen] {
      return body("body").presentation_detents(
          {PresentationDetent::medium, PresentationDetent::large},
          composition::Binding<PresentationDetent>{
              [&chosen] { return chosen.get(); },
              [&chosen](PresentationDetent value) { chosen.set(value); }});
    });
  }};
  Application application = start(providers, Stage{&scene});
  shown->set(true);
  present(providers, application);
  press(providers, application, Button::right_stick);
  present(providers, application);
  EXPECT_EQ(chosen.get(), PresentationDetent::large);
  chosen.set(PresentationDetent::medium);
  present(providers, application);
  const float largest = screen(application).height *
                        presentation::PresentationLayer::sheet_height;
  EXPECT_LT(presented_of(application, "body").height, largest * 0.6F);
}

TEST_F(PresentationOptionsTest, DraggingTheGrabberDownDismisses) {
  provider::headless::HeadlessProviderFactory providers;
  std::optional<composition::StateHandle<bool>> shown;
  Scene scene{[&shown](composition::Context& context) {
    const auto flag = context.state(flag_key);
    shown = flag;
    return button("open").sheet(flag, [] {
      return body("body").presentation_detents(
          {PresentationDetent::medium, PresentationDetent::large});
    });
  }};
  Application application = start(providers, Stage{&scene});
  shown->set(true);
  present(providers, application);
  const composition::Rect area = presented_of(application, "body");
  const provider::PixelPoint grip{.x = area.x + (area.width * 0.5F),
                                  .y = area.y - sheet_padding + 8.0F};
  drag(providers, application, grip,
       provider::PixelPoint{.x = grip.x, .y = grip.y + 300.0F}, 6);
  frames(application, 2);
  EXPECT_FALSE(shown->get());
}

TEST_F(PresentationOptionsTest, BackgroundInteractionLetsTapsThrough) {
  provider::headless::HeadlessProviderFactory providers;
  std::optional<composition::StateHandle<bool>> shown;
  int taps = 0;
  Scene scene{[&shown, &taps](composition::Context& context) {
    const auto flag = context.state(flag_key);
    shown = flag;
    return components::VStack(
               components::Button("behind", [&taps] { ++taps; })
                   .frame({.width = 200.0F, .height = 60.0F})
                   .id("behind"),
               button("open").sheet(
                   flag,
                   [] {
                     return body("body")
                         .presentation_detents({PresentationDetent::medium})
                         .presentation_background_interaction(
                             composition::PresentationBackgroundInteraction::
                                 enabled);
                   }))
        .frame({.max_width = composition::infinity,
                .max_height = composition::infinity,
                .alignment = composition::Alignment::top});
  }};
  Application application = start(providers, Stage{&scene});
  const provider::PixelPoint behind = centre(application, "behind");
  shown->set(true);
  present(providers, application);
  tap(providers, application, behind);
  EXPECT_EQ(taps, 1);
  EXPECT_TRUE(shown->get());
}

TEST_F(PresentationOptionsTest, FormSizingNarrowsTheSheet) {
  provider::headless::HeadlessProviderFactory providers;
  std::optional<composition::StateHandle<bool>> shown;
  Scene scene{[&shown](composition::Context& context) {
    const auto flag = context.state(flag_key);
    shown = flag;
    return button("open").sheet(flag, [] {
      return body("body").presentation_sizing(
          composition::PresentationSizing::form);
    });
  }};
  Application application = start(providers, Stage{&scene});
  shown->set(true);
  present(providers, application);
  EXPECT_NEAR(presented_of(application, "body").width,
              (screen(application).width *
               presentation::PresentationLayer::form_width) -
                  (2.0F * sheet_padding),
              2.0F);
}

TEST_F(PresentationOptionsTest, DismissalConfirmationAsksBeforeClosing) {
  provider::headless::HeadlessProviderFactory providers;
  std::optional<composition::StateHandle<bool>> shown;
  std::vector<std::string> log;
  Scene scene{[&shown, &log](composition::Context& context) {
    const auto flag = context.state(flag_key);
    shown = flag;
    return button("open").sheet(flag, [&log] {
      return button("inside").dismissal_confirmation_dialog(
          "Discard?", true, [&log] {
            return components::Button("Discard",
                                      components::ButtonRole::destructive,
                                      [&log] { log.emplace_back("discard"); })
                .id("discard");
          });
    });
  }};
  Application application = start(providers, Stage{&scene});
  shown->set(true);
  present(providers, application);
  press(providers, application, Button::b);
  present(providers, application);
  EXPECT_TRUE(shown->get());
  EXPECT_EQ(layer_count(application), 2U);
  press(providers, application, Button::b);
  present(providers, application);
  EXPECT_TRUE(shown->get());
  EXPECT_EQ(layer_count(application), 1U);
  EXPECT_EQ(focused_key(application), "inside");
  press(providers, application, Button::b);
  present(providers, application);
  ASSERT_TRUE(application.root().activate(composition::ElementKey{"discard"}));
  present(providers, application);
  EXPECT_EQ(log, std::vector<std::string>{"discard"});
  EXPECT_FALSE(shown->get());
}

TEST_F(PresentationOptionsTest, PopoverAdaptsToASheetWhenCompact) {
  for (const bool handheld : {false, true}) {
    provider::headless::HeadlessProviderFactory providers;
    if (handheld) {
      providers.surface().set_display_mode(provider::DisplayMode::handheld);
    }
    std::optional<composition::StateHandle<bool>> shown;
    Scene scene{[&shown](composition::Context& context) {
      const auto flag = context.state(flag_key);
      shown = flag;
      return components::ZStack(
                 components::ZStackOptions{
                     .alignment = composition::Alignment::top_leading},
                 button("anchor").popover(
                     flag,
                     [] {
                       return components::ZStack()
                           .frame({.width = 200.0F, .height = 80.0F})
                           .id("panel")
                           .presentation_compact_adaptation(
                               composition::PresentationAdaptation::sheet);
                     }))
          .frame({.max_width = composition::infinity,
                  .max_height = composition::infinity});
    }};
    Application application = start(providers, Stage{&scene});
    shown->set(true);
    present(providers, application);
    const composition::Rect panel = presented_of(application, "panel");
    const composition::Rect anchor = presented_of(application, "anchor");
    const composition::Rect window = screen(application);
    if (handheld) {
      EXPECT_GT(panel.x, window.width * 0.15F) << "handheld";
    } else {
      EXPECT_NEAR(panel.x + (panel.width * 0.5F),
                  anchor.x + (anchor.width * 0.5F), 40.0F);
    }
    press(providers, application, Button::b);
    EXPECT_FALSE(shown->get());
  }
}

TEST_F(PresentationOptionsTest, ItemCoverAndPopoverPresentTheirItem) {
  provider::headless::HeadlessProviderFactory providers;
  std::optional<composition::StateHandle<std::optional<int>>> item;
  std::optional<composition::StateHandle<bool>> pop;
  Scene scene{[&item, &pop](composition::Context& context) {
    const auto chosen = context.state(item_key);
    const auto flag = context.state(flag_key);
    item = chosen;
    pop = flag;
    return components::VStack(
        button("cover").full_screen_cover(
            chosen,
            [](int shown) { return button(std::format("cover-{}", shown)); }),
        button("pop").popover(
            composition::Binding<std::optional<int>>{
                [flag] {
                  return flag.get() ? std::optional<int>{4} : std::nullopt;
                },
                [flag](std::optional<int> value) {
                  flag.set(value.has_value());
                }},
            [](int shown) { return button(std::format("pop-{}", shown)); }));
  }};
  Application application = start(providers, Stage{&scene});
  item->set(3);
  present(providers, application);
  EXPECT_EQ(focused_key(application), "cover-3");
  press(providers, application, Button::b);
  EXPECT_FALSE(item->get().has_value());
  present(providers, application);
  pop->set(true);
  present(providers, application);
  EXPECT_EQ(focused_key(application), "pop-4");
  press(providers, application, Button::b);
  EXPECT_FALSE(pop->get());
}

TEST_F(PresentationOptionsTest, AlertPresentingNeedsData) {
  provider::headless::HeadlessProviderFactory providers;
  std::optional<composition::StateHandle<bool>> shown;
  std::optional<int> data;
  std::vector<std::string> log;
  Scene scene{[&](composition::Context& context) {
    const auto flag = context.state(flag_key);
    shown = flag;
    return button("ask").alert("Title", flag, data, [&log](int value) {
      return components::Button(
                 std::format("Use {}", value),
                 [&log, value] { log.push_back(std::to_string(value)); })
          .id("use");
    });
  }};
  Application application = start(providers, Stage{&scene});
  shown->set(true);
  present(providers, application);
  EXPECT_EQ(layer_count(application), 0U);
  data = 9;
  shown->set(false);
  present(providers, application);
  shown->set(true);
  present(providers, application);
  EXPECT_EQ(layer_count(application), 1U);
  ASSERT_TRUE(application.root().activate(composition::ElementKey{"use"}));
  frames(application, 2);
  EXPECT_EQ(log, std::vector<std::string>{"9"});
  EXPECT_FALSE(shown->get());
}

TEST_F(PresentationOptionsTest, AlertItemDismissesByClearingTheItem) {
  provider::headless::HeadlessProviderFactory providers;
  std::optional<composition::StateHandle<std::optional<int>>> item;
  Scene scene{[&item](composition::Context& context) {
    const auto chosen = context.state(item_key);
    item = chosen;
    return button("ask").alert("Item", chosen, [](int value) {
      return components::Button(std::format("Open {}", value), [] {});
    });
  }};
  Application application = start(providers, Stage{&scene});
  item->set(5);
  present(providers, application);
  EXPECT_EQ(layer_count(application), 1U);
  EXPECT_TRUE(shows(application, "Open 5"));
  press(providers, application, Button::b);
  EXPECT_FALSE(item->get().has_value());
}

TEST_F(PresentationOptionsTest, ErrorAlertTitlesItselfFromTheError) {
  provider::headless::HeadlessProviderFactory providers;
  std::optional<composition::StateHandle<bool>> shown;
  Scene scene{[&shown](composition::Context& context) {
    const auto flag = context.state(flag_key);
    shown = flag;
    const std::expected<int, Failure> loaded = std::unexpected{Failure{}};
    return button("load").alert(flag, loaded, [](const Failure&) {
      return components::Button("OK", components::ButtonRole::cancel, [] {});
    });
  }};
  Application application = start(providers, Stage{&scene});
  shown->set(true);
  present(providers, application);
  EXPECT_TRUE(shows(application, "Load failed"));
  EXPECT_TRUE(shows(application, "Try again"));
  press(providers, application, Button::b);
  EXPECT_FALSE(shown->get());
}

TEST_F(PresentationOptionsTest, DialogExtrasShowIconAndSuppression) {
  provider::headless::HeadlessProviderFactory providers;
  std::optional<composition::StateHandle<bool>> shown;
  composition::Observable<bool> suppressed{false};
  Scene scene{[&shown, &suppressed](composition::Context& context) {
    const auto flag = context.state(flag_key);
    shown = flag;
    return button("ask").alert("Delete?", flag, [&suppressed] {
      return components::Group(
                 components::Button("Delete",
                                    components::ButtonRole::destructive, [] {}))
          .dialog_icon(components::Image::system_name("delete"))
          .dialog_severity(composition::DialogSeverity::critical)
          .dialog_suppression_toggle(composition::Binding<bool>{
              [&suppressed] { return suppressed.get(); },
              [&suppressed](bool value) { suppressed.set(value); }});
    });
  }};
  Application application = start(providers, Stage{&scene});
  shown->set(true);
  present(providers, application);
  EXPECT_TRUE(shows(application, std::string{Element::suppression_label}));
  const auto toggle = activatable_with_text(application.root().tree(),
                                            application.root().tree().root(),
                                            Element::suppression_label);
  ASSERT_TRUE(toggle.has_value());
  ASSERT_TRUE(application.root().activate(*toggle));
  frames(application, 2);
  EXPECT_TRUE(suppressed.get());
  EXPECT_TRUE(shown->get());
}

TEST_F(PresentationOptionsTest, InspectorDocksBesideContentOrBecomesASheet) {
  for (const bool handheld : {false, true}) {
    provider::headless::HeadlessProviderFactory providers;
    if (handheld) {
      providers.surface().set_display_mode(provider::DisplayMode::handheld);
    }
    std::optional<composition::StateHandle<bool>> shown;
    Scene scene{[&shown](composition::Context& context) {
      const auto flag = context.state(flag_key);
      shown = flag;
      return button("content").inspector(flag, [] {
        return button("details").inspector_column_width(300.0F);
      });
    }};
    Application application = start(providers, Stage{&scene});
    shown->set(true);
    present(providers, application);
    if (handheld) {
      EXPECT_EQ(layer_count(application), 1U);
    } else {
      EXPECT_EQ(layer_count(application), 0U);
      const composition::Rect details = presented_of(application, "details");
      EXPECT_GT(details.x, screen(application).width - 300.0F);
      EXPECT_LT(details.x, screen(application).width - 300.0F + 40.0F);
    }
    shown->set(false);
    present(providers, application);
    EXPECT_FALSE(node_of(application, "details").has_value() &&
                 layer_count(application) > 0U);
  }
}

}
