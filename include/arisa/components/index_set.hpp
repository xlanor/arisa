#pragma once

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <utility>
#include <vector>

namespace arisa::components {

class IndexSet {
 public:
  IndexSet() = default;

  explicit IndexSet(std::size_t index) : indices_{index} {}

  IndexSet(std::initializer_list<std::size_t> indices);

  explicit IndexSet(std::vector<std::size_t> indices);

  auto contains(std::size_t index) const noexcept -> bool;

  auto count() const noexcept -> std::size_t { return indices_.size(); }

  auto empty() const noexcept -> bool { return indices_.empty(); }

  auto begin() const noexcept { return indices_.begin(); }

  auto end() const noexcept { return indices_.end(); }

  template <class T>
  void remove_from(std::vector<T>& items) const {
    std::size_t index = 0;
    std::erase_if(items,
                  [this, &index](const T&) { return contains(index++); });
  }

  template <class T>
  void move_from(std::vector<T>& items, std::size_t destination) const {
    std::vector<T> moved;
    std::vector<T> kept;
    std::size_t before = 0;
    for (std::size_t index = 0; index < items.size(); ++index) {
      if (contains(index)) {
        moved.push_back(std::move(items[index]));
        if (index < destination) {
          ++before;
        }
      } else {
        kept.push_back(std::move(items[index]));
      }
    }
    const std::size_t at =
        std::min(destination - std::min(before, destination), kept.size());
    kept.insert(kept.begin() + static_cast<std::ptrdiff_t>(at),
                std::make_move_iterator(moved.begin()),
                std::make_move_iterator(moved.end()));
    items = std::move(kept);
  }

  friend auto operator==(const IndexSet&, const IndexSet&) -> bool = default;

 private:
  void normalise();

  std::vector<std::size_t> indices_;
};

}
