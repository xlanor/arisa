#pragma once

#include <arisa/provider/browser/provider.hpp>
#include <arisa/provider/browser/url.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

namespace arisa::components {

class OpenURLAction {
 public:
  class Result {
   public:
    enum class Kind : std::uint8_t {
      handled,
      discarded,
      system_action,
    };

    class SystemAction {
     public:
      operator Result() const { return Result{Kind::system_action}; }

      auto operator()(provider::Url url, bool prefers_in_app = false) const
          -> Result {
        Result redirected{Kind::system_action};
        redirected.url_ = std::move(url);
        redirected.prefers_in_app_ = prefers_in_app;
        return redirected;
      }
    };

    static const Result handled;
    static const Result discarded;
    static constexpr SystemAction system_action{};

    auto kind() const noexcept -> Kind { return kind_; }

    auto url() const noexcept -> const std::optional<provider::Url>& {
      return url_;
    }

    auto prefers_in_app() const noexcept -> bool { return prefers_in_app_; }

    friend auto operator==(const Result&, const Result&) -> bool = default;

   private:
    explicit Result(Kind kind) noexcept : kind_(kind) {}

    Kind kind_;
    std::optional<provider::Url> url_{};
    bool prefers_in_app_{false};
  };

  using Handler = std::function<Result(const provider::Url&)>;

  OpenURLAction() = default;

  explicit OpenURLAction(Handler handler);

  static auto system(provider::BrowserProvider& browser) -> OpenURLAction;

  static auto merged(const OpenURLAction& inherited,
                     const OpenURLAction& declared) -> OpenURLAction;

  auto operator()(const provider::Url& url) const -> bool;

  auto operator()(std::string_view url) const -> bool;

  friend auto operator==(const OpenURLAction&, const OpenURLAction&)
      -> bool = default;

 private:
  std::shared_ptr<const Handler> handler_;
  std::shared_ptr<const OpenURLAction> system_;
};

inline const OpenURLAction::Result OpenURLAction::Result::handled{
    OpenURLAction::Result::Kind::handled};

inline const OpenURLAction::Result OpenURLAction::Result::discarded{
    OpenURLAction::Result::Kind::discarded};

}
