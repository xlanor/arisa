#pragma once

#include <arisa/components/file_wrapper.hpp>
#include <arisa/composition/ut_type.hpp>

#include <utility>

namespace arisa::components {

class FileDocumentReadConfiguration {
 public:
  FileDocumentReadConfiguration(composition::UTType content_type,
                                FileWrapper file) noexcept
      : content_type_(std::move(content_type)), file_(std::move(file)) {}

  auto content_type() const noexcept -> const composition::UTType& {
    return content_type_;
  }

  auto file() const noexcept -> const FileWrapper& { return file_; }

 private:
  composition::UTType content_type_;
  FileWrapper file_;
};

}
