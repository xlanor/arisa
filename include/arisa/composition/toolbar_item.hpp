#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/matched_geometry.hpp>
#include <arisa/composition/namespace.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/composition/toolbar_customization.hpp>
#include <arisa/composition/toolbar_item_visibility_priority.hpp>
#include <arisa/composition/toolbar_vertical_behavior.hpp>
#include <arisa/composition/visibility.hpp>

#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace arisa::composition {

enum class ToolbarItemPlacement : std::uint8_t {
  automatic,
  top_bar_leading,
  principal,
  top_bar_trailing,
  status,
  bottom_bar,
  primary_action,
  secondary_action,
  confirmation_action,
  cancellation_action,
  navigation,
  destructive_action,
  title,
  subtitle,
  large_title,
  large_subtitle,
  top_bar_pinned_trailing,
  keyboard,
};

enum class ToolbarDefaultItemKind : std::uint8_t {
  sidebar_toggle,
  title,
  search,
};

enum class SpacerSizing : std::uint8_t {
  flexible,
  fixed,
};

struct ToolbarRemoval {
  static const EnvironmentKey<bool> sidebar_toggle_removed;
  static const EnvironmentKey<bool> title_removed;
  static const EnvironmentKey<bool> search_removed;

  ToolbarDefaultItemKind removing{ToolbarDefaultItemKind::sidebar_toggle};

  friend constexpr auto operator==(const ToolbarRemoval&, const ToolbarRemoval&)
      -> bool = default;
};

class ToolbarItem {
 public:
  using Source = std::shared_ptr<PlaceholderContentView::Source>;
  using Builder = std::shared_ptr<const std::function<Element()>>;

  enum class Kind : std::uint8_t {
    item,
    spacer,
    title_menu,
    default_item,
    overflow_menu,
  };

  class Group {
   public:
    Group() = default;

    explicit Group(Source label) noexcept : label_(std::move(label)) {}

    auto label() const noexcept -> const Source& { return label_; }

   private:
    Source label_;
  };

  explicit ToolbarItem(Element content)
      : ToolbarItem(ToolbarItemPlacement::automatic, std::move(content)) {}

  ToolbarItem(ToolbarItemPlacement placement, Element content)
      : placement_(placement),
        content_(std::make_shared<PlaceholderContentView::Source>(
            std::move(content))) {}

  ToolbarItem(std::string id, ToolbarItemPlacement placement, Element content)
      : ToolbarItem(placement, std::move(content)) {
    id_ = std::move(id);
  }

  ToolbarItem(std::string id, Element content)
      : ToolbarItem(std::move(id), ToolbarItemPlacement::automatic,
                    std::move(content)) {}

  template <class Build>
    requires std::invocable<const Build&> &&
             std::convertible_to<std::invoke_result_t<const Build&>, Element> &&
             (!std::convertible_to<Build, Element>)
  ToolbarItem(ToolbarItemPlacement placement, Build build)
      : ToolbarItem(placement, std::make_shared<const std::function<Element()>>(
                                   [build = std::move(build)] {
                                     return Element{std::invoke(build)};
                                   })) {}

  template <class Build>
    requires std::invocable<const Build&> &&
             std::convertible_to<std::invoke_result_t<const Build&>, Element> &&
             (!std::convertible_to<Build, Element>)
  ToolbarItem(std::string id, ToolbarItemPlacement placement, Build build)
      : ToolbarItem(placement, std::move(build)) {
    id_ = std::move(id);
  }

  static auto title_menu(std::function<Element()> content) -> ToolbarItem {
    ToolbarItem made{
        ToolbarItemPlacement::principal,
        std::make_shared<const std::function<Element()>>(std::move(content))};
    made.kind_ = Kind::title_menu;
    return made;
  }

  static auto spacer(SpacerSizing sizing, ToolbarItemPlacement placement)
      -> ToolbarItem {
    ToolbarItem made{placement, Source{}};
    made.kind_ = Kind::spacer;
    made.sizing_ = sizing;
    return made;
  }

