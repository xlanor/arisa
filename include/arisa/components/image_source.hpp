#pragma once

#include <arisa/async/task.hpp>
#include <arisa/components/url_request.hpp>
#include <arisa/components/url_session.hpp>
#include <arisa/provider/assets/provider.hpp>
#include <arisa/provider/error.hpp>
#include <arisa/provider/gpu/image.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::components {

class ImageSource {
 public:
  using Bytes = std::expected<std::vector<std::byte>, provider::ProviderError>;
  using Loader = std::function<async::Task<Bytes>()>;
  using Decoded = std::expected<std::shared_ptr<const provider::Image>,
                                provider::ProviderError>;
  using Assets =
      std::optional<std::reference_wrapper<const provider::AssetProvider>>;

  enum class Kind : std::uint8_t { asset, file, loader, url, bytes };

  static auto asset(std::string path) -> ImageSource;

  static auto file(const provider::AssetProvider& directory, std::string path)
      -> ImageSource;

  static auto loader(std::string key, Loader load) -> ImageSource;

  static auto url(UrlRequest request) -> ImageSource;

  static auto bytes(std::string key, std::vector<std::byte> encoded)
      -> ImageSource;

  auto resolved(const UrlSession& session) const -> ImageSource;

  auto request() const noexcept -> const std::optional<UrlRequest>& {
    return request_;
  }

  auto kind() const noexcept -> Kind { return kind_; }

  auto key() const noexcept -> std::string_view { return key_; }

  static auto load(ImageSource source, Assets assets) -> async::Task<Decoded>;

  static auto decode(const Bytes& bytes) -> Decoded;

  friend auto operator==(const ImageSource& left, const ImageSource& right)
      -> bool {
    return left.key_ == right.key_;
  }

 private:
  ImageSource(Kind kind, std::string path, std::string key, Assets directory,
              Loader loader) noexcept;

  static auto read(const provider::AssetProvider& assets, std::string_view path)
      -> Decoded;

  static auto decoded(std::span<const std::byte> bytes) -> Decoded;

  Kind kind_;
  std::string path_;
  std::string key_;
  Assets directory_;
  Loader loader_;
  std::optional<UrlRequest> request_;
  std::shared_ptr<const std::vector<std::byte>> encoded_;
};

}
