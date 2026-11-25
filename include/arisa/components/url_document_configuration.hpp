#pragma once

#include <arisa/components/document_creation_source.hpp>
#include <arisa/composition/file_url.hpp>
#include <arisa/provider/assets/file_stamp.hpp>

#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace arisa::components {

class URLDocumentConfiguration {
 public:
  URLDocumentConfiguration(
      std::optional<composition::FileURL> file_url,
      std::optional<DocumentCreationSource> creation_source)
      : state_(std::make_shared<State>(
            State{.file_url = std::move(file_url),
                  .last_content_modification_date = std::nullopt,
                  .creation_source = std::move(creation_source)})) {}

  auto file_url() const noexcept -> const std::optional<composition::FileURL>& {
    return state_->file_url;
  }

  void set_file_url(std::optional<composition::FileURL> file_url) const {
    state_->file_url = std::move(file_url);
  }

  auto last_content_modification_date() const noexcept
      -> std::optional<provider::FileStamp> {
    return state_->last_content_modification_date;
  }

  void set_last_content_modification_date(
      std::optional<provider::FileStamp> date) const noexcept {
    state_->last_content_modification_date = date;
  }

  auto creation_source() const noexcept
      -> const std::optional<DocumentCreationSource>& {
    return state_->creation_source;
  }

  auto description() const -> std::string {
    return state_->file_url.has_value()
               ? "URLDocumentConfiguration(" +
                     std::string{state_->file_url->path()} + ")"
               : std::string{"URLDocumentConfiguration(untitled)"};
  }

  friend auto operator==(const URLDocumentConfiguration& left,
                         const URLDocumentConfiguration& right) -> bool {
    return left.state_ == right.state_;
  }

 private:
  struct State {
    std::optional<composition::FileURL> file_url;
    std::optional<provider::FileStamp> last_content_modification_date;
    std::optional<DocumentCreationSource> creation_source;
  };

  std::shared_ptr<State> state_;
};

}
