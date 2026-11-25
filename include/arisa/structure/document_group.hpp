#pragma once

#include <arisa/async/frame_scheduler.hpp>
#include <arisa/async/job.hpp>
#include <arisa/components/document.hpp>
#include <arisa/components/document_creation_context.hpp>
#include <arisa/components/document_creation_source.hpp>
#include <arisa/components/document_error.hpp>
#include <arisa/components/file_document_configuration.hpp>
#include <arisa/components/file_storage.hpp>
#include <arisa/components/file_wrapper.hpp>
#include <arisa/components/new_document_action.hpp>
#include <arisa/components/reference_file_document_configuration.hpp>
#include <arisa/components/url_document_configuration.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/file_url.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/undo_manager.hpp>
#include <arisa/composition/ut_type.hpp>

#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace arisa::structure {

class DocumentGroup : public composition::Element {
 public:
  class Content;
  class Editor;
  class Session;
  class Kind;

  using Sources = std::map<std::string, components::DocumentCreationSource>;

  static constexpr std::chrono::milliseconds autosave_delay{1000};
  static constexpr std::string_view default_title = "Documents";
  static constexpr std::string_view untitled = "Untitled";
  static constexpr std::string_view folder = "Documents";
  static constexpr std::size_t max_untitled = 999;

  template <class Make>
  using Made = typename std::invoke_result_t<
      const Make&, const components::URLDocumentConfiguration&,
      const components::DocumentCreationContext&>::value_type;

  template <components::FileDocument D, class Build>
    requires std::invocable<const Build&,
                            components::FileDocumentConfiguration<D>>
  DocumentGroup(D new_document, Build editor)
      : DocumentGroup(std::make_shared<const FileKind<D, Build>>(
            std::optional<D>{std::move(new_document)}, std::move(editor),
            true)) {}

  template <components::FileDocument D, class Build>
    requires std::invocable<const Build&,
                            components::FileDocumentConfiguration<D>>
  DocumentGroup(std::type_identity<D> viewing, Build viewer)
      : DocumentGroup(std::make_shared<const FileKind<D, Build>>(
            std::nullopt, std::move(viewer), false)) {
    static_cast<void>(viewing);
  }

  template <class Make, class Build>
    requires std::invocable<const Make&> &&
             components::ReferenceFileDocument<
                 std::invoke_result_t<const Make&>> &&
             std::invocable<const Build&,
                            components::ReferenceFileDocumentConfiguration<
                                std::invoke_result_t<const Make&>>>
  DocumentGroup(Make new_document, Build editor)
      : DocumentGroup(
            std::make_shared<
                const ReferenceKind<std::invoke_result_t<const Make&>, Build>>(
                std::function<std::invoke_result_t<const Make&>()>{
                    std::move(new_document)},
                std::move(editor), true)) {}

  template <components::ReferenceFileDocument D, class Build>
    requires std::invocable<const Build&,
                            components::ReferenceFileDocumentConfiguration<D>>
  DocumentGroup(std::type_identity<D> viewing, Build viewer)
      : DocumentGroup(std::make_shared<const ReferenceKind<D, Build>>(
            std::function<D()>{}, std::move(viewer), false)) {
    static_cast<void>(viewing);
  }

  template <class Build, class Make>
    requires std::invocable<const Make&,
                            const components::URLDocumentConfiguration&,
                            const components::DocumentCreationContext&> &&
             components::Document<Made<Make>> &&
             std::invocable<const Build&, composition::Observable<Made<Make>>&>
  DocumentGroup(bool allow_creating, Build editor, Make make_document)
      : DocumentGroup(std::make_shared<const ModernKind<Made<Make>, Build>>(
            typename ModernKind<Made<Make>, Build>::Make{
                std::move(make_document)},
            std::move(editor), allow_creating, true)) {}

  template <class Build, class Make>
    requires std::invocable<const Make&,
                            const components::URLDocumentConfiguration&,
                            const components::DocumentCreationContext&> &&
             components::ReadableDocument<Made<Make>> &&
             std::invocable<const Build&, composition::Observable<Made<Make>>&>
  DocumentGroup(Build viewer, Make make_readable_document)
      : DocumentGroup(std::make_shared<const ModernKind<Made<Make>, Build>>(
            typename ModernKind<Made<Make>, Build>::Make{
                std::move(make_readable_document)},
            std::move(viewer), false, false)) {}

  static auto close_key() -> composition::ElementKey;

