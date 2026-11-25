#include <arisa/components/image_library.hpp>
#include <arisa/provider/assets/asset_path.hpp>

#include <utility>

namespace arisa::components {

ImageLibrary::ImageLibrary(const provider::AssetProvider& assets)
    : store_(std::make_shared<Store>(
          Store{.assets = assets, .images = {}, .reads = 0, .decodes = 0})) {}

auto ImageLibrary::image(std::string_view name) const -> Loaded {
  if (const auto found = store_->images.find(name);
      found != store_->images.end()) {
    return found->second;
  }
  Loaded loaded = load(*store_, name);
  store_->images.emplace(std::string{name}, loaded);
  return loaded;
}

auto ImageLibrary::cached() const noexcept -> std::size_t {
  return store_->images.size();
}

auto ImageLibrary::reads() const noexcept -> std::uint64_t {
  return store_->reads;
}

auto ImageLibrary::decodes() const noexcept -> std::uint64_t {
  return store_->decodes;
}

auto ImageLibrary::load(Store& store, std::string_view name) -> Loaded {
  const provider::AssetProvider& assets = store.assets;
  const auto path = provider::AssetPath::create(name);
  if (!path.has_value()) {
    return std::unexpected(path.error());
  }
  if (!assets.exists(*path)) {
    return std::unexpected(
        provider::ProviderError{provider::ProviderErrorCode::not_found});
  }
  ++store.reads;
  const auto blob = assets.read(*path);
  if (!blob.has_value()) {
    return std::unexpected(blob.error());
  }
  ++store.decodes;
  auto decoded = provider::Image::decode(blob->bytes());
  if (!decoded.has_value()) {
    return std::unexpected(decoded.error());
  }
  return std::make_shared<const provider::Image>(std::move(*decoded));
}

}
