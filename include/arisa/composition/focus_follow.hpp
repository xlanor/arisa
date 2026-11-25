#pragma once

#include <arisa/composition/element_type.hpp>
#include <arisa/composition/focus_request.hpp>

#include <functional>
#include <memory>

namespace arisa::composition {

class FocusFollow final : public FocusRequest {
 public:
  using Action = std::function<void()>;

  FocusFollow(std::unique_ptr<FocusRequest> request, Action follow) noexcept
      : request_(std::move(request)), follow_(std::move(follow)) {}

  auto kind() const noexcept -> ElementType override;

  auto requested() const -> bool override;

  void grant() const override;

  void release() const override;

  auto same_target(const FocusRequest& other) const -> bool override;

 private:
  std::unique_ptr<FocusRequest> request_;
  Action follow_;
};

}
