#pragma once

#include <arisa/provider/fonts/provider.hpp>

#include <cstddef>
#include <deque>
#include <flat_map>
#include <mutex>
#include <span>
#include <vector>

namespace arisa::provider::headless {

class MemoryFonts final : public FontProvider {
 public:
  void add(SystemFont font, std::span<const std::byte> bytes);

  [[nodiscard]] auto system_font(SystemFont font) const
      -> std::expected<FontData, ProviderError> override;

  auto available(SystemFont font) const -> bool override;

 private:
  mutable std::mutex mutex_;
  std::deque<std::vector<std::byte>> faces_;
  std::flat_map<SystemFont, std::span<const std::byte>> registered_;
};

}
