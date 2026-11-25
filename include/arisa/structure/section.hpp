#pragma once

#include <arisa/components/for_each.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/composition/section_parts.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/structure/list_cell.hpp>

#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace arisa::structure {

class Section : public composition::Element {
 public:
  class Content;
  class Row;
  class Cells;
  class Caption;

  struct Labels {
    std::optional<composition::Element> header;
    std::optional<composition::Element> footer;
  };

  struct Disclosure {
    bool collapsible{false};
    bool expanded{true};
    std::optional<composition::Binding<bool>> binding;
  };

  static constexpr float outline_indent = 20.0F;
  static constexpr float chevron_extent = 13.0F;
  static constexpr double expanded_degrees = 90.0;

  template <class... Rows>
    requires(composition::ViewContent<Rows> && ...)
  explicit Section(Rows&&... rows)
      : Section(Labels{}, collect(std::forward<Rows>(rows)...)) {}

  template <class... Rows>
    requires(composition::ViewContent<Rows> && ...)
  explicit Section(std::string header, Rows&&... rows)
      : Section(titled(std::move(header)),
                collect(std::forward<Rows>(rows)...)) {}

  template <class... Rows>
    requires(composition::ViewContent<Rows> && ...)
  explicit Section(Labels labels, Rows&&... rows)
      : Section(std::move(labels), collect(std::forward<Rows>(rows)...)) {}

  template <composition::BindingSource Expanded, class... Rows>
    requires std::same_as<composition::BoundValue<Expanded>, bool> &&
             (composition::ViewContent<Rows> && ...)
  explicit Section(Expanded is_expanded, Rows&&... rows)
      : Section(Labels{}, composition::Binding<bool>{std::move(is_expanded)},
                collect(std::forward<Rows>(rows)...)) {}

  template <composition::BindingSource Expanded, class... Rows>
    requires std::same_as<composition::BoundValue<Expanded>, bool> &&
             (composition::ViewContent<Rows> && ...)
  Section(std::string header, Expanded is_expanded, Rows&&... rows)
      : Section(titled(std::move(header)),
                composition::Binding<bool>{std::move(is_expanded)},
                collect(std::forward<Rows>(rows)...)) {}

  template <composition::BindingSource Expanded, class... Rows>
    requires std::same_as<composition::BoundValue<Expanded>, bool> &&
             (composition::ViewContent<Rows> && ...)
  Section(Labels labels, Expanded is_expanded, Rows&&... rows)
      : Section(std::move(labels),
                composition::Binding<bool>{std::move(is_expanded)},
                collect(std::forward<Rows>(rows)...)) {}

  Section(Labels labels, std::vector<composition::Element> rows);

  Section(Labels labels, composition::Binding<bool> is_expanded,
          std::vector<composition::Element> rows);

  auto collapsible(bool is_collapsible) && -> Section;

  static auto content(const composition::Element& element)
      -> std::optional<std::reference_wrapper<const Content>>;

  static auto render(const Content& section) -> composition::Element;

  static auto render(const Content& section, float spacing)
      -> composition::Element;

  static auto render(const Content& section, float spacing, bool pinned)
      -> composition::Element;

  static auto render(const Content& section, const std::vector<Row>& rows,
                     float spacing, bool pinned, const Disclosure& disclosure)
      -> composition::Element;

 private:
  using Disclosed = composition::StateHandle<std::vector<std::string>>;

  Section(Labels labels, std::optional<composition::Binding<bool>> is_expanded,
          std::vector<composition::Element> rows, bool);

  static auto titled(std::string header) -> Labels;

  static auto columned(const Content& section) -> composition::Element;

  static auto outlined(const std::vector<Row>& rows, Disclosed disclosed)
      -> std::vector<Row>;

  static void flatten(std::vector<Row>& out, composition::Element element,
                      std::size_t depth, const std::string& path,
                      const Disclosed& disclosed);

  static void disclose(std::vector<Row>& out, const Row& row, std::size_t depth,
                       const std::string& path, const Disclosed& disclosed);

  static auto toggler(composition::Binding<bool> open,
                      composition::Element label) -> composition::Element;
};

class Section::Row {
 public:
  using Source = std::shared_ptr<composition::PlaceholderContentView::Source>;

  struct Outline {
    Source label;
    std::optional<composition::Binding<bool>> is_expanded;
    std::function<composition::Element()> content;
  };

  static auto of(composition::Element element) -> std::vector<Row>;

  static auto one(composition::Element element) -> Row;

  static auto placed(composition::Element element, composition::ElementKey slot,
                     std::size_t depth) -> Row;

  auto depth() const noexcept -> std::size_t { return depth_; }

  auto outline() const noexcept -> const std::shared_ptr<const Outline>& {
    return outline_;
  }

