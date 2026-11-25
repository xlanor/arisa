#pragma once

#include <arisa/composition/bar_placement.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/navigation_chrome.hpp>
#include <arisa/composition/navigation_split_view_visibility.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/toolbar_item.hpp>
#include <arisa/presentation/toolbar_layout.hpp>
#include <arisa/structure/sidebar_toggle.hpp>
#include <arisa/theme/theme.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace arisa::structure {

struct ScaffoldOptions {
  bool shows_status{true};
  bool shows_hints{true};
  std::optional<composition::BarPlacement> header{};
  std::optional<composition::BarPlacement> footer{};

  friend constexpr auto operator==(const ScaffoldOptions&,
                                   const ScaffoldOptions&) -> bool = default;
};

class Scaffold : public composition::Element {
 public:
  class Content;

  static constexpr std::string_view header_key = "arisa.scaffold.header";
  static constexpr std::string_view content_key = "arisa.scaffold.content";
  static constexpr std::string_view leading_group_key =
      "arisa.scaffold.leading-group";
  static constexpr std::string_view trailing_group_key =
      "arisa.scaffold.trailing-group";
  static constexpr std::string_view hint_group_key =
      "arisa.scaffold.hint-group";
  static constexpr std::string_view footer_key = "arisa.scaffold.footer";
  static constexpr std::string_view sidebar_toggle_key =
      "arisa.scaffold.sidebar-toggle";
  static const composition::StateKey<std::optional<SidebarToggle>> toggle;

  explicit Scaffold(composition::Element content);

  Scaffold(ScaffoldOptions options, composition::Element content);
};

class Scaffold::Content final : public composition::ElementContent {
 public:
  using Source = std::shared_ptr<composition::PlaceholderContentView::Source>;
  using Items = std::vector<std::shared_ptr<const composition::ToolbarItem>>;
  using Visibility = composition::NavigationSplitViewVisibility;

  Content(ScaffoldOptions options, std::optional<std::string> title,
          Items toolbar, composition::NavigationChrome chrome,
          Source content) noexcept
      : options_(options),
        title_(std::move(title)),
        toolbar_(std::move(toolbar)),
        chrome_(std::move(chrome)),
        content_(std::move(content)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

  auto reads_preferences() const noexcept -> bool override { return true; }

  void resolve(composition::Context& context,
               const composition::Node& node) override;

 private:
  struct Declared {
    std::optional<std::string> title;
    Items toolbar;
    composition::NavigationChrome chrome;

    friend auto operator==(const Declared&, const Declared&) -> bool = default;
  };

  static const composition::StateKey<std::optional<Declared>> resolved;
  static const composition::StateKey<Declared> shown;
  static constexpr composition::StateKey<std::uint64_t> revision{
      "scaffold-revision", 0};

  using Chrome = presentation::ToolbarLayout::Metrics;
  using Layout = presentation::ToolbarLayout;

  auto header(const Declared& chosen,
              const std::optional<SidebarToggle>& sidebar, bool floating,
              Visibility panel, const Chrome& metrics) const
      -> composition::Element;

  auto footer(const Declared& chosen, bool floating,
              const Chrome& metrics) const -> composition::Element;

  static auto title(const Declared& chosen, bool floating)
      -> std::optional<composition::Element>;

  static auto trailing_groups(std::vector<Layout::Segment> segments,
                              std::optional<composition::Element> last,
                              const Chrome& metrics)
      -> std::vector<composition::Element>;

  static auto docked_group(std::vector<composition::Element> items, bool rail,
                           const Chrome& metrics) -> composition::Element;

  static auto edge_fade(composition::Context& context, bool top)
      -> composition::Element;

  static auto edge_effect(composition::Context& context, bool top,
                          const composition::NavigationChrome::Bar& bar)
      -> std::optional<composition::Element>;

  static auto rule() -> composition::Element;

  auto placement(composition::Context& context, bool top) const
      -> composition::BarPlacement;

  ScaffoldOptions options_;
  std::optional<std::string> title_;
  Items toolbar_;
  composition::NavigationChrome chrome_;
  Source content_;
};

}
