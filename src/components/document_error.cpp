#include <arisa/components/document_error.hpp>

#include <format>

namespace arisa::components {

auto DocumentError::stage_name(Stage stage) noexcept -> std::string_view {
  switch (stage) {
    case Stage::open:
      return "open";
    case Stage::read:
      return "read";
    case Stage::decode:
      return "decode";
    case Stage::encode:
      return "encode";
    case Stage::write:
      return "write";
    case Stage::create:
      return "create";
  }
  return "open";
}

auto DocumentError::explanation() const -> std::string {
  if (file_.has_value()) {
    return std::format("document {} failed: {}", stage_name(stage_),
                       file_->explanation());
  }
  return std::format("document {} failed: {}", stage_name(stage_), reason_);
}

}
