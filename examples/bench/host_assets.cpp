#include <host_assets.hpp>

#include <arisa/provider/assets/asset_path.hpp>

#include <gallery_fonts.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace arisa::examples {

void HostAssets::load(provider::headless::HeadlessProviderFactory& providers) {
  const std::filesystem::path root{ARISA_SOURCE_ASSETS};
  for (const auto& entry :
       std::filesystem::recursive_directory_iterator{root / "arisa"}) {
    if (!entry.is_regular_file()) {
      continue;
    }
    std::ifstream stream{entry.path(), std::ios::binary};
    const std::vector<char> raw{std::istreambuf_iterator<char>{stream},
                                std::istreambuf_iterator<char>{}};
    auto path = provider::AssetPath::create(
        entry.path().lexically_relative(root).generic_string());
    if (path.has_value()) {
      providers.assets().add(std::move(*path), std::as_bytes(std::span{raw}));
    }
  }
  const std::filesystem::path lettering{ARISA_LETTERING_FONT};
  if (!std::filesystem::is_regular_file(lettering)) {
    return;
  }
  std::ifstream stream{lettering, std::ios::binary};
  const std::vector<char> raw{std::istreambuf_iterator<char>{stream},
                              std::istreambuf_iterator<char>{}};
  auto path =
      provider::AssetPath::create(std::string{GalleryFonts::lettering_path});
  if (path.has_value()) {
    providers.assets().add(std::move(*path), std::as_bytes(std::span{raw}));
  }
}

}
