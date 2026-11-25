#pragma once

#include <arisa/composition/identity.hpp>
#include <arisa/composition/tag_value.hpp>

#include <algorithm>
#include <concepts>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace arisa::composition {

template <class Key>
concept ValueKey = requires(const Key& key) {
  typename Key::Value;
  { key.identity() } -> std::same_as<Identity>;
  { key.fallback() } -> std::convertible_to<const typename Key::Value&>;
};

class KeyedValues {
 public:
  template <ValueKey Key>
    requires std::equality_comparable<typename Key::Value> &&
             std::copy_constructible<typename Key::Value>
  void set(const Key& key, typename Key::Value value) {
    const Identity identity = key.identity();
    std::erase_if(entries_, [identity](const Entry& entry) {
      return entry.key == identity;
    });
    entries_.push_back(
        Entry{.key = identity, .value = TagValue::of(std::move(value))});
  }

  template <ValueKey Key>
  auto get(const Key& key) const -> const typename Key::Value& {
    const auto found = std::ranges::find(entries_, key.identity(), &Entry::key);
    if (found == entries_.end()) {
      return key.fallback();
    }
    const auto* typed = found->value->template get<typename Key::Value>();
    return typed != nullptr ? *typed : key.fallback();
  }

  template <ValueKey Key>
  auto contains(const Key& key) const -> bool {
    return std::ranges::find(entries_, key.identity(), &Entry::key) !=
           entries_.end();
  }

  void merge_missing(const KeyedValues& outer) {
    for (const Entry& entry : outer.entries_) {
      if (std::ranges::find(entries_, entry.key, &Entry::key) ==
          entries_.end()) {
        entries_.push_back(entry);
      }
    }
    if (tag_ == nullptr) {
      tag_ = outer.tag_;
    }
  }

  void set_tag(std::shared_ptr<const TagValue> tag) noexcept {
    tag_ = std::move(tag);
  }

  template <std::equality_comparable V>
    requires std::copy_constructible<V>
  auto tag(std::type_identity<V> = {}) const -> std::optional<V> {
    if (tag_ == nullptr) {
      return std::nullopt;
    }
    const V* typed = tag_->template get<V>();
    if (typed == nullptr) {
      return std::nullopt;
    }
    return *typed;
  }

  template <std::equality_comparable V>
    requires std::copy_constructible<V>
  auto has_tag(const V& value) const -> bool {
    const std::optional<V> held = tag<V>();
    return held.has_value() && *held == value;
  }

  auto empty() const noexcept -> bool {
    return entries_.empty() && tag_ == nullptr;
  }

  friend auto operator==(const KeyedValues& left, const KeyedValues& right)
      -> bool {
    const bool tagged = left.tag_ == right.tag_ ||
                        (left.tag_ != nullptr && right.tag_ != nullptr &&
                         left.tag_->equals(*right.tag_));
    return tagged &&
           std::ranges::equal(left.entries_, right.entries_,
                              [](const Entry& first, const Entry& second) {
                                return first.key == second.key &&
                                       (first.value == second.value ||
                                        first.value->equals(*second.value));
                              });
  }

 private:
  struct Entry {
    Identity key;
    std::shared_ptr<const TagValue> value;
  };

  std::vector<Entry> entries_;
  std::shared_ptr<const TagValue> tag_;
};

}
