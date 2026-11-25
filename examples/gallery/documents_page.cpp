#include <documents_page.hpp>

#include <arisa/components/button.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/file_document_configuration.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/subscription_view.hpp>
#include <arisa/composition/toolbar_item.hpp>
#include <arisa/composition/undo_manager.hpp>
#include <arisa/composition/visibility.hpp>
#include <arisa/controls/text_editor.hpp>
#include <arisa/controls/toggle.hpp>
#include <arisa/presentation/navigation_stack.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/structure/document_group.hpp>
#include <arisa/structure/document_group_launch_scene.hpp>
#include <arisa/structure/file_browser.hpp>
#include <arisa/structure/new_document_button.hpp>
#include <arisa/text/localized_string_key.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <format>
#include <string>
#include <utility>
#include <vector>

namespace arisa::examples {

using composition::Element;
using composition::infinity;
using theme::ColourRole;

auto DocumentsPage::Note::readable_content_types()
    -> std::vector<composition::UTType> {
  return {composition::UTType::plain_text()};
}

auto DocumentsPage::Note::read(
    const components::FileDocumentReadConfiguration& read)
    -> std::expected<Note, components::DocumentError> {
  std::optional<std::string> text = read.file().text();
  if (!text.has_value()) {
    return std::unexpected(components::DocumentError{
        components::DocumentError::Stage::decode, std::string{"folder"}});
  }
  return Note{std::move(*text)};
}

auto DocumentsPage::Note::file_wrapper(
    const components::FileDocumentWriteConfiguration&) const
    -> std::expected<components::FileWrapper, components::DocumentError> {
  return components::FileWrapper::regular_file(text_);
}

auto DocumentsPage::Demos::ticks() -> composition::Observable<int>& {
  static composition::Observable<int> published{0};
  return published;
}

auto DocumentsPage::Demos::card(std::string_view title, Element content)
    -> Element {
  return components::VStack(
             components::VStackOptions{
                 .alignment = composition::HorizontalAlignment::leading,
                 .spacing = theme::space::sm},
             components::Text(text::LocalizedStringKey{std::string{title}})
                 .font(theme::Font::headline),
             std::move(content))
      .padding(theme::space::lg)
      .frame(
          {.max_width = infinity, .alignment = composition::Alignment::leading})
      .background(ColourRole::surface_raised,
                  theme::RoundedRectangle(theme::radius::card));
}

auto DocumentsPage::Demos::notes() -> Element {
  return Element{
      structure::DocumentGroup{
          Note{"New note"},
          [](const components::FileDocumentConfiguration<Note>& configuration)
              -> Element {
            const composition::Binding<Note> bound =
                configuration.document_binding();
            return controls::TextEditor(
                       composition::Binding<std::string>{
                           [bound] { return bound.get().text(); },
                           [bound](std::string text) {
                             Note next = bound.get();
                             next.set_text(std::move(text));
                             bound.set(std::move(next));
                           }})
                .frame({.max_width = infinity, .min_height = 160.0F});
          }}}
      .document_group_launch_scene(structure::DocumentGroupLaunchScene{
          "Notes",
          [] {
            return Element{structure::NewDocumentButton{
                std::string{"New note"}, std::nullopt,
                components::DocumentCreationSource{"gallery"}}};
          }})
      .frame({.height = 420.0F, .max_width = infinity})
      .id(std::string{notes_scene});
}

auto DocumentsPage::Demos::files(composition::Context& context) -> Element {
  const auto opening = context.state(importing);
  const auto saving = context.state(exporting);
  const auto chosen = context.state(picked);
  const auto record = [chosen](structure::FileBrowser::Results results) {
    if (!results.has_value()) {
      chosen.set(results.error().explanation());
      return;
    }
    chosen.set(std::format("{}", results->size()));
  };
  return components::VStack(
      components::VStackOptions{
          .alignment = composition::HorizontalAlignment::leading,
          .spacing = theme::space::xs},
      Element{
          components::Button("Open several", [opening] { opening.set(true); })}
          .id(std::string{import_button})
          .file_importer(composition::Binding<bool>{opening},
                         {composition::UTType::plain_text()}, true, record),
      Element{components::Button("Export two notes",
                                 [saving] { saving.set(true); })}
          .id(std::string{export_button})
          .file_exporter(composition::Binding<bool>{saving},
                         std::vector<Note>{Note{"First"}, Note{"Second"}},
                         composition::UTType::plain_text(), record),
      components::Text(chosen.get()));
}

auto DocumentsPage::Demos::flow(composition::Context& context) -> Element {
  const auto count = context.state(received);
  const auto lazy = context.state(seeded);
  const std::optional<composition::UndoManager>& undo =
      context.environment(components::Environment::undo_manager);
  return components::VStack(
      components::VStackOptions{
          .alignment = composition::HorizontalAlignment::leading,
          .spacing = theme::space::xs},
      Element{
          components::Button("Publish", [] { ticks().set(ticks().get() + 1); })}
          .id(std::string{tick_button})
          .on_receive(ticks(),
                      [count](const int&) { count.set(count.get() + 1); }),
      components::Text(std::format("{}", count.get())),
      components::Text(std::format("{}", lazy.get())),
      components::Text(undo.has_value() && undo->can_undo() ? "Undo" : ""));
}

auto DocumentsPage::Demos::overlays(composition::Context& context) -> Element {
  const auto hidden = context.state(overlays_hidden);
  return Element{controls::Toggle(
                     composition::Binding<bool>{hidden},
                     components::Text(text::LocalizedStringKey{"Hide hints"}))}
      .id(std::string{overlays_toggle})
      .persistent_system_overlays(hidden.get()
                                      ? composition::Visibility::hidden
                                      : composition::Visibility::automatic);
}

auto DocumentsPage::Demos::body(composition::Context& context) const
    -> Element {
  return components::VStack(
      components::VStackOptions{
          .alignment = composition::HorizontalAlignment::leading,
          .spacing = theme::space::lg},
      card("Notes", notes()), card("Files", files(context)),
      card("Data flow", flow(context)),
      card("System overlays", overlays(context)));
}

auto DocumentsPage::body(composition::Context&) const -> Element {
  const auto shown_flag = shown_;
  return presentation::NavigationStack([shown_flag] {
    return Element{
        components::ScrollView(Element{Demos{}}.padding(theme::space::lg))
            .navigation_title("Documents and data")
            .toolbar(composition::ToolbarItem(
                composition::ToolbarItemPlacement::cancellation_action,
                components::Button("Close", [shown_flag] {
                  shown_flag.set(false);
                }).id(std::string{close_button})))};
  });
}

auto DocumentsPage::Opener::body(composition::Context& context) const
    -> Element {
  const auto shown = context.state(shown_key);
  return components::Button("Documents and data", [shown] { shown.set(true); })
      .id(std::string{button})
      .full_screen_cover(shown,
                         [shown] { return Element{DocumentsPage{shown}}; });
}

}
