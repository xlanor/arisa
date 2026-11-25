#include <arisa/components/vstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/namespace.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/frame_timings.hpp>
#include <arisa/runtime/root.hpp>
#include <arisa/theme/colour.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <utility>

namespace arisa::runtime {

using namespace std::chrono_literals;

class EffectsRuntimeTest : public ::testing::Test {
 protected:
  static constexpr composition::Namespace hero{"hero"};

  class Hero final : public composition::Component {
   public:
    explicit Hero(const composition::Observable<bool>& open) : open_(open) {}

    auto operator==(const Hero& other) const -> bool {
      return &open_.get() == &other.open_.get();
    }

    auto body(composition::Context& context) const
        -> composition::Element override {
      const bool open = context.read(open_.get());
      const composition::Namespace::ID space = context.namespace_id(hero);
      return components::VStack(components::VStack()
                                    .frame({.width = open ? 400.0F : 100.0F,
                                            .height = open ? 200.0F : 50.0F})
                                    .background(theme::Colour::hex(0x405060))
                                    .matched_geometry_effect("card", space)
                                    .id(open ? "detail" : "card"));
    }

   private:
    std::reference_wrapper<const composition::Observable<bool>> open_;
  };

  auto start() -> Application {
    auto created = Application::create(providers_, Hero{open_});
    EXPECT_TRUE(created.has_value());
    return std::move(*created);
  }

  void advance(std::chrono::nanoseconds step) {
    ASSERT_TRUE(providers_.clock().advance(step).has_value());
  }

  provider::headless::HeadlessProviderFactory providers_;
  composition::Observable<bool> open_{false};
};

TEST_F(EffectsRuntimeTest, MatchedMorphFramesSkipReconcileAndLayout) {
  Application application = start();
  ASSERT_TRUE(application.frame().has_value());
  motion::with_animation(motion::Animation::spring(),
                         [this] { open_.set(true); });
  ASSERT_TRUE(application.frame().has_value());
  EXPECT_TRUE(application.last_frame().laid_out);
  EXPECT_TRUE(application.last_frame().animating);
  EXPECT_EQ(application.root().tree().departing(), 0U);
  const std::uint64_t evaluations = application.root().tree().evaluations();
  const std::uint64_t revision = application.root().tree().revision();
  for (int step = 0; step < 12; ++step) {
    advance(16ms);
    ASSERT_TRUE(application.frame().has_value());
    EXPECT_EQ(application.last_frame().kind, FrameKind::animation);
    EXPECT_FALSE(application.last_frame().laid_out);
    EXPECT_TRUE(application.last_frame().presented);
  }
  EXPECT_EQ(application.root().tree().evaluations(), evaluations);
  EXPECT_EQ(application.root().tree().revision(), revision);
  advance(3s);
  ASSERT_TRUE(application.frame().has_value());
  EXPECT_FALSE(application.last_frame().animating);
}

}
