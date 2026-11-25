#include <arisa/provider/browser/url.hpp>

#include <gtest/gtest.h>

#include <array>
#include <string_view>

namespace arisa::provider {

TEST(UrlTest, AcceptsHttpAndHttps) {
  const auto secure = Url::create("https://example.com/path?q=1#frag");
  ASSERT_TRUE(secure.has_value());
  EXPECT_TRUE(secure->secure());
  EXPECT_EQ(secure->value(), "https://example.com/path?q=1#frag");
  const auto plain = Url::create("HTTP://example.com");
  ASSERT_TRUE(plain.has_value());
  EXPECT_FALSE(plain->secure());
}

TEST(UrlTest, RejectsOtherSchemesAndMalformedText) {
  constexpr std::array rejected{
      std::string_view{""},
      std::string_view{"ftp://example.com"},
      std::string_view{"file:///etc/passwd"},
      std::string_view{"https://"},
      std::string_view{"https://?q"},
      std::string_view{"https://exa mple.com"},
      std::string_view{"https://example.com/\n"},
      std::string_view{"javascript:alert(1)"},
      std::string_view{"example.com"},
  };
  for (const std::string_view candidate : rejected) {
    const auto url = Url::create(candidate);
    ASSERT_FALSE(url.has_value()) << candidate;
    EXPECT_EQ(url.error().code(), ProviderErrorCode::invalid_argument);
  }
}

}
