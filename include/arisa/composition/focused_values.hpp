#pragma once

#include <arisa/composition/environment_override.hpp>
#include <arisa/composition/focused_value_key.hpp>
#include <arisa/composition/identity.hpp>

#include <algorithm>
#include <flat_map>
#include <memory>
#include <optional>
#include <utility>

namespace arisa::composition {

class FocusedValues {
 public:
  using Values =
      std::flat_map<Identity, std::shared_ptr<const EnvironmentOverride>>;

  FocusedValues() = default;

  explicit FocusedValues(Values values) noexcept : values_(std::move(values)) {}

  template <class T>
  auto operator[](const FocusedValueKey<T>& key) const -> std::optional<T> {
    const auto found = values_.find(key.identity());
    if (found == values_.end()) {
      return std::nullopt;
    }
    return FocusedValueKey<T>::read(found->second->value());
  }

  auto size() const noexcept -> std::size_t { return values_.size(); }

  friend auto operator==(const FocusedValues& left, const FocusedValues& right)
      -> bool {
    return left.values_.size() == right.values_.size() &&
           std::ranges::equal(left.values_, right.values_,
                              [](const auto& first, const auto& second) {
                                return first.first == second.first &&
                                       first.second->same_value(*second.second);
                              });
  }

 private:
  Values values_;
};

}
