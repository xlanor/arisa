#include "focus_fixture.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/button_sizing.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/control_size.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/labeled_content.hpp>
#include <arisa/components/system_symbols.hpp>
#include <arisa/components/text.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/presentation/help.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/structure/disclosure_row.hpp>
#include <arisa/structure/form.hpp>
#include <arisa/structure/form_style.hpp>
#include <arisa/structure/section.hpp>
#include <arisa/theme/glass.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <memory>

namespace arisa::runtime {

class FormActivateTest : public FocusFixture {
 protected:
  auto go() const -> composition::Element {
    return components::Button("Go", [presses = presses_] { ++*presses; })
        .id("go");
  }

  static auto chevrons(const composition::Node& node) -> std::size_t {
    std::size_t found = 0;
    if (const auto text = node.content_as<components::Text::Content>();
        text.has_value() &&
        text->get().text() ==
            *components::SystemSymbols::text("chevron_right")) {
      ++found;
    }
    for (const composition::Node& child : node.children()) {
      found += chevrons(child);
    }
    return found;
  }

  provider::headless::HeadlessProviderFactory providers_;
  std::shared_ptr<int> presses_ = std::make_shared<int>(0);
};

TEST_F(FormActivateTest, ActivateByKeyReachesAButtonThatIsAFormRow) {
  Application application = start(
      providers_,
      structure::Form(structure::Section(components::Text("Title"), go())));
  EXPECT_TRUE(application.root().activate(composition::ElementKey{"go"}));
  frames(application, 2);
  EXPECT_EQ(*presses_, 1);
}

TEST_F(FormActivateTest, ActivateByKeyReachesAButtonInsideACardFormRow) {
  Application application =
      start(providers_, structure::Form(structure::Section(components::HStack(
                                            components::Text("Reset"), go())))
                            .form_style(structure::FormStyle::cards()));
  EXPECT_TRUE(application.root().activate(composition::ElementKey{"go"}));
  frames(application, 2);
  EXPECT_EQ(*presses_, 1);
}

TEST_F(FormActivateTest, ActivateByKeyReachesAStyledFlexibleButtonInACardForm) {
  Application application = start(
      providers_,
      structure::Form(
          structure::Section(
              components::LabeledContent("Mapping",
                                         components::Text("Default")),
              components::Button("Open", [presses = presses_] { ++*presses; })
                  .button_style(
                      components::ButtonStyle::glass(theme::Glass::clear()))
                  .control_size(components::ControlSize::large)
                  .button_sizing(components::ButtonSizing::flexible)
                  .id("open")))
          .form_style(structure::FormStyle::cards()));
  EXPECT_TRUE(application.root().activate(composition::ElementKey{"open"}));
  frames(application, 2);
  EXPECT_EQ(*presses_, 1);
}

TEST_F(FormActivateTest,
       ADisclosureRowOutsideANavigationStackShowsItsDetailAndActs) {
  Application application = start(
      providers_, structure::Form(structure::Section(
                                      structure::DisclosureRow(
                                          "Display", "1080p",
                                          [presses = presses_] { ++*presses; })
                                          .id("display")))
                      .form_style(structure::FormStyle::cards()));
  EXPECT_TRUE(shows(application, "Display"));
  EXPECT_TRUE(shows(application, "1080p"));
  EXPECT_EQ(focused_key(application), "display");
  press(providers_, application, provider::Button::a);
  EXPECT_EQ(*presses_, 1);
  EXPECT_TRUE(
      shows(application, *components::SystemSymbols::text("chevron_right")));
}

TEST_F(FormActivateTest, ADisclosureRowWithHelpKeepsItsChevronAndFullWidth) {
  Application application = start(
      providers_,
      structure::Form(
          structure::Section(
              structure::DisclosureRow("Display", "1080p",
                                       [presses = presses_] { ++*presses; })
                  .help("Display", "The resolution the console streams at")
                  .id("display"),
              structure::DisclosureRow("Audio", "Stereo", [] {}).id("audio")))
          .form_style(structure::FormStyle::cards()));
  const auto helped = node_named(application, "display");
  const auto plain = node_named(application, "audio");
  ASSERT_TRUE(helped.has_value());
  ASSERT_TRUE(plain.has_value());
  EXPECT_NEAR(helped->get().frame().width, plain->get().frame().width, 0.5F);
  EXPECT_NEAR(helped->get().frame().height, plain->get().frame().height, 0.5F);
  EXPECT_EQ(chevrons(application.root().tree().root()), 2U);
  press(providers_, application, provider::Button::a);
  EXPECT_EQ(*presses_, 1);
}
}
