#pragma once

#include <arisa/composition/element_key.hpp>
#include <arisa/structure/table_column_content.hpp>

#include <concepts>
#include <cstddef>
#include <functional>
#include <ranges>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace arisa::structure {

template <class RowValue>
class TableColumnForEach : public TableColumnContent<RowValue> {
 public:
  using Spec = typename TableColumnContent<RowValue>::Spec;

  template <std::ranges::input_range Data, class KeyOf, class Build>
    requires std::convertible_to<
                 std::invoke_result_t<const KeyOf&,
                                      const std::ranges::range_value_t<Data>&>,
                 composition::ElementKey> &&
             std::derived_from<
                 std::invoke_result_t<const Build&,
                                      const std::ranges::range_value_t<Data>&>,
                 TableColumnContent<RowValue>>
  TableColumnForEach(Data&& data, KeyOf key_of, Build content)
      : TableColumnForEach(columns(std::forward<Data>(data), key_of, content)) {
  }

  template <class Build>
    requires std::derived_from<std::invoke_result_t<const Build&, int>,
                               TableColumnContent<RowValue>>
  TableColumnForEach(int count, Build content)
      : TableColumnForEach(
            std::views::iota(0, count), [](int index) { return index; },
            content) {}

  auto data() const noexcept -> const std::vector<composition::ElementKey>& {
    return keys_;
  }

  auto content() const noexcept -> const std::vector<Spec>& {
    return this->specs();
  }

 private:
  using Built =
      std::pair<std::vector<Spec>, std::vector<composition::ElementKey>>;

  explicit TableColumnForEach(Built built)
      : TableColumnContent<RowValue>(std::move(built.first)),
        keys_(std::move(built.second)) {}

  template <class Data, class KeyOf, class Build>
  static auto columns(Data&& data, const KeyOf& key_of, const Build& content)
      -> Built {
    std::vector<Spec> made;
    std::vector<composition::ElementKey> keys;
    for (const auto& item : data) {
      const composition::ElementKey key{std::invoke(key_of, item)};
      keys.push_back(key);
      std::vector<Spec> built =
          std::move(std::invoke(content, item)).take_specs();
      for (Spec& spec : built) {
        if (spec.customization.empty()) {
          spec.customization = key.description();
        }
        made.push_back(std::move(spec));
      }
    }
    return Built{std::move(made), std::move(keys)};
  }

  std::vector<composition::ElementKey> keys_;
};

}