  static auto undo_key() -> composition::ElementKey;

  static auto redo_key() -> composition::ElementKey;

  static auto failure_key() -> composition::ElementKey;

 private:
  template <class D, class Build>
  class FileKind;

  template <class D, class Build>
  class ReferenceKind;

  template <class D, class Build>
  class ModernKind;

  template <class D, class Build>
  class FileSession;

  template <class D, class Build>
  class ReferenceSession;

  template <class D, class Build>
  class ModernSession;

  explicit DocumentGroup(std::shared_ptr<const Kind> kind);

  static auto create(const Kind& kind, const components::FileStorage& storage,
                     Sources& sources,
                     components::NewDocumentAction::Request request)
      -> std::expected<composition::FileURL, components::DocumentError>;
};

class DocumentGroup::Session : public std::enable_shared_from_this<Session> {
 public:
  struct Place {
    components::FileStorage storage;
    composition::FileURL url;
    composition::UTType type;
    bool editable{false};
    std::optional<components::DocumentCreationSource> source{};
    std::reference_wrapper<async::FrameScheduler> frames;
  };

  explicit Session(Place place) noexcept : place_(std::move(place)) {}

  Session(const Session&) = delete;
  auto operator=(const Session&) -> Session& = delete;
  Session(Session&&) = delete;
  auto operator=(Session&&) -> Session& = delete;
  virtual ~Session() = default;

  virtual auto editor() -> composition::Element = 0;

  [[nodiscard]] auto save() -> std::expected<void, components::DocumentError>;

  auto url() const noexcept -> const composition::FileURL& {
    return place_.url;
  }

  auto content_type() const noexcept -> const composition::UTType& {
    return place_.type;
  }

  auto editable() const noexcept -> bool { return place_.editable; }

  auto source() const noexcept
      -> const std::optional<components::DocumentCreationSource>& {
    return place_.source;
  }

  auto undo() const noexcept -> const composition::UndoManager& {
    return undo_;
  }

  auto revision() const noexcept
      -> const composition::Observable<std::uint64_t>& {
    return revision_;
  }

  auto dirty() const noexcept -> bool { return dirty_; }

  auto saves() const noexcept -> std::uint64_t { return saves_; }

  auto failure() const noexcept
      -> const std::optional<components::DocumentError>& {
    return failure_;
  }

 protected:
  void changed();

  auto storage() const noexcept -> const components::FileStorage& {
    return place_.storage;
  }

  virtual auto write() -> std::expected<void, components::DocumentError> = 0;

 private:
  Place place_;
  composition::UndoManager undo_;
  composition::Observable<std::uint64_t> revision_{0};
  std::optional<async::Job> autosave_;
  std::optional<components::DocumentError> failure_;
  std::uint64_t saves_{0};
  bool dirty_{false};
};

class DocumentGroup::Kind {
 public:
  Kind() = default;
  Kind(const Kind&) = delete;
  auto operator=(const Kind&) -> Kind& = delete;
  Kind(Kind&&) = delete;
  auto operator=(Kind&&) -> Kind& = delete;
  virtual ~Kind() = default;

  virtual auto readable() const -> std::vector<composition::UTType> = 0;

  virtual auto writable() const -> std::vector<composition::UTType> = 0;

  virtual auto allows_creating() const noexcept -> bool = 0;

  virtual auto open(Session::Place place) const
      -> std::expected<std::shared_ptr<Session>, components::DocumentError> = 0;

  virtual auto write_initial(
      const components::FileStorage& storage, const composition::FileURL& url,
      const composition::UTType& type,
      const std::optional<components::DocumentCreationSource>& source) const
      -> std::expected<void, components::DocumentError> = 0;
};

template <class D, class Build>
class DocumentGroup::FileSession final : public Session {
 public:
  FileSession(Place place, D document, Build build)
      : Session(std::move(place)),
        document_(std::move(document)),
        build_(std::move(build)) {}

  auto editor() -> composition::Element override {
    const std::shared_ptr<FileSession> self =
        std::static_pointer_cast<FileSession>(shared_from_this());
    composition::Binding<D> bound{
        [self] { return self->document_; },
        [self](D value) { self->assign(std::move(value)); }};
    return composition::Element{std::invoke(
        build_, components::FileDocumentConfiguration<D>{
                    std::move(bound), url(), editable(), source()})};
  }

  auto document() const noexcept -> const D& { return document_; }

