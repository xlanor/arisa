#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>

#include <bench_screen.hpp>

#include <cstdint>
#include <memory>
#include <optional>

namespace arisa::examples {

class SoakScreen final : public composition::Component {
 public:
  enum class Mode : std::uint8_t { gallery, fling, sheet, navigation };

  struct Remote {
    std::optional<composition::StateHandle<Mode>> mode;
    std::shared_ptr<BenchScreen::Remote> bench{
        std::make_shared<BenchScreen::Remote>()};
  };

  static constexpr composition::StateKey<Mode> showing{"soak-mode",
                                                       Mode::gallery};

  explicit SoakScreen(std::shared_ptr<Remote> remote) noexcept
      : remote_(std::move(remote)) {}

  auto operator==(const SoakScreen& other) const -> bool {
    return remote_ == other.remote_;
  }

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  std::shared_ptr<Remote> remote_;
};

}
