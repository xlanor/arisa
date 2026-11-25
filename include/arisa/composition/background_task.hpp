#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace arisa::composition {

class BackgroundTask {
 public:
  enum class Kind : std::uint8_t {
    app_refresh,
    processing_task,
    url_session,
  };

  using Matching = std::function<bool(std::string_view identifier)>;

  struct Request {
    Kind kind{Kind::app_refresh};
    std::string identifier{};

    friend auto operator==(const Request&, const Request&) -> bool = default;
  };

  using Action = std::function<void(const Request&)>;

  class URLSession {
   public:
    operator BackgroundTask() const {
      return BackgroundTask{Kind::url_session, std::nullopt, nullptr};
    }

    auto operator()(std::string identifier) const -> BackgroundTask {
      return BackgroundTask{Kind::url_session, std::move(identifier), nullptr};
    }

    auto operator()(Matching matching) const -> BackgroundTask {
      return BackgroundTask{
          Kind::url_session, std::nullopt,
          std::make_shared<const Matching>(std::move(matching))};
    }
  };

  static constexpr URLSession url_session{};

  static auto app_refresh(std::string identifier) -> BackgroundTask;

  static auto processing_task(std::string identifier) -> BackgroundTask;

  auto kind() const noexcept -> Kind { return kind_; }

  auto identifier() const noexcept -> const std::optional<std::string>& {
    return identifier_;
  }

  auto matches(const Request& request) const -> bool;

  friend auto operator==(const BackgroundTask& left,
                         const BackgroundTask& right) -> bool {
    return left.kind_ == right.kind_ && left.identifier_ == right.identifier_ &&
           left.matching_ == right.matching_;
  }

 private:
  BackgroundTask(Kind kind, std::optional<std::string> identifier,
                 std::shared_ptr<const Matching> matching) noexcept
      : kind_(kind),
        identifier_(std::move(identifier)),
        matching_(std::move(matching)) {}

  Kind kind_;
  std::optional<std::string> identifier_;
  std::shared_ptr<const Matching> matching_;
};

}
