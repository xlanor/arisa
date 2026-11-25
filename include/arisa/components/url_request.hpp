#pragma once

#include <arisa/provider/browser/url.hpp>

#include <string>
#include <utility>
#include <vector>

namespace arisa::components {

class UrlRequest {
 public:
  struct Header {
    std::string name;
    std::string value;

    friend auto operator==(const Header&, const Header&) -> bool = default;
  };

  explicit UrlRequest(provider::Url url) noexcept : url_(std::move(url)) {}

  UrlRequest(provider::Url url, std::vector<Header> headers) noexcept
      : url_(std::move(url)), headers_(std::move(headers)) {}

  auto url() const noexcept -> const provider::Url& { return url_; }

  auto headers() const noexcept -> const std::vector<Header>& {
    return headers_;
  }

  friend auto operator==(const UrlRequest&, const UrlRequest&)
      -> bool = default;

 private:
  provider::Url url_;
  std::vector<Header> headers_;
};

}