 private:
  void assign(D value) {
    if (!editable()) {
      return;
    }
    undo().register_undo(
        [weak =
             std::weak_ptr<FileSession>{
                 std::static_pointer_cast<FileSession>(shared_from_this())},
         previous = document_](const composition::UndoManager&) {
          if (const std::shared_ptr<FileSession> alive = weak.lock()) {
            alive->assign(previous);
          }
        });
    document_ = std::move(value);
    changed();
  }

  auto write() -> std::expected<void, components::DocumentError> override {
    std::optional<components::FileWrapper> existing;
    if (auto read = components::FileWrapper::read(storage(), url());
        read.has_value()) {
      existing = std::move(*read);
    }
    auto wrapper =
        document_.file_wrapper(components::FileDocumentWriteConfiguration{
            content_type(), std::move(existing)});
    if (!wrapper.has_value()) {
      return std::unexpected(wrapper.error());
    }
    if (auto written = wrapper->write(storage(), url()); !written.has_value()) {
      return std::unexpected(components::DocumentError{
          components::DocumentError::Stage::write, written.error()});
    }
    return {};
  }

  D document_;
  Build build_;
};

template <class D, class Build>
class DocumentGroup::ReferenceSession final : public Session {
 public:
  ReferenceSession(Place place, D document, Build build)
      : Session(std::move(place)),
        document_(
            std::make_shared<composition::Observable<D>>(std::move(document))),
        build_(std::move(build)) {}

  void watch() {
    watching_ = document_->subscribe(
        [weak = std::weak_ptr<ReferenceSession>{
             std::static_pointer_cast<ReferenceSession>(shared_from_this())}] {
          if (const std::shared_ptr<ReferenceSession> alive = weak.lock()) {
            alive->changed();
          }
        });
  }

  auto editor() -> composition::Element override {
    return composition::Element{
        std::invoke(build_, components::ReferenceFileDocumentConfiguration<D>{
                                *document_, url(), editable()})};
  }

 private:
  auto write() -> std::expected<void, components::DocumentError> override {
    auto snapshot = document_->get().snapshot(content_type());
    if (!snapshot.has_value()) {
      return std::unexpected(snapshot.error());
    }
    auto wrapper =
        D::file_wrapper(*snapshot, components::FileDocumentWriteConfiguration{
                                       content_type(), std::nullopt});
    if (!wrapper.has_value()) {
      return std::unexpected(wrapper.error());
    }
    if (auto written = wrapper->write(storage(), url()); !written.has_value()) {
      return std::unexpected(components::DocumentError{
          components::DocumentError::Stage::write, written.error()});
    }
    return {};
  }

  std::shared_ptr<composition::Observable<D>> document_;
  Build build_;
  composition::Subscription watching_;
};

template <class D, class Build>
class DocumentGroup::ModernSession final : public Session {
 public:
  using Snapshot = typename D::Reader::Snapshot;

  ModernSession(Place place, D document, std::optional<Snapshot> previous,
                Build build)
      : Session(std::move(place)),
        document_(
            std::make_shared<composition::Observable<D>>(std::move(document))),
        previous_(std::move(previous)),
        build_(std::move(build)) {}

  void watch() {
    watching_ = document_->subscribe(
        [weak = std::weak_ptr<ModernSession>{
             std::static_pointer_cast<ModernSession>(shared_from_this())}] {
          if (const std::shared_ptr<ModernSession> alive = weak.lock()) {
            alive->changed();
          }
        });
  }

  auto editor() -> composition::Element override {
    return composition::Element{std::invoke(build_, *document_)};
  }

 private:
  auto write() -> std::expected<void, components::DocumentError> override {
    if constexpr (components::WritableDocument<D>) {
      const D& held = document_->get();
      auto snapshot = held.snapshot(content_type());
      if (!snapshot.has_value()) {
        return std::unexpected(snapshot.error());
      }
      const auto writer =
          held.writer(components::DocumentWriteConfiguration{content_type()});
      std::optional<typename D::Writer::Snapshot> previous;
      if constexpr (std::same_as<typename D::Writer::Snapshot, Snapshot>) {
        previous = previous_;
      }
      if (auto written = writer.write(*snapshot, storage(), url(), previous);
          !written.has_value()) {
        return std::unexpected(written.error());
      }
      if constexpr (std::same_as<typename D::Writer::Snapshot, Snapshot>) {
        previous_ = std::move(*snapshot);
      }
      return {};
    } else {
      return {};
    }
  }

