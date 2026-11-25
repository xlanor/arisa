#include <arisa/components/index_set.hpp>

namespace arisa::components {

IndexSet::IndexSet(std::initializer_list<std::size_t> indices)
    : indices_(indices) {
  normalise();
}

IndexSet::IndexSet(std::vector<std::size_t> indices)
    : indices_(std::move(indices)) {
  normalise();
}

auto IndexSet::contains(std::size_t index) const noexcept -> bool {
  return std::ranges::binary_search(indices_, index);
}

void IndexSet::normalise() {
  std::ranges::sort(indices_);
  const auto [first, last] = std::ranges::unique(indices_);
  indices_.erase(first, last);
}

}
