#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>

#include <cstdint>
#include <memory>
#include <optional>

namespace arisa::examples {

class SplitScreen final : public composition::Component {
 public:
  class Detail;

  struct Remote {
    std::optional<composition::StateHandle<int>> selection;
    std::uint64_t detail_builds{0};
  };

  static constexpr int rows = 12;
  static constexpr composition::StateKey<int> selected{"split-bench-selected",
                                                       0};

  explicit SplitScreen(std::shared_ptr<Remote> remote) noexcept
      : remote_(std::move(remote)) {}

  auto operator==(const SplitScreen&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  std::shared_ptr<Remote> remote_;
};

class SplitScreen::Detail final : public composition::Component {
 public:
  Detail(int row, std::shared_ptr<Remote> remote) noexcept
      : row_(row), remote_(std::move(remote)) {}

  auto operator==(const Detail&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  int row_;
  std::shared_ptr<Remote> remote_;
};

}
