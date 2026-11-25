#include <arisa/structure/file_browser.hpp>

#include <arisa/components/button.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/file_storage.hpp>
#include <arisa/components/for_each.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/label.hpp>
#include <arisa/components/lazy_vstack.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/controls/text_field.hpp>
#include <arisa/text/localized_string_key.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <format>
#include <utility>

namespace arisa::structure {

using composition::Element;

FileBrowser::FileBrowser(Request request,
                         composition::Binding<bool> is_presented,
                         Completion completion)
    : Element(std::make_unique<Content>(std::move(request),
                                        std::move(is_presented),
                                        adapted(std::move(completion)))) {}

FileBrowser::FileBrowser(Request request,
                         composition::Binding<bool> is_presented,
                         Completions completion)
    : Element(std::make_unique<Content>(
          std::move(request), std::move(is_presented), std::move(completion))) {
}

auto FileBrowser::adapted(Completion completion) -> Completions {
  return [completion = std::move(completion)](Results results) {
    if (!completion) {
      return;
    }
    if (!results.has_value()) {
      completion(std::unexpected(results.error()));
      return;
    }
    if (results->empty()) {
      completion(std::unexpected(composition::FileError{
          composition::FileError::Stage::read,
          provider::ProviderError{provider::ProviderErrorCode::not_found}}));
      return;
    }
    completion(results->front());
  };
}

auto FileBrowser::exported(std::vector<std::vector<std::byte>> documents,
                           const composition::UTType& type)
    -> std::vector<Exported> {
  const std::string extension =
      type.preferred_filename_extension().value_or(std::string{});
  std::vector<Exported> made;
  made.reserve(documents.size());
  for (std::size_t index = 0; index < documents.size(); ++index) {
    std::string name = index == 0 ? std::string{"Untitled"}
                                  : std::format("Untitled {}", index + 1);
    if (!extension.empty()) {
      name += "." + extension;
    }
    made.push_back(
        Exported{.name = std::move(name),
                 .bytes = std::make_shared<const std::vector<std::byte>>(
                     std::move(documents[index]))});
  }
  return made;
}

auto FileBrowser::row_key(std::string_view name) -> composition::ElementKey {
  return composition::ElementKey{
      std::format("arisa.file-browser.row.{}", name)};
}

auto FileBrowser::action_key() -> composition::ElementKey {
  return composition::ElementKey{std::string{"arisa.file-browser.action"}};
}

auto FileBrowser::message_key() -> composition::ElementKey {
  return composition::ElementKey{"arisa.file-browser.message"};
}

auto FileBrowser::up_key() -> composition::ElementKey {
  return composition::ElementKey{std::string{"arisa.file-browser.up"}};
}

auto FileBrowser::name_key() -> composition::ElementKey {
  return composition::ElementKey{std::string{"arisa.file-browser.name"}};
}

auto FileBrowser::extensions(const composition::UTType& type)
    -> std::vector<std::string_view> {
  const std::string_view identifier = type.identifier();
  constexpr std::string_view prefix = "public.filename-extension.";
  if (identifier.starts_with(prefix)) {
    return {identifier.substr(prefix.size())};
  }
  if (identifier == "public.plain-text" || identifier == "public.text") {
    return {"txt", "md", "log", "ini", "toml", "json", "csv", "xml", "cfg"};
  }
  if (identifier == "public.json") {
    return {"json"};
  }
  if (identifier == "public.image") {
    return {"png", "jpg", "jpeg", "bmp", "gif", "webp"};
  }
  return {};
}

auto FileBrowser::accepts(const std::vector<composition::UTType>& allowed,
                          std::string_view name) -> bool {
  if (allowed.empty()) {
    return true;
  }
  const std::size_t dot = name.rfind('.');
  std::string extension;
  if (dot != std::string_view::npos) {
    extension = std::string{name.substr(dot + 1)};
    std::ranges::transform(extension, extension.begin(), [](char character) {
      return static_cast<char>(
          std::tolower(static_cast<unsigned char>(character)));
    });
  }
  return std::ranges::any_of(allowed, [&extension](
                                          const composition::UTType& type) {
    const std::string_view identifier = type.identifier();
    if (identifier == "public.item" || identifier == "public.data" ||
        identifier == "public.content") {
      return true;
    }
    return std::ranges::contains(extensions(type), std::string_view{extension});
  });
}

auto FileBrowser::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto FileBrowser::Content::title(Mode mode) -> std::string_view {
  switch (mode) {
    case Mode::import:
      return "Open";
    case Mode::export_document:
      return "Save";
    case Mode::move:
      return "Move";
  }
  return "Open";
}

void FileBrowser::Finish::operator()(Result result) const {
  if (!result.has_value()) {
    (*this)(Results{std::unexpected(result.error())});
    return;
  }
  (*this)(Results{std::vector<composition::FileURL>{std::move(*result)}});
}

void FileBrowser::Finish::operator()(Results results) const {
  is_presented_.set(false);
  if (completion_ != nullptr && *completion_) {
    (*completion_)(std::move(results));
  }
}

void FileBrowser::Content::commit_all(
    const std::optional<composition::FileURL>& folder,
    const components::FileStorage& storage, const Request& request,
    const Finish& finish) {
  using Stage = composition::FileError::Stage;
  std::vector<composition::FileURL> landed;
  const auto place = [&folder](std::string_view name) {
    return folder.has_value() ? folder->appending(name)
                              : composition::FileURL::create(name);
  };
  if (request.mode == Mode::export_document) {
    for (const Exported& document : request.documents) {
      const auto target = place(document.name);
      if (!target.has_value()) {
        finish(Results{std::unexpected(composition::FileError{
            Stage::name, provider::ProviderError{
                             provider::ProviderErrorCode::invalid_argument}})});
        return;
      }
      const std::vector<std::byte> empty;
      if (auto written = storage.write(
              *target, document.bytes != nullptr ? *document.bytes : empty);
          !written.has_value()) {
        finish(Results{std::unexpected(written.error())});
        return;
      }
      landed.push_back(*target);
    }
    finish(Results{std::move(landed)});
    return;
  }
  for (const composition::FileURL& file : request.files) {
    const auto target = place(file.last_path_component());
    if (!target.has_value()) {
      finish(Results{std::unexpected(composition::FileError{
          Stage::name, provider::ProviderError{
                           provider::ProviderErrorCode::invalid_argument}})});
      return;
    }
    if (auto moved = storage.move(file, *target); !moved.has_value()) {
      finish(Results{std::unexpected(moved.error())});
      return;
    }
    landed.push_back(*target);
  }
  finish(Results{std::move(landed)});
}

void FileBrowser::Content::commit(
    const std::optional<composition::FileURL>& folder,
    const components::FileStorage& storage, const std::string& name,
    const Request& request, const Finish& finish) {
  using Stage = composition::FileError::Stage;
  const auto target = folder.has_value() ? folder->appending(name)
                                         : composition::FileURL::create(name);
  if (!target.has_value()) {
    finish(Result{std::unexpected(composition::FileError{
        Stage::name, provider::ProviderError{
                         provider::ProviderErrorCode::invalid_argument}})});
    return;
  }
  if (request.mode == Mode::export_document) {
    const std::vector<std::byte> empty;
    const std::vector<std::byte>& bytes =
        request.document != nullptr ? *request.document : empty;
    if (auto written = storage.write(*target, bytes); !written.has_value()) {
      finish(Result{std::unexpected(written.error())});
      return;
    }
    finish(*target);
    return;
  }
  if (!request.file.has_value()) {
    finish(Result{std::unexpected(composition::FileError{
        Stage::read,
        provider::ProviderError{provider::ProviderErrorCode::not_found}})});
    return;
  }
  if (auto moved = storage.move(*request.file, *target); !moved.has_value()) {
    finish(Result{std::unexpected(moved.error())});
    return;
  }
  finish(*target);
}

auto FileBrowser::Content::row(
    const provider::DirectoryEntry& entry,
    const std::optional<composition::FileURL>& folder, const Request& request,
    const Finish& finish, composition::Binding<std::string> current,
    composition::Binding<std::vector<std::string>> chosen,
    const composition::FileDialogEnvironment::Enabled& enabled) -> Element {
  const bool is_directory =
      entry.kind == provider::DirectoryEntry::Kind::directory;
  const auto target = folder.has_value()
                          ? folder->appending(entry.name)
                          : composition::FileURL::create(entry.name);
  const bool permitted = enabled == nullptr || !*enabled ||
                         !target.has_value() || (*enabled)(*target);
  const bool selectable =
      permitted &&
      (is_directory || (request.mode == Mode::import &&
                        FileBrowser::accepts(request.allowed, entry.name)));
  components::Button::Action action = [] {};
  if (target.has_value() && is_directory) {
    action = [current, path = std::string{target->path()}] {
      current.set(path);
    };
  } else if (target.has_value() && selectable && request.multiple) {
    action = [chosen, path = std::string{target->path()}] {
      std::vector<std::string> next = chosen.get();
      if (const auto found = std::ranges::find(next, path);
          found != next.end()) {
        next.erase(found);
      } else {
        next.push_back(path);
      }
      chosen.set(std::move(next));
    };
  } else if (target.has_value() && selectable) {
    action = [finish, file = *target] { finish(FileBrowser::Result{file}); };
  }
  const bool picked =
      target.has_value() &&
      std::ranges::contains(chosen.get(), std::string{target->path()});
  Element label = components::Label(
      entry.name,
      components::Image::system_name(
          is_directory ? "folder" : (picked ? "check_circle" : "description")));
  return Element{components::Button(std::move(action), std::move(label))}
      .frame({.max_width = composition::infinity,
              .alignment = composition::Alignment::leading})
      .disabled(!selectable)
      .id(FileBrowser::row_key(entry.name));
}

auto FileBrowser::Content::footer(
    const std::optional<composition::FileURL>& folder,
    const components::FileStorage& storage, const Request& request,
    const Finish& finish, const std::string& label,
    composition::StateHandle<std::string> named,
    composition::Binding<std::vector<std::string>> chosen) -> Element {
  std::vector<Element> parts;
  if (request.multiple) {
    const std::size_t count = request.mode == Mode::import ? chosen.get().size()
                              : request.mode == Mode::move
                                  ? request.files.size()
                                  : request.documents.size();
    parts.push_back(components::Text(std::format("{}", count)));
    parts.push_back(components::Spacer());
    parts.push_back(
        Element{components::Button(
                    [folder, files = storage, request, finish, chosen] {
                      if (request.mode != Mode::import) {
                        commit_all(folder, files, request, finish);
                        return;
                      }
                      std::vector<composition::FileURL> picked;
                      for (const std::string& path : chosen.get()) {
                        if (auto made = composition::FileURL::create(path)) {
                          picked.push_back(std::move(*made));
                        }
                      }
                      finish(Results{std::move(picked)});
                    },
                    components::Text(text::LocalizedStringKey{label}))}
            .disabled(count == 0)
            .id(FileBrowser::action_key()));
    return components::HStack(
        components::HStackOptions{
            .alignment = components::VerticalAlignment::center,
            .spacing = theme::space::md},
        std::move(parts));
  }
  const std::string typed =
      named.get().empty() ? request.default_filename : named.get();
  const std::string picked =
      request.mode == Mode::move && request.file.has_value()
          ? std::string{request.file->last_path_component()}
          : typed;
  if (request.mode == Mode::export_document) {
    parts.push_back(
        Element{controls::TextField("Name", composition::Binding<std::string>{
                                                [typed] { return typed; },
                                                [named](std::string value) {
                                                  named.set(std::move(value));
                                                }})}
            .frame({.max_width = composition::infinity})
            .id(FileBrowser::name_key()));
  } else {
    parts.push_back(components::Text(picked));
    parts.push_back(components::Spacer());
  }
  parts.push_back(
      Element{components::Button(
                  [folder, files = storage, picked, request, finish] {
                    commit(folder, files, picked, request, finish);
                  },
                  components::Text(text::LocalizedStringKey{label}))}
          .disabled(picked.empty())
          .id(FileBrowser::action_key()));
  return components::HStack(
      components::HStackOptions{
          .alignment = components::VerticalAlignment::center,
          .spacing = theme::space::md},
      std::move(parts));
}

auto FileBrowser::Content::body(composition::Context& context) const
    -> std::vector<Element> {
  const std::optional<std::string>& customized =
      context.environment(composition::FileDialogEnvironment::customization_id);
  const composition::Binding<std::string> current =
      customized.has_value()
          ? context.app_storage(
                std::format("arisa.file-dialog.{}", *customized),
                FileBrowser::directory)
          : composition::Binding<std::string>{
                context.state(FileBrowser::directory)};
  const auto named = context.state(FileBrowser::filename);
  const composition::Binding<std::vector<std::string>> chosen{
      context.state(FileBrowser::selected)};
  const auto was_seeded = context.state(FileBrowser::seeded);
  const std::optional<composition::FileURL>& start = context.environment(
      composition::FileDialogEnvironment::default_directory);
  const composition::FileDialogEnvironment::Enabled& enabled =
      context.environment(composition::FileDialogEnvironment::url_enabled);
  const std::optional<std::string>& confirming = context.environment(
      composition::FileDialogEnvironment::confirmation_label);
  const std::optional<std::string>& said =
      context.environment(composition::FileDialogEnvironment::message);
  const std::string label =
      confirming.value_or(std::string{title(request_.mode)});
  const std::optional<components::FileStorage>& storage =
      context.environment(components::Environment::file_storage);
  const std::optional<composition::FileURL> folder =
      composition::FileURL::create(current.get());
  std::vector<Element> rows;
  rows.push_back(components::HStack(
      components::HStackOptions{
          .alignment = components::VerticalAlignment::center,
          .spacing = theme::space::md},
      components::Text(text::LocalizedStringKey{label})
          .font(theme::Font::title2),
      components::Text(folder.has_value() ? std::format("/{}", folder->path())
                                          : std::string{"/"})
          .font(theme::Font::callout)
          .foreground_color(theme::ColourRole::text_secondary),
      components::Spacer(),
      Element{components::Button(
                  [current, folder] {
                    const auto parent =
                        folder.has_value()
                            ? folder->deleting_last_path_component()
                            : std::nullopt;
                    current.set(parent.has_value() ? std::string{parent->path()}
                                                   : std::string{});
                  },
                  components::Label(
                      components::Text(text::LocalizedStringKey{"Up"}),
                      components::Image::system_name("arrow_upward")))}
          .disabled(!folder.has_value())
          .id(FileBrowser::up_key())));
  if (said.has_value()) {
    rows.push_back(components::Text(text::LocalizedStringKey{*said})
                       .font(theme::Font::callout)
                       .foreground_color(theme::ColourRole::text_secondary)
                       .id(FileBrowser::message_key()));
  }
  if (!storage.has_value()) {
    rows.push_back(components::Text(text::LocalizedStringKey{"No files"})
                       .foreground_color(theme::ColourRole::text_secondary));
    return rows;
  }
  auto listed = storage->entries(folder);
  if (!listed.has_value()) {
    rows.push_back(components::Text(listed.error().explanation())
                       .foreground_color(theme::ColourRole::destructive));
  } else if (listed->empty()) {
    rows.push_back(components::Text(text::LocalizedStringKey{"No files"})
                       .foreground_color(theme::ColourRole::text_secondary));
  } else {
    const std::shared_ptr<const std::vector<provider::DirectoryEntry>> entries =
        std::make_shared<const std::vector<provider::DirectoryEntry>>(
            std::move(*listed));
    rows.push_back(Element{
        components::ScrollView(components::LazyVStack(
            components::LazyVStackOptions{
                .alignment = components::HorizontalAlignment::leading,
                .spacing = theme::space::xxs},
            components::ForEach(
                *entries,
                [](const provider::DirectoryEntry& entry) {
                  return FileBrowser::row_key(entry.name);
                },
                [request = request_, finish = finish_, folder, current, chosen,
                 enabled](const provider::DirectoryEntry& entry) {
                  return row(entry, folder, request, finish, current, chosen,
                             enabled);
                })))}.frame({.max_width = composition::infinity,
                             .max_height = composition::infinity}));
  }
  if (request_.mode != Mode::import || request_.multiple) {
    rows.push_back(
        footer(folder, *storage, request_, finish_, label, named, chosen));
  }
  Element listed_rows =
      components::VStack(
          components::VStackOptions{
              .alignment = components::HorizontalAlignment::leading,
              .spacing = theme::space::md},
          std::move(rows))
          .padding(theme::space::xl)
          .frame({.max_width = composition::infinity,
                  .max_height = composition::infinity,
                  .alignment = composition::Alignment::top_leading});
  if (start.has_value() && !was_seeded.get()) {
    listed_rows = std::move(listed_rows)
                      .on_appear([current, was_seeded,
                                  directory = std::string{start->path()}] {
                        was_seeded.set(true);
                        if (current.get().empty()) {
                          current.set(directory);
                        }
                      });
  }
  std::vector<Element> shown;
  shown.push_back(std::move(listed_rows));
  return shown;
}

}