  static auto overflow_menu(std::function<Element()> content) -> ToolbarItem {
    ToolbarItem made{
        ToolbarItemPlacement::secondary_action,
        std::make_shared<const std::function<Element()>>(std::move(content))};
    made.kind_ = Kind::overflow_menu;
    return made;
  }

  auto grouped(std::shared_ptr<const Group> group) && -> ToolbarItem {
    group_ = std::move(group);
    return std::move(*this);
  }

  auto hidden(bool hides = true) && -> ToolbarItem {
    hidden_ = hides;
    return std::move(*this);
  }

  auto axis_behavior(ToolbarItemAxisBehavior behavior) && -> ToolbarItem {
    axis_ = behavior;
    return std::move(*this);
  }

  auto content_margins_removed(bool removed = true) && -> ToolbarItem {
    margins_removed_ = removed;
    return std::move(*this);
  }

  auto matched_transition_source(ElementKey id,
                                 Namespace::ID space) && -> ToolbarItem {
    transition_source_ =
        MatchedGeometry::Key{.space = space, .id = std::move(id)};
    return std::move(*this);
  }

  auto shared_background_visibility(Visibility visibility) && -> ToolbarItem {
    shared_background_ = visibility;
    return std::move(*this);
  }

  auto customization_behavior(
      ToolbarCustomizationBehavior behavior) && -> ToolbarItem {
    customization_ = behavior;
    return std::move(*this);
  }

  auto default_customization(
      Visibility visibility,
      ToolbarCustomizationOptions options = {}) && -> ToolbarItem {
    default_visibility_ = visibility;
    options_ = options;
    return std::move(*this);
  }

  auto visibility_priority(
      ToolbarItemVisibilityPriority priority) && -> ToolbarItem {
    priority_ = priority;
    return std::move(*this);
  }

  auto customizing(
      std::shared_ptr<const std::string> toolbar) && -> ToolbarItem {
    toolbar_ = std::move(toolbar);
    return std::move(*this);
  }

  auto standing_in(ToolbarDefaultItemKind kind) && -> ToolbarItem {
    system_ = kind;
    return std::move(*this);
  }

  auto placed(ToolbarItemPlacement placement) && -> ToolbarItem {
    placement_ = placement;
    return std::move(*this);
  }

  auto placement() const noexcept -> ToolbarItemPlacement { return placement_; }

  auto kind() const noexcept -> Kind { return kind_; }

  auto sizing() const noexcept -> SpacerSizing { return sizing_; }

  auto group() const noexcept -> const std::shared_ptr<const Group>& {
    return group_;
  }

  auto id() const noexcept -> const std::string& { return id_; }

  auto toolbar() const noexcept -> const std::shared_ptr<const std::string>& {
    return toolbar_;
  }

  auto is_hidden() const noexcept -> bool { return hidden_; }

  auto axis() const noexcept -> ToolbarItemAxisBehavior { return axis_; }

  auto margins_removed() const noexcept -> bool { return margins_removed_; }

  auto transition_source() const noexcept
      -> const std::optional<MatchedGeometry::Key>& {
    return transition_source_;
  }

  auto shared_background() const noexcept -> Visibility {
    return shared_background_;
  }

  auto customization() const noexcept -> ToolbarCustomizationBehavior {
    return customization_;
  }

  auto default_visibility() const noexcept -> Visibility {
    return default_visibility_;
  }

  auto options() const noexcept -> ToolbarCustomizationOptions {
    return options_;
  }

  auto priority() const noexcept -> ToolbarItemVisibilityPriority {
    return priority_;
  }

  auto default_kind() const noexcept -> ToolbarDefaultItemKind {
    return default_kind_;
  }

  auto system() const noexcept -> const std::optional<ToolbarDefaultItemKind>& {
    return system_;
  }

