#pragma once

#include <arisa/composition/binding.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/identity.hpp>
#include <arisa/composition/namespace.hpp>
#include <arisa/composition/navigation_chrome.hpp>
#include <arisa/composition/navigation_declarations.hpp>
#include <arisa/composition/navigation_path.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/presentation_background.hpp>
#include <arisa/composition/scroll_geometry_observer.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/toolbar_customization.hpp>
#include <arisa/composition/toolbar_minimization.hpp>
#include <arisa/presentation/toolbar_layout.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::presentation {

class NavigationStack : public composition::Element {
  template <class Values>
  struct Stacked {};

  template <class T, class Allocator>
    requires std::equality_comparable<T> && std::copy_constructible<T>
  struct Stacked<std::vector<T, Allocator>> {
    using Value = T;
  };

 public:
  using Root = std::move_only_function<composition::Element() const>;

  class Path;
  class Navigator;
  class ViewDestination;
  class Content;
  struct History;
  struct Drag;

  template <class T>
  class VectorPath;

  class ErasedPath;
  class StackedPath;

  static constexpr float edge_width = 20.0F;
  static constexpr float commit_distance = 140.0F;
  static constexpr float commit_prediction = 280.0F;
  static constexpr float parallax = 60.0F;

  static const composition::EnvironmentKey<Navigator> navigator;
  static const composition::EnvironmentKey<Navigator> detail_navigator;

  static constexpr float large_title_collapse = 8.0F;
  static constexpr float large_title_travel = 41.0F;
  static constexpr float large_title_stretch = 60.0F;
  static constexpr float large_title_growth = 0.1F;
  static constexpr std::size_t collapsed_limit = 16;
  static constexpr std::string_view back_label = "Back";
  static constexpr std::string_view large_title_id = "arisa.navigation.large";
  static constexpr std::string_view bottom_bar_id = "arisa.navigation.bottom";
  static constexpr std::string_view keyboard_bar_id =
      "arisa.navigation.keyboard";
  static constexpr std::string_view bar_id = "arisa.navigation.bar";
  static constexpr float minimized_height = 30.0F;
  static constexpr float minimize_threshold = 4.0F;
  static constexpr float scroll_edge = 8.0F;

  explicit NavigationStack(Root root);

  static auto top_chrome(const composition::Node& node)
      -> std::optional<composition::NavigationChrome>;

  NavigationStack(composition::Binding<composition::NavigationPath> path,
                  Root root);

  template <composition::BindingSource Source>
    requires requires {
      typename Stacked<composition::BoundValue<Source>>::Value;
    }
  NavigationStack(Source path, Root root)
      : NavigationStack(
            std::make_shared<const VectorPath<
                typename Stacked<composition::BoundValue<Source>>::Value>>(
                composition::Binding<composition::BoundValue<Source>>{
                    std::move(path)}),
            std::move(root)) {}

 private:
  class Screen;

  NavigationStack(std::shared_ptr<const Path> path, Root root);
};

class NavigationStack::Path {
 public:
  Path() = default;
  Path(const Path&) = delete;
  auto operator=(const Path&) -> Path& = delete;
  Path(Path&&) = delete;
  auto operator=(Path&&) -> Path& = delete;
  virtual ~Path() = default;

  virtual auto entries() const
      -> std::vector<composition::NavigationPath::Entry> = 0;

  virtual void truncate(std::size_t count) const = 0;

  virtual auto push(const composition::NavigationPath::Entry& entry) const
      -> bool = 0;

  virtual auto identity() const noexcept -> composition::Identity = 0;
};

class NavigationStack::ErasedPath final : public NavigationStack::Path {
 public:
  explicit ErasedPath(
      composition::Binding<composition::NavigationPath> path) noexcept
      : path_(std::move(path)) {}

  auto entries() const
      -> std::vector<composition::NavigationPath::Entry> override;

  void truncate(std::size_t count) const override;

  auto push(const composition::NavigationPath::Entry& entry) const
      -> bool override;

  auto identity() const noexcept -> composition::Identity override {
    return path_.identity();
  }

 private:
  composition::Binding<composition::NavigationPath> path_;
};

template <class T>
class NavigationStack::VectorPath final : public NavigationStack::Path {
 public:
  explicit VectorPath(composition::Binding<std::vector<T>> path) noexcept
      : path_(std::move(path)) {}

