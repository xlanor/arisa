#include <arisa/components/button.hpp>
#include <arisa/components/command_mapping.hpp>
#include <arisa/components/divider.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/popover_attachment_anchor.hpp>
#include <arisa/controls/context_menu.hpp>
#include <arisa/controls/menu.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/theme/metrics.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/shape.hpp>

#include <string>
#include <utility>

namespace arisa::controls {

const composition::EnvironmentKey<bool> ContextMenu::hosted{
    "context_menu_hosted", false};

const composition::EnvironmentKey<
    std::shared_ptr<const ContextMenu::ForSelection>>
    ContextMenu::selection{"context_menu_for_selection", nullptr};

auto ContextMenu::ForSelection::items(Tags tags) const -> Builder {
  return [menu = menu_, tags = std::move(tags)] { return menu(tags); };
}

void ContextMenu::ForSelection::perform(const Tags& tags) const {
  if (primary_) {
    primary_(tags);
  }
}

auto ContextMenu::relabel(
    composition::Element menu,
    const std::function<composition::Element(composition::Element)>& change)
    -> composition::Element {
  if (menu.type() != composition::ElementType::of<Content>()) {
    return menu;
  }
  auto& held = static_cast<Content&>(content_of(menu));
  if (std::optional<composition::Element> label = held.take_label()) {
    held.hold(std::make_shared<composition::PlaceholderContentView::Source>(
        change(std::move(*label))));
  }
  return menu;
}

ContextMenu::ContextMenu(composition::Element content, Builder items,
                         std::optional<Builder> preview)
    : Element(std::make_unique<Content>(
          nullptr, std::move(items), std::move(preview), wrapped_of(content))) {
  inherit_identity(*this, content);
  edit<Content>().set_preview_shape(
      content.interactions().context_menu_preview_shape);
  edit<Content>().hold(
      std::make_shared<composition::PlaceholderContentView::Source>(
          std::move(content)));
}

auto ContextMenu::wrapped_of(composition::Element& content) -> Wrapped {
  Wrapped wrapped{.activatable = content.content().activatable(),
                  .button = false,
                  .discloses = false};
  if (const auto button = components::Button::content(content)) {
    wrapped.button = true;
    wrapped.discloses = button->get().discloses();
  }
  return wrapped;
}

auto ContextMenu::content(const composition::Element& element)
    -> std::optional<std::reference_wrapper<const Content>> {
  if (element.type() != composition::ElementType::of<Content>()) {
    return std::nullopt;
  }
  return std::cref(static_cast<const Content&>(element.content()));
}

auto ContextMenu::panel(composition::Element items,
                        std::optional<composition::Element> preview,
                        composition::Binding<bool> shown,
                        const std::optional<graphics::AnyShape>& preview_shape)
    -> composition::Element {
  composition::Element menu = Menu::items(std::move(items), std::move(shown));
  if (!preview.has_value()) {
    return menu;
  }
  return components::VStack(
      components::VStackOptions{
          .alignment = components::HorizontalAlignment::leading,
          .spacing = theme::space::none},
      std::move(*preview)
          .frame({.width = Menu::panel_width,
                  .alignment = composition::Alignment::center})
          .clip_shape(preview_shape.value_or(graphics::AnyShape{
              theme::Shape::rounded(theme::radius::control, true)}))
          .padding(theme::Insets{.top = theme::metric::menu_vertical_padding,
                                 .leading = theme::space::none,
                                 .bottom = theme::space::none,
                                 .trailing = theme::space::none}),
      std::move(menu));
}

auto ContextMenu::with_clipboard(
    composition::Element target,
    const std::function<void(ClipboardCommands&)>& edit)
    -> composition::Element {
  if (target.type() != composition::ElementType::of<Content>()) {
    target = ContextMenu(std::move(target), Builder{}, std::nullopt);
  }
  edit(static_cast<Content&>(content_of(target)).clipboard());
  return target;
}

auto ContextMenu::with_items(composition::Element target, Builder items,
                             std::optional<Builder> preview)
    -> composition::Element {
  if (target.type() != composition::ElementType::of<Content>()) {
    return ContextMenu(std::move(target), std::move(items), std::move(preview));
  }
  static_cast<Content&>(content_of(target))
      .list(std::move(items), std::move(preview));
  return target;
}

auto ContextMenu::Content::clipboard() -> ClipboardCommands& {
  if (clipboard_ == nullptr) {
    clipboard_ = std::make_shared<ClipboardCommands>();
  }
  return *clipboard_;
}

auto ContextMenu::Content::items() const -> Builder {
  if (clipboard_ == nullptr || clipboard_->empty()) {
    return items_;
  }
  return [clipboard = clipboard_, items = items_] {
    std::vector<composition::Element> rows = clipboard->items();
    if (items) {
      rows.push_back(components::Divider());
      for (composition::Element& member : components::Group::members(items())) {
        rows.push_back(std::move(member));
      }
    }
    return composition::Element{components::Group(std::move(rows))};
  };
}

auto ContextMenu::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto ContextMenu::attach(composition::Element target,
                         composition::Binding<bool> shown, Builder items,
                         std::optional<Builder> preview,
                         std::optional<graphics::AnyShape> preview_shape)
    -> composition::Element {
  const auto present = [shown] { shown.set(true); };
  return std::move(target)
      .on_command(components::CommandMapping::options,
                  std::string{components::CommandMapping::options_label},
                  present)
      .on_command(components::CommandMapping::options_hold,
                  std::string{components::CommandMapping::options_label},
                  present, components::CommandMapping::hold)
      .on_long_press_gesture(present)
      .popover(shown, composition::PopoverAttachmentAnchor::rect(),
               theme::Edge::top,
               [items = std::move(items), preview = std::move(preview), shown,
                preview_shape = std::move(preview_shape)] {
                 std::optional<composition::Element> shown_preview;
                 if (preview.has_value() && *preview) {
                   shown_preview = (*preview)();
                 }
                 return panel(items(), std::move(shown_preview), shown,
                              preview_shape);
               });
}

auto ContextMenu::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  composition::Element label =
      composition::Element{composition::PlaceholderContentView(label_)};
  if (clipboard_ != nullptr) {
    clipboard_->use(context.environment(composition::Pasteboard::general));
    label = clipboard_->keys(std::move(label));
  }
  std::vector<composition::Element> produced;
  if (context.environment(hosted)) {
    produced.push_back(std::move(label).environment(hosted, false));
    return produced;
  }
  produced.push_back(attach(std::move(label), context.state(shown), items(),
                            preview_, preview_shape_));
  return produced;
}

}
