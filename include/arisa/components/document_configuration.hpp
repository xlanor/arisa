#pragma once

#include <arisa/composition/file_url.hpp>

#include <optional>
#include <utility>

namespace arisa::components {

class DocumentConfiguration {
 public:
  DocumentConfiguration(bool is_editable,
                        std::optional<composition::FileURL> file_url) noexcept
      : is_editable_(is_editable), file_url_(std::move(file_url)) {}

  auto is_editable() const noexcept -> bool { return is_editable_; }

  auto file_url() const noexcept -> const std::optional<composition::FileURL>& {
    return file_url_;
  }

  friend auto operator==(const DocumentConfiguration&,
                         const DocumentConfiguration&) -> bool = default;

 private:
  bool is_editable_;
  std::optional<composition::FileURL> file_url_;
};

}
