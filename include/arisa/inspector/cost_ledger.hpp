#pragma once

#include <arisa/composition/node_cost.hpp>
#include <arisa/composition/node_id.hpp>
#include <arisa/runtime/introspection.hpp>

#include <cstddef>
#include <flat_map>
#include <span>
#include <vector>

namespace arisa::inspector {

class CostLedger {
 public:
  struct Ranked {
    composition::NodeId id{0};
    composition::NodeCost::Counts cost{};

    friend constexpr auto operator==(const Ranked&, const Ranked&)
        -> bool = default;
  };

  using Records = std::span<const runtime::Introspection::CostRecord>;

  void reset(Records now);

  auto advance(Records now, std::size_t limit) -> std::vector<Ranked>;

  void absorb(Records now);

  auto window(std::size_t limit) const -> std::vector<Ranked>;

 private:
  using Ledger =
      std::flat_map<composition::NodeId, composition::NodeCost::Counts>;

  static auto indexed(Records now) -> Ledger;

  static auto since(const composition::NodeCost::Counts& now,
                    const composition::NodeCost::Counts& before) noexcept
      -> composition::NodeCost::Counts;

  static auto ranked(std::vector<Ranked> all, std::size_t limit)
      -> std::vector<Ranked>;

  Ledger previous_;
  Ledger window_;
};

}
