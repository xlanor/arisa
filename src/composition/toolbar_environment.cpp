#include <arisa/composition/context.hpp>
#include <arisa/composition/toolbar_environment.hpp>

#include <utility>

namespace arisa::composition {

const EnvironmentKey<ToolbarTitleAlignment> ToolbarEnvironment::title_alignment{
    "toolbar_title_alignment", ToolbarTitleAlignment::automatic};

const EnvironmentKey<std::optional<VerticalEdge>>
    ToolbarEnvironment::vertical_edge{"toolbar_vertical_edge", std::nullopt};

const EnvironmentKey<bool> ToolbarEnvironment::customizing{
    "toolbar_customizing", false};

const EnvironmentKey<bool> ToolbarEnvironment::compressed{"toolbar_compressed",
                                                          false};

ToolbarEnvironment::HiddenItem::HiddenItem(Element item)
    : Element(std::make_unique<Content>(
          std::make_shared<PlaceholderContentView::Source>(std::move(item)))) {}

auto ToolbarEnvironment::HiddenItem::Content::type() const noexcept
    -> ElementType {
  return ElementType::of<Content>();
}

auto ToolbarEnvironment::HiddenItem::Content::body(Context& context) const
    -> std::vector<Element> {
  std::vector<Element> shown;
  if (context.environment(customizing)) {
    shown.push_back(PlaceholderContentView(item_).opacity(0.5F));
  }
  return shown;
}

}
