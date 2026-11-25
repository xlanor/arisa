#include <arisa/structure/tab.hpp>

#include <utility>
#include <vector>

namespace arisa::structure {

Tab::Tab(std::string title, std::string system_image, Builder content)
    : Tab(std::move(title), std::move(system_image), nullptr, std::nullopt,
          std::move(content)) {}

Tab::Tab(std::string title, std::string system_image, TabRole role,
         Builder content)
    : Tab(std::move(title), std::move(system_image), nullptr,
          std::optional<TabRole>{role}, std::move(content)) {}

Tab::Tab(std::string title, TabImage image, Builder content)
    : Tab(std::move(title), std::move(image), nullptr, std::nullopt,
          std::move(content)) {}

Tab::Tab(std::string title, TabImage image, TabRole role, Builder content)
    : Tab(std::move(title), std::move(image), nullptr,
          std::optional<TabRole>{role}, std::move(content)) {}

Tab::Tab(TabRole role, Builder content)
    : Tab(role, std::shared_ptr<const composition::TagValue>{},
          std::move(content)) {}

Tab::Tab(Builder content)
    : Tab(std::shared_ptr<const composition::TagValue>{}, std::nullopt,
          std::move(content), Builder{}) {}

Tab::Tab(Builder content, Builder label)
    : Tab(std::shared_ptr<const composition::TagValue>{}, std::nullopt,
          std::move(content), std::move(label)) {}

Tab::Tab(std::string title, std::string system_image,
         std::shared_ptr<const composition::TagValue> value,
         std::optional<TabRole> role, Builder content)
    : Element(std::make_unique<Content>(
          std::move(title), std::move(system_image), std::move(value), role,
          std::move(content))) {}

Tab::Tab(std::string title, TabImage image,
         std::shared_ptr<const composition::TagValue> value,
         std::optional<TabRole> role, Builder content)
    : Element(std::make_unique<Content>(std::move(title), image.name(),
                                        std::move(value), role,
                                        std::move(content))) {
  edit<Content>().edit_traits().icon = TabTraits::Icon::asset;
}

Tab::Tab(TabRole role, std::shared_ptr<const composition::TagValue> value,
         Builder content)
    : Tab(role == TabRole::search ? std::string{search_title} : std::string{},
          role == TabRole::search ? std::string{search_image} : std::string{},
          std::move(value), std::optional<TabRole>{role}, std::move(content)) {
  if (role != TabRole::search) {
    edit<Content>().edit_traits().icon = TabTraits::Icon::none;
  }
}

Tab::Tab(std::shared_ptr<const composition::TagValue> value,
         std::optional<TabRole> role, Builder content, Builder label)
    : Tab(std::string{}, std::string{}, std::move(value), role,
          std::move(content)) {
  edit<Content>().edit_traits().icon = TabTraits::Icon::none;
  edit<Content>().label_with(std::move(label));
}

auto Tab::customization_id(std::string id) && -> Tab {
  edit<Content>().identify(std::move(id));
  return std::move(*this);
}

auto Tab::customization_behavior(TabCustomizationBehavior behavior) && -> Tab {
  edit<Content>().behave(behavior);
  return std::move(*this);
}

auto Tab::customization_behavior(TabCustomizationBehavior behavior,
                                 AdaptableTabBarPlacement placement) && -> Tab {
  return std::move(*this).traits([behavior, placement](TabTraits& held) {
    if (placement != AdaptableTabBarPlacement::tab_bar) {
      held.sidebar_behavior = behavior;
    }
    if (placement != AdaptableTabBarPlacement::sidebar) {
      held.tab_bar_behavior = behavior;
    }
  });
}

auto Tab::default_visibility(composition::Visibility visibility,
                             AdaptableTabBarPlacement placement) && -> Tab {
  return std::move(*this).traits([visibility, placement](TabTraits& held) {
    if (placement != AdaptableTabBarPlacement::tab_bar) {
      held.sidebar_default = visibility;
    }
    if (placement != AdaptableTabBarPlacement::sidebar) {
      held.tab_bar_default = visibility;
    }
  });
}

auto Tab::tab_placement(TabPlacement placement) && -> Tab {
  return std::move(*this).traits(
      [placement](TabTraits& held) { held.placement = placement; });
}

auto Tab::disabled(bool is_disabled) && -> Tab {
  return std::move(*this).traits(
      [is_disabled](TabTraits& held) { held.disabled = is_disabled; });
}

auto Tab::hidden(bool is_hidden) && -> Tab {
  return std::move(*this).traits(
      [is_hidden](TabTraits& held) { held.hidden = is_hidden; });
}

auto Tab::swipe_actions(
    composition::HorizontalEdge edge, bool allows_full_swipe,
    std::function<composition::Element()> content) && -> Tab {
  edit<Content>().swipe_with(edge, composition::ListRow::SwipeActions{
                                       .content = std::move(content),
                                       .allows_full_swipe = allows_full_swipe});
  return std::move(*this);
}

auto Tab::swipe_actions(
    std::function<composition::Element()> content) && -> Tab {
  return std::move(*this).swipe_actions(composition::HorizontalEdge::trailing,
                                        true, std::move(content));
}

auto Tab::spring_loading_behavior(
    composition::SpringLoadingBehavior behavior) && -> Tab {
  return std::move(*this).traits(
      [behavior](TabTraits& held) { held.spring_loading = behavior; });
}

auto Tab::help(std::string text) && -> Tab {
  return std::move(*this).traits(
      [&text](TabTraits& held) { held.help = std::move(text); });
}

auto Tab::accessibility_label(std::string label, bool is_enabled) && -> Tab {
  return std::move(*this).traits([&label, is_enabled](TabTraits& held) {
    held.accessibility_label =
        is_enabled ? std::optional<std::string>{std::move(label)}
                   : std::nullopt;
  });
}

auto Tab::accessibility_hint(std::string hint, bool is_enabled) && -> Tab {
  return std::move(*this).traits([&hint, is_enabled](TabTraits& held) {
    held.accessibility_hint =
        is_enabled ? std::optional<std::string>{std::move(hint)} : std::nullopt;
  });
}

auto Tab::accessibility_value(std::string value, bool is_enabled) && -> Tab {
  return std::move(*this).traits([&value, is_enabled](TabTraits& held) {
    held.accessibility_value =
        is_enabled ? std::optional<std::string>{std::move(value)}
                   : std::nullopt;
  });
}

auto Tab::accessibility_identifier(std::string identifier,
                                   bool is_enabled) && -> Tab {
  return std::move(*this).traits([&identifier, is_enabled](TabTraits& held) {
    held.accessibility_identifier =
        is_enabled ? std::optional<std::string>{std::move(identifier)}
                   : std::nullopt;
  });
}

auto Tab::accessibility_input_labels(
    std::vector<composition::AccessibilityText> input_labels,
    bool is_enabled) && -> Tab {
  return std::move(*this).traits([&input_labels, is_enabled](TabTraits& held) {
    held.accessibility_input_labels =
        is_enabled ? std::move(input_labels)
                   : std::vector<composition::AccessibilityText>{};
  });
}

auto Tab::decorate_with(Decoration decoration) && -> Tab {
  edit<Content>().decorate(std::move(decoration));
  return std::move(*this);
}

auto Tab::content(const composition::Element& element)
    -> std::optional<std::reference_wrapper<const Content>> {
  if (element.type() != composition::ElementType::of<Content>()) {
    return std::nullopt;
  }
  return std::cref(static_cast<const Content&>(element.content()));
}

auto Tab::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto Tab::Content::body(composition::Context&) const
    -> std::vector<composition::Element> {
  std::vector<composition::Element> produced;
  if (content_) {
    produced.push_back(content_());
  }
  return produced;
}

TabSection::TabSection(std::string title,
                       std::vector<composition::Element> tabs)
    : Element(std::make_unique<Content>(std::move(title), std::move(tabs),
                                        nullptr)) {}

TabSection::TabSection(std::vector<composition::Element> tabs,
                       composition::Element header)
    : Element(std::make_unique<Content>(
          std::string{}, std::move(tabs),
          std::make_shared<composition::PlaceholderContentView::Source>(
              std::move(header)))) {}

auto TabSection::customization_id(std::string id) && -> TabSection {
  edit<Content>().identify(std::move(id));
  return std::move(*this);
}

auto TabSection::default_section_expansion(
    TabSectionExpansion expansion) && -> TabSection {
  edit<Content>().expand(expansion);
  return std::move(*this);
}

auto TabSection::acting(
    std::shared_ptr<const Tab::Builder> actions) && -> TabSection {
  edit<Content>().act(std::move(actions));
  return std::move(*this);
}

auto TabSection::content(composition::Element& element)
    -> std::optional<std::reference_wrapper<Content>> {
  if (element.type() != composition::ElementType::of<Content>()) {
    return std::nullopt;
  }
  return std::ref(static_cast<Content&>(content_of(element)));
}

auto TabSection::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto TabSection::Content::body(composition::Context&) const
    -> std::vector<composition::Element> {
  return {};
}

}
