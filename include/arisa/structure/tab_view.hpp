#pragma once

#include <arisa/async/task.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/focus_binding.hpp>
#include <arisa/composition/focus_state.hpp>
#include <arisa/composition/namespace.hpp>
#include <arisa/composition/navigation_chrome.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/composition/scroll_geometry_observer.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/tag_value.hpp>
#include <arisa/composition/toolbar_vertical_behavior.hpp>
#include <arisa/structure/detail_presenter.hpp>
#include <arisa/structure/navigation_split_view.hpp>
#include <arisa/structure/tab.hpp>
#include <arisa/structure/tab_bar_placement.hpp>
#include <arisa/structure/tab_view_customization.hpp>
#include <arisa/theme/metrics.hpp>

#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::structure {

class TabView : public composition::Element {
 public:
  static constexpr std::string_view sidebar_edit_label = "Edit";
  static constexpr std::string_view sidebar_done_label = "Done";

  class Content;
  class RailLabel;
  class Pane;
  class Item;
  class Minimizer;

  struct Entry {
    std::string title;
    std::string system_image;
    std::shared_ptr<const composition::TagValue> value;
    std::shared_ptr<const Tab::Builder> content;
    std::optional<TabRole> role;
    std::optional<std::string> badge;
    std::string customization;
    TabCustomizationBehavior behavior{TabCustomizationBehavior::automatic};
    std::optional<std::size_t> section;
    TabTraits traits;
    std::shared_ptr<const Tab::Builder> label;
    std::vector<Tab::Decoration> decorations;
    composition::ListRow::SwipeActions leading;
    composition::ListRow::SwipeActions trailing;
  };

  struct SectionHeading {
    TabSection::Content::Source header;
    std::string customization;
    TabSectionExpansion expansion{TabSectionExpansion::automatic};
  };

  struct Entries {
    std::vector<Entry> tabs;
    std::vector<std::string> sections;
    std::vector<std::shared_ptr<const Tab::Builder>> actions;
    std::vector<SectionHeading> headings;
  };

  using SidebarContent =
      std::shared_ptr<const std::function<composition::Element()>>;

  struct Selection {
    std::function<bool(const composition::TagValue&)> is_selected;
    std::function<void(const composition::TagValue&)> select;
  };

  struct ItemStyle {
    bool selected{false};
    bool sidebar{true};
    bool emphasized{true};
    bool rail{false};
    bool prominent{false};
    std::string title;
    std::optional<std::string> badge;

    auto make_body(components::ButtonStyle::Configuration configuration) const
        -> composition::Element;

    friend auto operator==(const ItemStyle&, const ItemStyle&)
        -> bool = default;
  };

  static constexpr float sidebar_width = theme::metrics::list.sidebar_width;
  static constexpr std::string_view capsule_id = "arisa.tab.capsule";
  static constexpr std::string_view bar_id = "arisa.tab.items";
  static constexpr std::string_view bar_space = "arisa.tab.bar";
  static constexpr std::string_view page_id = "arisa.tab.page";
  static constexpr composition::StateKey<std::size_t> chosen{"tab-view-chosen",
                                                             0};
  static constexpr composition::StateKey<composition::Rect> bar_frame{
      "tab-view-bar-frame", composition::Rect{}};
  static const composition::StateKey<std::vector<composition::Rect>>
      item_frames;
  static constexpr composition::FocusState<std::size_t> sidebar_focus{
      "tab-view-sidebar-focus"};
  static constexpr std::string_view rail_label_id = "arisa.tab.rail-label";
  static constexpr std::string_view search_id = "arisa.tab.search";
  static constexpr std::string_view dots_id = "arisa.tab.dots";
  static constexpr std::string_view badge_id = "arisa.tab.badge";
  static constexpr float dot_extent = 7.0F;
  static constexpr float dot_spacing = 8.0F;
  static constexpr float index_backing = 0.16F;
  static constexpr float page_commit = 0.25F;
  static constexpr composition::StateKey<float> page_drag{"tab-view-page-drag",
                                                          0.0F};
  static constexpr composition::Namespace pager{"tab-view-pager"};
  static const composition::EnvironmentKey<SidebarContent> sidebar_header;
  static const composition::EnvironmentKey<SidebarContent> sidebar_footer;
  static const composition::EnvironmentKey<SidebarContent> sidebar_bottom_bar;
  static const composition::EnvironmentKey<SidebarContent> sidebar_toolbar;
  static const composition::EnvironmentKey<SidebarContent> bottom_accessory;
  static const composition::EnvironmentKey<TabBarMinimizeBehavior>
      minimize_behavior;
  static const composition::EnvironmentKey<TabSearchActivation>
      search_activation;
  static const composition::EnvironmentKey<
      std::optional<AdaptableTabBarPlacement>>
      default_placement;
  static const composition::EnvironmentKey<
      std::optional<AdaptableTabBarPlacement>>
      adaptable_placement;
  static const composition::EnvironmentKey<std::optional<TabBarPlacement>>
      bar_placement;
  static const composition::EnvironmentKey<bool> sidebar_available;
  static const composition::EnvironmentKey<bool> showing_sections;
  static const composition::EnvironmentKey<
      std::optional<TabViewBottomAccessoryPlacement>>
      accessory_placement;
  static constexpr std::string_view prominent_id = "arisa.tab.prominent";
  static constexpr std::string_view accessory_id = "arisa.tab.accessory";
  static constexpr std::string_view sidebar_bottom_id =
      "arisa.tab.sidebar-bottom";
  static constexpr std::string_view sidebar_toolbar_id =
      "arisa.tab.sidebar-toolbar";
  static constexpr composition::StateKey<bool> bar_minimized{
      "tab-view-bar-minimized", false};
  static constexpr composition::StateKey<std::size_t> previous{
      "tab-view-previous", 0};

