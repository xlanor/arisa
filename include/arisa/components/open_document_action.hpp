#pragma once

#include <arisa/components/document_error.hpp>
#include <arisa/composition/file_url.hpp>

#include <expected>
#include <functional>
#include <memory>
#include <utility>

namespace arisa::components {

class OpenDocumentAction {
 public:
  using Handler = std::function<std::expected<void, DocumentError>(
      const composition::FileURL&)>;

  OpenDocumentAction() = default;

  explicit OpenDocumentAction(Handler handler)
      : handler_(std::make_shared<const Handler>(std::move(handler))) {}

  [[nodiscard]] auto operator()(const composition::FileURL& url) const
      -> std::expected<void, DocumentError> {
    if (handler_ == nullptr || !*handler_) {
      return std::unexpected(DocumentError{DocumentError::Stage::open,
                                           std::string{"no document scene"}});
    }
    return (*handler_)(url);
  }

  auto available() const noexcept -> bool {
    return handler_ != nullptr && static_cast<bool>(*handler_);
  }

  friend auto operator==(const OpenDocumentAction&, const OpenDocumentAction&)
      -> bool = default;

 private:
  std::shared_ptr<const Handler> handler_;
};

}
