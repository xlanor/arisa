#pragma once

#include <arisa/composition/state_key.hpp>

#include <concepts>
#include <functional>
#include <optional>
#include <string_view>
#include <utility>

namespace arisa::composition {

class UserDefaults;

template <class T>
concept StoredOptional = requires { typename T::value_type; } &&
                         std::same_as<T, std::optional<typename T::value_type>>;

template <class T>
class AppStorage {
 public:
  constexpr AppStorage(std::string_view name, T initial)
      : name_(name), local_(name, std::move(initial)) {}

  constexpr AppStorage(std::string_view name, T initial,
                       const UserDefaults& store)
      : name_(name), local_(name, std::move(initial)), store_(&store) {}

  constexpr explicit AppStorage(std::string_view name)
    requires StoredOptional<T>
      : name_(name), local_(name, T{}) {}

  constexpr AppStorage(std::string_view name, const UserDefaults& store)
    requires StoredOptional<T>
      : name_(name), local_(name, T{}), store_(&store) {}

  AppStorage(const AppStorage&) = delete;
  auto operator=(const AppStorage&) -> AppStorage& = delete;
  AppStorage(AppStorage&&) = delete;
  auto operator=(AppStorage&&) -> AppStorage& = delete;
  ~AppStorage() = default;

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
