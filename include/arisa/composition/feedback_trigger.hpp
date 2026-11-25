#pragma once

#include <arisa/composition/element_type.hpp>
#include <arisa/composition/sensory_feedback.hpp>

#include <concepts>
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace arisa::composition {

template <class Resolve, class T>
concept FeedbackResolver =
    std::invocable<const Resolve&, const T&, const T&> &&
    std::convertible_to<
        std::invoke_result_t<const Resolve&, const T&, const T&>,
        std::optional<SensoryFeedback>>;

class FeedbackTrigger {
 public:
  template <class T, class Resolve>
  class Typed;

  virtual ~FeedbackTrigger() = default;

  virtual auto kind() const noexcept -> ElementType = 0;

  virtual auto resolve(const FeedbackTrigger& previous) const
      -> std::optional<SensoryFeedback> = 0;

  template <std::equality_comparable T>
    requires std::copy_constructible<T>
  static auto fixed(SensoryFeedback feedback, T value)
      -> std::unique_ptr<FeedbackTrigger>;

  template <std::equality_comparable T, FeedbackResolver<T> Resolve>
    requires std::copy_constructible<T>
  static auto resolved(T value, Resolve resolve)
      -> std::unique_ptr<FeedbackTrigger>;
};

template <class T, class Resolve>
class FeedbackTrigger::Typed final : public FeedbackTrigger {
 public:
  Typed(T value, Resolve resolve)
      : value_(std::move(value)), resolve_(std::move(resolve)) {}

  auto kind() const noexcept -> ElementType override {
    return ElementType::of<Typed>();
  }

  auto resolve(const FeedbackTrigger& previous) const
      -> std::optional<SensoryFeedback> override {
    if (previous.kind() != kind()) {
      return std::nullopt;
    }
    const T& before = static_cast<const Typed&>(previous).value_;
    if (before == value_) {
      return std::nullopt;
    }
    return std::invoke(resolve_, before, value_);
  }

 private:
  T value_;
  [[no_unique_address]] Resolve resolve_;
};

template <std::equality_comparable T>
  requires std::copy_constructible<T>
auto FeedbackTrigger::fixed(SensoryFeedback feedback, T value)
    -> std::unique_ptr<FeedbackTrigger> {
  const auto constant = [feedback](const T&,
                                   const T&) -> std::optional<SensoryFeedback> {
    return feedback;
  };
  return std::make_unique<Typed<T, decltype(constant)>>(std::move(value),
                                                        constant);
}

template <std::equality_comparable T, FeedbackResolver<T> Resolve>
  requires std::copy_constructible<T>
auto FeedbackTrigger::resolved(T value, Resolve resolve)
    -> std::unique_ptr<FeedbackTrigger> {
  return std::make_unique<Typed<T, Resolve>>(std::move(value),
                                             std::move(resolve));
}

}
