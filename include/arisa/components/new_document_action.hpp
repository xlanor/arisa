#pragma once

#include <arisa/components/document_creation_source.hpp>
#include <arisa/components/document_error.hpp>
#include <arisa/components/file_storage.hpp>
#include <arisa/composition/file_url.hpp>
#include <arisa/composition/ut_type.hpp>

#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

namespace arisa::components {

class NewDocumentAction {
 public:
  using Initial = std::function<std::expected<void, DocumentError>(
      const FileStorage&, const composition::FileURL&,
      const composition::UTType&)>;
  using Prepare = std::function<std::optional<composition::FileURL>()>;

  struct Request {
    std::optional<composition::UTType> content_type{};
    std::optional<DocumentCreationSource> source{};
    Initial initial{};
    Prepare prepare{};
  };

  using Handler = std::function<void(Request)>;

  NewDocumentAction() = default;

  explicit NewDocumentAction(Handler handler)
      : handler_(std::make_shared<const Handler>(std::move(handler))) {}

  void operator()(
      std::optional<composition::UTType> content_type = std::nullopt) const {
    perform(Request{.content_type = std::move(content_type)});
  }

  template <class Initialised>
    requires std::constructible_from<Initial, Initialised>
  void operator()(
      Initialised initial,
      std::optional<composition::UTType> content_type = std::nullopt) const {
    perform(Request{.content_type = std::move(content_type),
                    .initial = Initial{std::move(initial)}});
  }

  void perform(Request request) const {
    if (handler_ != nullptr && *handler_) {
      (*handler_)(std::move(request));
    }
  }

  auto available() const noexcept -> bool {
    return handler_ != nullptr && static_cast<bool>(*handler_);
  }

  friend auto operator==(const NewDocumentAction&, const NewDocumentAction&)
      -> bool = default;

 private:
  std::shared_ptr<const Handler> handler_;
};

}
