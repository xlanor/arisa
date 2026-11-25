#pragma once

#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/environment_object.hpp>
#include <arisa/composition/environment_override.hpp>
#include <arisa/composition/identity.hpp>
#include <arisa/composition/observable.hpp>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace arisa::composition {

class EnvironmentValues {
 public:
  struct Entry {
    Identity key;
    std::shared_ptr<const EnvironmentOverride::Value> value;
  };

  EnvironmentValues() noexcept = default;

  static constexpr auto everything() noexcept -> Identity {
    return Identity{&everything_anchor};
  }

  explicit EnvironmentValues(std::vector<Entry> entries)
      : entries_(entries.empty() ? nullptr
                                 : std::make_shared<const std::vector<Entry>>(
                                       std::move(entries))) {}

  template <class T>
  auto operator[](const EnvironmentKey<T>& key) const -> const T& {
    const EnvironmentOverride::Value* found = find(key.identity());
    if (found == nullptr) {
      return key.fallback();
    }
    return static_cast<const EnvironmentOverride::Typed<T>&>(*found).get();
  }

  template <class T>
  void set(const EnvironmentKey<T>& key, T value) {
    std::vector<Entry> next;
    if (entries_ != nullptr) {
      next = *entries_;
    }
    std::erase_if(next, [&key](const Entry& entry) {
      return entry.key == key.identity();
    });
    next.push_back(Entry{
        .key = key.identity(),
        .value = EnvironmentOverride::of(key, std::move(value)).shared()});
    entries_ = std::make_shared<const std::vector<Entry>>(std::move(next));
  }

  template <class T>
  auto object() const -> std::optional<std::reference_wrapper<Observable<T>>> {
    const std::optional<EnvironmentObject<T>>& held =
        (*this)[environment_object_key<T>];
    if (!held.has_value()) {
      return std::nullopt;
    }
    return std::ref(held->get());
  }

  template <class T>
  void set_object(Observable<T>& object) {
    set(environment_object_key<T>,
        std::optional<EnvironmentObject<T>>{EnvironmentObject<T>{object}});
  }

  template <class T>
  auto contains(const EnvironmentKey<T>& key) const noexcept -> bool {
    return find(key.identity()) != nullptr;
  }

  auto size() const noexcept -> std::size_t {
    return entries_ == nullptr ? 0 : entries_->size();
  }

  friend auto operator==(const EnvironmentValues& left,
                         const EnvironmentValues& right) -> bool {
    if (left.entries_ == right.entries_) {
      return true;
    }
    if (left.size() != right.size()) {
      return false;
    }
    return std::ranges::all_of(*left.entries_, [&right](const Entry& entry) {
      const EnvironmentOverride::Value* other = right.find(entry.key);
      return other != nullptr &&
             (other == entry.value.get() || entry.value->equals(*other));
    });
  }

 private:
  auto find(Identity key) const noexcept -> const EnvironmentOverride::Value* {
    if (entries_ == nullptr) {
      return nullptr;
    }
    const auto found = std::ranges::find(*entries_, key, &Entry::key);
    return found == entries_->end() ? nullptr : found->value.get();
  }

  static constexpr char everything_anchor{0};

  std::shared_ptr<const std::vector<Entry>> entries_;
};

}
