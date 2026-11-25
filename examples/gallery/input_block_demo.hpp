#pragma once

#include <arisa/async/task.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <input_blocker.hpp>

#include <chrono>
#include <memory>
#include <string_view>

namespace arisa::examples {

class InputBlockDemo final : public composition::Component {
 public:
  static constexpr composition::StateKey<bool> held{"input-block-held", false};
  static constexpr std::string_view block_button = "input-block";
  static constexpr std::chrono::seconds duration{3};

  auto operator==(const InputBlockDemo&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  static auto expire(composition::StateHandle<bool> holding,
                     std::shared_ptr<InputBlocker> blocker)
      -> async::Task<void>;
};

}
