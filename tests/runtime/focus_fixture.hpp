#pragma once

#include <arisa/components/button.hpp>
#include <arisa/components/text.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/node_id.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/provider/input/provider.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/application_options.hpp>
#include <arisa/runtime/root.hpp>

#include <interaction/input_router.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::runtime {

class FocusFixture : public ::testing::Test {
 protected:
  static constexpr std::chrono::milliseconds frame_step{16};

  static auto start(provider::headless::HeadlessProviderFactory& providers,
                    composition::Element content,
                    interaction::InteractionOptions options = {})
      -> Application {
    auto application =
        Application::create(providers, std::move(content),
                            ApplicationOptions{.executors = std::nullopt,
                                               .fonts = {},
                                               .atlas = {},
                                               .interaction = options});
    EXPECT_TRUE(application.has_value());
    Application started = std::move(*application);
    frames(started, 2);
    return started;
  }

  static auto button(std::string key, float width = 100.0F,
                     float height = 50.0F) -> composition::Element {
    return components::Button(key, [] {})
        .frame({.width = width, .height = height})
        .id(std::move(key));
  }

  static auto pad(provider::Buttons buttons, provider::Stick left = {})
      -> provider::InputFrame {
    provider::InputFrame frame;
    frame.pads[0] = provider::PadState{
        .connected = true, .buttons = buttons, .left = left, .right = {}};
    return frame;
  }

  static void frames(Application& application, int count) {
    for (int index = 0; index < count; ++index) {
      ASSERT_TRUE(application.frame().has_value());
    }
  }

  static void press(provider::headless::HeadlessProviderFactory& providers,
                    Application& application, provider::Button which) {
    providers.input().push(pad({which}));
    providers.input().push(pad({}));
    frames(application, 3);
  }

  static void hold(provider::headless::HeadlessProviderFactory& providers,
                   Application& application, provider::InputFrame held,
                   std::chrono::milliseconds duration) {
    for (std::chrono::milliseconds elapsed{0}; elapsed <= duration;
         elapsed += frame_step) {
      providers.input().push(held);
      frames(application, 1);
      ASSERT_TRUE(providers.clock().advance(frame_step).has_value());
    }
    providers.input().push(pad({}));
    frames(application, 2);
  }

  static void tap(provider::headless::HeadlessProviderFactory& providers,
                  Application& application, provider::PixelPoint point) {
    provider::InputFrame down;
    down.touches.push_back(
        provider::Touch{.id = provider::TouchId{3},
                        .position = point,
                        .phase = provider::TouchPhase::began});
    provider::InputFrame up;
    up.touches.push_back(provider::Touch{.id = provider::TouchId{3},
                                         .position = point,
                                         .phase = provider::TouchPhase::ended});
    providers.input().push(down);
    providers.input().push(up);
    frames(application, 3);
  }

  static auto node_of(Application& application, std::string_view key)
      -> std::optional<composition::NodeId> {
    return application.root().tree().find(composition::ElementKey{key});
  }

  static auto node_named(Application& application, std::string_view key)
      -> std::optional<std::reference_wrapper<const composition::Node>> {
    const auto id = node_of(application, key);
    if (!id.has_value()) {
      ADD_FAILURE() << "no node keyed " << key;
      return std::nullopt;
    }
    return application.root().tree().node(*id);
  }

  static auto focused_key(Application& application) -> std::string {
    const auto focused = application.interaction().focused();
    if (!focused.has_value()) {
      return "none";
    }
    const auto node = application.root().tree().node(*focused);
    if (!node.has_value() || !node->get().key().has_value()) {
      return "unkeyed";
    }
    return node->get().key()->description();
  }

  static auto centre(Application& application, std::string_view key)
      -> provider::PixelPoint {
    const auto node = node_named(application, key);
    if (!node.has_value()) {
      return provider::PixelPoint{};
    }
    const composition::Rect frame = node->get().presented().has_value()
                                        ? node->get().presented()->bounds
                                        : node->get().frame();
    return provider::PixelPoint{.x = frame.x + (frame.width * 0.5F),
                                .y = frame.y + (frame.height * 0.5F)};
  }

  static auto texts(Application& application) -> std::vector<std::string> {
    std::vector<std::string> found;
    collect(application.root().tree().root(), found);
    return found;
  }

  static auto shows(Application& application, std::string_view text) -> bool {
    for (const std::string& each : texts(application)) {
      if (each == text) {
        return true;
      }
    }
    return false;
  }

 private:
  static void collect(const composition::Node& node,
                      std::vector<std::string>& found) {
    if (node.phase() == composition::NodeLifecyclePhase::departing) {
      return;
    }
    if (const auto text = node.content_as<components::Text::Content>()) {
      found.emplace_back(text->get().text());
    }
    for (const composition::Node& child : node.children()) {
      collect(child, found);
    }
  }
};

}
