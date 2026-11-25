#include <arisa/support/dismantle.hpp>
#include <arisa/support/full_order_walk.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace arisa::support {

struct Branch {
  std::string name;
  std::vector<Branch> children;
};

struct Rooted {
  std::string name;
  std::vector<std::unique_ptr<Rooted>> children;
};

struct BranchAccess {
  using Node = Branch;
  using Owned = Branch;

  static auto children(Branch& branch) noexcept -> std::vector<Branch>& {
    return branch.children;
  }
};

struct RootedAccess {
  using Node = Rooted;
  using Owned = std::unique_ptr<Rooted>;

  static auto children(Rooted& node) noexcept
      -> std::vector<std::unique_ptr<Rooted>>& {
    return node.children;
  }

  static auto children(std::unique_ptr<Rooted>& node) noexcept
      -> std::vector<std::unique_ptr<Rooted>>& {
    return node->children;
  }
};

class Recorder {
 public:
  using Walk = FullOrderWalk<BranchAccess, int>;

  explicit Recorder(std::string pruned = {}) : pruned_(std::move(pruned)) {}

  auto enter(Branch& branch, const int& depth) -> Walk::Step {
    events_.push_back("+" + branch.name + std::to_string(depth + 1));
    return Walk::Step{.state = depth + 1, .descend = branch.name != pruned_};
  }

  void leave(Branch& branch, int& depth) {
    events_.push_back("-" + branch.name + std::to_string(depth));
  }

  auto events() const -> const std::vector<std::string>& { return events_; }

 private:
  std::string pruned_;
  std::vector<std::string> events_;
};

class FullOrderWalkTest : public ::testing::Test {
 protected:
  static auto sample() -> Branch {
    return Branch{
        .name = "a",
        .children = {Branch{.name = "b",
                            .children = {Branch{.name = "c", .children = {}}}},
                     Branch{.name = "d", .children = {}}}};
  }
};

TEST_F(FullOrderWalkTest, EntersParentsBeforeChildrenAndLeavesAfter) {
  Branch tree = sample();
  Recorder recorder;
  Recorder::Walk walk;
  walk.run(tree, 0, recorder);
  EXPECT_EQ(recorder.events(),
            (std::vector<std::string>{"+a1", "+b2", "+c3", "-c3", "-b2", "+d2",
                                      "-d2", "-a1"}));
  EXPECT_EQ(walk.depth(), 0U);
}

TEST_F(FullOrderWalkTest, ASkippedSubtreeStillLeavesItsRoot) {
  Branch tree = sample();
  Recorder recorder{"b"};
  Recorder::Walk walk;
  walk.run(tree, 0, recorder);
  EXPECT_EQ(recorder.events(), (std::vector<std::string>{"+a1", "+b2", "-b2",
                                                         "+d2", "-d2", "-a1"}));
}

TEST_F(FullOrderWalkTest, EachLevelKeepsItsOwnStateUntilItLeaves) {
  class Summer {
   public:
    using Walk = FullOrderWalk<BranchAccess, std::size_t>;

    auto enter(Branch& branch, const std::size_t& above) -> Walk::Step {
      return Walk::Step{.state = above + branch.name.size()};
    }

    void leave(Branch& branch, std::size_t& total) {
      seen_.push_back(branch.name + "=" + std::to_string(total));
    }

    auto seen() const -> const std::vector<std::string>& { return seen_; }

   private:
    std::vector<std::string> seen_;
  };
  Branch tree{
      .name = "x",
      .children = {Branch{
          .name = "yy", .children = {Branch{.name = "zzz", .children = {}}}}}};
  Summer summer;
  Summer::Walk walk;
  walk.run(tree, 10, summer);
  EXPECT_EQ(summer.seen(),
            (std::vector<std::string>{"zzz=16", "yy=13", "x=11"}));
}

TEST_F(FullOrderWalkTest, ASecondWalkReusesItsLevels) {
  Branch tree = sample();
  Recorder::Walk walk;
  Recorder first;
  walk.run(tree, 0, first);
  const std::size_t capacity = walk.capacity();
  Recorder second;
  walk.run(tree, 0, second);
  EXPECT_EQ(walk.capacity(), capacity);
  EXPECT_EQ(first.events(), second.events());
}

TEST_F(FullOrderWalkTest, PointerChildrenAreWalkedThroughTheirOwners) {
  class Counter {
   public:
    using Walk = FullOrderWalk<RootedAccess, int>;

    auto enter(Rooted& node, const int&) -> Walk::Step {
      names_ += node.name;
      return Walk::Step{.state = 0};
    }

    void leave(Rooted&, int&) {}

    auto names() const -> const std::string& { return names_; }

   private:
    std::string names_;
  };
  Rooted root{.name = "r", .children = {}};
  root.children.push_back(
      std::make_unique<Rooted>(Rooted{.name = "s", .children = {}}));
  root.children.push_back(
      std::make_unique<Rooted>(Rooted{.name = "t", .children = {}}));
  Counter counter;
  Counter::Walk walk;
  walk.run(root, 0, counter);
  EXPECT_EQ(counter.names(), "rst");
}

class Chooser {
 public:
  struct Cursor {
    std::size_t next{0};
    bool second{false};
  };

  using Walk = FullOrderWalk<BranchAccess, Cursor>;

  explicit Chooser(std::string stop = {}) : stop_(std::move(stop)) {}

