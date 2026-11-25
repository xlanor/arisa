#pragma once

#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/provider/assets/asset_path.hpp>
#include <arisa/provider/error.hpp>
#include <arisa/provider/storage/provider.hpp>

#include <concepts>
#include <cstdint>
#include <expected>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

namespace arisa::composition {

template <class T>
concept Encodable = requires(const T& value, std::string_view text) {
  { value.encoded() } -> std::convertible_to<std::string>;
  { T::decoded(text) } -> std::same_as<std::optional<T>>;
};

class UserDefaults {
 public:
  using Value = std::variant<bool, std::int64_t, double, std::string>;
  using Values = std::map<std::string, Value, std::less<>>;

  static const EnvironmentKey<std::optional<UserDefaults>> app_storage;
  static const EnvironmentKey<std::optional<UserDefaults>> scene_storage;

  UserDefaults();

  [[nodiscard]] static auto open(provider::StorageProvider& storage,
                                 provider::AssetPath path)
      -> std::expected<UserDefaults, provider::ProviderError>;

  auto value(std::string_view key) const -> std::optional<Value>;

  void set(std::string_view key, Value value) const;

  void remove(std::string_view key) const;

  auto values() const noexcept -> const Observable<Values>&;

  static auto channel(std::string_view key) noexcept -> std::uintptr_t;

  auto pending() const noexcept -> bool;

  [[nodiscard]] auto flush() const
      -> std::expected<void, provider::ProviderError>;

  template <class T>
  auto get(std::string_view key) const -> std::optional<T> {
    const std::optional<Value> stored = value(key);
    if (!stored.has_value()) {
      return std::nullopt;
    }
    if constexpr (std::same_as<T, bool>) {
      if (const bool* flag = std::get_if<bool>(&*stored)) {
        return *flag;
      }
    } else if constexpr (std::is_enum_v<T>) {
      if (const std::int64_t* number = std::get_if<std::int64_t>(&*stored)) {
        return static_cast<T>(*number);
      }
    } else if constexpr (std::integral<T>) {
      if (const std::int64_t* number = std::get_if<std::int64_t>(&*stored)) {
        return static_cast<T>(*number);
      }
    } else if constexpr (std::floating_point<T>) {
      if (const double* number = std::get_if<double>(&*stored)) {
        return static_cast<T>(*number);
      }
      if (const std::int64_t* number = std::get_if<std::int64_t>(&*stored)) {
        return static_cast<T>(*number);
      }
    } else if constexpr (Encodable<T>) {
      if (const std::string* text = std::get_if<std::string>(&*stored)) {
        return T::decoded(*text);
      }
    } else {
      static_assert(std::same_as<T, std::string>);
      if (const std::string* text = std::get_if<std::string>(&*stored)) {
        return *text;
      }
    }
    return std::nullopt;
  }

  template <class T>
  void put(std::string_view key, const T& stored) const {
    if constexpr (std::same_as<T, bool>) {
      set(key, Value{stored});
    } else if constexpr (std::is_enum_v<T>) {
      set(key, Value{static_cast<std::int64_t>(stored)});
    } else if constexpr (std::integral<T>) {
      set(key, Value{static_cast<std::int64_t>(stored)});
    } else if constexpr (std::floating_point<T>) {
      set(key, Value{static_cast<double>(stored)});
    } else if constexpr (Encodable<T>) {
      set(key, Value{stored.encoded()});
    } else {
      static_assert(std::same_as<T, std::string>);
      set(key, Value{stored});
    }
  }

  friend auto operator==(const UserDefaults& left, const UserDefaults& right)
      -> bool {
    return left.state_ == right.state_;
  }

 private:
  struct State;

  explicit UserDefaults(std::shared_ptr<State> state) noexcept;

  static auto parse(std::string_view text, std::string_view source)
      -> std::optional<Values>;

  static auto serialise(const Values& values) -> std::string;

  std::shared_ptr<State> state_;
};

}
