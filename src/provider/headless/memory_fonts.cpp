#include <arisa/provider/headless/memory_fonts.hpp>

namespace arisa::provider::headless {

void MemoryFonts::add(SystemFont font, std::span<const std::byte> bytes) {
  const std::scoped_lock lock{mutex_};
  const std::vector<std::byte>& face =
      faces_.emplace_back(bytes.begin(), bytes.end());
  registered_.insert_or_assign(font, std::span<const std::byte>{face});
}

auto MemoryFonts::system_font(SystemFont font) const
    -> std::expected<FontData, ProviderError> {
  const std::scoped_lock lock{mutex_};
  const auto found = registered_.find(font);
  if (found == registered_.end()) {
    return std::unexpected(ProviderError{ProviderErrorCode::not_found});
  }
  return FontData{found->second};
}

auto MemoryFonts::available(SystemFont font) const -> bool {
  const std::scoped_lock lock{mutex_};
  return registered_.contains(font);
}

}
