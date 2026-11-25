#pragma once

#include <arisa/motion/animation.hpp>
#include <arisa/motion/animation_completion_criteria.hpp>
#include <arisa/motion/completion_queue.hpp>
#include <arisa/motion/transaction.hpp>
#include <arisa/motion/transaction_stack.hpp>

#include <concepts>
#include <functional>
#include <optional>
#include <type_traits>
#include <utility>

namespace arisa::motion {

template <std::invocable Body>
auto with_transaction(Transaction transaction, Body&& body)
    -> std::invoke_result_t<Body> {
  const TransactionStack::Scope scope{std::move(transaction)};
  return std::invoke(std::forward<Body>(body));
}

template <std::invocable Body>
auto with_animation(std::optional<Animation> animation, Body&& body)
    -> std::invoke_result_t<Body> {
  return with_transaction(Transaction{std::move(animation)},
                          std::forward<Body>(body));
}

template <std::invocable Body>
auto with_animation(Body&& body) -> std::invoke_result_t<Body> {
  return with_animation(Animation::default_animation(),
                        std::forward<Body>(body));
}

template <std::invocable Body>
auto with_animation(std::optional<Animation> animation, Body&& body,
                    CompletionQueue::Completion completion)
    -> std::invoke_result_t<Body> {
  Transaction transaction{std::move(animation)};
  transaction.add_animation_completion(std::move(completion));
  return with_transaction(std::move(transaction), std::forward<Body>(body));
}

template <std::invocable Body>
auto with_animation(std::optional<Animation> animation,
                    AnimationCompletionCriteria criteria, Body&& body,
                    CompletionQueue::Completion completion)
    -> std::invoke_result_t<Body> {
  Transaction transaction{std::move(animation)};
  transaction.add_animation_completion(criteria, std::move(completion));
  return with_transaction(std::move(transaction), std::forward<Body>(body));
}

}
