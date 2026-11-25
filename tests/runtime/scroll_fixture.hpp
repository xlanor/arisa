#pragma once

#include <arisa/composition/node.hpp>
#include <arisa/composition/node_id.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/provider.hpp>
#include <arisa/runtime/application.hpp>

#include <focus_fixture.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <optional>
#include <string_view>

namespace arisa::runtime {

class ScrollFixture : public FocusFixture {
 protected:
  static constexpr provider::TouchId finger{7};

  static auto touching(provider::PixelPoint point, provider::TouchPhase phase)
      -> provider::InputFrame {
    provider::InputFrame frame;
    frame.touches.push_back(
        provider::Touch{.id = finger, .position = point, .phase = phase});
    return frame;
  }

  static void step(provider::headless::HeadlessProviderFactory& providers,
                   Application& application,
                   std::chrono::milliseconds duration = frame_step) {
    ASSERT_TRUE(providers.clock().advance(duration).has_value());
    frames(application, 1);
  }

  static void touch_down(provider::headless::HeadlessProviderFactory& providers,
                         Application& application, provider::PixelPoint point) {
    providers.input().push(touching(point, provider::TouchPhase::began));
    frames(application, 1);
  }

  static void touch_move(provider::headless::HeadlessProviderFactory& providers,
                         Application& application, provider::PixelPoint point,
                         std::chrono::milliseconds duration = frame_step) {
    ASSERT_TRUE(providers.clock().advance(duration).has_value());
    providers.input().push(touching(point, provider::TouchPhase::moved));
    frames(application, 1);
  }

  static void touch_up(provider::headless::HeadlessProviderFactory& providers,
                       Application& application, provider::PixelPoint point,
                       std::chrono::milliseconds duration = frame_step) {
    ASSERT_TRUE(providers.clock().advance(duration).has_value());
    providers.input().push(touching(point, provider::TouchPhase::ended));
    frames(application, 1);
  }

  static auto layouts_during(
      provider::headless::HeadlessProviderFactory& providers,
      Application& application, int count,
      std::chrono::milliseconds duration = frame_step) -> int {
    int laid = 0;
    for (int index = 0; index < count; ++index) {
      ScrollFixture::step(providers, application, duration);
      laid += application.last_frame().laid_out ? 1 : 0;
    }
    return laid;
  }

  static void drag(provider::headless::HeadlessProviderFactory& providers,
                   Application& application, provider::PixelPoint from,
                   provider::PixelPoint to, int steps, bool release = true) {
    touch_down(providers, application, from);
    for (int index = 1; index <= steps; ++index) {
      const float progress =
          static_cast<float>(index) / static_cast<float>(steps);
      touch_move(
          providers, application,
          provider::PixelPoint{.x = from.x + ((to.x - from.x) * progress),
                               .y = from.y + ((to.y - from.y) * progress)});
    }
    if (release) {
      touch_up(providers, application, to);
    }
  }

  static auto offset(Application& application, std::string_view key)
      -> composition::Point {
    const auto node = node_named(application, key);
    if (!node.has_value()) {
      return composition::Point{};
    }
    return node->get().scroll_presentation().offset;
  }

  static auto frame_of(Application& application, std::string_view key)
      -> composition::Rect {
    const auto node = node_named(application, key);
    if (!node.has_value()) {
      return composition::Rect{};
    }
    return node->get().frame();
  }

  static auto presented_of(Application& application, std::string_view key)
      -> composition::Rect {
    const auto node = node_named(application, key);
    if (!node.has_value()) {
      return composition::Rect{};
    }
    const auto& presented = node->get().presented();
    if (!presented.has_value()) {
      ADD_FAILURE() << "node keyed " << key << " is not presented";
      return composition::Rect{};
    }
    return presented->bounds;
  }

  static auto settle(provider::headless::HeadlessProviderFactory& providers,
                     Application& application, int limit = 400) -> int {
    int laid = 0;
    for (int index = 0; index < limit; ++index) {
      ScrollFixture::step(providers, application);
      laid += application.last_frame().laid_out ? 1 : 0;
      if (application.waiting()) {
        break;
      }
    }
    return laid;
  }

  static auto count_keyed(const composition::Node& node,
                          std::string_view prefix) -> std::size_t {
    std::size_t total = 0;
    if (node.phase() != composition::NodeLifecyclePhase::departing &&
        node.key().has_value() &&
        node.key()->description().starts_with(prefix)) {
      ++total;
    }
    for (const composition::Node& child : node.children()) {
      total += count_keyed(child, prefix);
    }
    return total;
  }
};

}
