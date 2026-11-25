#include <arisa/components/url_session.hpp>

#include <utility>

namespace arisa::components {

const composition::EnvironmentKey<UrlSession> UrlSession::key{
    "async_image_url_session", UrlSession::shared()};

UrlSession::UrlSession(Load load)
    : load_(load ? std::make_shared<const Load>(std::move(load)) : nullptr) {}

auto UrlSession::shared() -> UrlSession { return UrlSession{}; }

auto UrlSession::supported() const noexcept -> bool { return load_ != nullptr; }

auto UrlSession::data(UrlRequest request) const -> async::Task<Bytes> {
  if (load_ == nullptr) {
    return refused(std::move(request));
  }
  return (*load_)(std::move(request));
}

auto UrlSession::refused(UrlRequest request) -> async::Task<Bytes> {
  static_cast<void>(request);
  co_return std::unexpected(provider::ProviderError::unsupported_capability());
}

}
