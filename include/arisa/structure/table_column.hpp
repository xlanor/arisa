#pragma once

#include <arisa/components/text.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/structure/key_path_comparator.hpp>
#include <arisa/structure/table_column_content.hpp>
#include <arisa/text/localized_string_key.hpp>

#include <compare>
#include <concepts>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace arisa::structure {

template <class RowValue>
class TableColumn : public TableColumnContent<RowValue> {
 public:
  using Spec = typename TableColumnContent<RowValue>::Spec;

  template <class Build>
    requires std::convertible_to<
        std::invoke_result_t<const Build&, const RowValue&>,
        composition::Element>
  TableColumn(std::string title, Build content)
      : TableColumn(
            titled(std::move(title), cell(std::move(content)), std::nullopt)) {}

  template <class Build>
    requires std::convertible_to<
        std::invoke_result_t<const Build&, const RowValue&>,
        composition::Element>
  TableColumn(components::Text label, Build content)
      : TableColumn(labelled(std::move(label), cell(std::move(content)),
                             std::nullopt)) {}

  template <class Build>
    requires std::convertible_to<
        std::invoke_result_t<const Build&, const RowValue&>,
        composition::Element>
  TableColumn(std::string title, KeyPathComparator<RowValue> sort_using,
              Build content)
      : TableColumn(titled(std::move(title), cell(std::move(content)),
                           std::move(sort_using))) {}

  template <class Key>
    requires std::invocable<const Key&, const RowValue&> &&
             std::convertible_to<
                 std::invoke_result_t<const Key&, const RowValue&>,
                 std::string> &&
             (!std::convertible_to<
                 std::invoke_result_t<const Key&, const RowValue&>,
                 composition::Element>)
  TableColumn(std::string title, Key value)
      : TableColumn(titled(std::move(title), text_of(value),
                           KeyPathComparator<RowValue>{std::move(value)})) {}

  template <class Key, class Compare>
    requires std::invocable<const Key&, const RowValue&> &&
             std::convertible_to<
                 std::invoke_result_t<const Key&, const RowValue&>,
                 std::string> &&
             std::invocable<const Compare&,
                            std::invoke_result_t<const Key&, const RowValue&>,
                            std::invoke_result_t<const Key&, const RowValue&>>
  TableColumn(std::string title, Key value, Compare comparator)
      : TableColumn(titled(std::move(title), text_of(value),
                           KeyPathComparator<RowValue>{
                               std::move(value), std::move(comparator)})) {}

  template <class Key, class Build>
    requires std::invocable<const Key&, const RowValue&> &&
             std::convertible_to<
                 std::invoke_result_t<const Build&, const RowValue&>,
                 composition::Element> &&
             (!std::convertible_to<
                 std::invoke_result_t<const Key&, const RowValue&>,
                 composition::Element>)
  TableColumn(std::string title, Key value, Build content)
      : TableColumn(titled(std::move(title), cell(std::move(content)),
                           KeyPathComparator<RowValue>{std::move(value)})) {}

  template <class Key, class Compare, class Build>
    requires std::invocable<const Key&, const RowValue&> &&
             std::convertible_to<
                 std::invoke_result_t<const Build&, const RowValue&>,
                 composition::Element>
  TableColumn(std::string title, Key value, Compare comparator, Build content)
      : TableColumn(titled(std::move(title), cell(std::move(content)),
                           KeyPathComparator<RowValue>{
                               std::move(value), std::move(comparator)})) {}

  auto width(std::optional<float> width = std::nullopt) && -> TableColumn {
    this->edit_specs().front().width =
        TableColumnWidth{.fixed = width, .min = {}, .ideal = {}, .max = {}};
    return std::move(*this);
  }

  auto width(std::optional<float> min, std::optional<float> ideal,
             std::optional<float> max) && -> TableColumn {
    this->edit_specs().front().width =
        TableColumnWidth{.fixed = {}, .min = min, .ideal = ideal, .max = max};
    return std::move(*this);
  }

 private:
  explicit TableColumn(Spec spec)
      : TableColumnContent<RowValue>(std::vector<Spec>{std::move(spec)}) {}

  template <class Build>
  static auto cell(Build content) ->
      typename TableColumnContent<RowValue>::Cell {
    return [content = std::move(content)](const RowValue& row) {
      return composition::Element{std::invoke(content, row)};
    };
  }

  template <class Key>
  static auto text_of(const Key& value) ->
      typename TableColumnContent<RowValue>::Cell {
    return [value](const RowValue& row) {
      return composition::Element{
          components::Text(std::string{std::invoke(value, row)})};
    };
  }

  static auto titled(std::string title,
                     typename TableColumnContent<RowValue>::Cell content,
                     std::optional<KeyPathComparator<RowValue>> comparator)
      -> Spec {
    Spec spec;
    spec.title = std::move(title);
    spec.content = std::move(content);
    spec.comparator = std::move(comparator);
    return spec;
  }

  static auto labelled(components::Text label,
                       typename TableColumnContent<RowValue>::Cell content,
                       std::optional<KeyPathComparator<RowValue>> comparator)
      -> Spec {
    Spec spec;
    spec.title = std::string{
        static_cast<const components::Text::Content&>(label.content()).text()};
    spec.label = std::make_shared<composition::PlaceholderContentView::Source>(
        composition::Element{std::move(label)});
    spec.content = std::move(content);
    spec.comparator = std::move(comparator);
    return spec;
  }
};

}
