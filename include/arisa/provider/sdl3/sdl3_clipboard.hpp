#pragma once

#include <arisa/provider/clipboard/provider.hpp>
#include <arisa/provider/sdl3/sdl3_context.hpp>

#include <cstddef>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::provider::sdl3 {

class Sdl3Clipboard final : public ClipboardProvider {
 public:
  [[nodiscard]] static auto create()
      -> std::expected<Sdl3Clipboard, ProviderError>;

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
  struct Offered {
    std::vector<ClipboardEntry> entries;
    std::vector<const char*> types;
  };

  static auto supply(void* held, const char* mime_type, std::size_t* size)
      -> const void*;

  static void release(void* held);

  explicit Sdl3Clipboard(Sdl3Context video) noexcept;

  Sdl3Context video_;
};

}
