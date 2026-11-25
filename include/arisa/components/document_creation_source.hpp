#pragma once

#include <string>
#include <string_view>
#include <utility>

namespace arisa::components {

class DocumentCreationSource {
 public:
  explicit DocumentCreationSource(std::string id) noexcept
      : id_(std::move(id)) {}

  auto id() const noexcept -> std::string_view { return id_; }

  friend auto operator==(const DocumentCreationSource&,
                         const DocumentCreationSource&) -> bool = default;

 private:
  std::string id_;
};

}
