#pragma once

#include <arisa/composition/alignment.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/focus_state.hpp>
#include <arisa/composition/navigation_path.hpp>
#include <arisa/composition/navigation_split_view_column.hpp>
#include <arisa/composition/navigation_split_view_column_width.hpp>
#include <arisa/composition/navigation_split_view_style.hpp>
#include <arisa/composition/navigation_split_view_visibility.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/tag_key.hpp>
#include <arisa/presentation/navigation_stack.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/structure/detail_presenter.hpp>
#include <arisa/structure/sidebar_selection.hpp>
#include <arisa/theme/colour.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::structure {

struct SidebarCollapse {
  static const composition::EnvironmentKey<SidebarCollapse> key;

  bool detail_only{true};
  bool icons_only{false};

  friend constexpr auto operator==(const SidebarCollapse&,
                                   const SidebarCollapse&) -> bool = default;
};

class NavigationSplitViewStyleConfiguration {
 public:
  using Source = std::shared_ptr<composition::PlaceholderContentView::Source>;

  NavigationSplitViewStyleConfiguration(
      Source body,
      composition::NavigationSplitViewVisibility visibility) noexcept
      : body_(std::move(body)), visibility_(visibility) {}

  auto body() const -> composition::Element {
    return composition::PlaceholderContentView(body_);
  }

  auto visibility() const noexcept
      -> composition::NavigationSplitViewVisibility {
    return visibility_;
  }

 private:
  Source body_;
  composition::NavigationSplitViewVisibility visibility_;
};

class NavigationSplitView : public composition::Element {
 public:
  class Content;

  using CustomStyle = std::shared_ptr<const std::function<composition::Element(
      const NavigationSplitViewStyleConfiguration&)>>;

  static const composition::EnvironmentKey<CustomStyle> custom_style;
  class Column;
  class DetailPath;

  using Visibility = composition::NavigationSplitViewVisibility;

  struct Binding {
    std::function<Visibility()> get;
    std::function<void(Visibility)> set;
  };

  static constexpr std::string_view sidebar_id = "arisa.split.sidebar";
  static constexpr std::string_view detail_id = "arisa.split.detail";
  static constexpr std::string_view content_id = "arisa.split.content";
  static constexpr float content_width = 200.0F;
  static constexpr std::string_view page_space = "arisa.split.page";
  static constexpr std::string_view toggle_label = "Sidebar";
  static constexpr provider::Button toggle_button = provider::Button::minus;
  static constexpr composition::StateKey<std::optional<Visibility>> chosen{
      "navigation-split-view-visibility", std::nullopt};
  static constexpr composition::StateKey<composition::NavigationPath>
      linked_detail{"navigation-split-view-linked-detail"};
  static const composition::FocusState<composition::TagKey> sidebar_focus;
  static const composition::StateKey<std::shared_ptr<SidebarSelection::Reader>>
      reader;

  NavigationSplitView(composition::Element sidebar,
                      composition::Element detail);

  NavigationSplitView(composition::Binding<Visibility> column_visibility,
                      composition::Element sidebar,
                      composition::Element detail);

  NavigationSplitView(std::optional<Binding> column_visibility,
                      composition::Element sidebar,
                      composition::Element detail);

  NavigationSplitView(composition::Element sidebar,
                      composition::Element content,
                      composition::Element detail);

  NavigationSplitView(composition::Binding<Visibility> column_visibility,
                      composition::Element sidebar,
                      composition::Element content,
                      composition::Element detail);

  NavigationSplitView(std::optional<Binding> column_visibility,
                      composition::Element sidebar,
                      std::optional<composition::Element> content,
                      composition::Element detail);

  NavigationSplitView(
      composition::Binding<composition::NavigationSplitViewColumn>
          preferred_compact_column,
      composition::Element sidebar, composition::Element detail);

  NavigationSplitView(
      composition::Binding<Visibility> column_visibility,
      composition::Binding<composition::NavigationSplitViewColumn>
          preferred_compact_column,
      composition::Element sidebar, composition::Element detail);

  NavigationSplitView(
      composition::Binding<composition::NavigationSplitViewColumn>
          preferred_compact_column,
      composition::Element sidebar, composition::Element content,
      composition::Element detail);

  NavigationSplitView(
      composition::Binding<Visibility> column_visibility,
      composition::Binding<composition::NavigationSplitViewColumn>
          preferred_compact_column,
      composition::Element sidebar, composition::Element content,
      composition::Element detail);

  static auto compact_visibility(composition::NavigationSplitViewColumn column,
                                 bool three) noexcept -> Visibility;

  static auto compact_column(Visibility visibility, bool three) noexcept
      -> composition::NavigationSplitViewColumn;

  static constexpr float separator_width = 1.0F;

  static auto resolved(Visibility visibility, SidebarCollapse forms,
                       bool fixed) noexcept -> Visibility;

  static auto columns(Visibility visibility, bool compact,
                      composition::NavigationSplitViewStyle style) noexcept
      -> Visibility;

  static auto next(Visibility visibility, SidebarCollapse forms) noexcept
      -> Visibility;

