#pragma once

#include <arisa/components/subview.hpp>
#include <arisa/components/subviews_collection_slice.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/keyed_values.hpp>

#include <cstddef>
#include <functional>
#include <optional>
#include <span>
#include <vector>

namespace arisa::composition {

class Context;

}

namespace arisa::components {

class SubviewsCollection {
 public:
  explicit SubviewsCollection(composition::Element content);

  explicit SubviewsCollection(std::vector<composition::Element> content);

  SubviewsCollection(composition::Context& context,
                     composition::Element content);

  SubviewsCollection(composition::Context& context,
                     std::vector<composition::Element> content);

  auto size() const noexcept -> std::size_t { return subviews_.size(); }

  auto empty() const noexcept -> bool { return subviews_.empty(); }

  auto operator[](std::size_t index) -> Subview& { return subviews_[index]; }

  auto operator[](std::size_t index) const -> const Subview& {
    return subviews_[index];
  }

  auto begin() noexcept { return subviews_.begin(); }

  auto end() noexcept { return subviews_.end(); }

  auto begin() const noexcept { return subviews_.begin(); }

  auto end() const noexcept { return subviews_.end(); }

  auto slice(std::size_t first) -> SubviewsCollectionSlice {
    return SubviewsCollectionSlice{subviews_}.slice(first);
  }

  auto slice(std::size_t first, std::size_t last) -> SubviewsCollectionSlice {
    return SubviewsCollectionSlice{subviews_}.slice(first, last);
  }

  auto take() && -> std::vector<Subview> { return std::move(subviews_); }

 private:
  void flatten(composition::Element element,
               const composition::KeyedValues& inherited);

  auto expands(const composition::Element& element) const -> bool;

  std::vector<Subview> subviews_;
  std::optional<std::reference_wrapper<composition::Context>> context_;
};

}
