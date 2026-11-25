#include <arisa/components/environment.hpp>
#include <arisa/components/user_interface_size_class.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/presentation/navigation_stack.hpp>
#include <arisa/structure/navigation_split_view.hpp>
#include <arisa/structure/navigation_view.hpp>

#include <utility>

namespace arisa::structure {

const composition::EnvironmentKey<NavigationViewStyle> NavigationViewStyle::key{
    "navigation_view_style", NavigationViewStyle::automatic};

NavigationViewStyle::NavigationViewStyle(DefaultNavigationViewStyle) noexcept
    : kind_(Kind::automatic) {}

NavigationViewStyle::NavigationViewStyle(StackNavigationViewStyle) noexcept
    : kind_(Kind::stack) {}

NavigationViewStyle::NavigationViewStyle(
    DoubleColumnNavigationViewStyle) noexcept
    : kind_(Kind::columns) {}

NavigationView::NavigationView(std::vector<composition::Element> children)
    : Element([&children] {
        std::vector<Content::Source> columns;
        columns.reserve(children.size());
        for (composition::Element& child : children) {
          columns.push_back(
              std::make_shared<composition::PlaceholderContentView::Source>(
                  std::move(child)));
        }
        return std::make_unique<Content>(std::move(columns));
      }()) {
  edit_modifiers().frame.max_width = composition::infinity;
  edit_modifiers().frame.max_height = composition::infinity;
}

auto NavigationView::Content::type() const noexcept
    -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto NavigationView::Content::shows_columns(NavigationViewStyle style,
                                            std::size_t count,
                                            bool compact) noexcept -> bool {
  switch (style.kind()) {
    case NavigationViewStyle::Kind::stack:
      return false;
    case NavigationViewStyle::Kind::columns:
      return count >= 2;
    case NavigationViewStyle::Kind::automatic:
      break;
  }
  return count >= 2 && !compact;
}

auto NavigationView::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  std::vector<composition::Element> produced;
  if (columns_.empty()) {
    produced.push_back(components::ZStack());
    return produced;
  }
  const bool compact =
      context.environment(components::Environment::horizontal_size_class) ==
      components::UserInterfaceSizeClass::compact;
  const auto column = [](const Source& source) {
    return composition::Element{composition::PlaceholderContentView(source)};
  };
  if (!shows_columns(context.environment(NavigationViewStyle::key),
                     columns_.size(), compact)) {
    produced.push_back(presentation::NavigationStack([root = columns_.front()] {
      return composition::Element{composition::PlaceholderContentView(root)};
    }));
    return produced;
  }
  if (columns_.size() >= 3) {
    produced.push_back(NavigationSplitView(
        column(columns_[0]), column(columns_[1]), column(columns_.back())));
  } else {
    produced.push_back(
        NavigationSplitView(column(columns_[0]), column(columns_.back())));
  }
  return produced;
}

}