  static auto next_column(Visibility visibility) noexcept -> Visibility;

  static auto toggle(const Binding& binding, SidebarCollapse forms) -> void;

 private:
  auto preferring(composition::Binding<composition::NavigationSplitViewColumn>
                      column) && -> NavigationSplitView;
};

class NavigationSplitView::DetailPath final
    : public presentation::NavigationStack::Path {
 public:
  explicit DetailPath(
      composition::StateHandle<composition::NavigationPath> shown) noexcept
      : shown_(std::move(shown)) {}

  auto entries() const
      -> std::vector<composition::NavigationPath::Entry> override;

  void truncate(std::size_t count) const override;

  auto push(const composition::NavigationPath::Entry& entry) const
      -> bool override;

  auto identity() const noexcept -> composition::Identity override {
    return shown_.identity();
  }

 private:
  composition::StateHandle<composition::NavigationPath> shown_;
};

class NavigationSplitView::Column : public composition::Element {
 public:
  class Content;

  using Width = std::optional<composition::NavigationSplitViewColumnWidth>;

  Column(composition::Element content, composition::StateHandle<Width> width,
         composition::Alignment alignment);
};

class NavigationSplitView::Column::Content final
    : public composition::ElementContent {
 public:
  explicit Content(composition::StateHandle<Width> declared) noexcept
      : declared_(std::move(declared)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

  auto isolates(composition::Identity preference) const noexcept
      -> bool override;

  auto reads_preferences() const noexcept -> bool override { return true; }

  void resolve(composition::Context& context,
               const composition::Node& node) override;

  void hold(std::shared_ptr<composition::PlaceholderContentView::Source>
                content) noexcept {
    content_ = std::move(content);
  }

 private:
  composition::StateHandle<Width> declared_;
  std::shared_ptr<composition::PlaceholderContentView::Source> content_;
};

class NavigationSplitView::Content final : public composition::ElementContent {
 public:
  using Source = std::shared_ptr<composition::PlaceholderContentView::Source>;

  static constexpr composition::StateKey<Column::Width> sidebar_width{
      "navigation-split-view-sidebar-width", std::nullopt};
  static constexpr composition::StateKey<Column::Width> content_width{
      "navigation-split-view-content-width", std::nullopt};
  static constexpr composition::StateKey<Column::Width> detail_width{
      "navigation-split-view-detail-width", std::nullopt};
  static constexpr composition::StateKey<std::optional<float>> sidebar_dragged{
      "navigation-split-view-sidebar-dragged", std::nullopt};
  static constexpr composition::StateKey<std::optional<float>> sidebar_anchor{
      "navigation-split-view-sidebar-anchor", std::nullopt};
  static constexpr composition::StateKey<std::optional<float>> content_dragged{
      "navigation-split-view-content-dragged", std::nullopt};
  static constexpr composition::StateKey<std::optional<float>> content_anchor{
      "navigation-split-view-content-anchor", std::nullopt};

  Content(std::optional<Binding> binding, Source sidebar, Source content,
          Source detail) noexcept
      : binding_(std::move(binding)),
        sidebar_(std::move(sidebar)),
        content_(std::move(content)),
        detail_(std::move(detail)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

  auto hosts_sidebar() const noexcept -> bool override { return true; }

  void prefer(
      composition::Binding<composition::NavigationSplitViewColumn> column) {
    compact_ = std::move(column);
  }

 private:
  auto binding(composition::Context& context) const -> Binding;

  auto pages(const DetailPresenter& presenter,
             const composition::NavigationPath& linked) const
      -> composition::Element;

  static auto below_header(composition::Element content, theme::Colour fade,
                           float depth) -> composition::Element;

  auto middle(composition::Context& context, float width, bool hidden,
              bool floating, composition::Element handle) const
      -> composition::Element;

  static auto chosen_width(const Column::Width& declared,
                           const std::optional<float>& dragged)
      -> std::optional<float>;

  static auto handle(composition::Context& context,
                     const Column::Width& declared,
                     const composition::StateKey<std::optional<float>>& dragged,
                     const composition::StateKey<std::optional<float>>& anchor,
                     float current) -> composition::Element;

  std::optional<Binding> binding_;
  Source sidebar_;
  Source content_;
  Source detail_;
  std::optional<composition::Binding<composition::NavigationSplitViewColumn>>
      compact_;
};

}

namespace arisa::composition {

template <class Style>
  requires(!std::convertible_to<Style, NavigationSplitViewStyle>)
auto Element::navigation_split_view_style(Style style) && -> Element {
  return std::move(*this).environment(
      structure::NavigationSplitView::custom_style,
      std::make_shared<const std::function<Element(
          const structure::NavigationSplitViewStyleConfiguration&)>>(
          [held = std::move(style)](
              const structure::NavigationSplitViewStyleConfiguration& made) {
            return Element{held.make_body(made)};
          }));
}

template <class Forms>
auto Element::sidebar_collapse(Forms forms) && -> Element {
  return std::move(*this).environment(structure::SidebarCollapse::key,
                                      structure::SidebarCollapse{forms});
}

}
