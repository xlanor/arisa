#pragma once

#include <arisa/composition/drop_info.hpp>
#include <arisa/composition/drop_proposal.hpp>

#include <optional>

namespace arisa::composition {

class DropDelegate {
 public:
  DropDelegate() = default;
  DropDelegate(const DropDelegate&) = delete;
  auto operator=(const DropDelegate&) -> DropDelegate& = delete;
  DropDelegate(DropDelegate&&) = delete;
  auto operator=(DropDelegate&&) -> DropDelegate& = delete;
  virtual ~DropDelegate() = default;

  virtual auto validate_drop(const DropInfo& info) -> bool;

  virtual auto perform_drop(const DropInfo& info) -> bool = 0;

  virtual void drop_entered(const DropInfo& info);

  virtual auto drop_updated(const DropInfo& info)
      -> std::optional<DropProposal>;

  virtual void drop_exited(const DropInfo& info);
};

}
