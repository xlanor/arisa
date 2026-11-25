#pragma once

#include <arisa/provider/assets/provider.hpp>
#include <arisa/provider/error.hpp>
#include <arisa/provider/gpu/image.hpp>

#include <cstdint>
#include <expected>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>

namespace arisa::components {

class ImageLibrary {
 public:
  using Loaded = std::expected<std::shared_ptr<const provider::Image>,
                               provider::ProviderError>;

  explicit ImageLibrary(const provider::AssetProvider& assets);

  auto image(std::string_view name) const -> Loaded;

  auto cached() const noexcept -> std::size_t;

  auto reads() const noexcept -> std::uint64_t;

  auto decodes() const noexcept -> std::uint64_t;

  friend auto operator==(const ImageLibrary& left, const ImageLibrary& right)
      -> bool {
    return left.store_ == right.store_;
  }

 private:
  struct Store {
    std::reference_wrapper<const provider::AssetProvider> assets;
    std::map<std::string, Loaded, std::less<>> images;
    std::uint64_t reads{0};
    std::uint64_t decodes{0};
  };

  static auto load(Store& store, std::string_view name) -> Loaded;

  std::shared_ptr<Store> store_;
};

}
