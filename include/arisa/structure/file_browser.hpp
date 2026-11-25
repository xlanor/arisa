#pragma once

#include <arisa/components/document.hpp>
#include <arisa/components/file_document_write_configuration.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/file_dialog_environment.hpp>
#include <arisa/composition/file_url.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/transfer_item.hpp>
#include <arisa/composition/transferable.hpp>
#include <arisa/composition/ut_type.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/provider/storage/directory_entry.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::components {

class FileStorage;

}

namespace arisa::structure {

class FileBrowser : public composition::Element {
 public:
  class Content;

  enum class Mode : std::uint8_t {
    import,
    export_document,
    move,
  };

  using Result = std::expected<composition::FileURL, composition::FileError>;
  using Completion = std::function<void(Result)>;
  using Results =
      std::expected<std::vector<composition::FileURL>, composition::FileError>;
  using Completions = std::function<void(Results)>;

  class Finish {
   public:
    Finish(composition::Binding<bool> is_presented,
           std::shared_ptr<const Completions> completion) noexcept
        : is_presented_(std::move(is_presented)),
          completion_(std::move(completion)) {}

    void operator()(Result result) const;

    void operator()(Results results) const;

   private:
    composition::Binding<bool> is_presented_;
    std::shared_ptr<const Completions> completion_;
  };

  struct Exported {
    std::string name;
    std::shared_ptr<const std::vector<std::byte>> bytes;
  };

  struct Request {
    Mode mode{Mode::import};
    std::vector<composition::UTType> allowed{};
    std::shared_ptr<const std::vector<std::byte>> document{};
    std::string default_filename{};
    std::optional<composition::FileURL> file{};
    bool multiple{false};
    std::vector<Exported> documents{};
    std::vector<composition::FileURL> files{};
  };

  static constexpr composition::StateKey<std::string> directory{
      "arisa.file-browser.directory", std::string{}};
  static constexpr composition::StateKey<std::string> filename{
      "arisa.file-browser.filename", std::string{}};
  static constexpr composition::StateKey<bool> seeded{
      "arisa.file-browser.seeded", false};
  static inline const composition::StateKey<std::vector<std::string>> selected{
      "arisa.file-browser.selected", std::vector<std::string>{}};

  FileBrowser(Request request, composition::Binding<bool> is_presented,
              Completion completion);

  FileBrowser(Request request, composition::Binding<bool> is_presented,
              Completions completion);

  static auto adapted(Completion completion) -> Completions;

  static auto exported(std::vector<std::vector<std::byte>> documents,
                       const composition::UTType& type)
      -> std::vector<Exported>;

  static auto accepts(const std::vector<composition::UTType>& allowed,
                      std::string_view name) -> bool;

  static auto row_key(std::string_view name) -> composition::ElementKey;

  static auto action_key() -> composition::ElementKey;

  static auto up_key() -> composition::ElementKey;

  static auto message_key() -> composition::ElementKey;

  static auto name_key() -> composition::ElementKey;

 private:
  static auto extensions(const composition::UTType& type)
      -> std::vector<std::string_view>;
};

class FileBrowser::Content final : public composition::ElementContent {
 public:
  Content(Request request, composition::Binding<bool> is_presented,
          Completions completion) noexcept
      : request_(std::move(request)),
        finish_(std::move(is_presented),
                std::make_shared<const Completions>(std::move(completion))) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

 private:
  static auto row(const provider::DirectoryEntry& entry,
                  const std::optional<composition::FileURL>& folder,
                  const Request& request, const Finish& finish,
                  composition::Binding<std::string> current,
                  composition::Binding<std::vector<std::string>> chosen,
                  const composition::FileDialogEnvironment::Enabled& enabled)
      -> composition::Element;

  static void commit(const std::optional<composition::FileURL>& folder,
                     const components::FileStorage& storage,
                     const std::string& name, const Request& request,
                     const Finish& finish);

  static void commit_all(const std::optional<composition::FileURL>& folder,
                         const components::FileStorage& storage,
                         const Request& request, const Finish& finish);

  static auto footer(const std::optional<composition::FileURL>& folder,
                     const components::FileStorage& storage,
                     const Request& request, const Finish& finish,
                     const std::string& label,
                     composition::StateHandle<std::string> named,
                     composition::Binding<std::vector<std::string>> chosen)
      -> composition::Element;

  static auto title(Mode mode) -> std::string_view;

  Request request_;
  Finish finish_;
};

}

