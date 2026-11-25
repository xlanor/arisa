#pragma once

#include <optional>

namespace arisa::structure {

struct TableColumnWidth {
  std::optional<float> fixed;
  std::optional<float> min;
  std::optional<float> ideal;
  std::optional<float> max;

  friend auto operator==(const TableColumnWidth&, const TableColumnWidth&)
      -> bool = default;
};

}
