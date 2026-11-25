#pragma once

#include <arisa/components/document_error.hpp>
#include <arisa/components/document_read_configuration.hpp>
#include <arisa/components/document_write_configuration.hpp>
#include <arisa/components/file_document_read_configuration.hpp>
#include <arisa/components/file_document_write_configuration.hpp>
#include <arisa/components/file_storage.hpp>
#include <arisa/components/file_wrapper.hpp>
#include <arisa/composition/file_url.hpp>
#include <arisa/composition/ut_type.hpp>

#include <concepts>
#include <expected>
#include <optional>
#include <utility>
#include <vector>

namespace arisa::components {

template <class D>
concept DocumentContentTyped = requires {
  {
    D::readable_content_types()
  } -> std::convertible_to<std::vector<composition::UTType>>;
};

template <class D>
concept FileDocument =
    std::copy_constructible<D> && DocumentContentTyped<D> &&
    requires(const D& document, const FileDocumentReadConfiguration& read,
             const FileDocumentWriteConfiguration& write) {
      { D::read(read) } -> std::same_as<std::expected<D, DocumentError>>;
      {
        document.file_wrapper(write)
      } -> std::same_as<std::expected<FileWrapper, DocumentError>>;
    };

template <class D>
concept ReferenceFileDocument =
    DocumentContentTyped<D> && std::move_constructible<D> &&
    requires(const D& document, const FileDocumentReadConfiguration& read,
             const FileDocumentWriteConfiguration& write,
             const composition::UTType& type,
             const typename D::Snapshot& snapshot) {
      { D::read(read) } -> std::same_as<std::expected<D, DocumentError>>;
      {
        document.snapshot(type)
      } -> std::same_as<std::expected<typename D::Snapshot, DocumentError>>;
      {
        D::file_wrapper(snapshot, write)
      } -> std::same_as<std::expected<FileWrapper, DocumentError>>;
    };

template <class R>
concept DocumentReader = requires(const R& reader, const FileStorage& storage,
                                  const composition::FileURL& source) {
  typename R::Snapshot;
  {
    reader.read(storage, source)
  } -> std::same_as<std::expected<typename R::Snapshot, DocumentError>>;
};

template <class W>
concept DocumentWriter = requires(
    const W& writer, const typename W::Snapshot& snapshot,
    const FileStorage& storage, const composition::FileURL& destination,
    const std::optional<typename W::Snapshot>& previous) {
  {
    writer.write(snapshot, storage, destination, previous)
  } -> std::same_as<std::expected<void, DocumentError>>;
};

template <class D>
concept ReadableDocument =
    DocumentContentTyped<D> && std::move_constructible<D> &&
    DocumentReader<typename D::Reader> &&
    requires(D& document, const D& held,
             const DocumentReadConfiguration& configuration,
             typename D::Reader::Snapshot snapshot,
             std::optional<typename D::Reader::Snapshot> previous) {
      { held.reader(configuration) } -> std::same_as<typename D::Reader>;
      {
        document.apply(std::move(snapshot), std::move(previous))
      } -> std::same_as<std::expected<void, DocumentError>>;
    };

template <class D>
concept WritableDocument =
    DocumentWriter<typename D::Writer> &&
    requires(const D& held, const DocumentWriteConfiguration& configuration,
             const composition::UTType& type) {
      {
        D::writable_content_types()
      } -> std::convertible_to<std::vector<composition::UTType>>;
      { held.writer(configuration) } -> std::same_as<typename D::Writer>;
      {
        held.snapshot(type)
      } -> std::same_as<
          std::expected<typename D::Writer::Snapshot, DocumentError>>;
    };

template <class D>
concept Document = ReadableDocument<D> && WritableDocument<D>;

class DocumentContentTypes {
 public:
  template <class D>
  static auto readable() -> std::vector<composition::UTType> {
    return D::readable_content_types();
  }

  template <class D>
  static auto writable() -> std::vector<composition::UTType> {
    if constexpr (requires { D::writable_content_types(); }) {
      return D::writable_content_types();
    } else {
      return D::readable_content_types();
    }
  }

  static auto matching(const std::vector<composition::UTType>& types,
                       const composition::FileURL& file) -> composition::UTType;
};

}
