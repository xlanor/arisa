#pragma once

#include <arisa/components/document_error.hpp>
#include <arisa/components/document_write_configuration.hpp>
#include <arisa/components/file_storage.hpp>
#include <arisa/components/file_wrapper.hpp>
#include <arisa/composition/file_url.hpp>

#include <expected>
#include <functional>
#include <optional>
#include <utility>

namespace arisa::components {

template <class SnapshotType>
class FileWrapperDocumentWriter {
 public:
  using Snapshot = SnapshotType;
  using WriteConfiguration = DocumentWriteConfiguration;
  using MakeFileWrapper =
      std::function<std::expected<FileWrapper, DocumentError>(
          const Snapshot&, const std::optional<FileWrapper>&)>;

  FileWrapperDocumentWriter(WriteConfiguration configuration,
                            MakeFileWrapper make_file_wrapper)
      : configuration_(std::move(configuration)),
        make_file_wrapper_(std::move(make_file_wrapper)) {}

  auto configuration() const noexcept -> const WriteConfiguration& {
    return configuration_;
  }

  auto write(const Snapshot& snapshot, const FileStorage& storage,
             const composition::FileURL& destination,
             const std::optional<Snapshot>& previous) const
      -> std::expected<void, DocumentError> {
    std::optional<FileWrapper> existing;
    if (previous.has_value()) {
      if (auto read = FileWrapper::read(storage, destination);
          read.has_value()) {
        existing = std::move(*read);
      }
    }
    auto made = make_file_wrapper_(snapshot, existing);
    if (!made.has_value()) {
      return std::unexpected(made.error());
    }
    if (auto written = made->write(storage, destination);
        !written.has_value()) {
      return std::unexpected(
          DocumentError{DocumentError::Stage::write, written.error()});
    }
    return {};
  }

 private:
  WriteConfiguration configuration_;
  MakeFileWrapper make_file_wrapper_;
};

}
