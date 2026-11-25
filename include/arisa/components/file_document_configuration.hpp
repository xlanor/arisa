#pragma once

#include <arisa/components/document_creation_source.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/file_url.hpp>

#include <optional>
#include <utility>

namespace arisa::components {

template <class Document>
class FileDocumentConfiguration {
 public:
  FileDocumentConfiguration(
      composition::Binding<Document> document,
      std::optional<composition::FileURL> file_url, bool is_editable,
      std::optional<DocumentCreationSource> creation_source) noexcept
      : document_(std::move(document)),
        file_url_(std::move(file_url)),
        is_editable_(is_editable),
        creation_source_(std::move(creation_source)) {}

  auto document() const -> Document { return document_.get(); }

  void set_document(Document document) const {
    document_.set(std::move(document));
  }

  auto document_binding() const -> const composition::Binding<Document>& {
    return document_;
  }

  auto file_url() const noexcept -> const std::optional<composition::FileURL>& {
    return file_url_;
  }

  auto is_editable() const noexcept -> bool { return is_editable_; }

  auto creation_source() const noexcept
      -> const std::optional<DocumentCreationSource>& {
    return creation_source_;
  }

 private:
  composition::Binding<Document> document_;
  std::optional<composition::FileURL> file_url_;
  bool is_editable_;
  std::optional<DocumentCreationSource> creation_source_;
};

}
