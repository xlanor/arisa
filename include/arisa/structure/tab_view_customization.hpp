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

class TabViewCustomization {
 public:
  class TabCustomization {
   public:
    TabCustomization(composition::Visibility sidebar,
                     composition::Visibility tab_bar) noexcept
        : sidebar_(sidebar), tab_bar_(tab_bar) {}

    auto sidebar_visibility() const noexcept -> composition::Visibility {
      return sidebar_;
    }

    auto tab_bar_visibility() const noexcept -> composition::Visibility {
      return tab_bar_;
    }

    friend auto operator==(const TabCustomization&, const TabCustomization&)
        -> bool = default;

   private:
    composition::Visibility sidebar_;
    composition::Visibility tab_bar_;
  };

  class SectionCustomization {
   public:
    SectionCustomization() = default;

    explicit SectionCustomization(std::vector<std::string> order)
        : order_(std::move(order)) {}

    auto tab_order() const -> std::optional<std::vector<std::string>> {
      if (order_.empty()) {
        return std::nullopt;
      }
      return order_;
    }

    void reset_tab_order() noexcept { order_.clear(); }

    friend auto operator==(const SectionCustomization&,
                           const SectionCustomization&) -> bool = default;

   private:
    std::vector<std::string> order_;
  };

  TabViewCustomization() = default;

  auto section(std::string_view id) const -> SectionCustomization;

  void set_section(std::string_view id, SectionCustomization section);

  auto section_order(std::string_view id) const
      -> std::optional<std::vector<std::string>>;

  void reorder_section(std::string_view id, std::vector<std::string> order);

  void reset_section_order();

  void reset_section_order(std::string_view id);

  auto tab(std::string_view id) const -> TabCustomization;

  auto tab_bar_visibility(std::string_view id) const -> composition::Visibility;

  void set_tab_bar_visibility(std::string_view id,
                              composition::Visibility visibility);

  auto sidebar_visibility(std::string_view id) const -> composition::Visibility;

  void set_sidebar_visibility(std::string_view id,
                              composition::Visibility visibility);

  void reset_visibility(std::span<const std::string> ids);

  void reset_tab_order();

  auto order() const noexcept -> std::span<const std::string> { return order_; }

  void reorder(std::vector<std::string> order);

  auto arranged(std::span<const std::string> declared) const
      -> std::vector<std::size_t>;

  auto ordered(std::span<const std::string> declared) const
      -> std::vector<std::size_t>;

  auto encoded() const -> std::string;

  static auto decoded(std::string_view text)
      -> std::optional<TabViewCustomization>;

  friend auto operator==(const TabViewCustomization&,
                         const TabViewCustomization&) -> bool = default;

 private:
  static constexpr char order_mark = 'o';
  static constexpr char hidden_mark = 'h';
  static constexpr char visible_mark = 'v';
  static constexpr char bar_hidden_mark = 'H';
  static constexpr char bar_visible_mark = 'V';
  static constexpr char section_mark = 's';
  static constexpr char section_separator = '\t';

  std::vector<std::string> order_;
  std::flat_map<std::string, composition::Visibility, std::less<>> visibility_;
  std::flat_map<std::string, composition::Visibility, std::less<>> bar_;
  std::flat_map<std::string, std::vector<std::string>, std::less<>> sections_;
};

}
