#pragma once

#include <arisa/composition/scroll_target.hpp>
#include <arisa/composition/scroll_target_behavior_properties.hpp>

#include <concepts>
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace arisa::composition {

template <class Behavior>
concept CustomScrollTargetBehavior =
    std::copy_constructible<Behavior> &&
    requires(const Behavior& behavior, ScrollTarget& target,
             const ScrollTargetContext& context) {
      behavior.update_target(target, context);
    };

template <class Behavior>
concept DescribedScrollTargetBehavior =
    CustomScrollTargetBehavior<Behavior> &&
    requires(const Behavior& behavior,
             const ScrollTargetBehaviorPropertiesContext& context) {
      {
        behavior.properties(context)
      } -> std::convertible_to<ScrollTargetBehaviorProperties>;
    };

class AnyScrollTargetBehavior {
 public:
  template <class Behavior>
    requires(
        !std::same_as<std::remove_cvref_t<Behavior>, AnyScrollTargetBehavior> &&
        CustomScrollTargetBehavior<Behavior>)
  AnyScrollTargetBehavior(Behavior behavior)
      : model_(std::make_shared<const Typed<Behavior>>(std::move(behavior))) {}

  void update_target(ScrollTarget& target,
                     const ScrollTargetContext& context) const {
    model_->update_target(target, context);
  }

  auto properties(const ScrollTargetBehaviorPropertiesContext& context) const
      -> ScrollTargetBehaviorProperties {
    return model_->properties(context);
  }

  template <CustomScrollTargetBehavior Behavior>
  auto base() const noexcept
      -> std::optional<std::reference_wrapper<const Behavior>> {
    if (model_->kind() != &tag<Behavior>) {
      return std::nullopt;
    }
    return std::cref(static_cast<const Typed<Behavior>&>(*model_).behavior());
  }

  friend auto operator==(const AnyScrollTargetBehavior&,
                         const AnyScrollTargetBehavior&) -> bool = default;

 private:
  template <class Behavior>
  static constexpr char tag{};

  class Model {
   public:
    Model() = default;
    Model(const Model&) = delete;
    auto operator=(const Model&) -> Model& = delete;
    Model(Model&&) = delete;
    auto operator=(Model&&) -> Model& = delete;
    virtual ~Model() = default;

    virtual void update_target(ScrollTarget& target,
                               const ScrollTargetContext& context) const = 0;

    virtual auto properties(
        const ScrollTargetBehaviorPropertiesContext& context) const
        -> ScrollTargetBehaviorProperties = 0;

    virtual auto kind() const noexcept -> const void* = 0;
  };

  template <class Behavior>
  class Typed final : public Model {
   public:
    explicit Typed(Behavior behavior) : behavior_(std::move(behavior)) {}

    void update_target(ScrollTarget& target,
                       const ScrollTargetContext& context) const override {
      behavior_.update_target(target, context);
    }

    auto properties(const ScrollTargetBehaviorPropertiesContext& context) const
        -> ScrollTargetBehaviorProperties override {
      if constexpr (DescribedScrollTargetBehavior<Behavior>) {
        return behavior_.properties(context);
      } else {
        static_cast<void>(context);
        return ScrollTargetBehaviorProperties{};
      }
    }

    auto kind() const noexcept -> const void* override {
      return &tag<Behavior>;
    }

    auto behavior() const noexcept -> const Behavior& { return behavior_; }

   private:
    Behavior behavior_;
  };

  std::shared_ptr<const Model> model_;
};

}
