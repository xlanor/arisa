#pragma once

#include <arisa/composition/visibility.hpp>

#include <cstddef>
#include <flat_map>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::structure {

class TableColumnCustomization {
 public:
  TableColumnCustomization() = default;

  auto visibility(std::string_view id) const -> composition::Visibility;

  void set_visibility(std::string_view id, composition::Visibility visibility);

  auto width(std::string_view id) const -> std::optional<float>;

  void set_width(std::string_view id, std::optional<float> width);

  auto order() const noexcept -> std::span<const std::string> { return order_; }

  void reorder(std::vector<std::string> order);

  void reset_order() noexcept { order_.clear(); }

  auto arranged(std::span<const std::string> declared) const
      -> std::vector<std::size_t>;

  auto encoded() const -> std::string;

  static auto decoded(std::string_view text)
      -> std::optional<TableColumnCustomization>;

  friend auto operator==(const TableColumnCustomization&,
                         const TableColumnCustomization&) -> bool = default;

 private:
  static constexpr char order_mark = 'o';
  static constexpr char hidden_mark = 'h';
  static constexpr char visible_mark = 'v';
  static constexpr char width_mark = 'w';
  static constexpr char width_separator = '\t';

  std::vector<std::string> order_;
  std::flat_map<std::string, composition::Visibility, std::less<>> visibility_;
  std::flat_map<std::string, float, std::less<>> widths_;
};

}
