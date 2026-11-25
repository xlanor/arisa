#include <arisa/provider/clipboard/provider.hpp>

#include <algorithm>

namespace arisa::provider {

auto ClipboardProvider::write_entries(std::span<const ClipboardEntry> entries)
    -> std::expected<void, ProviderError> {
  const auto text =
      std::ranges::find_if(entries, [](const ClipboardEntry& entry) {
        return entry.mime_type.starts_with("text/plain");
      });
  if (text == entries.end()) {
    return std::unexpected(ProviderError::unsupported_capability());
  }
  return write_text(text->data);
}

auto ClipboardProvider::read_entry(std::string_view mime_type) const
    -> std::expected<std::string, ProviderError> {
  if (mime_type.starts_with("text/plain")) {
    return read_text();
  }
  return std::unexpected(ProviderError::unsupported_capability());
}

auto ClipboardProvider::mime_types() const -> std::vector<std::string> {
  return {};
}

}
