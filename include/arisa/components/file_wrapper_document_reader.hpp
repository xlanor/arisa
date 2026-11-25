#pragma once

#include <arisa/components/document_error.hpp>
#include <arisa/components/document_read_configuration.hpp>
#include <arisa/components/file_storage.hpp>
#include <arisa/components/file_wrapper.hpp>
#include <arisa/composition/file_url.hpp>

#include <expected>
#include <functional>
#include <utility>

namespace arisa::components {

template <class SnapshotType>
class FileWrapperDocumentReader {
 public:
  using Snapshot = SnapshotType;
  using ReadConfiguration = DocumentReadConfiguration;
  using MakeSnapshot =
      std::function<std::expected<Snapshot, DocumentError>(const FileWrapper&)>;

  FileWrapperDocumentReader(ReadConfiguration configuration,
                            MakeSnapshot make_snapshot)
      : configuration_(std::move(configuration)),
        make_snapshot_(std::move(make_snapshot)) {}

  auto configuration() const noexcept -> const ReadConfiguration& {
    return configuration_;
  }

  auto read(const FileStorage& storage,
            const composition::FileURL& source) const
      -> std::expected<Snapshot, DocumentError> {
    auto wrapper = FileWrapper::read(storage, source);
    if (!wrapper.has_value()) {
      return std::unexpected(
          DocumentError{DocumentError::Stage::read, wrapper.error()});
    }
    return make_snapshot_(*wrapper);
  }

 private:
  ReadConfiguration configuration_;
  MakeSnapshot make_snapshot_;
};

}