  template <class... Tabs>
    requires(composition::ViewContent<Tabs> && ...)
  explicit TabView(Tabs&&... tabs)
      : TabView(std::nullopt, collect(std::forward<Tabs>(tabs)...)) {}

  template <composition::BindingSource Source, class... Tabs>
    requires std::equality_comparable<composition::BoundValue<Source>> &&
             std::copy_constructible<composition::BoundValue<Source>> &&
             (composition::ViewContent<Tabs> && ...)
  explicit TabView(Source selection, Tabs&&... tabs)
      : TabView(bind(composition::Binding<composition::BoundValue<Source>>{
                    std::move(selection)}),
                collect(std::forward<Tabs>(tabs)...)) {}

  template <composition::BindingSource Source>
    requires std::equality_comparable<composition::BoundValue<Source>> &&
             std::copy_constructible<composition::BoundValue<Source>>
  TabView(Source selection, std::vector<composition::Element> tabs)
      : TabView(bind(composition::Binding<composition::BoundValue<Source>>{
                    std::move(selection)}),
                std::move(tabs)) {}

  TabView(std::optional<Selection> selection,
          std::vector<composition::Element> tabs);

  static auto item_key(std::size_t index) -> composition::ElementKey;

  auto column_visibility(
      composition::Binding<composition::NavigationSplitViewVisibility>
          visibility) && -> TabView;

  auto tab_view_customization(
      composition::Binding<TabViewCustomization> customization) && -> TabView;

 private:
  template <class T>
  static auto bind(composition::Binding<T> selection) -> Selection {
    return Selection{.is_selected =
                         [selection](const composition::TagValue& tag) {
                           const T* value = tag.get<T>();
                           return value != nullptr && *value == selection.get();
                         },
                     .select =
                         [selection](const composition::TagValue& tag) {
                           if (const T* value = tag.get<T>()) {
                             selection.set(*value);
                           }
                         }};
  }

  static auto entries(std::vector<composition::Element> tabs) -> Entries;

  static void add(Entries& found, const composition::Element& tab,
                  std::optional<std::size_t> section);
};

class TabView::RailLabel final : public composition::Component {
 public:
  static constexpr composition::StateKey<bool> visible{"rail-label-visible",
                                                       true};

  RailLabel(std::string title, std::size_t index)
      : title_(std::move(title)), index_(index) {}

  auto operator==(const RailLabel&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  static auto linger(composition::StateHandle<bool> shown) -> async::Task<void>;

  std::string title_;
  std::size_t index_;
};

class TabView::Item final : public composition::Component {
 public:
  using Choose = std::shared_ptr<const std::function<void(std::size_t, bool)>>;

  struct Inputs {
    std::size_t index{0};
    std::size_t shown{0};
    std::shared_ptr<const composition::TagValue> value;
    ItemStyle style;
    std::string system_image;
    TabTraits traits;
    std::shared_ptr<const Tab::Builder> label;
    std::vector<Tab::Decoration> decorations;
    composition::StateHandle<std::vector<composition::Rect>> frames;
    std::optional<composition::FocusBinding<std::size_t>> focus;
  };