  auto slot() const noexcept -> const std::optional<composition::ElementKey>& {
    return slot_;
  }

  auto count() const noexcept -> std::size_t;

  auto key(std::size_t index) const -> std::optional<composition::ElementKey>;

  auto build(std::size_t index) const -> composition::Element;

  auto edits() const noexcept -> const components::ForEach::Edits& {
    return edits_;
  }

  auto repeated() const noexcept -> bool { return each_ != nullptr; }

  auto each() const noexcept
      -> const std::shared_ptr<const components::ForEach::Source>& {
    return each_;
  }

  auto traits() const noexcept -> const ListCell::Traits& { return traits_; }

  auto matches(const Row& other) const -> bool;

  void forward(const Row& newer) const;

 private:
  Row(std::shared_ptr<const components::ForEach::Source> each,
      components::ForEach::Edits edits, Source single,
      std::optional<composition::ElementKey> key, ListCell::Traits traits,
      std::shared_ptr<const Outline> outline) noexcept
      : each_(std::move(each)),
        edits_(std::move(edits)),
        single_(std::move(single)),
        key_(std::move(key)),
        traits_(traits),
        outline_(std::move(outline)) {}

  std::shared_ptr<const components::ForEach::Source> each_;
  components::ForEach::Edits edits_;
  Source single_;
  std::optional<composition::ElementKey> key_;
  ListCell::Traits traits_;
  std::shared_ptr<const Outline> outline_;
  std::optional<composition::ElementKey> slot_;
  std::size_t depth_{0};
};

class Section::Cells final : public components::ForEach::Source {
 public:
  Cells(Row row, bool opens, bool closes, composition::ListRow::Separator above,
        std::size_t position) noexcept
      : row_(std::move(row)),
        opens_(opens),
        closes_(closes),
        above_(above),
        position_(position) {}

  auto count() const noexcept -> std::size_t override;

  auto key(std::size_t index) const -> composition::ElementKey override;

  auto build(std::size_t index) const -> composition::Element override;

  auto bottom(std::size_t index) const -> composition::ListRow::Separator;

 private:
  void remember(std::size_t index,
                composition::ListRow::Separator separator) const;

  Row row_;
  bool opens_;
  bool closes_;
  composition::ListRow::Separator above_;
  std::size_t position_;
  mutable std::vector<std::optional<composition::ListRow::Separator>> bottoms_;
};

class Section::Caption : public composition::Element {
 public:
  class Content;

  Caption(std::shared_ptr<composition::PlaceholderContentView::Source> source,
          bool header);

  Caption(std::shared_ptr<composition::PlaceholderContentView::Source> source,
          bool header, Disclosure disclosure);
};

class Section::Caption::Content final : public composition::ElementContent {
 public:
  using Source = std::shared_ptr<composition::PlaceholderContentView::Source>;

  Content(Source source, bool header, Disclosure disclosure) noexcept
      : source_(std::move(source)),
        header_(header),
        disclosure_(std::move(disclosure)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

 private:
  Source source_;
  bool header_;
  Disclosure disclosure_;
};

class Section::Content final : public composition::ElementContent {
 public:
  using Source = std::shared_ptr<composition::PlaceholderContentView::Source>;

  static constexpr composition::StateKey<bool> expanded{"section-expanded",
                                                        true};
  static const composition::StateKey<std::vector<std::string>> disclosed;

  Content(Source header, Source footer, std::vector<Row> rows,
          std::optional<composition::Binding<bool>> expansion) noexcept
      : header_(std::move(header)),
        footer_(std::move(footer)),
        rows_(std::move(rows)),
        expansion_(std::move(expansion)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

  auto header() const noexcept -> const Source& { return header_; }

  auto footer() const noexcept -> const Source& { return footer_; }

  auto rows() const noexcept -> const std::vector<Row>& { return rows_; }

  auto expansion() const noexcept
      -> const std::optional<composition::Binding<bool>>& {
    return expansion_;
  }

  auto collapsible() const noexcept -> std::optional<bool> {
    return collapsible_;
  }

  void set_collapsible(bool is_collapsible) noexcept {
    collapsible_ = is_collapsible;
  }

  auto same_as(const composition::ElementContent& other) const -> bool override;

  auto adopt(composition::ElementContent& incoming) -> bool override;

  auto section_parts() const
      -> std::optional<composition::SectionParts> override;

 private:
  static void forward(const Source& held, const Source& newer);

  auto disclosure(composition::Context& context) const -> Disclosure;

  Source header_;
  Source footer_;
  std::vector<Row> rows_;
  std::optional<composition::Binding<bool>> expansion_;
  std::optional<bool> collapsible_;
  mutable bool shown_open_{true};
};

}