  auto entries() const
      -> std::vector<composition::NavigationPath::Entry> override {
    std::vector<composition::NavigationPath::Entry> found;
    const std::vector<T> values = path_.get();
    found.reserve(values.size());
    for (const T& value : values) {
      found.push_back(composition::NavigationPath::Entry::of(value));
    }
    return found;
  }

  void truncate(std::size_t count) const override {
    if (count >= path_.get().size()) {
      return;
    }

    path_.update([count](std::vector<T>& values) { values.resize(count); });
  }

  auto push(const composition::NavigationPath::Entry& entry) const
      -> bool override {
    const T* value = entry.get<T>();
    if (value == nullptr) {
      return false;
    }
    path_.update([value](std::vector<T>& values) { values.push_back(*value); });
    return true;
  }

  auto identity() const noexcept -> composition::Identity override {
    return path_.identity();
  }

 private:
  composition::Binding<std::vector<T>> path_;
};

class NavigationStack::StackedPath final : public NavigationStack::Path {
 public:
  StackedPath(std::shared_ptr<const Path> base,
              composition::StateHandle<composition::NavigationPath> views)
      : base_(std::move(base)), views_(std::move(views)) {}

  auto entries() const
      -> std::vector<composition::NavigationPath::Entry> override;

  void truncate(std::size_t count) const override;

  auto push(const composition::NavigationPath::Entry& entry) const
      -> bool override;

  auto identity() const noexcept -> composition::Identity override {
    return base_->identity();
  }

 private:
  std::shared_ptr<const Path> base_;
  composition::StateHandle<composition::NavigationPath> views_;
};

class NavigationStack::ViewDestination {
 public:
  using Build = std::function<composition::Element()>;

  explicit ViewDestination(Build build)
      : build_(std::make_shared<const Build>(std::move(build))) {}

  auto build() const -> composition::Element { return (*build_)(); }

  auto identity() const noexcept -> composition::Identity {
    return composition::Identity{build_.get()};
  }

  friend auto operator==(const ViewDestination&, const ViewDestination&)
      -> bool = default;

 private:
  std::shared_ptr<const Build> build_;
};

class NavigationStack::Navigator {
 public:
  Navigator() = default;

  explicit Navigator(std::shared_ptr<const Path> path) noexcept
      : path_(std::move(path)) {}

  auto push(const composition::NavigationPath::Entry& entry) const -> bool {
    return path_ != nullptr && path_->push(entry);
  }

  auto push(ViewDestination::Build build) const -> bool {
    return push(composition::NavigationPath::Entry::of(
        ViewDestination{std::move(build)}));
  }

  auto available() const noexcept -> bool { return path_ != nullptr; }

  auto contains(const composition::NavigationPath::Entry& entry) const -> bool;

  auto remove(const composition::NavigationPath::Entry& entry) const -> bool;

  auto contains(composition::Identity view) const -> bool;

  auto remove(composition::Identity view) const -> bool;

  friend auto operator==(const Navigator& left, const Navigator& right)
      -> bool {
    if (left.path_ == nullptr || right.path_ == nullptr) {
      return left.path_ == right.path_;
    }
    return left.path_->identity() == right.path_->identity();
  }

 private:
  std::shared_ptr<const Path> path_;
};

struct NavigationStack::History {
  std::vector<composition::NavigationPath::Entry> entries;
  std::vector<std::uint64_t> ids;
  std::uint64_t next{1};
};

struct NavigationStack::Drag {
  float offset{0.0F};
  std::uint64_t screen{0};

  friend auto operator==(const Drag&, const Drag&) -> bool = default;
};

class NavigationStack::Content final : public composition::ElementContent {
 public:
  static constexpr composition::StateKey<composition::NavigationPath>
      internal_path{"navigation-path"};
  static constexpr composition::StateKey<Drag> dragging{"navigation-drag"};
  static constexpr composition::StateKey<composition::NavigationPath> views{
      "navigation-views"};
  static const composition::StateKey<History> history;
  static constexpr composition::Namespace screens{"navigation-screens"};

