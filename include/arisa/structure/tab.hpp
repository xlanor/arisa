#pragma once

#include <arisa/composition/accessibility_text.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/horizontal_edge.hpp>
#include <arisa/composition/list_row.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/composition/popover_attachment_anchor.hpp>
#include <arisa/composition/presented_item.hpp>
#include <arisa/composition/tag_value.hpp>
#include <arisa/composition/visibility.hpp>
#include <arisa/controls/context_menu.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/structure/tab_bar_placement.hpp>
#include <arisa/structure/tab_section_expansion.hpp>
#include <arisa/theme/space.hpp>

#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::structure {

enum class TabRole : std::uint8_t {
  search,
  prominent,
};

enum class TabCustomizationBehavior : std::uint8_t {
  automatic,
  reorderable,
  disabled,
};

class TabImage {
 public:
  explicit TabImage(std::string name) noexcept : name_(std::move(name)) {}

  auto name() const noexcept -> const std::string& { return name_; }

  friend auto operator==(const TabImage&, const TabImage&) -> bool = default;

 private:
  std::string name_;
};

struct TabTraits {
  enum class Icon : std::uint8_t {
    system,
    asset,
    none,
  };

  Icon icon{Icon::system};
  bool disabled{false};
  bool hidden{false};
  TabPlacement placement{TabPlacement::automatic};
  std::optional<std::string> help;
  std::optional<std::string> accessibility_label;
  std::optional<std::string> accessibility_hint;
  std::optional<std::string> accessibility_value;
  std::optional<std::string> accessibility_identifier;
  std::vector<composition::AccessibilityText> accessibility_input_labels;
  composition::Visibility sidebar_default{composition::Visibility::automatic};
  composition::Visibility tab_bar_default{composition::Visibility::automatic};
  std::optional<TabCustomizationBehavior> sidebar_behavior;
  std::optional<TabCustomizationBehavior> tab_bar_behavior;
  std::optional<composition::SpringLoadingBehavior> spring_loading;

  friend auto operator==(const TabTraits&, const TabTraits&) -> bool = default;
};

class Tab : public composition::Element {
 public:
  class Content;

  using Builder = std::function<composition::Element()>;
  using Decoration = std::shared_ptr<
      const std::function<composition::Element(composition::Element)>>;

  static constexpr std::string_view search_title = "Search";
  static constexpr std::string_view search_image = "search";

  template <std::equality_comparable T>
    requires std::copy_constructible<T>
  Tab(std::string title, std::string system_image, T value, Builder content)
      : Tab(std::move(title), std::move(system_image),
            composition::TagValue::of(std::move(value)), std::nullopt,
            std::move(content)) {}

  template <std::equality_comparable T>
    requires std::copy_constructible<T>
  Tab(std::string title, std::string system_image, T value, TabRole role,
      Builder content)
      : Tab(std::move(title), std::move(system_image),
            composition::TagValue::of(std::move(value)),
            std::optional<TabRole>{role}, std::move(content)) {}

  template <std::equality_comparable T>
    requires std::copy_constructible<T>
  Tab(TabRole role, T value, Builder content)
      : Tab(role, composition::TagValue::of(std::move(value)),
            std::move(content)) {}

  template <std::equality_comparable T>
    requires std::copy_constructible<T>
  Tab(std::string title, TabImage image, T value, Builder content)
      : Tab(std::move(title), std::move(image),
            composition::TagValue::of(std::move(value)), std::nullopt,
            std::move(content)) {}

  template <std::equality_comparable T>
    requires std::copy_constructible<T>
  Tab(std::string title, TabImage image, T value, TabRole role, Builder content)
      : Tab(std::move(title), std::move(image),
            composition::TagValue::of(std::move(value)),
            std::optional<TabRole>{role}, std::move(content)) {}

  template <std::equality_comparable T>
    requires std::copy_constructible<T>
  Tab(T value, Builder content)
      : Tab(composition::TagValue::of(std::move(value)), std::nullopt,
            std::move(content), Builder{}) {}

  template <std::equality_comparable T>
    requires std::copy_constructible<T>
  Tab(T value, TabRole role, Builder content)
      : Tab(composition::TagValue::of(std::move(value)),
            std::optional<TabRole>{role}, std::move(content), Builder{}) {}

  template <std::equality_comparable T>
    requires std::copy_constructible<T>
  Tab(T value, Builder content, Builder label)
      : Tab(composition::TagValue::of(std::move(value)), std::nullopt,
            std::move(content), std::move(label)) {}

  template <std::equality_comparable T>
    requires std::copy_constructible<T>
  Tab(T value, TabRole role, Builder content, Builder label)
      : Tab(composition::TagValue::of(std::move(value)),
            std::optional<TabRole>{role}, std::move(content),
            std::move(label)) {}

  Tab(std::string title, std::string system_image, Builder content);

