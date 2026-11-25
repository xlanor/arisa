#pragma once

#include <arisa/provider/clipboard/clipboard_entry.hpp>
#include <arisa/provider/error.hpp>

#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::provider {

class ClipboardProvider {
 public:
  virtual ~ClipboardProvider() = default;

  [[nodiscard]] virtual auto read_text() const
      -> std::expected<std::string, ProviderError> = 0;

  [[nodiscard]] virtual auto write_text(std::string_view text)
      -> std::expected<void, ProviderError> = 0;

  [[nodiscard]] virtual auto write_entries(
      std::span<const ClipboardEntry> entries)
      -> std::expected<void, ProviderError>;

  [[nodiscard]] virtual auto read_entry(std::string_view mime_type) const
      -> std::expected<std::string, ProviderError>;

  virtual auto mime_types() const -> std::vector<std::string>;
};

}
