#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/composition/visibility.hpp>
#include <arisa/structure/key_path_comparator.hpp>
#include <arisa/structure/table_column_alignment.hpp>
#include <arisa/structure/table_column_customization_behavior.hpp>
#include <arisa/structure/table_column_width.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace arisa::structure {

template <class RowValue>
class TableColumnContent {
 public:
  using TableRowValue = RowValue;
  using Cell = std::function<composition::Element(const RowValue&)>;

  struct Spec {
    std::string title;
    std::shared_ptr<composition::PlaceholderContentView::Source> label;
    Cell content;
    std::optional<KeyPathComparator<RowValue>> comparator;
    TableColumnWidth width;
    TableColumnAlignment alignment{TableColumnAlignment::automatic};
    std::string customization;
    composition::Visibility default_visibility{
        composition::Visibility::automatic};
    TableColumnCustomizationBehavior disabled;
  };

  auto specs() const noexcept -> const std::vector<Spec>& { return specs_; }

  auto take_specs() && -> std::vector<Spec> { return std::move(specs_); }

  template <class Self>
  auto default_visibility(this Self&& self, composition::Visibility visibility)
      -> std::remove_cvref_t<Self> {
    for (Spec& spec : static_cast<TableColumnContent&>(self).specs_) {
      spec.default_visibility = visibility;
    }
    return std::forward<Self>(self);
  }

  template <class Self>
  auto customization_id(this Self&& self, std::string id)
      -> std::remove_cvref_t<Self> {
    std::vector<Spec>& found = static_cast<TableColumnContent&>(self).specs_;
    for (std::size_t index = 0; index < found.size(); ++index) {
      found[index].customization =
          found.size() == 1 ? id : id + "." + std::to_string(index);
    }
    return std::forward<Self>(self);
  }

  template <class Self>
  auto disabled_customization_behavior(
      this Self&& self, TableColumnCustomizationBehavior behavior)
      -> std::remove_cvref_t<Self> {
    for (Spec& spec : static_cast<TableColumnContent&>(self).specs_) {
      spec.disabled = behavior;
    }
    return std::forward<Self>(self);
  }

  template <class Self>
  auto alignment(this Self&& self, TableColumnAlignment alignment)
      -> std::remove_cvref_t<Self> {
    for (Spec& spec : static_cast<TableColumnContent&>(self).specs_) {
      spec.alignment = alignment;
    }
    return std::forward<Self>(self);
  }

 protected:
  TableColumnContent() = default;

  explicit TableColumnContent(std::vector<Spec> specs)
      : specs_(std::move(specs)) {}

  auto edit_specs() noexcept -> std::vector<Spec>& { return specs_; }

  static void append(std::vector<Spec>& into, TableColumnContent&& content) {
    for (Spec& spec : content.specs_) {
      into.push_back(std::move(spec));
    }
  }

 private:
  std::vector<Spec> specs_;
};

}