  Content(std::shared_ptr<const Path> path, Root root) noexcept
      : path_(std::move(path)), root_(std::move(root)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override;

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

  auto isolates(composition::Identity preference) const noexcept
      -> bool override;

  auto reads_preferences() const noexcept -> bool override { return true; }

  void resolve(composition::Context& context,
               const composition::Node& node) override;

 private:
  struct Declared {
    std::uint64_t page{0};
    std::optional<std::string> title;
    composition::NavigationDeclarations::Toolbar toolbar;
    std::optional<composition::PresentationBackground> background;
    composition::NavigationChrome chrome;

    friend auto operator==(const Declared&, const Declared&) -> bool = default;
  };

  static const composition::StateKey<std::vector<Declared>> resolved;
  static const composition::StateKey<std::vector<Declared>> displayed_key;
  static constexpr composition::StateKey<std::uint64_t> revision{
      "navigation-revision", 0};
  static const composition::StateKey<
      composition::NavigationDeclarations::Destinations>
      found;
  static constexpr composition::StateKey<bool> keyboard_showing{
      "navigation-keyboard", false};
  static constexpr composition::StateKey<bool> unresolved{
      "navigation-unresolved", false};
  static const composition::StateKey<std::vector<std::uint64_t>> collapsed;
  static const composition::StateKey<
      std::map<std::uint64_t, std::shared_ptr<float>>>
      title_offsets;
  static const composition::StateKey<std::vector<std::uint64_t>>
      minimized_pages;
  static const composition::StateKey<composition::ToolbarCustomization>
      customized;

  static auto page_key(std::size_t level, const History& recorded)
      -> composition::ElementKey;

  struct Placement {
    std::size_t level{0};
    std::size_t depth{0};
    std::uint64_t id{0};
    float drag{0.0F};
    bool changed{false};
    bool floating{false};
    std::optional<composition::PresentationBackground> background;
    std::optional<std::string> title;
    std::vector<std::shared_ptr<const composition::ToolbarItem>> toolbar;
    composition::NavigationChrome chrome;
    bool collapsed{false};
    ToolbarLayout::Metrics metrics;
    composition::Namespace::ID scope;
    std::shared_ptr<const Path> path;
    composition::StateHandle<Drag> dragging;
    composition::StateHandle<std::vector<std::uint64_t>> collapsing;
    std::optional<composition::NavigationTransition> transition;
    std::shared_ptr<float> travelled;
    std::optional<ToolbarLayout::Customization> customization;
    bool vertical{false};
    bool keyboard{false};
    bool minimized{false};
    bool keeps_slot{false};
    bool mirrored{false};
    composition::ToolbarMinimizationBehavior minimization{
        composition::ToolbarMinimizationBehavior::never};
    composition::ToolbarMinimizationRestoration restoration{
        composition::ToolbarMinimizationRestoration::automatic};
    composition::StateHandle<std::vector<std::uint64_t>> minimizing;

    auto layout() const -> ToolbarLayout;
  };

  static auto identify(
      History& history,
      const std::vector<composition::NavigationPath::Entry>& entries) -> bool;

  static auto screen(composition::Element content, const Placement& placement)
      -> composition::Element;

  static auto bar(const Placement& placement)
      -> std::optional<composition::Element>;

  static auto edge(const Placement& placement) -> composition::Element;

  static auto large_title(const Placement& placement)
      -> std::optional<composition::Element>;

  static auto keyboard_bar(const Placement& placement)
      -> std::optional<composition::Element>;

  static auto bottom_bar(const Placement& placement)
      -> std::optional<composition::Element>;

  static auto vertical_bar(const Placement& placement)
      -> std::optional<composition::Element>;

  static auto minimizes(
      const composition::NavigationChrome& chrome,
      const composition::NavigationDeclarations::Toolbar& toolbar)
      -> composition::ToolbarMinimizationBehavior;

  class TitleTracker;

  static auto collapse(const Placement& placement)
      -> std::shared_ptr<composition::ScrollGeometryObserver>;

  static auto pop(std::shared_ptr<const Path> path, std::size_t level)
      -> std::function<void()>;

  static auto destination(
      const std::vector<std::shared_ptr<
          const composition::NavigationDestination>>& destinations,
      const composition::NavigationPath::Entry& entry)
      -> std::optional<composition::Element>;

  std::shared_ptr<const Path> path_;
  Root root_;
};

}

#include <arisa/presentation/presented_destination.hpp>
