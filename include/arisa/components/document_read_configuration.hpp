#pragma once

#include <arisa/composition/ut_type.hpp>

#include <string>
#include <utility>

namespace arisa::components {

class DocumentReadConfiguration {
 public:
  explicit DocumentReadConfiguration(composition::UTType content_type) noexcept
      : content_type_(std::move(content_type)) {}

  auto content_type() const noexcept -> const composition::UTType& {
    return content_type_;
  }

  auto description() const -> std::string {
    return "DocumentReadConfiguration(" +
           std::string{content_type_.identifier()} + ")";
  }

  friend auto operator==(const DocumentReadConfiguration&,
                         const DocumentReadConfiguration&) -> bool = default;

 private:
  composition::UTType content_type_;
};

}
