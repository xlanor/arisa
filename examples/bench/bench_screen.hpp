#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/provider/gpu/image.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace arisa::examples {

class BenchScreen final : public composition::Component {
 public:
  enum class Mode : std::uint8_t { fling, navigation, sheet, sheet_glass };

  using Photo = std::shared_ptr<const provider::Image>;

  struct Remote {
    std::optional<composition::StateHandle<std::vector<int>>> path;
    std::optional<composition::StateHandle<bool>> sheet;
  };

  static constexpr int rows = 10000;
  static const composition::StateKey<std::vector<int>> path;
  static constexpr composition::StateKey<bool> presented{"bench-sheet", false};

  BenchScreen(Mode mode, std::shared_ptr<Remote> remote,
              Photo photo = nullptr) noexcept
      : mode_(mode), remote_(std::move(remote)), photo_(std::move(photo)) {}

  auto operator==(const BenchScreen& other) const -> bool {
    return mode_ == other.mode_ && remote_ == other.remote_ &&
           photo_ == other.photo_;
  }

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  static auto fling() -> composition::Element;

  static auto navigation(composition::StateHandle<std::vector<int>> levels)
      -> composition::Element;

  static auto sheet(composition::StateHandle<bool> shown)
      -> composition::Element;

  static auto sheet_glass(composition::StateHandle<bool> shown,
                          const Photo& photo) -> composition::Element;

  Mode mode_;
  std::shared_ptr<Remote> remote_;
  Photo photo_;
};

}
