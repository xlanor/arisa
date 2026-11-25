#pragma once

#include <functional>
#include <memory>
#include <utility>

namespace arisa::components {

class RenameAction {
 public:
  explicit RenameAction(std::function<void()> rename)
      : rename_(
            std::make_shared<const std::function<void()>>(std::move(rename))) {}

  void operator()() const {
    if (*rename_) {
      (*rename_)();
    }
  }

  friend auto operator==(const RenameAction&, const RenameAction&)
      -> bool = default;

 private:
  std::shared_ptr<const std::function<void()>> rename_;
};

}
