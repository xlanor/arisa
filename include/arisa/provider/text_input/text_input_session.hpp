#pragma once

#include <arisa/provider/error.hpp>

#include <expected>
#include <memory>
#include <optional>
#include <string>

namespace arisa::provider {

using TextInputOutcome = std::expected<std::string, ProviderError>;

class TextInputSession {
 public:
  class Implementation {
   public:
    virtual ~Implementation() = default;
    virtual auto poll() -> std::optional<TextInputOutcome> = 0;

    virtual auto draft() const -> std::optional<std::string> {
      return std::nullopt;
    }
  };

  explicit TextInputSession(
      std::unique_ptr<Implementation> implementation) noexcept;

  auto poll() -> std::optional<TextInputOutcome>;

  auto draft() const -> std::optional<std::string>;

  auto active() const noexcept -> bool { return implementation_ != nullptr; }

 private:
  std::unique_ptr<Implementation> implementation_;
};

}