  Tab(std::string title, std::string system_image, TabRole role,
      Builder content);

  Tab(std::string title, TabImage image, Builder content);

  Tab(std::string title, TabImage image, TabRole role, Builder content);

  Tab(TabRole role, Builder content);

  explicit Tab(Builder content);

  Tab(Builder content, Builder label);

  auto customization_id(std::string id) && -> Tab;

  auto customization_behavior(TabCustomizationBehavior behavior) && -> Tab;

  auto customization_behavior(TabCustomizationBehavior behavior,
                              AdaptableTabBarPlacement placement) && -> Tab;

  auto default_visibility(composition::Visibility visibility,
                          AdaptableTabBarPlacement placement) && -> Tab;

  auto tab_placement(TabPlacement placement) && -> Tab;

  auto disabled(bool is_disabled = true) && -> Tab;

  auto hidden(bool is_hidden = true) && -> Tab;

  auto help(std::string text) && -> Tab;

  auto spring_loading_behavior(
      composition::SpringLoadingBehavior behavior) && -> Tab;

  auto swipe_actions(composition::HorizontalEdge edge, bool allows_full_swipe,
                     std::function<composition::Element()> content) && -> Tab;

  auto swipe_actions(std::function<composition::Element()> content) && -> Tab;

  auto accessibility_label(std::string label, bool is_enabled = true) && -> Tab;

  auto accessibility_hint(std::string hint, bool is_enabled = true) && -> Tab;

  auto accessibility_value(std::string value, bool is_enabled = true) && -> Tab;

  auto accessibility_identifier(std::string identifier,
                                bool is_enabled = true) && -> Tab;

  auto accessibility_input_labels(
      std::vector<composition::AccessibilityText> input_labels,
      bool is_enabled = true) && -> Tab;

  template <class Items>
  auto context_menu(Items items) && -> Tab {
    return std::move(*this).decorated(
        [held = std::make_shared<const Items>(std::move(items))](
            composition::Element item) {
          return std::move(item).context_menu(
              [held] { return composition::Element{std::invoke(*held)}; });
        });
  }

  template <class Shown>
  auto popover(composition::Binding<bool> is_presented,
               Shown content) && -> Tab {
    return std::move(*this).popover(
        std::move(is_presented), composition::PopoverAttachmentAnchor::rect(),
        theme::Edge::top, std::move(content));
  }

  template <class Shown>
  auto popover(composition::Binding<bool> is_presented,
               composition::PopoverAttachmentAnchor attachment_anchor,
               theme::Edge arrow_edge, Shown content) && -> Tab {
    return std::move(*this).decorated(
        [is_presented, attachment_anchor, arrow_edge,
         held = std::make_shared<const Shown>(std::move(content))](
            composition::Element item) {
          return std::move(item).popover(
              is_presented, attachment_anchor, arrow_edge,
              [held] { return composition::Element{std::invoke(*held)}; });
        });
  }

  template <composition::PresentedItem Item, class Shown>
  auto popover(Item item, Shown content) && -> Tab {
    return std::move(*this).popover(
        std::move(item), composition::PopoverAttachmentAnchor::rect(),
        theme::Edge::top, std::move(content));
  }

  template <composition::PresentedItem Item, class Shown>
  auto popover(Item item,
               composition::PopoverAttachmentAnchor attachment_anchor,
               theme::Edge arrow_edge, Shown content) && -> Tab {
    const auto bound = presentation::Presentations::bound(std::move(item));
    return std::move(*this).decorated(
        [bound, attachment_anchor, arrow_edge,
         held = std::make_shared<const Shown>(std::move(content))](
            composition::Element tab_item) {
          return std::move(tab_item).popover(bound, attachment_anchor,
                                             arrow_edge, *held);
        });
  }

  static auto content(const composition::Element& element)
      -> std::optional<std::reference_wrapper<const Content>>;

 private:
  Tab(std::string title, std::string system_image,
      std::shared_ptr<const composition::TagValue> value,
      std::optional<TabRole> role, Builder content);

  Tab(std::string title, TabImage image,
      std::shared_ptr<const composition::TagValue> value,
      std::optional<TabRole> role, Builder content);

  Tab(TabRole role, std::shared_ptr<const composition::TagValue> value,
      Builder content);

  Tab(std::shared_ptr<const composition::TagValue> value,
      std::optional<TabRole> role, Builder content, Builder label);

  template <class Decorate>
  auto decorated(Decorate decorate) && -> Tab {
    return std::move(*this).decorate_with(
        std::make_shared<
            const std::function<composition::Element(composition::Element)>>(
            std::move(decorate)));
  }

  auto decorate_with(Decoration decoration) && -> Tab;

  template <class Edit>
  auto traits(Edit edit) && -> Tab;
};