namespace arisa::composition {

template <class OnCompletion>
auto Element::file_importer(Binding<bool> is_presented,
                            std::vector<UTType> allowed_content_types,
                            OnCompletion on_completion) && -> Element {
  structure::FileBrowser::Request request{
      .mode = structure::FileBrowser::Mode::import,
      .allowed = std::move(allowed_content_types),
      .document = nullptr,
      .default_filename = {},
      .file = std::nullopt};
  structure::FileBrowser::Completion completion{std::move(on_completion)};
  return std::move(*this).sheet(
      is_presented, [request = std::move(request), is_presented,
                     completion = std::move(completion)] {
        return Element{
            structure::FileBrowser{request, is_presented, completion}};
      });
}

template <class OnCompletion>
auto Element::file_exporter(Binding<bool> is_presented,
                            std::vector<std::byte> document,
                            UTType content_type, std::string default_filename,
                            OnCompletion on_completion) && -> Element {
  structure::FileBrowser::Request request{
      .mode = structure::FileBrowser::Mode::export_document,
      .allowed = {std::move(content_type)},
      .document =
          std::make_shared<const std::vector<std::byte>>(std::move(document)),
      .default_filename = std::move(default_filename),
      .file = std::nullopt};
  structure::FileBrowser::Completion completion{std::move(on_completion)};
  return std::move(*this).sheet(
      is_presented, [request = std::move(request), is_presented,
                     completion = std::move(completion)] {
        return Element{
            structure::FileBrowser{request, is_presented, completion}};
      });
}

template <class OnCompletion>
auto Element::file_mover(Binding<bool> is_presented,
                         std::optional<FileURL> file,
                         OnCompletion on_completion) && -> Element {
  structure::FileBrowser::Request request{
      .mode = structure::FileBrowser::Mode::move,
      .allowed = {},
      .document = nullptr,
      .default_filename = {},
      .file = std::move(file)};
  structure::FileBrowser::Completion completion{std::move(on_completion)};
  return std::move(*this).sheet(
      is_presented, [request = std::move(request), is_presented,
                     completion = std::move(completion)] {
        return Element{
            structure::FileBrowser{request, is_presented, completion}};
      });
}

template <class OnCompletion>
auto Element::file_importer(Binding<bool> is_presented,
                            std::vector<UTType> allowed_content_types,
                            bool allows_multiple_selection,
                            OnCompletion on_completion) && -> Element {
  structure::FileBrowser::Request request{
      .mode = structure::FileBrowser::Mode::import,
      .allowed = std::move(allowed_content_types),
      .multiple = allows_multiple_selection};
  structure::FileBrowser::Completions completion{std::move(on_completion)};
  return std::move(*this).sheet(
      is_presented, [request = std::move(request), is_presented,
                     completion = std::move(completion)] {
        return Element{
            structure::FileBrowser{request, is_presented, completion}};
      });
}

template <class Document, class OnCompletion>
auto Element::file_exporter(Binding<bool> is_presented,
                            std::vector<Document> documents,
                            UTType content_type,
                            OnCompletion on_completion) && -> Element {
  static_assert(components::FileDocument<Document>,
                "documents are FileDocument values");
  std::vector<std::vector<std::byte>> encoded;
  for (const Document& document : documents) {
    auto wrapper = document.file_wrapper(
        components::FileDocumentWriteConfiguration{content_type, std::nullopt});
    const auto bytes =
        wrapper.has_value() ? wrapper->regular_file_contents() : std::nullopt;
    encoded.emplace_back(
        bytes.has_value() ? std::vector<std::byte>{bytes->begin(), bytes->end()}
                          : std::vector<std::byte>{});
  }
  structure::FileBrowser::Request request{
      .mode = structure::FileBrowser::Mode::export_document,
      .allowed = {content_type},
      .multiple = true,
      .documents =
          structure::FileBrowser::exported(std::move(encoded), content_type)};
  structure::FileBrowser::Completions completion{std::move(on_completion)};
  return std::move(*this).sheet(
      is_presented, [request = std::move(request), is_presented,
                     completion = std::move(completion)] {
        return Element{
            structure::FileBrowser{request, is_presented, completion}};
      });
}

template <class Item, class OnCompletion>
auto Element::file_exporter(Binding<bool> is_presented,
                            std::optional<Item> item,
                            std::vector<UTType> content_types,
                            std::string default_filename,
                            OnCompletion on_completion) && -> Element {
  static_assert(Transferable<Item>, "the exported item is Transferable");
  std::vector<std::byte> bytes;
  if (item.has_value()) {
    const std::string data = TransferItem::of(*item).data();
    bytes.resize(data.size());
    std::ranges::transform(data, bytes.begin(), [](char character) {
      return static_cast<std::byte>(character);
    });
  }
  UTType type = content_types.empty() ? TransferItem::content_type<Item>()
                                      : content_types.front();
  return std::move(*this).file_exporter(
      std::move(is_presented), std::move(bytes), std::move(type),
      std::move(default_filename), std::move(on_completion));
}

template <class Item, class OnCompletion>
auto Element::file_exporter(Binding<bool> is_presented, std::vector<Item> items,
                            std::vector<UTType> content_types,
                            OnCompletion on_completion) && -> Element {
  static_assert(Transferable<Item>, "exported items are Transferable");
  std::vector<std::vector<std::byte>> encoded;
  for (const Item& item : items) {
    const std::string data = TransferItem::of(item).data();
    std::vector<std::byte> bytes(data.size());
    std::ranges::transform(data, bytes.begin(), [](char character) {
      return static_cast<std::byte>(character);
    });
    encoded.push_back(std::move(bytes));
  }
  UTType type = content_types.empty() ? TransferItem::content_type<Item>()
                                      : content_types.front();
  structure::FileBrowser::Request request{
      .mode = structure::FileBrowser::Mode::export_document,
      .allowed = {type},
      .multiple = true,
      .documents = structure::FileBrowser::exported(std::move(encoded), type)};
  structure::FileBrowser::Completions completion{std::move(on_completion)};
  return std::move(*this).sheet(
      is_presented, [request = std::move(request), is_presented,
                     completion = std::move(completion)] {
        return Element{
            structure::FileBrowser{request, is_presented, completion}};
      });
}

template <class OnCompletion>
auto Element::file_mover(Binding<bool> is_presented, std::vector<FileURL> files,
                         OnCompletion on_completion) && -> Element {
  structure::FileBrowser::Request request{
      .mode = structure::FileBrowser::Mode::move,
      .multiple = true,
      .files = std::move(files)};
  structure::FileBrowser::Completions completion{std::move(on_completion)};
  return std::move(*this).sheet(
      is_presented, [request = std::move(request), is_presented,
                     completion = std::move(completion)] {
        return Element{
            structure::FileBrowser{request, is_presented, completion}};
      });
}

}
