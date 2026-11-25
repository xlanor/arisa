#pragma once

#include <arisa/components/alignment.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/theme/space.hpp>

#include <concepts>
#include <optional>
#include <utility>
#include <vector>

namespace arisa::components {

class GridLayout;

struct GridOptions {
  Alignment alignment{};
  std::optional<theme::Space> horizontal_spacing{};
  std::optional<theme::Space> vertical_spacing{};

  friend constexpr auto operator==(const GridOptions&, const GridOptions&)
      -> bool = default;
};

class Grid : public composition::Element {
 public:
  class Content;

  template <class... Children>
    requires(composition::ViewContent<Children> && ...)
  explicit Grid(Children&&... children)
      : Grid(GridOptions{}, collect(std::forward<Children>(children)...)) {}

  template <class... Children>
    requires(composition::ViewContent<Children> && ...)
  explicit Grid(GridOptions options, Children&&... children)
      : Grid(options, collect(std::forward<Children>(children)...)) {}

  Grid(GridOptions options, std::vector<composition::Element> children);

 private:
  friend class GridLayout;

  struct Switchable {};

  Grid(Switchable, GridOptions options,
       std::vector<composition::Element> children);
};

class Grid::Content final : public composition::ElementContent {
 public:
  explicit Content(GridOptions options, bool switchable = false) noexcept
      : options_(options), switchable_(switchable) {}

  auto type() const noexcept -> composition::ElementType override;

  auto kind() const noexcept -> composition::ElementType override;

  auto paints_like(const composition::ElementContent& other) const
      -> bool override;

  auto options() const noexcept -> const GridOptions& { return options_; }

 private:
  GridOptions options_;
  bool switchable_;
};

}
