#pragma once

#include <arisa/components/alignment.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>

#include <concepts>
#include <optional>
#include <utility>
#include <vector>

namespace arisa::components {

struct GridRowOptions {
  std::optional<VerticalAlignment> alignment{};

  friend constexpr auto operator==(const GridRowOptions&, const GridRowOptions&)
      -> bool = default;
};

class GridRow : public composition::Element {
 public:
  class Content;

  template <class... Cells>
    requires(composition::ViewContent<Cells> && ...)
  explicit GridRow(Cells&&... cells)
      : GridRow(GridRowOptions{}, collect(std::forward<Cells>(cells)...)) {}

  template <class... Cells>
    requires(composition::ViewContent<Cells> && ...)
  explicit GridRow(GridRowOptions options, Cells&&... cells)
      : GridRow(options, collect(std::forward<Cells>(cells)...)) {}

  GridRow(GridRowOptions options, std::vector<composition::Element> cells);
};

class GridRow::Content final : public composition::ElementContent {
 public:
  explicit Content(GridRowOptions options) noexcept : options_(options) {}

  auto type() const noexcept -> composition::ElementType override;

  auto paints_like(const composition::ElementContent& other) const
      -> bool override;

  auto distributes_decoration() const noexcept -> bool override { return true; }

  auto options() const noexcept -> const GridRowOptions& { return options_; }

 private:
  GridRowOptions options_;
};

}