  std::shared_ptr<composition::Observable<D>> document_;
  std::optional<Snapshot> previous_;
  Build build_;
  composition::Subscription watching_;
};

template <class D, class Build>
class DocumentGroup::FileKind final : public Kind {
 public:
  FileKind(std::optional<D> initial, Build build, bool editable)
      : initial_(std::move(initial)),
        build_(std::move(build)),
        editable_(editable) {}

  auto readable() const -> std::vector<composition::UTType> override {
    return components::DocumentContentTypes::readable<D>();
  }

  auto writable() const -> std::vector<composition::UTType> override {
    return components::DocumentContentTypes::writable<D>();
  }

  auto allows_creating() const noexcept -> bool override {
    return editable_ && initial_.has_value();
  }

  auto open(Session::Place place) const
      -> std::expected<std::shared_ptr<Session>,
                       components::DocumentError> override {
    auto wrapper = components::FileWrapper::read(place.storage, place.url);
    if (!wrapper.has_value()) {
      return std::unexpected(components::DocumentError{
          components::DocumentError::Stage::read, wrapper.error()});
    }
    auto document = D::read(
        components::FileDocumentReadConfiguration{place.type, *wrapper});
    if (!document.has_value()) {
      return std::unexpected(document.error());
    }
    place.editable = editable_;
    return std::make_shared<FileSession<D, Build>>(
        std::move(place), std::move(*document), build_);
  }

  auto write_initial(
      const components::FileStorage& storage, const composition::FileURL& url,
      const composition::UTType& type,
      const std::optional<components::DocumentCreationSource>&) const
      -> std::expected<void, components::DocumentError> override {
    if (!initial_.has_value()) {
      return std::unexpected(
          components::DocumentError{components::DocumentError::Stage::create,
                                    std::string{"documents are read only"}});
    }
    auto wrapper = initial_->file_wrapper(
        components::FileDocumentWriteConfiguration{type, std::nullopt});
    if (!wrapper.has_value()) {
      return std::unexpected(wrapper.error());
    }
    if (auto written = wrapper->write(storage, url); !written.has_value()) {
      return std::unexpected(components::DocumentError{
          components::DocumentError::Stage::write, written.error()});
    }
    return {};
  }

 private:
  std::optional<D> initial_;
  Build build_;
  bool editable_;
};

template <class D, class Build>
class DocumentGroup::ReferenceKind final : public Kind {
 public:
  ReferenceKind(std::function<D()> make, Build build, bool editable)
      : make_(std::move(make)), build_(std::move(build)), editable_(editable) {}

  auto readable() const -> std::vector<composition::UTType> override {
    return components::DocumentContentTypes::readable<D>();
  }

  auto writable() const -> std::vector<composition::UTType> override {
    return components::DocumentContentTypes::writable<D>();
  }

  auto allows_creating() const noexcept -> bool override {
    return editable_ && static_cast<bool>(make_);
  }

  auto open(Session::Place place) const
      -> std::expected<std::shared_ptr<Session>,
                       components::DocumentError> override {
    auto wrapper = components::FileWrapper::read(place.storage, place.url);
    if (!wrapper.has_value()) {
      return std::unexpected(components::DocumentError{
          components::DocumentError::Stage::read, wrapper.error()});
    }
    auto document = D::read(
        components::FileDocumentReadConfiguration{place.type, *wrapper});
    if (!document.has_value()) {
      return std::unexpected(document.error());
    }
    place.editable = editable_;
    auto session = std::make_shared<ReferenceSession<D, Build>>(
        std::move(place), std::move(*document), build_);
    session->watch();
    return session;
  }

  auto write_initial(
      const components::FileStorage& storage, const composition::FileURL& url,
      const composition::UTType& type,
      const std::optional<components::DocumentCreationSource>&) const
      -> std::expected<void, components::DocumentError> override {
    if (!make_) {
      return std::unexpected(
          components::DocumentError{components::DocumentError::Stage::create,
                                    std::string{"documents are read only"}});
    }
    const D fresh = make_();
    auto snapshot = fresh.snapshot(type);
    if (!snapshot.has_value()) {
      return std::unexpected(snapshot.error());
    }
    auto wrapper = D::file_wrapper(
        *snapshot,
        components::FileDocumentWriteConfiguration{type, std::nullopt});
    if (!wrapper.has_value()) {
      return std::unexpected(wrapper.error());
    }
    if (auto written = wrapper->write(storage, url); !written.has_value()) {
      return std::unexpected(components::DocumentError{
          components::DocumentError::Stage::write, written.error()});
    }
    return {};
  }

