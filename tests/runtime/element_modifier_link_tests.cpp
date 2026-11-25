#include <arisa/components/button_sizing.hpp>
#include <arisa/components/control_size.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/composition/element.hpp>

#include <gtest/gtest.h>

namespace arisa::composition {

TEST(ElementModifierLinkTest, SizingModifiersLinkWithoutTheEnvironmentHeader) {
  const Element sized = Element{components::Spacer()}
                            .control_size(components::ControlSize::mini)
                            .button_sizing(components::ButtonSizing::flexible)
                            .id("sized");
  ASSERT_TRUE(sized.key().has_value());
  EXPECT_EQ(sized.key()->description(), "sized");
}

}
