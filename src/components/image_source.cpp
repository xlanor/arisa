#include <arisa/components/image_source.hpp>
#include <arisa/provider/assets/asset_path.hpp>

#include <format>
#include <utility>

namespace arisa::components {

ImageSource::ImageSource(Kind kind, std::string path, std::string key,
                         Assets directory, Loader loader) noexcept
    : kind_(kind),
      path_(std::move(path)),
      key_(std::move(key)),
      directory_(directory),
      loader_(std::move(loader)) {}

auto ImageSource::asset(std::string path) -> ImageSource {
  std::string key = std::format("asset:{}", path);
  return ImageSource{Kind::asset, std::move(path), std::move(key), std::nullopt,
                     nullptr};
}

auto ImageSource::file(const provider::AssetProvider& directory,
                       std::string path) -> ImageSource {
  std::string key =
      std::format("file:{}:{}", static_cast<const void*>(&directory), path);
  return ImageSource{Kind::file, std::move(path), std::move(key),
                     std::cref(directory), nullptr};
}

auto ImageSource::loader(std::string key, Loader load) -> ImageSource {
  std::string keyed = std::format("loader:{}", key);
  return ImageSource{Kind::loader, std::move(key), std::move(keyed),
                     std::nullopt, std::move(load)};
}

auto ImageSource::url(UrlRequest request) -> ImageSource {
  std::string key = std::format("url:{}", request.url().value());
  ImageSource made{Kind::url, std::string{request.url().value()},
                   std::move(key), std::nullopt, nullptr};
  made.request_ = std::move(request);
  return made;
}

auto ImageSource::bytes(std::string key, std::vector<std::byte> encoded)
    -> ImageSource {
  std::string keyed = std::format("bytes:{}", key);
  ImageSource made{Kind::bytes, std::move(key), std::move(keyed), std::nullopt,
                   nullptr};
  made.encoded_ =
      std::make_shared<const std::vector<std::byte>>(std::move(encoded));
  return made;
}

auto ImageSource::resolved(const UrlSession& session) const -> ImageSource {
  if (kind_ != Kind::url || !request_.has_value()) {
    return *this;
  }
  ImageSource made{
      Kind::loader, path_, key_, std::nullopt,
      [session, request = *request_] { return session.data(request); }};
  made.request_ = request_;
  return made;
}

auto ImageSource::read(const provider::AssetProvider& assets,
                       std::string_view path) -> Decoded {
  const auto located = provider::AssetPath::create(path);
  if (!located.has_value()) {
    return std::unexpected(located.error());
  }
  const auto blob = assets.read(*located);
  if (!blob.has_value()) {
    return std::unexpected(blob.error());
  }
  return decoded(blob->bytes());
}

auto ImageSource::decode(const Bytes& bytes) -> Decoded {
  if (!bytes.has_value()) {
    return std::unexpected(bytes.error());
  }
  return decoded(*bytes);
}

auto ImageSource::decoded(std::span<const std::byte> bytes) -> Decoded {
  auto image = provider::Image::decode(bytes);
  if (!image.has_value()) {
    return std::unexpected(image.error());
  }
  return std::make_shared<const provider::Image>(std::move(*image));
}

auto ImageSource::load(ImageSource source, Assets assets)
    -> async::Task<Decoded> {
  switch (source.kind_) {
    case Kind::asset:
      if (!assets.has_value()) {
        co_return std::unexpected(
            provider::ProviderError{provider::ProviderErrorCode::not_found});
      }
      co_return read(assets->get(), source.path_);
    case Kind::file:
      co_return read(source.directory_->get(), source.path_);
    case Kind::loader:
      if (!source.loader_) {
        co_return std::unexpected(
            provider::ProviderError{provider::ProviderErrorCode::not_found});
      }
      co_return decode(co_await source.loader_());
    case Kind::url:
      co_return std::unexpected(
          provider::ProviderError::unsupported_capability());
    case Kind::bytes:
      co_return decoded(*source.encoded_);
  }
  co_return std::unexpected(
      provider::ProviderError{provider::ProviderErrorCode::invalid_argument});
}

}