  Item(Inputs inputs, Choose choose) noexcept
      : inputs_(std::move(inputs)), choose_(std::move(choose)) {}

  auto operator==(const Item& other) const -> bool;

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  auto follows(const Item& other) const noexcept -> bool;

  auto face() const -> composition::Element;

  auto dressed(composition::Element item) const -> composition::Element;

  Inputs inputs_;
  Choose choose_;
};

class TabView::Pane : public composition::Element {
 public:
  class Content;

  Pane(std::shared_ptr<const Tab::Builder> builder, bool frozen,
       bool covered = false,
       std::shared_ptr<composition::ScrollGeometryObserver> observer = nullptr);
};

class TabView::Minimizer final : public composition::ScrollGeometryObserver {
 public:
  static constexpr float threshold = 4.0F;
  static constexpr float edge = 8.0F;

  Minimizer(composition::StateHandle<bool> minimized,
            TabBarMinimizeBehavior behavior) noexcept
      : minimized_(std::move(minimized)), behavior_(behavior) {}

  auto differs(const composition::ScrollGeometry& before,
               const composition::ScrollGeometry& after) const -> bool override;

  void notify(const composition::ScrollGeometry& before,
              const composition::ScrollGeometry& after) override;

  static auto wanted(TabBarMinimizeBehavior behavior, bool now, float before,
                     float after) noexcept -> bool;

 private:
  composition::StateHandle<bool> minimized_;
  TabBarMinimizeBehavior behavior_;
};

class TabView::Pane::Content final : public composition::ElementContent {
 public:
  Content(std::shared_ptr<const Tab::Builder> builder, bool frozen) noexcept
      : builder_(std::move(builder)), frozen_(frozen) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto same_as(const composition::ElementContent& other) const -> bool override;

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

