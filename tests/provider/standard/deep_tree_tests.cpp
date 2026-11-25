#include <arisa/provider/diagnostics/allocation_meter.hpp>
#include <arisa/provider/standard/std_threading.hpp>
#include <arisa/support/dismantle.hpp>
#include <arisa/support/full_order_walk.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stop_token>
#include <utility>
#include <vector>

namespace arisa::provider::standard {

struct Link {
  std::vector<std::unique_ptr<Link>> children;
};

struct LinkAccess {
  using Node = Link;
  using Owned = std::unique_ptr<Link>;

  static auto children(Link& link) noexcept
      -> std::vector<std::unique_ptr<Link>>& {
    return link.children;
  }

  static auto children(std::unique_ptr<Link>& link) noexcept
      -> std::vector<std::unique_ptr<Link>>& {
    return link->children;
  }
};

class DepthGauge {
 public:
  using Walk = support::FullOrderWalk<LinkAccess, std::size_t>;

  auto enter(Link&, const std::size_t& above) -> Walk::Step {
    deepest_ = std::max(deepest_, above + 1);
    return Walk::Step{.state = above + 1};
  }

  void leave(Link&, std::size_t&) { ++left_; }

  auto deepest() const noexcept -> std::size_t { return deepest_; }

  auto left() const noexcept -> std::size_t { return left_; }

 private:
  std::size_t deepest_{0};
  std::size_t left_{0};
};

TEST(DeepTreeTest,
     AHundredThousandLevelChainWalksAndDismantlesOnTheThreadStack) {
  constexpr std::size_t levels = 100'000;
  StdThreadingProvider threading;
  std::atomic<std::size_t> deepest{0};
  std::atomic<std::size_t> left{0};
  std::atomic<std::uint64_t> settled{1};
  std::atomic<bool> emptied{false};
  {
    const auto thread = threading.start(
        ThreadSpec{.name = "deep-tree", .role = ThreadRole::ui},
        [&](const std::stop_token&) {
          Link root;
          Link* tip = &root;
          for (std::size_t index = 1; index < levels; ++index) {
            tip->children.push_back(std::make_unique<Link>());
            tip = tip->children.back().get();
          }
          DepthGauge::Walk walk;
          DepthGauge warm;
          walk.run(root, 0, warm);
          DepthGauge gauge;
          const std::uint64_t before = AllocationMeter::count().value_or(0);
          walk.run(root, 0, gauge);
          settled.store(AllocationMeter::count().value_or(0) - before);
          deepest.store(gauge.deepest());
          left.store(gauge.left());
          support::Dismantle<LinkAccess> dismantle;
          dismantle.run(root.children);
          emptied.store(root.children.empty());
        });
    ASSERT_TRUE(thread.has_value());
  }
  EXPECT_EQ(deepest.load(), levels);
  EXPECT_EQ(left.load(), levels);
  EXPECT_TRUE(emptied.load());
  if (AllocationMeter::counting()) {
    EXPECT_EQ(settled.load(), 0U);
  }
}

}
