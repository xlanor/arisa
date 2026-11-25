#pragma once

#include <arisa/composition/environment_key.hpp>
#include <arisa/runtime/input_block.hpp>
#include <arisa/runtime/input_passthrough.hpp>
#include <arisa/runtime/root.hpp>

#include <functional>
#include <memory>
#include <optional>

namespace arisa::examples {

class InputBlocker {
 public:
  static const composition::EnvironmentKey<std::shared_ptr<InputBlocker>> key;

  void attach(runtime::Root& root) noexcept { root_ = root; }

  void hold();

  void release() noexcept;

  auto holding() const noexcept -> bool { return block_.has_value(); }

 private:
  std::optional<std::reference_wrapper<runtime::Root>> root_;
  std::optional<runtime::InputBlock> block_;
  std::optional<runtime::InputPassthrough> pass_;
};

}
