#pragma once

#include <arisa/components/button.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/node_task.hpp>

#include <functional>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

namespace arisa::attr {

class Id {
 public:
  explicit Id(composition::ElementKey key) : key_(std::move(key)) {}

  auto apply(composition::Element element) && -> composition::Element {
    return std::move(element).id(std::move(key_));
  }

 private:
  composition::ElementKey key_;
};

class Action {
 public:
  explicit Action(components::Button::Action action) noexcept
      : action_(std::move(action)) {}

  void configure(components::Button::Options& options) && noexcept {
    options.action = std::move(action_);
  }

 private:
  components::Button::Action action_;
};

class Role {
 public:
  constexpr explicit Role(components::ButtonRole role) noexcept : role_(role) {}

  constexpr void configure(components::Button::Options& options) && noexcept {
    options.role = role_;
  }

 private:
  components::ButtonRole role_;
};

class OnAppear {
 public:
  explicit OnAppear(std::move_only_function<void()> action) noexcept
      : action_(std::move(action)) {}

  auto apply(composition::Element element) && -> composition::Element {
    return std::move(element).on_appear(std::move(action_));
  }

 private:
  std::move_only_function<void()> action_;
};

class OnDisappear {
 public:
  explicit OnDisappear(std::move_only_function<void()> action) noexcept
      : action_(std::move(action)) {}

  auto apply(composition::Element element) && -> composition::Element {
    return std::move(element).on_disappear(std::move(action_));
  }

 private:
  std::move_only_function<void()> action_;
};

class Task {
 public:
  explicit Task(composition::NodeTask::Plain body) noexcept
      : body_(std::move(body)) {}

  explicit Task(composition::NodeTask::Fallible body) noexcept
      : body_(std::move(body)) {}

  Task(composition::ElementKey key, composition::NodeTask::Plain body)
      : key_(std::move(key)), body_(std::move(body)) {}

  Task(composition::ElementKey key, composition::NodeTask::Fallible body)
      : key_(std::move(key)), body_(std::move(body)) {}

  auto apply(composition::Element element) && -> composition::Element {
    return std::visit(
        [this, &element](auto& body) {
          if (key_.has_value()) {
            return std::move(element).task(std::move(*key_), std::move(body));
          }
          return std::move(element).task(std::move(body));
        },
        body_);
  }

 private:
  std::optional<composition::ElementKey> key_;
  composition::NodeTask::Body body_;
};

class EnvironmentValue {
 public:
  template <class T>
  EnvironmentValue(const composition::EnvironmentKey<T>& key,
                   std::type_identity_t<T> value)
      : write_([&key,
                held = std::move(value)](composition::Element element) mutable {
          return std::move(element).environment(key, std::move(held));
        }) {}

  auto apply(composition::Element element) && -> composition::Element {
    return write_(std::move(element));
  }

 private:
  std::move_only_function<composition::Element(composition::Element)> write_;
};

using id = Id;
using action = Action;
using role = Role;
using on_appear = OnAppear;
using on_disappear = OnDisappear;
using task = Task;
using environment = EnvironmentValue;

}
