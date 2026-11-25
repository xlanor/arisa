#pragma once

#include <cstddef>
#include <list>
#include <optional>
#include <unordered_map>
#include <utility>

namespace arisa::text {

template <class Key, class Value, class Hash, class Equal>
class LruCache {
 public:
  explicit LruCache(std::size_t capacity) noexcept : capacity_(capacity) {}

  template <class Probe>
  auto find(const Probe& key) -> std::optional<Value> {
    const auto found = index_.find(key);
    if (found == index_.end()) {
      return std::nullopt;
    }
    order_.splice(order_.begin(), order_, found->second);
    return found->second->second;
  }

  void store(Key key, Value value) {
    if (const auto found = index_.find(key); found != index_.end()) {
      found->second->second = std::move(value);
      order_.splice(order_.begin(), order_, found->second);
      return;
    }
    order_.emplace_front(key, std::move(value));
    index_.emplace(std::move(key), order_.begin());
    while (order_.size() > capacity_) {
      index_.erase(order_.back().first);
      order_.pop_back();
    }
  }

  void clear() noexcept {
    index_.clear();
    order_.clear();
  }

  auto size() const noexcept -> std::size_t { return order_.size(); }

 private:
  using Entry = std::pair<Key, Value>;

  std::size_t capacity_;
  std::list<Entry> order_;
  std::unordered_map<Key, typename std::list<Entry>::iterator, Hash, Equal>
      index_;
};

}
