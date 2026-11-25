#pragma once

#include <arisa/components/file_wrapper.hpp>
#include <arisa/composition/ut_type.hpp>

#include <optional>
#include <utility>

namespace arisa::components {

class FileDocumentWriteConfiguration {
 public:
  FileDocumentWriteConfiguration(
      composition::UTType content_type,
      std::optional<FileWrapper> existing_file) noexcept
      : content_type_(std::move(content_type)),
        existing_file_(std::move(existing_file)) {}

  auto content_type() const noexcept -> const composition::UTType& {
    return content_type_;
  }

  auto existing_file() const noexcept -> const std::optional<FileWrapper>& {
    return existing_file_;
  }

 private:
  composition::UTType content_type_;
  std::optional<FileWrapper> existing_file_;
};

}
