#pragma once

#include <arisa/async/task.hpp>
#include <arisa/components/url_request.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/environment_key.hpp>
#include <arisa/provider/error.hpp>

#include <cstddef>
#include <expected>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace arisa::components {

class UrlSession {
 public:
  using Bytes = std::expected<std::vector<std::byte>, provider::ProviderError>;
  using Load = std::function<async::Task<Bytes>(UrlRequest)>;

  static const composition::EnvironmentKey<UrlSession> key;

  explicit UrlSession(Load load);

  static auto shared() -> UrlSession;

  auto supported() const noexcept -> bool;

  auto data(UrlRequest request) const -> async::Task<Bytes>;

  friend auto operator==(const UrlSession& left, const UrlSession& right)
      -> bool {
    return left.load_ == right.load_;
  }

 private:
  UrlSession() noexcept = default;

  static auto refused(UrlRequest request) -> async::Task<Bytes>;

  std::shared_ptr<const Load> load_;
};

}

namespace arisa::composition {

template <class Session>
auto Element::async_image_url_session(Session session) && -> Element {
  return std::move(*this).environment(components::UrlSession::key,
                                      components::UrlSession{session});
}

}
