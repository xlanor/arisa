#include <arisa/components/button.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/provider/headless/headless_factory.hpp>

#include <scroll_fixture.hpp>

#include <gtest/gtest.h>

#include <format>
#include <optional>
#include <string>

namespace arisa::runtime {

class ItemSheetTest : public ScrollFixture {
 protected:
  struct Probe {
    std::optional<composition::StateHandle<std::optional<int>>> item;
    int dismissals{0};
  };

  static constexpr composition::StateKey<std::optional<int>> item_key{
      "sheet-item", std::nullopt};

  class Items final : public composition::Component {
   public:
    explicit Items(Probe* probe) noexcept : probe_(probe) {}

    auto operator==(const Items&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      const auto item = context.state(item_key);
      probe_->item = item;
      Probe* probe = probe_;
      return components::VStack(
          components::Button("Open", [] {})
              .frame({.width = 120.0F, .height = 40.0F})
              .id("open")
              .sheet(
                  item, [probe] { ++probe->dismissals; },
                  [](int shown) {
                    return components::Text(std::format("Item {}", shown))
                        .id("label");
                  }));
    }

   private:
    Probe* probe_;
  };
};

TEST_F(ItemSheetTest, AnIdentifiableItemKeysItsSheet) {
  EXPECT_EQ(composition::ElementKey::identifying(7),
            composition::ElementKey{7});
  EXPECT_EQ(composition::ElementKey::identifying(std::string{"a"}),
            composition::ElementKey{"a"});
  EXPECT_NE(composition::ElementKey::identifying(1.5),
            composition::ElementKey::identifying(2.5));
}

TEST_F(ItemSheetTest, ChangingTheItemDismissesAndPresentsAgain) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  Application application = start(providers, Items{&probe});
  probe.item->set(1);
  frames(application, 2);
  settle(providers, application);
  EXPECT_TRUE(shows(application, "Item 1"));
  const auto first = node_of(application, "label");
  ASSERT_TRUE(first.has_value());
  motion::with_animation(motion::Animation::default_animation(),
                         [&probe] { probe.item->set(2); });
  frames(application, 2);
  EXPECT_GT(application.root().tree().departing(), 0U);
  EXPECT_TRUE(shows(application, "Item 2"));
  settle(providers, application);
  EXPECT_EQ(application.root().tree().departing(), 0U);
  EXPECT_EQ(probe.item->get(), std::optional<int>{2});
  EXPECT_TRUE(shows(application, "Item 2"));
  EXPECT_FALSE(shows(application, "Item 1"));
  EXPECT_NE(node_of(application, "label"), first);
  EXPECT_EQ(probe.dismissals, 0);
}

TEST_F(ItemSheetTest, TheSameItemKeepsItsSheet) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  Application application = start(providers, Items{&probe});
  probe.item->set(3);
  frames(application, 2);
  settle(providers, application);
  const auto first = node_of(application, "label");
  probe.item->set(3);
  frames(application, 2);
  EXPECT_EQ(application.root().tree().departing(), 0U);
  EXPECT_EQ(node_of(application, "label"), first);
}

TEST_F(ItemSheetTest, ClearingTheItemDismissesOnce) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  Application application = start(providers, Items{&probe});
  probe.item->set(4);
  frames(application, 2);
  settle(providers, application);
  probe.item->set(std::nullopt);
  frames(application, 2);
  settle(providers, application);
  EXPECT_FALSE(shows(application, "Item 4"));
  EXPECT_EQ(probe.dismissals, 1);
}

}
