#include <arisa/provider/text_input/text_input_session.hpp>

#include <utility>

namespace arisa::provider {

TextInputSession::TextInputSession(
    std::unique_ptr<Implementation> implementation) noexcept
    : implementation_(std::move(implementation)) {}

auto TextInputSession::poll() -> std::optional<TextInputOutcome> {
  if (implementation_ == nullptr) {
    return std::nullopt;
  }
  return implementation_->poll();
}

auto TextInputSession::draft() const -> std::optional<std::string> {
  if (implementation_ == nullptr) {
    return std::nullopt;
  }
  return implementation_->draft();
}

}
