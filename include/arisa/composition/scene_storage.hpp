#pragma once

#include <arisa/composition/app_storage.hpp>
#include <arisa/composition/state_key.hpp>

#include <functional>
#include <optional>
#include <string_view>
#include <utility>

namespace arisa::composition {

class UserDefaults;

template <class T>
class SceneStorage {
 public:
  constexpr SceneStorage(std::string_view name, T initial)
      : name_(name), local_(name, std::move(initial)) {}

  constexpr SceneStorage(std::string_view name, T initial,
                         const UserDefaults& store)
      : name_(name), local_(name, std::move(initial)), store_(&store) {}

  constexpr explicit SceneStorage(std::string_view name)
    requires StoredOptional<T>
      : name_(name), local_(name, T{}) {}

  SceneStorage(const SceneStorage&) = delete;
  auto operator=(const SceneStorage&) -> SceneStorage& = delete;
  SceneStorage(SceneStorage&&) = delete;
  auto operator=(SceneStorage&&) -> SceneStorage& = delete;
  ~SceneStorage() = default;

  constexpr auto name() const noexcept -> std::string_view { return name_; }

  constexpr auto initial() const noexcept -> const T& {
    return local_.initial();
  }

  constexpr auto local() const noexcept -> const StateKey<T>& { return local_; }

  constexpr auto store() const noexcept
      -> std::optional<std::reference_wrapper<const UserDefaults>> {
    if (store_ == nullptr) {
      return std::nullopt;
    }
    return std::cref(*store_);
  }

 private:
  std::string_view name_;
  StateKey<T> local_;
  const UserDefaults* store_{nullptr};
};

}
