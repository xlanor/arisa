#include <arisa/inspector/cost_ledger.hpp>

#include <algorithm>
#include <utility>

namespace arisa::inspector {

auto CostLedger::indexed(Records now) -> Ledger {
  std::vector<composition::NodeId> ids;
  std::vector<composition::NodeCost::Counts> counts;
  std::vector<runtime::Introspection::CostRecord> sorted{now.begin(),
                                                         now.end()};
  std::ranges::sort(sorted, {}, &runtime::Introspection::CostRecord::id);
  const auto [first, last] =
      std::ranges::unique(sorted, {}, &runtime::Introspection::CostRecord::id);
  sorted.erase(first, last);
  ids.reserve(sorted.size());
  counts.reserve(sorted.size());
  for (const runtime::Introspection::CostRecord& record : sorted) {
    ids.push_back(record.id);
    counts.push_back(record.cost);
  }
  return Ledger{std::sorted_unique, std::move(ids), std::move(counts)};
}

auto CostLedger::since(const composition::NodeCost::Counts& now,
                       const composition::NodeCost::Counts& before) noexcept
    -> composition::NodeCost::Counts {
  const auto field = [](std::uint64_t later, std::uint64_t earlier) {
    return later >= earlier ? later - earlier : later;
  };
  return composition::NodeCost::Counts{
      .evaluations = field(now.evaluations, before.evaluations),
      .layouts = field(now.layouts, before.layouts),
      .records = field(now.records, before.records),
      .shaping = field(now.shaping, before.shaping)};
}

auto CostLedger::ranked(std::vector<Ranked> all, std::size_t limit)
    -> std::vector<Ranked> {
  std::erase_if(all,
                [](const Ranked& entry) { return entry.cost.total() == 0; });
  std::ranges::sort(all, [](const Ranked& left, const Ranked& right) {
    if (left.cost.total() != right.cost.total()) {
      return left.cost.total() > right.cost.total();
    }
    return left.id < right.id;
  });
  if (all.size() > limit) {
    all.resize(limit);
  }
  return all;
}

void CostLedger::reset(Records now) {
  previous_ = indexed(now);
  window_.clear();
}

void CostLedger::absorb(Records now) { previous_ = indexed(now); }

auto CostLedger::advance(Records now, std::size_t limit)
    -> std::vector<Ranked> {
  Ledger current = indexed(now);
  std::vector<Ranked> frame;
  for (const auto& [id, counts] : current) {
    const auto before = previous_.find(id);
    const composition::NodeCost::Counts delta =
        before == previous_.end() ? counts : since(counts, before->second);
    if (delta.total() == 0) {
      continue;
    }
    frame.push_back(Ranked{.id = id, .cost = delta});
    const auto held = window_.find(id);
    if (held == window_.end()) {
      window_.emplace(id, delta);
    } else {
      held->second = held->second + delta;
    }
  }
  previous_ = std::move(current);
  return ranked(std::move(frame), limit);
}

auto CostLedger::window(std::size_t limit) const -> std::vector<Ranked> {
  std::vector<Ranked> all;
  all.reserve(window_.size());
  for (const auto& [id, counts] : window_) {
    all.push_back(Ranked{.id = id, .cost = counts});
  }
  return ranked(std::move(all), limit);
}

}
