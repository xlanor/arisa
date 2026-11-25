#pragma once

#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/environment_override.hpp>
#include <arisa/composition/identity.hpp>

#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace arisa::composition {

class EnvironmentTransform {
 public:
  template <class T>
  using Edit = std::move_only_function<void(T&) const>;

  template <class T>
  static auto of(const EnvironmentKey<T>& key,
                 std::type_identity_t<Edit<T>> edit) -> EnvironmentTransform {
    return EnvironmentTransform{
        key.identity(), std::make_shared<const Typed<T>>(key, std::move(edit))};
  }

  auto key() const noexcept -> Identity { return key_; }

  auto apply(const EnvironmentOverride::Value* inherited) const
      -> EnvironmentOverride {
    return edit_->apply(inherited);
  }

 private:
  class Step {
   public:
    virtual ~Step() = default;

    virtual auto apply(const EnvironmentOverride::Value* inherited) const
        -> EnvironmentOverride = 0;
  };

  template <class T>
  class Typed final : public Step {
   public:
    Typed(const EnvironmentKey<T>& key, Edit<T> edit)
        : key_(key), edit_(std::move(edit)) {}

    auto apply(const EnvironmentOverride::Value* inherited) const
        -> EnvironmentOverride override {
      T value =
          inherited != nullptr
              ? static_cast<const EnvironmentOverride::Typed<T>&>(*inherited)
                    .get()
              : key_.get().fallback();
      edit_(value);
      return EnvironmentOverride::of(key_.get(), std::move(value));
    }

   private:
    std::reference_wrapper<const EnvironmentKey<T>> key_;
    Edit<T> edit_;
  };

  EnvironmentTransform(Identity key, std::shared_ptr<const Step> edit) noexcept
      : key_(key), edit_(std::move(edit)) {}

  Identity key_;
  std::shared_ptr<const Step> edit_;
};

}
