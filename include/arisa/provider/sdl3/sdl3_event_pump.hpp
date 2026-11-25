#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/provider/sdl3/sdl3_context.hpp>
#include <arisa/provider/threading/provider.hpp>

#include <SDL3/SDL_events.h>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace arisa::provider::sdl3 {

enum class Sdl3EventCategory : std::uint8_t {
  lifecycle,
  window,
  input,
  text,
};

class Sdl3EventPump {
 public:
  static constexpr std::size_t queue_limit = 256;

  class Subscription;

  [[nodiscard]] static auto create(const ThreadingProvider& threading)
      -> std::expected<Sdl3EventPump, ProviderError>;

  auto subscribe(Sdl3EventCategory category) -> Subscription;

  void pump();

  auto owner() const noexcept -> ThreadId { return owner_; }

  static auto classify(const SDL_Event& event) noexcept
      -> std::optional<Sdl3EventCategory>;

 private:
  class Queue {
   public:
    void push(const SDL_Event& event);

    auto pop() -> std::optional<SDL_Event>;

    auto size() const noexcept -> std::size_t { return events_.size(); }

   private:
    std::deque<SDL_Event> events_;
  };

  struct Consumer {
    Sdl3EventCategory category;
    std::weak_ptr<Queue> queue;
  };

  Sdl3EventPump(Sdl3Context events, const ThreadingProvider& threading,
                ThreadId owner) noexcept;

  void dispatch(const SDL_Event& event);

  Sdl3Context events_;
  std::reference_wrapper<const ThreadingProvider> threading_;
  ThreadId owner_;
  std::vector<Consumer> consumers_;
};

class Sdl3EventPump::Subscription {
 public:
  auto next() -> std::optional<SDL_Event>;

  auto pending() const noexcept -> std::size_t;

  auto category() const noexcept -> Sdl3EventCategory { return category_; }

 private:
  friend class Sdl3EventPump;

  Subscription(Sdl3EventCategory category,
               std::shared_ptr<Queue> queue) noexcept;

  Sdl3EventCategory category_;
  std::shared_ptr<Queue> queue_;
};

}