class Tab::Content final : public composition::ElementContent {
 public:
  Content(std::string title, std::string system_image,
          std::shared_ptr<const composition::TagValue> value,
          std::optional<TabRole> role, Builder content) noexcept
      : title_(std::move(title)),
        system_image_(std::move(system_image)),
        value_(std::move(value)),
        role_(role),
        content_(std::move(content)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

  auto title() const noexcept -> const std::string& { return title_; }

  auto system_image() const noexcept -> const std::string& {
    return system_image_;
  }

  auto value() const noexcept
      -> const std::shared_ptr<const composition::TagValue>& {
    return value_;
  }

  auto role() const noexcept -> std::optional<TabRole> { return role_; }

  auto customization() const noexcept -> const std::string& {
    return customization_;
  }

  auto customization_behavior() const noexcept -> TabCustomizationBehavior {
    return behavior_;
  }

  auto builder() const noexcept -> const Builder& { return content_; }

  auto label() const noexcept -> const Builder& { return label_; }

  auto traits() const noexcept -> const TabTraits& { return traits_; }

  auto decorations() const noexcept -> const std::vector<Decoration>& {
    return decorations_;
  }

  auto swipe(composition::HorizontalEdge edge) const noexcept
      -> const composition::ListRow::SwipeActions& {
    return edge == composition::HorizontalEdge::leading ? leading_ : trailing_;
  }

  void swipe_with(composition::HorizontalEdge edge,
                  composition::ListRow::SwipeActions actions) {
    (edge == composition::HorizontalEdge::leading ? leading_ : trailing_) =
        std::move(actions);
  }

  void identify(std::string id) { customization_ = std::move(id); }

  void behave(TabCustomizationBehavior behavior) noexcept {
    behavior_ = behavior;
  }

  void label_with(Builder label) { label_ = std::move(label); }

  auto edit_traits() noexcept -> TabTraits& { return traits_; }

  void decorate(Decoration decoration) {
    decorations_.push_back(std::move(decoration));
  }

 private:
  std::string title_;
  std::string system_image_;
  std::shared_ptr<const composition::TagValue> value_;
  std::optional<TabRole> role_;
  std::string customization_;
  TabCustomizationBehavior behavior_{TabCustomizationBehavior::automatic};
  Builder content_;
  Builder label_;
  TabTraits traits_;
  std::vector<Decoration> decorations_;
  composition::ListRow::SwipeActions leading_;
  composition::ListRow::SwipeActions trailing_;
};

template <class Edit>
auto Tab::traits(Edit edit) && -> Tab {
  edit(this->edit<Content>().edit_traits());
  return std::move(*this);
}

class TabSection : public composition::Element {
 public:
  class Content;

  template <class... Tabs>
    requires(composition::ViewContent<Tabs> && ...)
  explicit TabSection(std::string title, Tabs&&... tabs)
      : TabSection(std::move(title), collect(std::forward<Tabs>(tabs)...)) {}

  TabSection(std::string title, std::vector<composition::Element> tabs);

  TabSection(std::vector<composition::Element> tabs,
             composition::Element header);

  auto customization_id(std::string id) && -> TabSection;

  auto default_section_expansion(
      TabSectionExpansion expansion) && -> TabSection;

  template <class Actions>
  auto section_actions(Actions actions) && -> TabSection {
    return std::move(*this).acting(
        std::make_shared<const Tab::Builder>([held = std::move(actions)] {
          return composition::Element{std::invoke(held)};
        }));
  }

  static auto content(composition::Element& element)
      -> std::optional<std::reference_wrapper<Content>>;

 private:
  auto acting(std::shared_ptr<const Tab::Builder> actions) && -> TabSection;
};

class TabSection::Content final : public composition::ElementContent {
 public:
  using Source = std::shared_ptr<composition::PlaceholderContentView::Source>;

  Content(std::string title, std::vector<composition::Element> tabs,
          Source header) noexcept
      : title_(std::move(title)),
        tabs_(std::move(tabs)),
        header_(std::move(header)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

  auto title() const noexcept -> const std::string& { return title_; }

  auto tabs() noexcept -> std::vector<composition::Element>& { return tabs_; }

  auto actions() const noexcept -> const std::shared_ptr<const Tab::Builder>& {
    return actions_;
  }

  void act(std::shared_ptr<const Tab::Builder> actions) noexcept {
    actions_ = std::move(actions);
  }

  auto header() const noexcept -> const Source& { return header_; }

  auto customization() const noexcept -> const std::string& {
    return customization_;
  }

  void identify(std::string id) { customization_ = std::move(id); }

  auto expansion() const noexcept -> TabSectionExpansion { return expansion_; }

  void expand(TabSectionExpansion expansion) noexcept {
    expansion_ = expansion;
  }

 private:
  std::string title_;
  std::vector<composition::Element> tabs_;
  std::shared_ptr<const Tab::Builder> actions_;
  Source header_;
  std::string customization_;
  TabSectionExpansion expansion_{TabSectionExpansion::automatic};
};

}