 private:
  std::shared_ptr<const Tab::Builder> builder_;
  bool frozen_;
};

class TabView::Content final : public composition::ElementContent {
 public:
  Content(std::optional<Selection> selection, Entries entries) noexcept
      : selection_(std::move(selection)),
        entries_(std::move(entries.tabs)),
        sections_(std::move(entries.sections)),
        actions_(std::move(entries.actions)),
        headings_(std::move(entries.headings)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

  auto count() const noexcept -> std::size_t { return entries_.size(); }

  auto hosts_sidebar() const noexcept -> bool override { return true; }

  auto reads_preferences() const noexcept -> bool override { return true; }

  void resolve(composition::Context& context,
               const composition::Node& node) override;

  void bind_visibility(NavigationSplitView::Binding binding) {
    visibility_ = std::move(binding);
  }

  void bind_customization(
      composition::Binding<TabViewCustomization> customization) {
    customization_ = std::move(customization);
  }

 private:
  using Choose = Item::Choose;

  static const composition::StateKey<composition::NavigationChrome::Bar>
      tab_bar_style;
  static constexpr composition::StateKey<
      composition::ToolbarVerticalCompressionBehavior>
      compression{"tab-view-compression",
                  composition::ToolbarVerticalCompressionBehavior::automatic};

  static constexpr composition::StateKey<bool> editing_sidebar{
      "tab-view-editing-sidebar", false};
  static const composition::StateKey<std::vector<std::size_t>> toggled_sections;
  static constexpr float section_chevron_extent = 11.0F;

  auto selected(composition::Context& context) const -> std::size_t;

  auto pane(const DetailPresenter::Slot& slot,
            const std::shared_ptr<composition::ScrollGeometryObserver>&
                observer) const -> composition::Element;

  auto visible(std::size_t index, bool sidebar) const -> bool;

  auto top_bar(composition::Context& context,
               std::span<const std::size_t> arranged, std::size_t shown,
               const Choose& choose, bool minimized,
               std::optional<composition::Element> inline_accessory) const
      -> std::optional<composition::Element>;
  auto accessory(composition::Context& context, bool minimized) const
      -> std::optional<composition::Element>;

  auto items(std::span<const std::size_t> order, std::size_t shown,
             const Choose& choose, bool sidebar, bool rail,
             composition::StateHandle<std::vector<composition::Rect>> frames,
             std::optional<composition::FocusBinding<std::size_t>> focus) const
      -> std::vector<composition::Element>;

  auto order(bool sidebar) const -> std::vector<std::size_t>;

  auto customizable(std::size_t index) const -> bool;

  auto visibility_rows() const -> std::vector<composition::Element>;

  static auto edit_toggle(composition::StateHandle<bool> editing)
      -> composition::Element;

  auto sidebar_rows(std::span<const std::size_t> order,
                    std::vector<composition::Element> items, bool rail,
                    composition::StateHandle<std::vector<std::size_t>> toggled)
      const -> std::vector<composition::Element>;

  auto section_heading(
      std::size_t section,
      composition::StateHandle<std::vector<std::size_t>> toggled) const
      -> composition::Element;

  auto section_expanded(std::size_t section,
                        const std::vector<std::size_t>& toggled) const noexcept
      -> bool;

  auto collapsible(std::size_t section) const noexcept -> bool;

  void arrange_sections(std::vector<std::size_t>& arranged) const;

  static auto swiped(composition::Element item, const Entry& entry)
      -> composition::Element;

  auto paged(composition::Context& context, std::size_t shown,
             const Choose& choose) const -> composition::Element;

  static auto dots(std::size_t count, std::size_t shown, const Choose& choose,
                   bool backed) -> composition::Element;

  auto visibility(composition::Context& context) const
      -> NavigationSplitView::Binding;

  static auto bar(
      std::vector<composition::Element> items, std::size_t shown, bool sidebar,
      bool emphasized, std::optional<composition::Element> pill,
      composition::StateHandle<std::vector<composition::Rect>> frames,
      composition::StateHandle<composition::Rect> origin)
      -> composition::Element;

  std::optional<Selection> selection_;
  std::vector<Entry> entries_;
  std::vector<std::string> sections_;
  std::vector<std::shared_ptr<const Tab::Builder>> actions_;
  std::vector<SectionHeading> headings_;
  std::optional<NavigationSplitView::Binding> visibility_;
  std::optional<composition::Binding<TabViewCustomization>> customization_;
};

}

namespace arisa::composition {

template <class Content>
auto Element::tab_view_sidebar_header(Content content) && -> Element {
  return std::move(*this).environment(
      structure::TabView::sidebar_header,
      std::make_shared<const std::function<Element()>>(
          [held = std::move(content)] { return Element{std::invoke(held)}; }));
}

template <class Content>
auto Element::tab_view_sidebar_bottom_bar(Content content) && -> Element {
  return std::move(*this).environment(
      structure::TabView::sidebar_bottom_bar,
      std::make_shared<const std::function<Element()>>(
          [held = std::move(content)] { return Element{std::invoke(held)}; }));
}

template <class Placement, class Content>
auto Element::content_toolbar(Placement, Content content) && -> Element {
  return std::move(*this).environment(
      structure::TabView::sidebar_toolbar,
      std::make_shared<const std::function<Element()>>(
          [held = std::move(content)] { return Element{std::invoke(held)}; }));
}

template <class Content>
auto Element::tab_view_bottom_accessory(Content content) && -> Element {
  return std::move(*this).tab_view_bottom_accessory(true, std::move(content));
}

template <class Content>
auto Element::tab_view_bottom_accessory(bool is_enabled,
                                        Content content) && -> Element {
  return std::move(*this).environment(
      structure::TabView::bottom_accessory,
      is_enabled ? std::make_shared<const std::function<Element()>>(
                       [held = std::move(content)] {
                         return Element{std::invoke(held)};
                       })
                 : structure::TabView::SidebarContent{});
}

template <class Behavior>
auto Element::tab_bar_minimize_behavior(Behavior behavior) && -> Element {
  return std::move(*this).environment(
      structure::TabView::minimize_behavior,
      structure::TabBarMinimizeBehavior{behavior});
}

template <class Activation>
auto Element::tab_view_search_activation(Activation activation) && -> Element {
  return std::move(*this).environment(
      structure::TabView::search_activation,
      structure::TabSearchActivation{activation});
}

template <class Placement>
auto Element::default_tab_bar_placement(Placement placement) && -> Element {
  return std::move(*this).environment(
      structure::TabView::default_placement,
      std::optional<structure::AdaptableTabBarPlacement>{placement});
}

template <class Placement>
auto Element::default_adaptable_tab_bar_placement(
    Placement placement) && -> Element {
  return std::move(*this).environment(
      structure::TabView::adaptable_placement,
      std::optional<structure::AdaptableTabBarPlacement>{placement});
}

template <class Content>
auto Element::tab_view_sidebar_footer(Content content) && -> Element {
  return std::move(*this).environment(
      structure::TabView::sidebar_footer,
      std::make_shared<const std::function<Element()>>(
          [held = std::move(content)] { return Element{std::invoke(held)}; }));
}

}
