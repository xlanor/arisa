#include <plasma_shaders.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <bit>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <vector>

namespace arisa::examples {

class ShaderEmbeddingTest : public ::testing::Test {
 protected:
  static auto read(const std::string& name) -> std::vector<std::uint8_t> {
    std::ifstream file{std::string{ARISA_PLASMA_SHADERS} + "/" + name + "." +
                           ARISA_PLASMA_SHADER_FORMAT,
                       std::ios::binary};
    return {std::istreambuf_iterator<char>{file},
            std::istreambuf_iterator<char>{}};
  }

  static auto same(std::span<const std::uint8_t> embedded,
                   const std::vector<std::uint8_t>& compiled) -> bool {
    return !compiled.empty() &&
           std::ranges::equal(embedded, std::span{compiled});
  }
};

TEST_F(ShaderEmbeddingTest, EachStageEmbedsTheCompiledBinaryByteForByte) {
  EXPECT_TRUE(same(PlasmaShaders::vertex(), read("plasma_vert")));
  EXPECT_TRUE(same(PlasmaShaders::fragment(), read("plasma_frag")));
}

TEST_F(ShaderEmbeddingTest, HostBinariesAreSpirvAlignedForTheLoader) {
  constexpr std::uint32_t spirv_magic = 0x07230203U;
  for (const std::span<const std::uint8_t> code :
       {PlasmaShaders::vertex(), PlasmaShaders::fragment()}) {
    ASSERT_GE(code.size(), 4U);
    EXPECT_EQ(code.size() % 4, 0U);
    EXPECT_EQ(std::bit_cast<std::uintptr_t>(code.data()) % 4, 0U);
    const std::uint32_t magic = static_cast<std::uint32_t>(code[0]) |
                                (static_cast<std::uint32_t>(code[1]) << 8U) |
                                (static_cast<std::uint32_t>(code[2]) << 16U) |
                                (static_cast<std::uint32_t>(code[3]) << 24U);
    EXPECT_EQ(magic, spirv_magic);
  }
}

}
