#include <arisa/provider/diagnostics/heap_meter.hpp>

#include <gtest/gtest.h>

#include <optional>

namespace arisa::provider {

TEST(HeapMeterTest, FromCapacityReportsTheUsedPart) {
  const HeapUsage usage = HeapMeter::from_capacity(0x1000'0000, 0x0C00'0000);
  EXPECT_EQ(usage.bytes, 0x0400'0000U);
  EXPECT_EQ(usage.blocks, std::nullopt);
}

TEST(HeapMeterTest, FromCapacityNeverUnderflows) {
  EXPECT_EQ(HeapMeter::from_capacity(0x1000, 0x2000).bytes, 0U);
  EXPECT_EQ(HeapMeter::from_capacity(0, 0).bytes, 0U);
}

}
