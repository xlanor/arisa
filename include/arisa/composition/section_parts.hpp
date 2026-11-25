#pragma once

#include <arisa/composition/element.hpp>

#include <optional>
#include <vector>

namespace arisa::composition {

struct SectionParts {
  std::optional<Element> header;
  std::optional<Element> footer;
  std::vector<Element> content;
};

}
