#include <arisa/composition/pasteboard.hpp>

#include <algorithm>
#include <expected>
#include <string>
#include <utility>

namespace arisa::composition {

struct Pasteboard::State {
  std::vector<TransferItem> items;
  provider::ClipboardProvider* clipboard{nullptr};
  std::string written;
  std::vector<std::string> offered;
  std::uint64_t changes{0};
};

const EnvironmentKey<std::optional<Pasteboard>> Pasteboard::general{
    "arisa.pasteboard", std::nullopt};

Pasteboard::Pasteboard() : state_(std::make_shared<State>()) {}

Pasteboard::Pasteboard(provider::ClipboardProvider& clipboard)
    : state_(std::make_shared<State>()) {
  state_->clipboard = &clipboard;
}

void Pasteboard::set_items(std::vector<TransferItem> items) const {
  State& state = *state_;
  state.items = std::move(items);
  state.offered.clear();
  ++state.changes;
  if (state.clipboard == nullptr) {
    return;
  }
  std::vector<provider::ClipboardEntry> entries;
  entries.reserve(state.items.size());
  for (const TransferItem& item : state.items) {
    std::string mime = item.type().mime_type();
    if (std::ranges::contains(entries, mime,
                              &provider::ClipboardEntry::mime_type)) {
      continue;
    }
    entries.push_back(provider::ClipboardEntry{.mime_type = std::move(mime),
                                               .data = item.data()});
  }
  if (!entries.empty() && state.clipboard->write_entries(entries).has_value()) {
    for (const provider::ClipboardEntry& entry : entries) {
      state.offered.push_back(entry.mime_type);
    }
    state.written = state.clipboard->read_text().value_or(std::string{});
    return;
  }
  const auto text =
      std::ranges::find(state.items, UTType::plain_text(), &TransferItem::type);
  if (text != state.items.end() &&
      state.clipboard->write_text(text->data()).has_value()) {
    state.written = text->data();
    return;
  }
  state.written = state.clipboard->read_text().value_or(std::string{});
}

auto Pasteboard::items() const -> std::vector<TransferItem> {
  const State& state = *state_;
  if (state.clipboard == nullptr) {
    return state.items;
  }
  const std::vector<std::string> types = state.clipboard->mime_types();
  const std::expected<std::string, provider::ProviderError> text =
      state.clipboard->read_text();
  const bool ours = (types.empty() || types == state.offered) &&
                    text.value_or(std::string{}) == state.written;
  if (ours) {
    return state.items;
  }
  std::vector<TransferItem> found;
  for (const std::string& mime : types) {
    const std::optional<UTType> type = UTType::from_mime_type(mime);
    if (!type.has_value() ||
        std::ranges::contains(found, *type, &TransferItem::type)) {
      continue;
    }
    if (std::expected<std::string, provider::ProviderError> data =
            state.clipboard->read_entry(mime);
        data.has_value()) {
      found.emplace_back(*type, std::move(*data));
    }
  }
  if (found.empty() && text.has_value() && !text->empty()) {
    found.push_back(TransferItem::of(*text));
  }
  return found.empty() ? state.items : found;
}

auto Pasteboard::contains(const UTType& type) const -> bool {
  return std::ranges::contains(items(), type, &TransferItem::type);
}

auto Pasteboard::change_count() const noexcept -> std::uint64_t {
  return state_->changes;
}

}
