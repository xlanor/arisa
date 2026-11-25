#include <arisa/provider/sdl3/sdl3_event_pump.hpp>
#include <arisa/provider/sdl3/sdl3_lifecycle.hpp>
#include <arisa/provider/standard/std_threading.hpp>

#include "sdl3_test.hpp"

#include <SDL3/SDL_events.h>

#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

namespace arisa::provider::sdl3 {

class Sdl3EventPumpTest : public Sdl3Test {
 protected:
  static void push(SDL_EventType type) {
    SDL_Event event{};
    event.type = type;
    ASSERT_TRUE(SDL_PushEvent(&event));
  }

  static auto drain(Sdl3EventPump::Subscription& subscription)
      -> std::vector<Uint32> {
    std::vector<Uint32> types;
    while (const std::optional<SDL_Event> event = subscription.next()) {
      types.push_back(event->type);
    }
    return types;
  }

  standard::StdThreadingProvider threading_;
};

TEST_F(Sdl3EventPumpTest, ClassifiesEventsByCategory) {
  const auto category = [](SDL_EventType type) {
    SDL_Event event{};
    event.type = type;
    return Sdl3EventPump::classify(event);
  };
  EXPECT_EQ(category(SDL_EVENT_QUIT), Sdl3EventCategory::lifecycle);
  EXPECT_EQ(category(SDL_EVENT_WINDOW_FOCUS_LOST),
            Sdl3EventCategory::lifecycle);
  EXPECT_EQ(category(SDL_EVENT_DID_ENTER_BACKGROUND),
            Sdl3EventCategory::lifecycle);
  EXPECT_EQ(category(SDL_EVENT_WINDOW_RESIZED), Sdl3EventCategory::window);
  EXPECT_EQ(category(SDL_EVENT_DISPLAY_ADDED), Sdl3EventCategory::window);
  EXPECT_EQ(category(SDL_EVENT_KEY_DOWN), Sdl3EventCategory::input);
  EXPECT_EQ(category(SDL_EVENT_GAMEPAD_BUTTON_DOWN), Sdl3EventCategory::input);
  EXPECT_EQ(category(SDL_EVENT_FINGER_MOTION), Sdl3EventCategory::input);
  EXPECT_EQ(category(SDL_EVENT_TEXT_INPUT), Sdl3EventCategory::text);
  EXPECT_EQ(category(SDL_EVENT_CLIPBOARD_UPDATE), std::nullopt);
  EXPECT_EQ(category(SDL_EVENT_USER), std::nullopt);
}

TEST_F(Sdl3EventPumpTest, RoutesEachEventToEveryConsumerOfItsCategory) {
  auto pump = Sdl3EventPump::create(threading_);
  ASSERT_TRUE(pump.has_value());
  EXPECT_EQ(pump->owner(), threading_.current_thread());
  auto lifecycle = pump->subscribe(Sdl3EventCategory::lifecycle);
  auto second = pump->subscribe(Sdl3EventCategory::lifecycle);
  auto window = pump->subscribe(Sdl3EventCategory::window);
  pump->pump();
  static_cast<void>(drain(lifecycle));
  static_cast<void>(drain(second));
  static_cast<void>(drain(window));
  push(SDL_EVENT_WINDOW_FOCUS_GAINED);
  push(SDL_EVENT_WINDOW_RESIZED);
  push(SDL_EVENT_USER);
  pump->pump();
  EXPECT_EQ(drain(lifecycle),
            std::vector<Uint32>{SDL_EVENT_WINDOW_FOCUS_GAINED});
  EXPECT_EQ(drain(second), std::vector<Uint32>{SDL_EVENT_WINDOW_FOCUS_GAINED});
  EXPECT_EQ(drain(window), std::vector<Uint32>{SDL_EVENT_WINDOW_RESIZED});
  EXPECT_EQ(lifecycle.category(), Sdl3EventCategory::lifecycle);
}

TEST_F(Sdl3EventPumpTest, DroppedSubscriptionsStopReceiving) {
  auto pump = Sdl3EventPump::create(threading_);
  ASSERT_TRUE(pump.has_value());
  auto kept = pump->subscribe(Sdl3EventCategory::lifecycle);
  {
    const auto dropped = pump->subscribe(Sdl3EventCategory::lifecycle);
  }
  pump->pump();
  static_cast<void>(drain(kept));
  push(SDL_EVENT_QUIT);
  pump->pump();
  EXPECT_EQ(drain(kept), std::vector<Uint32>{SDL_EVENT_QUIT});
}

TEST_F(Sdl3EventPumpTest, QueuesAreBoundedAndKeepTheNewest) {
  auto pump = Sdl3EventPump::create(threading_);
  ASSERT_TRUE(pump.has_value());
  auto input = pump->subscribe(Sdl3EventCategory::input);
  pump->pump();
  static_cast<void>(drain(input));
  for (std::size_t index = 0; index < Sdl3EventPump::queue_limit; ++index) {
    push(SDL_EVENT_KEY_DOWN);
  }
  push(SDL_EVENT_KEY_UP);
  pump->pump();
  EXPECT_EQ(input.pending(), Sdl3EventPump::queue_limit);
  const std::vector<Uint32> types = drain(input);
  ASSERT_FALSE(types.empty());
  EXPECT_EQ(types.back(), SDL_EVENT_KEY_UP);
}

TEST_F(Sdl3EventPumpTest, LifecycleTranslatesAndTracksPhase) {
  auto pump = Sdl3EventPump::create(threading_);
  ASSERT_TRUE(pump.has_value());
  Sdl3Lifecycle lifecycle{*pump};
  while (lifecycle.poll().has_value()) {
  }
  EXPECT_EQ(lifecycle.phase(), ScenePhase::active);
  push(SDL_EVENT_WINDOW_FOCUS_LOST);
  push(SDL_EVENT_WINDOW_RESIZED);
  push(SDL_EVENT_DID_ENTER_BACKGROUND);
  push(SDL_EVENT_WILL_ENTER_FOREGROUND);
  push(SDL_EVENT_DID_ENTER_FOREGROUND);
  push(SDL_EVENT_WINDOW_FOCUS_GAINED);
  push(SDL_EVENT_QUIT);
  EXPECT_EQ(lifecycle.poll(), LifecycleEvent::focus_lost);
  EXPECT_EQ(lifecycle.phase(), ScenePhase::inactive);
  EXPECT_EQ(lifecycle.poll(), LifecycleEvent::suspending);
  EXPECT_EQ(lifecycle.phase(), ScenePhase::background);
  EXPECT_EQ(lifecycle.poll(), LifecycleEvent::resumed);
  EXPECT_EQ(lifecycle.phase(), ScenePhase::inactive);
  EXPECT_EQ(lifecycle.poll(), LifecycleEvent::focus_gained);
  EXPECT_EQ(lifecycle.phase(), ScenePhase::active);
  EXPECT_EQ(lifecycle.poll(), LifecycleEvent::exit_requested);
  EXPECT_EQ(lifecycle.poll(), std::nullopt);
}

TEST_F(Sdl3EventPumpTest, LifecycleCannotBlockHome) {
  auto pump = Sdl3EventPump::create(threading_);
  ASSERT_TRUE(pump.has_value());
  Sdl3Lifecycle lifecycle{*pump};
  const auto block = lifecycle.block_home_button();
  ASSERT_FALSE(block.has_value());
  EXPECT_EQ(block.error().code(), ProviderErrorCode::unsupported_capability);
}

}