  auto customizable() const noexcept -> bool {
    return toolbar_ != nullptr && !id_.empty() && kind_ == Kind::item &&
           customization_ == ToolbarCustomizationBehavior::default_behavior;
  }

  auto trailing() const noexcept -> bool {
    return placement_ == ToolbarItemPlacement::automatic ||
           placement_ == ToolbarItemPlacement::top_bar_trailing ||
           placement_ == ToolbarItemPlacement::primary_action ||
           placement_ == ToolbarItemPlacement::confirmation_action ||
           placement_ == ToolbarItemPlacement::destructive_action ||
           placement_ == ToolbarItemPlacement::top_bar_pinned_trailing;
  }

  auto leading() const noexcept -> bool {
    return placement_ == ToolbarItemPlacement::top_bar_leading ||
           placement_ == ToolbarItemPlacement::cancellation_action ||
           placement_ == ToolbarItemPlacement::navigation;
  }

  auto pinned() const noexcept -> bool {
    return placement_ == ToolbarItemPlacement::top_bar_pinned_trailing;
  }

  auto content() const noexcept -> const Source& { return content_; }

  auto builder() const noexcept -> const Builder& { return builder_; }

  auto same_shape(const ToolbarItem& other) const noexcept -> bool {
    return placement_ == other.placement_ && kind_ == other.kind_ &&
           sizing_ == other.sizing_ &&
           (group_ == nullptr) == (other.group_ == nullptr) &&
           id_ == other.id_ && hidden_ == other.hidden_ &&
           axis_ == other.axis_ && margins_removed_ == other.margins_removed_ &&
           transition_source_ == other.transition_source_ &&
           shared_background_ == other.shared_background_ &&
           customization_ == other.customization_ &&
           default_visibility_ == other.default_visibility_ &&
           options_ == other.options_ && priority_ == other.priority_ &&
           default_kind_ == other.default_kind_ && system_ == other.system_ &&
           (toolbar_ == nullptr) == (other.toolbar_ == nullptr) &&
           (toolbar_ == nullptr || *toolbar_ == *other.toolbar_);
  }

 protected:
  ToolbarItem(ToolbarItemPlacement placement, Source content) noexcept
      : placement_(placement), content_(std::move(content)) {}

  ToolbarItem(ToolbarItemPlacement placement, Builder builder)
      : placement_(placement),
        content_(
            std::make_shared<PlaceholderContentView::Source>((*builder)())),
        builder_(std::move(builder)) {}

  void stand_for(ToolbarDefaultItemKind kind) noexcept {
    kind_ = Kind::default_item;
    default_kind_ = kind;
  }

 private:
  ToolbarItemPlacement placement_;
  Kind kind_{Kind::item};
  SpacerSizing sizing_{SpacerSizing::flexible};
  std::shared_ptr<const Group> group_;
  std::string id_;
  std::shared_ptr<const std::string> toolbar_;
  bool hidden_{false};
  ToolbarItemAxisBehavior axis_{ToolbarItemAxisBehavior::automatic};
  bool margins_removed_{false};
  std::optional<MatchedGeometry::Key> transition_source_;
  Visibility shared_background_{Visibility::automatic};
  ToolbarCustomizationBehavior customization_{
      ToolbarCustomizationBehavior::default_behavior};
  Visibility default_visibility_{Visibility::automatic};
  ToolbarCustomizationOptions options_;
  ToolbarItemVisibilityPriority priority_{
      ToolbarItemVisibilityPriority::automatic};
  ToolbarDefaultItemKind default_kind_{ToolbarDefaultItemKind::title};
  std::optional<ToolbarDefaultItemKind> system_;
  Source content_;
  Builder builder_;
};

class DefaultToolbarItem : public ToolbarItem {
 public:
  explicit DefaultToolbarItem(
      ToolbarDefaultItemKind kind,
      ToolbarItemPlacement placement = ToolbarItemPlacement::automatic)
      : ToolbarItem(placement, Source{}) {
    stand_for(kind);
  }
};

