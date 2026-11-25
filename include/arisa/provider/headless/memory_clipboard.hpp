#pragma once

#include <arisa/provider/clipboard/provider.hpp>

#include <mutex>
#include <string>
#include <vector>

namespace arisa::provider::headless {

class MemoryClipboard final : public ClipboardProvider {
 public:
  [[nodiscard]] auto read_text() const
      -> std::expected<std::string, ProviderError> override;

  [[nodiscard]] auto write_text(std::string_view text)
      -> std::expected<void, ProviderError> override;

  [[nodiscard]] auto write_entries(std::span<const ClipboardEntry> entries)
      -> std::expected<void, ProviderError> override;

  [[nodiscard]] auto read_entry(std::string_view mime_type) const
      -> std::expected<std::string, ProviderError> override;

  auto mime_types() const -> std::vector<std::string> override;

 private:
  static constexpr std::string_view text_type = "text/plain;charset=utf-8";

  mutable std::mutex mutex_;
  std::vector<ClipboardEntry> entries_;
};

}
