#pragma once

#include <arisa/components/for_each.hpp>
#include <arisa/composition/element.hpp>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace arisa::components {

class GridSection {
 public:
  explicit GridSection(const ForEach& each);

  GridSection(composition::Element header, const ForEach& each);

  GridSection(composition::Element header, const ForEach& each,
              composition::Element footer);

  template <class... Sections>
  static auto collect(Sections&&... sections) -> std::vector<GridSection> {
    std::vector<GridSection> collected;
    collected.reserve(sizeof...(Sections));
    (collected.push_back(std::forward<Sections>(sections)), ...);
    return collected;
  }

  auto take_header() -> std::optional<composition::Element> {
    return std::exchange(header_, std::nullopt);
  }

  auto take_footer() -> std::optional<composition::Element> {
    return std::exchange(footer_, std::nullopt);
  }

  auto source() const noexcept
      -> const std::shared_ptr<const ForEach::Source>& {
    return source_;
  }

 private:
  std::optional<composition::Element> header_;
  std::shared_ptr<const ForEach::Source> source_;
  std::optional<composition::Element> footer_;
};

}
