#include <arisa/structure/document_group.hpp>

#include <arisa/components/button.hpp>
#include <arisa/components/command_mapping.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/label.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/dismiss_action.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/structure/default_document_group_launch_actions.hpp>
#include <arisa/structure/document_group_launch_scene.hpp>
#include <arisa/structure/document_launch_view.hpp>
#include <arisa/text/localized_string_key.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <format>
#include <utility>

namespace arisa::structure {

using composition::Element;

DocumentGroup::DocumentGroup(std::shared_ptr<const Kind> kind)
    : Element(std::make_unique<Content>(std::move(kind))) {}

auto DocumentGroup::close_key() -> composition::ElementKey {
  return composition::ElementKey{std::string{"arisa.document-group.close"}};
}

auto DocumentGroup::undo_key() -> composition::ElementKey {
  return composition::ElementKey{std::string{"arisa.document-group.undo"}};
}

auto DocumentGroup::redo_key() -> composition::ElementKey {
  return composition::ElementKey{std::string{"arisa.document-group.redo"}};
}

auto DocumentGroup::failure_key() -> composition::ElementKey {
  return composition::ElementKey{std::string{"arisa.document-group.failure"}};
}

auto DocumentGroup::create(const Kind& kind,
                           const components::FileStorage& storage,
                           Sources& sources,
                           components::NewDocumentAction::Request request)
    -> std::expected<composition::FileURL, components::DocumentError> {
  using Stage = components::DocumentError::Stage;
  if (!kind.allows_creating() && !request.prepare) {
    return std::unexpected(components::DocumentError{
        Stage::create, std::string{"this scene does not create documents"}});
  }
  const std::vector<composition::UTType> writable = kind.writable();
  const composition::UTType type = request.content_type.value_or(
      writable.empty() ? composition::UTType::plain_text() : writable.front());
  const std::optional<composition::FileURL> directory =
      composition::FileURL::create(folder);
  if (!directory.has_value()) {
    return std::unexpected(
        components::DocumentError{Stage::create, std::string{folder}});
  }
  if (auto made = storage.create_directory(*directory); !made.has_value()) {
    return std::unexpected(
        components::DocumentError{Stage::create, made.error()});
  }
  const std::optional<std::string> extension =
      type.preferred_filename_extension();
  std::optional<composition::FileURL> target;
  for (std::size_t number = 1; number <= max_untitled; ++number) {
    std::string name = number == 1 ? std::string{untitled}
                                   : std::format("{} {}", untitled, number);
    if (extension.has_value()) {
      name += "." + *extension;
    }
    target = directory->appending(name);
    if (target.has_value() && !storage.exists(*target)) {
      break;
    }
    target.reset();
  }
  if (!target.has_value()) {
    return std::unexpected(components::DocumentError{
        Stage::create, std::string{"no free document name"}});
  }
  if (request.prepare) {
    const std::optional<composition::FileURL> prepared = request.prepare();
    if (prepared.has_value()) {
      auto bytes = storage.read(*prepared);
      if (!bytes.has_value()) {
        return std::unexpected(
            components::DocumentError{Stage::read, bytes.error()});
      }
      if (auto written = storage.write(*target, *bytes); !written.has_value()) {
        return std::unexpected(
            components::DocumentError{Stage::write, written.error()});
      }
    } else if (auto made =
                   kind.write_initial(storage, *target, type, request.source);
               !made.has_value()) {
      return std::unexpected(made.error());
    }
  } else if (request.initial) {
    if (auto made = request.initial(storage, *target, type);
        !made.has_value()) {
      return std::unexpected(made.error());
    }
  } else if (auto made =
                 kind.write_initial(storage, *target, type, request.source);
             !made.has_value()) {
    return std::unexpected(made.error());
  }
  if (request.source.has_value()) {
    sources.insert_or_assign(std::string{target->path()}, *request.source);
  }
  return *target;
}

void DocumentGroup::Session::changed() {
  dirty_ = true;
  revision_.set(revision_.get() + 1);
  autosave_.reset();
  autosave_.emplace(place_.frames.get().after(
      autosave_delay, [weak = weak_from_this()](std::stop_token stop) {
        if (stop.stop_requested()) {
          return;
        }
        if (const std::shared_ptr<Session> alive = weak.lock()) {
          static_cast<void>(alive->save());
        }
      }));
}

auto DocumentGroup::Session::save()
    -> std::expected<void, components::DocumentError> {
  if (!dirty_ || !place_.editable) {
    return {};
  }
  auto written = write();
  if (!written.has_value()) {
    failure_ = written.error();
    revision_.set(revision_.get() + 1);
    return written;
  }
  const bool cleared = failure_.has_value();
  failure_.reset();
  dirty_ = false;
  ++saves_;
  if (cleared) {
    revision_.set(revision_.get() + 1);
  }
  return {};
}

auto DocumentGroup::Editor::chrome(
    composition::Context& context,
    const std::shared_ptr<Session>& session) const -> Element {
  static_cast<void>(context.read(session->revision()));
  const composition::UndoManager& undo = session->undo();
  const composition::UndoManager::Status& status = context.read(undo.status());
  const composition::DismissAction& dismiss =
      context.environment(components::Environment::dismiss);
  const auto close = [session, dismiss] {
    static_cast<void>(session->save());
    dismiss();
  };
  std::vector<Element> bar;
  bar.push_back(
      Element{components::Button(
                  close, components::Label(
                             components::Text(text::LocalizedStringKey{
                                 std::string{default_title}}),
                             components::Image::system_name("arrow_back")))}
          .id(close_key()));
  bar.push_back(components::Text(std::string{url_.last_path_component()})
                    .font(theme::Font::headline));
  bar.push_back(components::Spacer());
  if (session->failure().has_value()) {
    bar.push_back(components::Text(session->failure()->explanation())
                      .foreground_color(theme::ColourRole::destructive)
                      .id(failure_key()));
  }
  if (session->editable()) {
    bar.push_back(
        Element{components::Button(
                    [undo] { undo.undo(); },
                    components::Label(
                        components::Text(text::LocalizedStringKey{"Undo"}),
                        components::Image::system_name("undo")))}
            .disabled(!status.can_undo)
            .id(undo_key()));
    bar.push_back(
        Element{components::Button(
                    [undo] { undo.redo(); },
                    components::Label(
                        components::Text(text::LocalizedStringKey{"Redo"}),
                        components::Image::system_name("redo")))}
            .disabled(!status.can_redo)
            .id(redo_key()));
  }
  std::vector<Element> column;
  column.push_back(components::HStack(
                       components::HStackOptions{
                           .alignment = components::VerticalAlignment::center,
                           .spacing = theme::space::md},
                       std::move(bar))
                       .padding(theme::Insets::symmetric(theme::space::lg,
                                                         theme::space::sm)));
  column.push_back(session->editor().frame(
      {.max_width = composition::infinity,
       .max_height = composition::infinity,
       .alignment = composition::Alignment::top_leading}));
  Element shown =
      components::VStack(
          components::VStackOptions{
              .alignment = components::HorizontalAlignment::leading,
              .spacing = theme::space::sm},
          std::move(column))
          .frame({.max_width = composition::infinity,
                  .max_height = composition::infinity,
                  .alignment = composition::Alignment::top_leading})
          .environment(
              components::Environment::document_configuration,
              std::optional<components::DocumentConfiguration>{
                  components::DocumentConfiguration{session->editable(), url_}})
          .environment(components::Environment::undo_manager,
                       std::optional<composition::UndoManager>{undo})
          .on_exit_command(std::string{components::CommandMapping::close_label},
                           close)
          .on_disappear([session] { static_cast<void>(session->save()); });
  if (status.can_undo) {
    shown = std::move(shown).on_command(
        components::CommandMapping::undo,
        std::string{components::CommandMapping::undo_label},
        [undo] { undo.undo(); });
  }
  if (status.can_redo) {
    shown = std::move(shown).on_command(
        components::CommandMapping::redo,
        std::string{components::CommandMapping::redo_label},
        [undo] { undo.redo(); });
  }
  return shown;
}

auto DocumentGroup::Editor::body(composition::Context& context) const
    -> Element {
  const std::optional<components::FileStorage>& storage =
      context.environment(components::Environment::file_storage);
  std::shared_ptr<Session>& session = context.retained(session_key);
  if (!storage.has_value()) {
    return components::Text(text::LocalizedStringKey{"No files"});
  }
  if (session == nullptr || session->url() != url_) {
    std::optional<components::DocumentCreationSource> source;
    if (const auto found = sources_->find(std::string{url_.path()});
        found != sources_->end()) {
      source = found->second;
    }
    auto opened = kind_->open(Session::Place{
        .storage = *storage,
        .url = url_,
        .type =
            components::DocumentContentTypes::matching(kind_->readable(), url_),
        .editable = false,
        .source = std::move(source),
        .frames = context.services().frames});
    if (!opened.has_value()) {
      const composition::DismissAction& dismiss =
          context.environment(components::Environment::dismiss);
      return components::VStack(
                 components::VStackOptions{
                     .alignment = components::HorizontalAlignment::leading,
                     .spacing = theme::space::md},
                 Element{components::Button(
                             [dismiss] { dismiss(); },
                             components::Text(text::LocalizedStringKey{
                                 std::string{default_title}}))}
                     .id(close_key()),
                 components::Text(opened.error().explanation())
                     .foreground_color(theme::ColourRole::destructive)
                     .id(failure_key()))
          .padding(theme::space::xl);
    }
    session = std::move(*opened);
  }
  if (context.environment(components::Environment::scene_phase) ==
          provider::ScenePhase::background &&
      session->dirty()) {
    static_cast<void>(session->save());
  }
  return chrome(context, session);
}

auto DocumentGroup::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto DocumentGroup::Content::body(composition::Context& context) const
    -> std::vector<Element> {
  const std::optional<components::FileStorage>& storage =
      context.environment(components::Environment::file_storage);
  const std::optional<DocumentGroupLaunchScene>& scene =
      context.environment(DocumentGroupLaunchScene::key);
  const std::optional<std::string>& titled =
      context.environment(components::Environment::document_launch_title);
  const std::optional<std::string>& subtitled =
      context.environment(components::Environment::document_launch_subtitle);
  DocumentLaunchView::Options options =
      scene.has_value() ? scene->options() : DocumentLaunchView::Options{};
  if (!options.subtitle.has_value()) {
    options.subtitle = subtitled;
  }
  options.create = [kind = kind_, storage, sources = sources_](
                       components::NewDocumentAction::Request request)
      -> std::expected<composition::FileURL, components::DocumentError> {
    if (!storage.has_value()) {
      return std::unexpected(components::DocumentError{
          components::DocumentError::Stage::create, std::string{"no files"}});
    }
    return create(*kind, *storage, *sources, std::move(request));
  };
  std::string title = scene.has_value()
                          ? scene->title()
                          : titled.value_or(std::string{default_title});
  DocumentLaunchView::Build actions =
      scene.has_value() ? scene->actions() : DocumentLaunchView::Build{[] {
        return Element{DefaultDocumentGroupLaunchActions{}};
      }};
  std::vector<Element> produced;
  produced.push_back(Element{
      DocumentLaunchView{
          std::move(title), kind_->readable(), std::move(actions),
          [kind = kind_, sources = sources_](const composition::FileURL& url) {
            return Element{Editor{kind, url, sources}};
          },
          std::move(
              options)}}.environment(DocumentLaunchView::creates_documents,
                                     kind_->allows_creating()));
  return produced;
}

}
