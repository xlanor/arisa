#include <arisa/provider/error.hpp>

#include <gtest/gtest.h>

namespace arisa::provider {

TEST(ProviderErrorTest, CarriesCodeAndNativeResult) {
  const ProviderError error{ProviderErrorCode::platform_failure, 0x2A8U};
  EXPECT_EQ(error.code(), ProviderErrorCode::platform_failure);
  EXPECT_EQ(error.native_result(), 0x2A8U);
  EXPECT_EQ(error.description(), "platform failure");
}

TEST(ProviderErrorTest, NativeResultIsOptional) {
  const ProviderError error{ProviderErrorCode::not_found};
  EXPECT_FALSE(error.native_result().has_value());
}

TEST(ProviderErrorTest, DescribesCancellation) {
  EXPECT_EQ(ProviderError{ProviderErrorCode::cancelled}.description(),
            "cancelled");
}

TEST(ProviderErrorTest, UnsupportedCapabilityFactory) {
  EXPECT_EQ(ProviderError::unsupported_capability(),
            ProviderError{ProviderErrorCode::unsupported_capability});
  EXPECT_NE(ProviderError::unsupported_capability(),
            ProviderError{ProviderErrorCode::io_failure});
}

TEST(ProviderErrorTest, ExplainsCodeDetailAndNativeResult) {
  EXPECT_EQ(ProviderError(ProviderErrorCode::invalid_argument,
                          ProviderErrorDetail::empty_region)
                .explanation(),
            "invalid_argument (invalid argument): the render pass region is "
            "empty");
  EXPECT_EQ(ProviderError::vulkan(ProviderErrorCode::resource_exhausted, -2)
                .explanation(),
            "resource_exhausted (resource exhausted), VkResult -2");
  EXPECT_EQ(
      ProviderError(ProviderErrorCode::platform_failure, 0x2A8U).explanation(),
      "platform_failure (platform failure), result 0x2a8");
}

}
