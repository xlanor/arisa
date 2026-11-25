#include <arisa/provider/sdl3/sdl3_event_pump.hpp>

#include <SDL3/SDL_version.h>

#include <cassert>
#include <utility>

namespace arisa::provider::sdl3 {

Sdl3EventPump::Sdl3EventPump(Sdl3Context events,
                             const ThreadingProvider& threading,
                             ThreadId owner) noexcept
    : events_(std::move(events)), threading_(threading), owner_(owner) {}

auto Sdl3EventPump::create(const ThreadingProvider& threading)
    -> std::expected<Sdl3EventPump, ProviderError> {
  return Sdl3Context::create(SDL_INIT_EVENTS)
      .transform([&threading](Sdl3Context events) {
        return Sdl3EventPump{std::move(events), threading,
                             threading.current_thread()};
      });
}

auto Sdl3EventPump::subscribe(Sdl3EventCategory category) -> Subscription {
  auto queue = std::make_shared<Queue>();
  consumers_.push_back(Consumer{.category = category, .queue = queue});
  return Subscription{category, std::move(queue)};
}

void Sdl3EventPump::pump() {
  assert(threading_.get().current_thread() == owner_);
  std::erase_if(consumers_, [](const Consumer& consumer) {
    return consumer.queue.expired();
  });
  SDL_Event event{};
  while (SDL_PollEvent(&event)) {
    dispatch(event);
  }
}

auto Sdl3EventPump::classify(const SDL_Event& event) noexcept
    -> std::optional<Sdl3EventCategory> {
  const auto type = static_cast<SDL_EventType>(event.type);
  switch (type) {
    case SDL_EVENT_QUIT:
    case SDL_EVENT_TERMINATING:
    case SDL_EVENT_WILL_ENTER_BACKGROUND:
    case SDL_EVENT_DID_ENTER_BACKGROUND:
    case SDL_EVENT_WILL_ENTER_FOREGROUND:
    case SDL_EVENT_DID_ENTER_FOREGROUND:
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
    case SDL_EVENT_WINDOW_FOCUS_LOST:
      return Sdl3EventCategory::lifecycle;
    case SDL_EVENT_TEXT_EDITING:
    case SDL_EVENT_TEXT_INPUT:
    case SDL_EVENT_TEXT_EDITING_CANDIDATES:
#if SDL_VERSION_ATLEAST(3, 4, 0)
    case SDL_EVENT_SCREEN_KEYBOARD_SHOWN:
    case SDL_EVENT_SCREEN_KEYBOARD_HIDDEN:
#endif
      return Sdl3EventCategory::text;
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
    case SDL_EVENT_KEYMAP_CHANGED:
    case SDL_EVENT_KEYBOARD_ADDED:
    case SDL_EVENT_KEYBOARD_REMOVED:
    case SDL_EVENT_SENSOR_UPDATE:
      return Sdl3EventCategory::input;
    default:
      break;
  }
#if SDL_VERSION_ATLEAST(3, 4, 0)
  constexpr SDL_EventType last_touch = SDL_EVENT_PINCH_END;
#else
  constexpr SDL_EventType last_touch = SDL_EVENT_FINGER_CANCELED;
#endif
  const auto within = [type](SDL_EventType first, SDL_EventType last) {
    return type >= first && type <= last;
  };
  if (within(SDL_EVENT_WINDOW_FIRST, SDL_EVENT_WINDOW_LAST) ||
      within(SDL_EVENT_DISPLAY_FIRST, SDL_EVENT_DISPLAY_LAST)) {
    return Sdl3EventCategory::window;
  }
  if (within(SDL_EVENT_MOUSE_MOTION, SDL_EVENT_MOUSE_REMOVED) ||
      within(SDL_EVENT_JOYSTICK_AXIS_MOTION,
             SDL_EVENT_GAMEPAD_STEAM_HANDLE_UPDATED) ||
      within(SDL_EVENT_FINGER_DOWN, last_touch) ||
      within(SDL_EVENT_PEN_PROXIMITY_IN, SDL_EVENT_PEN_AXIS)) {
    return Sdl3EventCategory::input;
  }
  return std::nullopt;
}

void Sdl3EventPump::dispatch(const SDL_Event& event) {
  const std::optional<Sdl3EventCategory> category = classify(event);
  if (!category.has_value()) {
    return;
  }
  for (const Consumer& consumer : consumers_) {
    if (consumer.category != *category) {
      continue;
    }
    if (const std::shared_ptr<Queue> queue = consumer.queue.lock()) {
      queue->push(event);
    }
  }
}

void Sdl3EventPump::Queue::push(const SDL_Event& event) {
  if (events_.size() == queue_limit) {
    events_.pop_front();
  }
  events_.push_back(event);
}

auto Sdl3EventPump::Queue::pop() -> std::optional<SDL_Event> {
  if (events_.empty()) {
    return std::nullopt;
  }
  const SDL_Event event = events_.front();
  events_.pop_front();
  return event;
}

Sdl3EventPump::Subscription::Subscription(Sdl3EventCategory category,
                                          std::shared_ptr<Queue> queue) noexcept
    : category_(category), queue_(std::move(queue)) {}

auto Sdl3EventPump::Subscription::next() -> std::optional<SDL_Event> {
  return queue_->pop();
}

auto Sdl3EventPump::Subscription::pending() const noexcept -> std::size_t {
  return queue_->size();
}

}