  auto enter(Branch& branch, const Cursor&) -> Walk::Step {
    events_.push_back("+" + branch.name);
    stopped_ = stopped_ || branch.name == stop_;
    return Walk::Step{.state = Cursor{}};
  }

  auto next(Branch& branch, Cursor& cursor) -> Branch* {
    const std::size_t count = branch.children.size();
    while (!stopped_ && cursor.next < count) {
      Branch& child = branch.children[count - 1 - cursor.next++];
      if (!cursor.second && child.name.starts_with("p")) {
        continue;
      }
      if (cursor.second && !child.name.starts_with("p")) {
        continue;
      }
      return &child;
    }
    if (!stopped_ && !cursor.second) {
      cursor = Cursor{.next = 0, .second = true};
      return next(branch, cursor);
    }
    return nullptr;
  }

  void leave(Branch& branch, Cursor&) { events_.push_back("-" + branch.name); }

  auto events() const -> const std::vector<std::string>& { return events_; }

 private:
  std::string stop_;
  bool stopped_{false};
  std::vector<std::string> events_;
};

class FullOrderChoiceTest : public ::testing::Test {
 protected:
  static auto sample() -> Branch {
    return Branch{
        .name = "a",
        .children = {Branch{.name = "b",
                            .children = {Branch{.name = "c", .children = {}},
                                         Branch{.name = "pd", .children = {}}}},
                     Branch{.name = "pe", .children = {}},
                     Branch{.name = "f", .children = {}}}};
  }
};

TEST_F(FullOrderChoiceTest, TheVisitorChoosesTheOrderAndASecondPass) {
  Branch tree = sample();
  Chooser chooser;
  Chooser::Walk walk;
  walk.run(tree, Chooser::Cursor{}, chooser);
  EXPECT_EQ(chooser.events(),
            (std::vector<std::string>{"+a", "+f", "-f", "+b", "+c", "-c", "+pd",
                                      "-pd", "-b", "+pe", "-pe", "-a"}));
}

TEST_F(FullOrderChoiceTest, StoppingLeavesEveryOpenLevelInnermostFirst) {
  Branch tree = sample();
  Chooser chooser{"c"};
  Chooser::Walk walk;
  walk.run(tree, Chooser::Cursor{}, chooser);
  EXPECT_EQ(chooser.events(),
            (std::vector<std::string>{"+a", "+f", "-f", "+b", "+c", "-c", "-b",
                                      "-a"}));
  EXPECT_EQ(walk.depth(), 0U);
}

TEST_F(FullOrderChoiceTest, ADeepChainNeedsNoMachineStackPerLevel) {
  Branch tree{.name = "r", .children = {}};
  Branch* tip = &tree;
  constexpr std::size_t depth = 20000;
  for (std::size_t level = 0; level < depth; ++level) {
    tip->children.push_back(Branch{.name = "n", .children = {}});
    tip = &tip->children.back();
  }
  Recorder recorder;
  Recorder::Walk walk;
  walk.run(tree, 0, recorder);
  EXPECT_EQ(recorder.events().size(), 2 * (depth + 1));
  EXPECT_EQ(recorder.events()[depth], "+n" + std::to_string(depth + 1));
  Dismantle<BranchAccess> dismantle;
  dismantle.run(tree.children);
}

TEST_F(FullOrderChoiceTest, ALeavingLevelHandsItsResultToItsParent) {
  class Counter {
   public:
    using Walk = FullOrderWalk<BranchAccess, std::size_t>;

    auto enter(Branch&, const std::size_t&) -> Walk::Step {
      return Walk::Step{.state = 1};
    }

    void leave(Branch& branch, std::size_t& size, std::size_t* above) {
      sizes_.push_back(branch.name + "=" + std::to_string(size));
      if (above != nullptr) {
        *above += size;
      } else {
        total_ = size;
      }
    }

    auto sizes() const -> const std::vector<std::string>& { return sizes_; }

    auto total() const noexcept -> std::size_t { return total_; }

   private:
    std::vector<std::string> sizes_;
    std::size_t total_{0};
  };
  Branch tree = sample();
  Counter counter;
  Counter::Walk walk;
  walk.run(tree, 0, counter);
  EXPECT_EQ(counter.sizes(), (std::vector<std::string>{"c=1", "pd=1", "b=3",
                                                       "pe=1", "f=1", "a=6"}));
  EXPECT_EQ(counter.total(), 6U);
}

TEST(DismantleTest, TearsDownEveryDescendantAndReusesItsWorklist) {
  std::vector<Branch> forest;
  forest.push_back(Branch{
      .name = "a",
      .children = {Branch{.name = "b",
                          .children = {Branch{.name = "c", .children = {}}}}}});
  Dismantle<BranchAccess> dismantle;
  EXPECT_TRUE(Dismantle<BranchAccess>::nested(forest));
  dismantle.run(forest);
  EXPECT_TRUE(forest.empty());
  const std::size_t capacity = dismantle.capacity();
  forest.push_back(Branch{
      .name = "d",
      .children = {Branch{.name = "e",
                          .children = {Branch{.name = "f", .children = {}}}}}});
  dismantle.run(forest);
  EXPECT_EQ(dismantle.capacity(), capacity);
  EXPECT_FALSE(Dismantle<BranchAccess>::nested(forest));
}

}
