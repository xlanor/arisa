#include <arisa/provider/headless/memory_clipboard.hpp>

#include <algorithm>

namespace arisa::provider::headless {

auto MemoryClipboard::read_text() const
    -> std::expected<std::string, ProviderError> {
  const std::scoped_lock lock{mutex_};
  const auto found =
      std::ranges::find_if(entries_, [](const ClipboardEntry& entry) {
        return entry.mime_type.starts_with("text/plain");
      });
  return found != entries_.end() ? found->data : std::string{};
}

auto MemoryClipboard::write_text(std::string_view text)
    -> std::expected<void, ProviderError> {
  const std::scoped_lock lock{mutex_};
  entries_.clear();
  entries_.push_back(ClipboardEntry{.mime_type = std::string{text_type},
                                    .data = std::string{text}});
  return {};
}

auto MemoryClipboard::write_entries(std::span<const ClipboardEntry> entries)
    -> std::expected<void, ProviderError> {
  const std::scoped_lock lock{mutex_};
  entries_.assign(entries.begin(), entries.end());
  return {};
}

auto MemoryClipboard::read_entry(std::string_view mime_type) const
    -> std::expected<std::string, ProviderError> {
  const std::scoped_lock lock{mutex_};
  const auto found =
      std::ranges::find(entries_, mime_type, &ClipboardEntry::mime_type);
  if (found == entries_.end()) {
    return std::unexpected(ProviderError{ProviderErrorCode::not_found});
  }
  return found->data;
}

auto MemoryClipboard::mime_types() const -> std::vector<std::string> {
  const std::scoped_lock lock{mutex_};
  std::vector<std::string> types;
  types.reserve(entries_.size());
  for (const ClipboardEntry& entry : entries_) {
    types.push_back(entry.mime_type);
  }
  return types;
}

}