 private:
  std::function<D()> make_;
  Build build_;
  bool editable_;
};

template <class D, class Build>
class DocumentGroup::ModernKind final : public Kind {
 public:
  using Make = std::function<std::expected<D, components::DocumentError>(
      const components::URLDocumentConfiguration&,
      const components::DocumentCreationContext&)>;

  ModernKind(Make make, Build build, bool allow_creating, bool editable)
      : make_(std::move(make)),
        build_(std::move(build)),
        allow_creating_(allow_creating),
        editable_(editable) {}

  auto readable() const -> std::vector<composition::UTType> override {
    return components::DocumentContentTypes::readable<D>();
  }

  auto writable() const -> std::vector<composition::UTType> override {
    if constexpr (components::WritableDocument<D>) {
      return components::DocumentContentTypes::writable<D>();
    } else {
      return {};
    }
  }

  auto allows_creating() const noexcept -> bool override {
    return allow_creating_ && editable_;
  }

  auto open(Session::Place place) const
      -> std::expected<std::shared_ptr<Session>,
                       components::DocumentError> override {
    const components::URLDocumentConfiguration configuration{place.url,
                                                             place.source};
    configuration.set_last_content_modification_date(
        place.storage.stamp(place.url));
    auto document =
        make_(configuration, components::DocumentCreationContext{std::nullopt});
    if (!document.has_value()) {
      return std::unexpected(document.error());
    }
    const auto reader =
        document->reader(components::DocumentReadConfiguration{place.type});
    auto snapshot = reader.read(place.storage, place.url);
    if (!snapshot.has_value()) {
      return std::unexpected(snapshot.error());
    }
    std::optional<typename D::Reader::Snapshot> previous{*snapshot};
    if (auto applied = document->apply(std::move(*snapshot), std::nullopt);
        !applied.has_value()) {
      return std::unexpected(applied.error());
    }
    place.editable = editable_;
    auto session = std::make_shared<ModernSession<D, Build>>(
        std::move(place), std::move(*document), std::move(previous), build_);
    session->watch();
    return session;
  }

  auto write_initial(
      const components::FileStorage& storage, const composition::FileURL& url,
      const composition::UTType& type,
      const std::optional<components::DocumentCreationSource>& source) const
      -> std::expected<void, components::DocumentError> override {
    if constexpr (components::WritableDocument<D>) {
      const components::URLDocumentConfiguration configuration{url, source};
      auto document =
          make_(configuration, components::DocumentCreationContext{source});
      if (!document.has_value()) {
        return std::unexpected(document.error());
      }
      auto snapshot = document->snapshot(type);
      if (!snapshot.has_value()) {
        return std::unexpected(snapshot.error());
      }
      return document->writer(components::DocumentWriteConfiguration{type})
          .write(*snapshot, storage, url, std::nullopt);
    } else {
      return std::unexpected(
          components::DocumentError{components::DocumentError::Stage::create,
                                    std::string{"documents are read only"}});
    }
  }

 private:
  Make make_;
  Build build_;
  bool allow_creating_;
  bool editable_;
};

class DocumentGroup::Editor final : public composition::Component {
 public:
  Editor(std::shared_ptr<const Kind> kind, composition::FileURL url,
         std::shared_ptr<Sources> sources) noexcept
      : kind_(std::move(kind)),
        url_(std::move(url)),
        sources_(std::move(sources)) {}

  auto operator==(const Editor& other) const -> bool {
    return kind_ == other.kind_ && url_ == other.url_;
  }

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  auto chrome(composition::Context& context,
              const std::shared_ptr<Session>& session) const
      -> composition::Element;

  static inline const composition::StateKey<std::shared_ptr<Session>>
      session_key{"arisa.document-group.session", nullptr};

  std::shared_ptr<const Kind> kind_;
  composition::FileURL url_;
  std::shared_ptr<Sources> sources_;
};

class DocumentGroup::Content final : public composition::ElementContent {
 public:
  explicit Content(std::shared_ptr<const Kind> kind)
      : kind_(std::move(kind)), sources_(std::make_shared<Sources>()) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

 private:
  std::shared_ptr<const Kind> kind_;
  std::shared_ptr<Sources> sources_;
};

}
