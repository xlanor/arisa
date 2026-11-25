#pragma once

#include <arisa/support/strong_value.hpp>

#include <cstdint>
#include <memory>

namespace arisa::provider {

struct ThreadIdTag;

using ThreadId = support::StrongValue<std::uint64_t, ThreadIdTag>;

class Thread {
 public:
  class Implementation {
   public:
    virtual ~Implementation() = default;
    virtual void request_stop() noexcept = 0;
    virtual auto id() const noexcept -> ThreadId = 0;
  };

  explicit Thread(std::unique_ptr<Implementation> implementation) noexcept;

  void request_stop() noexcept;

  auto joinable() const noexcept -> bool;

  auto id() const noexcept -> ThreadId;

 private:
  std::unique_ptr<Implementation> implementation_;
};

}