class ToolbarOverflowMenu : public ToolbarItem {
 public:
  template <class Build>
    requires std::invocable<const Build&> &&
             std::convertible_to<std::invoke_result_t<const Build&>, Element>
  explicit ToolbarOverflowMenu(Build build)
      : ToolbarItem(ToolbarItem::overflow_menu([build = std::move(build)] {
          return Element{std::invoke(build)};
        })) {}
};

class ToolbarItemGroup {
 public:
  template <class... Items>
    requires(std::convertible_to<Items, Element> && ...)
  explicit ToolbarItemGroup(ToolbarItemPlacement placement, Items&&... items)
      : placement_(placement) {
    (items_.emplace_back(placement, Element{std::forward<Items>(items)}), ...);
  }

  ToolbarItemGroup(ToolbarItemPlacement placement, std::vector<Element> items)
      : placement_(placement) {
    for (Element& item : items) {
      items_.emplace_back(placement, std::move(item));
    }
  }

  ToolbarItemGroup(ToolbarItemPlacement placement, std::vector<Element> items,
                   Element label)
      : ToolbarItemGroup(placement, std::move(items)) {
    label_ = std::make_shared<PlaceholderContentView::Source>(std::move(label));
  }

  auto placement() const noexcept -> ToolbarItemPlacement { return placement_; }

  auto labelled() const noexcept -> bool { return label_ != nullptr; }

  auto hidden(bool hides = true) && -> ToolbarItemGroup {
    return std::move(*this).each(
        [hides](ToolbarItem item) { return std::move(item).hidden(hides); });
  }

  auto axis_behavior(ToolbarItemAxisBehavior behavior) && -> ToolbarItemGroup {
    return std::move(*this).each([behavior](ToolbarItem item) {
      return std::move(item).axis_behavior(behavior);
    });
  }

  auto content_margins_removed(bool removed = true) && -> ToolbarItemGroup {
    return std::move(*this).each([removed](ToolbarItem item) {
      return std::move(item).content_margins_removed(removed);
    });
  }

  auto shared_background_visibility(
      Visibility visibility) && -> ToolbarItemGroup {
    return std::move(*this).each([visibility](ToolbarItem item) {
      return std::move(item).shared_background_visibility(visibility);
    });
  }

  auto visibility_priority(
      ToolbarItemVisibilityPriority priority) && -> ToolbarItemGroup {
    return std::move(*this).each([priority](ToolbarItem item) {
      return std::move(item).visibility_priority(priority);
    });
  }

  auto items() && -> std::vector<ToolbarItem> {
    const auto group = std::make_shared<const ToolbarItem::Group>(label_);
    std::vector<ToolbarItem> made;
    made.reserve(items_.size());
    for (ToolbarItem& item : items_) {
      made.push_back(std::move(item).grouped(group));
    }
    return made;
  }

 private:
  template <class Change>
  auto each(Change change) && -> ToolbarItemGroup {
    for (ToolbarItem& item : items_) {
      item = change(std::move(item));
    }
    return std::move(*this);
  }

  ToolbarItemPlacement placement_;
  std::vector<ToolbarItem> items_;
  ToolbarItem::Source label_;
};

template <class Content>
auto Element::toolbar_overflow_menu(Content content) && -> Element {
  add_toolbar(ToolbarOverflowMenu{std::move(content)});
  return std::move(*this);
}

class ToolbarSpacer {
 public:
  explicit ToolbarSpacer(
      SpacerSizing sizing = SpacerSizing::flexible,
      ToolbarItemPlacement placement = ToolbarItemPlacement::automatic) noexcept
      : sizing_(sizing), placement_(placement) {}

  auto item() const -> ToolbarItem {
    return ToolbarItem::spacer(sizing_, placement_);
  }

 private:
  SpacerSizing sizing_;
  ToolbarItemPlacement placement_;
};

}
