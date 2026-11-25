#include <arisa/support/sparse.hpp>
#include <arisa/support/sparse_recycling.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace arisa::support {

struct Declared {
  std::string name;
  std::vector<int> values;

  friend auto operator==(const Declared&, const Declared&) -> bool = default;
};

struct Owned {
  std::unique_ptr<int> value;
  std::vector<int> values;
};

TEST(SparseTest, ReadingAnEmptyValueSeesTheDefaultAndAllocatesNothing) {
  const Sparse<Declared> empty;
  EXPECT_FALSE(empty.engaged());
  EXPECT_TRUE(empty->name.empty());
  EXPECT_EQ(*empty, Declared{});
  EXPECT_FALSE(empty.engaged());
}

TEST(SparseTest, EditingAllocatesOnceAndKeepsTheBlock) {
  Sparse<Declared> sparse;
  sparse.edit().name = "first";
  const Declared* block = &*sparse;
  sparse.edit().values.push_back(3);
  EXPECT_TRUE(sparse.engaged());
  EXPECT_EQ(&*sparse, block);
  EXPECT_EQ(sparse->name, "first");
  EXPECT_EQ(sparse->values, std::vector<int>{3});
}

TEST(SparseTest, CopyingCopiesTheBlockAndMovingStealsIt) {
  Sparse<Declared> original;
  original.edit().name = "kept";
  Sparse<Declared> copied = original;
  copied.edit().name = "changed";
  EXPECT_EQ(original->name, "kept");
  EXPECT_EQ(copied->name, "changed");
  const Declared* block = &*original;
  Sparse<Declared> moved = std::move(original);
  EXPECT_EQ(&*moved, block);
  Sparse<Declared> assigned;
  assigned = moved;
  EXPECT_NE(&*assigned, &*moved);
  EXPECT_EQ(assigned, moved);
}

TEST(SparseTest, EqualityComparesValuesNotStorage) {
  Sparse<Declared> empty;
  Sparse<Declared> defaulted;
  defaulted.edit();
  EXPECT_EQ(empty, defaulted);
  defaulted.edit().name = "set";
  EXPECT_NE(empty, defaulted);
}

TEST(SparseTest, MoveOnlyBlocksExtractAndTake) {
  Sparse<Owned> sparse;
  EXPECT_EQ(sparse.extract(&Owned::value), nullptr);
  EXPECT_FALSE(sparse.engaged());
  sparse.edit().value = std::make_unique<int>(7);
  sparse.edit().values = {1, 2};
  const std::unique_ptr<int> value = sparse.extract(&Owned::value);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, 7);
  EXPECT_TRUE(sparse.engaged());
  const Owned taken = sparse.take();
  EXPECT_EQ(taken.values, (std::vector<int>{1, 2}));
  EXPECT_FALSE(sparse.engaged());
  EXPECT_TRUE(sparse.take().values.empty());
}

TEST(SparseTest, ResetReturnsToTheDefault) {
  Sparse<Declared> sparse;
  sparse.edit().name = "gone";
  sparse.reset();
  EXPECT_FALSE(sparse.engaged());
  EXPECT_TRUE(sparse->name.empty());
}

TEST(SparseTest, ClearKeepsTheBlockAndRestoresTheDefault) {
  Sparse<Declared> sparse;
  sparse.edit().name = "cleared";
  sparse.edit().values.push_back(4);
  const Declared* block = &*sparse;
  sparse.clear();
  EXPECT_TRUE(sparse.engaged());
  EXPECT_EQ(&*sparse, block);
  EXPECT_EQ(*sparse, Declared{});
  Sparse<Declared> empty;
  empty.clear();
  EXPECT_FALSE(empty.engaged());
}

struct Recycled {
  std::string name;
};

TEST(SparseTest, AReleasedBlockIsReusedByTheNextWriter) {
  if (!SparseRecycling::active()) {
    GTEST_SKIP();
  }
  Sparse<Recycled> first;
  first.edit().name = "released";
  const Recycled* block = &*first;
  first.reset();
  Sparse<Recycled> second;
  EXPECT_TRUE(second.edit().name.empty());
  EXPECT_EQ(&*second, block);
  const Sparse<Recycled> copied = second;
  EXPECT_NE(&*copied, block);
  const Sparse<Recycled> moved = std::move(second);
  EXPECT_EQ(&*moved, block);
}

struct Suspended {
  std::string name;
};

TEST(SparseTest, ASuspensionStopsBlocksBeingShared) {
  if (!SparseRecycling::active()) {
    GTEST_SKIP();
  }
  Sparse<Suspended> kept;
  kept.edit();
  const Suspended* spare = &*kept;
  kept.reset();
  {
    const SparseRecycling::Suspension suspension;
    EXPECT_FALSE(SparseRecycling::active());
    Sparse<Suspended> fresh;
    fresh.edit();
    EXPECT_NE(&*fresh, spare);
  }
  EXPECT_TRUE(SparseRecycling::active());
  Sparse<Suspended> reused;
  reused.edit();
  EXPECT_EQ(&*reused, spare);
}

}
