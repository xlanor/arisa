#pragma once

#include <arisa/composition/file_url.hpp>
#include <arisa/composition/observable.hpp>

#include <functional>
#include <optional>
#include <utility>

namespace arisa::components {

template <class Document>
class ReferenceFileDocumentConfiguration {
 public:
  ReferenceFileDocumentConfiguration(
      composition::Observable<Document>& document,
      std::optional<composition::FileURL> file_url, bool is_editable) noexcept
      : document_(document),
        file_url_(std::move(file_url)),
        is_editable_(is_editable) {}

  auto document() const noexcept -> composition::Observable<Document>& {
    return document_.get();
  }

  auto file_url() const noexcept -> const std::optional<composition::FileURL>& {
    return file_url_;
  }

  auto is_editable() const noexcept -> bool { return is_editable_; }

 private:
  std::reference_wrapper<composition::Observable<Document>> document_;
  std::optional<composition::FileURL> file_url_;
  bool is_editable_;
};

}
