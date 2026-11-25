#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/structure/key_path_comparator.hpp>
#include <arisa/structure/table_column_customization.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::examples {

class TablesPage final : public composition::Component {
 public:
  class Opener;

  enum class Demo : std::uint8_t {
    files,
    outline,
    sections,
  };

  struct File {
    int id{0};
    std::string name;
    std::string kind;
    int size{0};
    std::optional<std::vector<File>> children;

    friend auto operator==(const File&, const File&) -> bool = default;
  };

  static constexpr std::size_t file_count = 240;
  static constexpr std::string_view close_button = "tables-close";
  static constexpr std::string_view demo_picker = "tables-demo";
  static constexpr std::string_view files_table = "tables-files";
  static constexpr std::string_view outline_table = "tables-outline";
  static constexpr std::string_view sections_list = "tables-sections";
  static constexpr std::string_view swipe_stack = "tables-swipe";
  static constexpr std::string_view opened_label = "tables-opened";

  static constexpr composition::StateKey<Demo> demo{"tables-demo", Demo::files};
  static const composition::StateKey<std::set<int>> chosen;
  static const composition::StateKey<
      std::vector<structure::KeyPathComparator<File>>>
      order;
  static const composition::StateKey<structure::TableColumnCustomization>
      arranged;
  static constexpr composition::StateKey<bool> inbox_open{"tables-inbox", true};
  static constexpr composition::StateKey<int> opened{"tables-opened", 0};

  explicit TablesPage(composition::StateHandle<bool> shown) noexcept
      : shown_(std::move(shown)) {}

  auto operator==(const TablesPage& other) const -> bool {
    return shown_.identity() == other.shown_.identity();
  }

  auto body(composition::Context& context) const
      -> composition::Element override;

  static auto files() -> std::vector<File>;

  static auto tree() -> std::vector<File>;

  static auto sorted(std::vector<File> rows,
                     const std::vector<structure::KeyPathComparator<File>>& by)
      -> std::vector<File>;

 private:
  static auto files_demo(
      composition::StateHandle<std::set<int>> picked,
      composition::StateHandle<std::vector<structure::KeyPathComparator<File>>>
          by,
      composition::StateHandle<structure::TableColumnCustomization> layout,
      composition::StateHandle<int> count) -> composition::Element;

  static auto outline_demo() -> composition::Element;

  static auto sections_demo(composition::StateHandle<bool> inbox)
      -> composition::Element;

  static auto swipe_demo() -> composition::Element;

  composition::StateHandle<bool> shown_;
};

class TablesPage::Opener final : public composition::Component {
 public:
  static constexpr composition::StateKey<bool> shown_key{"tables-shown", false};
  static constexpr std::string_view button = "tables-open";

  auto operator==(const Opener&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;
};

}
