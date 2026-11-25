#include <arisa/provider/sdl3/sdl3_clipboard.hpp>

#include <arisa/provider/sdl3/sdl3_owned.hpp>

#include <SDL3/SDL_clipboard.h>

#include <algorithm>
#include <memory>
#include <utility>

namespace arisa::provider::sdl3 {

Sdl3Clipboard::Sdl3Clipboard(Sdl3Context video) noexcept
    : video_(std::move(video)) {}

auto Sdl3Clipboard::create() -> std::expected<Sdl3Clipboard, ProviderError> {
  return Sdl3Context::create(SDL_INIT_VIDEO).transform([](Sdl3Context video) {
    return Sdl3Clipboard{std::move(video)};
  });
}

auto Sdl3Clipboard::read_text() const
    -> std::expected<std::string, ProviderError> {
  const Sdl3Owned<char> text{SDL_GetClipboardText()};
  if (!text) {
    return std::unexpected(ProviderError{ProviderErrorCode::platform_failure});
  }
  return std::string{text.get()};
}

auto Sdl3Clipboard::supply(void* held, const char* mime_type, std::size_t* size)
    -> const void* {
  const auto& offered = *static_cast<const Offered*>(held);
  if (mime_type == nullptr) {
    *size = 0;
    return nullptr;
  }
  const std::string_view wanted{mime_type};
  const auto found =
      std::ranges::find(offered.entries, wanted, &ClipboardEntry::mime_type);
  if (found == offered.entries.end()) {
    *size = 0;
    return nullptr;
  }
  *size = found->data.size();
  return found->data.data();
}

void Sdl3Clipboard::release(void* held) {
  const std::unique_ptr<Offered> owned{static_cast<Offered*>(held)};
}

auto Sdl3Clipboard::write_entries(std::span<const ClipboardEntry> entries)
    -> std::expected<void, ProviderError> {
  if (entries.empty()) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument});
  }
  auto offered = std::make_unique<Offered>();
  offered->entries.assign(entries.begin(), entries.end());
  for (const ClipboardEntry& entry : offered->entries) {
    offered->types.push_back(entry.mime_type.c_str());
  }
  const char** types = offered->types.data();
  const std::size_t count = offered->types.size();
  if (!SDL_SetClipboardData(&Sdl3Clipboard::supply, &Sdl3Clipboard::release,
                            offered.get(), types, count)) {
    return std::unexpected(ProviderError{ProviderErrorCode::platform_failure});
  }
  static_cast<void>(offered.release());
  return {};
}

auto Sdl3Clipboard::read_entry(std::string_view mime_type) const
    -> std::expected<std::string, ProviderError> {
  const std::string terminated{mime_type};
  std::size_t size = 0;
  const Sdl3Owned<char> data{
      static_cast<char*>(SDL_GetClipboardData(terminated.c_str(), &size))};
  if (!data) {
    return std::unexpected(ProviderError{ProviderErrorCode::not_found});
  }
  return std::string{data.get(), size};
}

auto Sdl3Clipboard::mime_types() const -> std::vector<std::string> {
  std::size_t count = 0;
  const Sdl3Owned<char*> listed{SDL_GetClipboardMimeTypes(&count)};
  std::vector<std::string> types;
  if (!listed) {
    return types;
  }
  types.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    types.emplace_back(listed.get()[index]);
  }
  return types;
}

auto Sdl3Clipboard::write_text(std::string_view text)
    -> std::expected<void, ProviderError> {
  if (text.contains('\0')) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument});
  }
  const std::string terminated{text};
  if (!SDL_SetClipboardText(terminated.c_str())) {
    return std::unexpected(ProviderError{ProviderErrorCode::platform_failure});
  }
  return {};
}

}
