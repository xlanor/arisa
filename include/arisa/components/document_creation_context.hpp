#pragma once

#include <arisa/components/document_creation_source.hpp>

#include <optional>
#include <string>
#include <utility>

namespace arisa::components {

class DocumentCreationContext {
 public:
  DocumentCreationContext() noexcept = default;

  explicit DocumentCreationContext(
      std::optional<DocumentCreationSource> creation_source) noexcept
      : creation_source_(std::move(creation_source)) {}

  auto creation_source() const noexcept
      -> const std::optional<DocumentCreationSource>& {
    return creation_source_;
  }

  auto description() const -> std::string {
    return creation_source_.has_value()
               ? "DocumentCreationContext(" +
                     std::string{creation_source_->id()} + ")"
               : std::string{"DocumentCreationContext(opened)"};
  }

  friend auto operator==(const DocumentCreationContext&,
                         const DocumentCreationContext&) -> bool = default;

 private:
  std::optional<DocumentCreationSource> creation_source_;
};

}
