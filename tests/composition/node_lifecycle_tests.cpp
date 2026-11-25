#include <composition/node_lifecycle.hpp>

#include <gtest/gtest.h>

#include <boost/sml.hpp>

#include <array>
#include <cstdint>

namespace arisa::composition {

class NodeLifecycleTableTest : public ::testing::Test {
 protected:
  using Testing = BasicNodeLifecycle<boost::sml::testing>;

  enum class Verb : std::uint8_t {
    appear,
    depart,
    retire,
    abandon,
  };

  struct Row {
    NodeLifecyclePhase from;
    Verb verb;
    bool accepted;
    NodeLifecyclePhase to;
  };

  static constexpr std::array rows{
      Row{NodeLifecyclePhase::mounting, Verb::appear, true,
          NodeLifecyclePhase::live},
      Row{NodeLifecyclePhase::mounting, Verb::depart, false,
          NodeLifecyclePhase::mounting},
      Row{NodeLifecyclePhase::mounting, Verb::retire, false,
          NodeLifecyclePhase::mounting},
      Row{NodeLifecyclePhase::mounting, Verb::abandon, true,
          NodeLifecyclePhase::retired},
      Row{NodeLifecyclePhase::live, Verb::appear, false,
          NodeLifecyclePhase::live},
      Row{NodeLifecyclePhase::live, Verb::depart, true,
          NodeLifecyclePhase::departing},
      Row{NodeLifecyclePhase::live, Verb::retire, false,
          NodeLifecyclePhase::live},
      Row{NodeLifecyclePhase::live, Verb::abandon, true,
          NodeLifecyclePhase::retired},
      Row{NodeLifecyclePhase::departing, Verb::appear, false,
          NodeLifecyclePhase::departing},
      Row{NodeLifecyclePhase::departing, Verb::depart, false,
          NodeLifecyclePhase::departing},
      Row{NodeLifecyclePhase::departing, Verb::retire, true,
          NodeLifecyclePhase::retired},
      Row{NodeLifecyclePhase::departing, Verb::abandon, true,
          NodeLifecyclePhase::retired},
      Row{NodeLifecyclePhase::retired, Verb::appear, false,
          NodeLifecyclePhase::retired},
      Row{NodeLifecyclePhase::retired, Verb::depart, false,
          NodeLifecyclePhase::retired},
      Row{NodeLifecyclePhase::retired, Verb::retire, false,
          NodeLifecyclePhase::retired},
      Row{NodeLifecyclePhase::retired, Verb::abandon, false,
          NodeLifecyclePhase::retired},
  };

  static auto apply(Testing& lifecycle, Verb verb) -> bool {
    switch (verb) {
      case Verb::appear:
        return lifecycle.appear();
      case Verb::depart:
        return lifecycle.depart();
      case Verb::retire:
        return lifecycle.retire();
      case Verb::abandon:
        return lifecycle.abandon();
    }
    return false;
  }
};

TEST_F(NodeLifecycleTableTest, DrivesEveryRowIncludingRejectedEvents) {
  for (const Row& row : rows) {
    Testing lifecycle;
    lifecycle.start_in(row.from);
    ASSERT_EQ(lifecycle.phase(), row.from);
    EXPECT_EQ(apply(lifecycle, row.verb), row.accepted)
        << "from " << static_cast<int>(row.from) << " verb "
        << static_cast<int>(row.verb);
    EXPECT_EQ(lifecycle.phase(), row.to)
        << "from " << static_cast<int>(row.from) << " verb "
        << static_cast<int>(row.verb);
  }
}

TEST(NodeLifecycleTest, StartsMountingAndWalksTheWholeLife) {
  NodeLifecycle lifecycle;
  EXPECT_EQ(lifecycle.phase(), NodeLifecyclePhase::mounting);
  EXPECT_TRUE(lifecycle.appear());
  EXPECT_EQ(lifecycle.phase(), NodeLifecyclePhase::live);
  EXPECT_TRUE(lifecycle.depart());
  EXPECT_EQ(lifecycle.phase(), NodeLifecyclePhase::departing);
  EXPECT_TRUE(lifecycle.retire());
  EXPECT_EQ(lifecycle.phase(), NodeLifecyclePhase::retired);
  EXPECT_FALSE(lifecycle.abandon());
}

TEST(NodeLifecycleTest, AbandonsALiveNodeWithoutDeparting) {
  NodeLifecycle lifecycle;
  ASSERT_TRUE(lifecycle.appear());
  EXPECT_TRUE(lifecycle.abandon());
  EXPECT_EQ(lifecycle.phase(), NodeLifecyclePhase::retired);
  EXPECT_FALSE(lifecycle.depart());
}

}
