#include <arisa/components/shape_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/background_environment.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/coordinate_space.hpp>
#include <arisa/composition/drag_gesture.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/geometry_effect.hpp>
#include <arisa/composition/geometry_proxy.hpp>
#include <arisa/composition/matched_transition_source_configuration.hpp>
#include <arisa/composition/namespace.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/graphics/gradient.hpp>
#include <arisa/graphics/projection_transform.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/content_transition.hpp>
#include <arisa/motion/presentation.hpp>
#include <arisa/motion/transition.hpp>
#include <arisa/motion/transition_content.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/root.hpp>
#include <arisa/theme/colour.hpp>

#include <scroll_fixture.hpp>

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace arisa::runtime {

using namespace std::chrono_literals;
using composition::Element;

class MotionParityRuntimeTest : public ScrollFixture {
 protected:
  static void sized(provider::headless::HeadlessProviderFactory& providers) {
    const provider::SurfaceSize size{.width = provider::SurfaceWidth{640},
                                     .height = provider::SurfaceHeight{360}};
    providers.surface().resize(size);
    providers.gpu().rasterize(size);
  }

  static auto presentation(Application& application, std::string_view key)
      -> motion::Presentation& {
    const auto id = node_of(application, key);
    composition::Tree& tree = application.root().tree();
    return tree.presentation(tree.node(*id)->get());
  }

  static auto drawn(Application& application, std::string_view key)
      -> motion::Presentation& {
    const composition::Node* at = &node_named(application, key)->get();
    while (!at->content_as<components::ShapeView::Content>().has_value() &&
           at->child_count() == 1) {
      at = &*at->children().begin();
    }
    return application.root().tree().presentation(*at);
  }

  static auto pixel(provider::headless::HeadlessProviderFactory& providers,
                    Application& application, std::string_view key)
      -> std::array<std::uint8_t, 4> {
    const composition::Rect laid = node_named(application, key)->get().frame();
    const auto image = providers.gpu().capture();
    EXPECT_TRUE(image.has_value());
    if (!image.has_value()) {
      return {};
    }
    const auto found =
        image->pixel(static_cast<std::uint32_t>(laid.x + (laid.width * 0.5F)),
                     static_cast<std::uint32_t>(laid.y + (laid.height * 0.5F)));
    return found.has_value() ? *found : std::array<std::uint8_t, 4>{};
  }

  class Shown final : public composition::Component {
   public:
    Shown(const composition::Observable<bool>& flag,
          motion::Transition transition)
        : flag_(flag), transition_(std::move(transition)) {}

    auto operator==(const Shown& other) const -> bool {
      return &flag_.get() == &other.flag_.get() &&
             transition_ == other.transition_;
    }

    auto body(composition::Context& context) const -> Element override {
      std::vector<Element> children;
      if (context.read(flag_.get())) {
        children.push_back(components::ZStack()
                               .frame({.width = 100.0F, .height = 100.0F})
                               .background(theme::Colour::hex(0xFF0000))
                               .transition(transition_)
                               .id("tile"));
      }
      return components::ZStack(components::ZStackOptions{},
                                std::move(children))
          .frame({.width = 300.0F, .height = 300.0F})
          .background(theme::Colour::hex(0xFFFFFF))
          .id("stage");
    }

   private:
    std::reference_wrapper<const composition::Observable<bool>> flag_;
    motion::Transition transition_;
  };

  class DragCard final : public composition::Component {
   public:
    static constexpr composition::StateKey<float> pulled{"pulled", 0.0F};

    explicit DragCard(std::shared_ptr<std::vector<bool>> seen)
        : seen_(std::move(seen)) {}

    auto operator==(const DragCard&) const -> bool { return true; }

    auto body(composition::Context& context) const -> Element override {
      const auto pull = context.state(pulled);
      if (pull.get() != 0.0F) {
        seen_->push_back(context.transaction().is_continuous());
      }
      return components::VStack(
          components::ZStack()
              .frame({.width = 100.0F, .height = 100.0F})
              .background(theme::Colour::hex(0x204060))
              .id("card")
              .gesture(composition::DragGesture().on_changed(
                  [pull](const composition::DragGesture::Value& value) {
                    pull.set(value.translation.height);
                  })));
    }

   private:
    std::shared_ptr<std::vector<bool>> seen_;
  };

  struct Shift {
    double amount{0.0};

    using AnimatableData = double;

    auto animatable_data() const -> double { return amount; }

    void set_animatable_data(double value) { amount = value; }

    auto effect_value(composition::Size) const
        -> graphics::ProjectionTransform {
      return graphics::ProjectionTransform{
          std::array<float, 9>{1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F,
                               static_cast<float>(amount), 0.0F, 1.0F}};
    }

    friend auto operator==(const Shift&, const Shift&) -> bool = default;
  };
};

TEST_F(MotionParityRuntimeTest,
       ATransitionsOwnAnimationRunsWithoutATransaction) {
  provider::headless::HeadlessProviderFactory providers;
  sized(providers);
  composition::Observable<bool> flag{false};
  Application application =
      start(providers, Shown{flag, motion::Transition::opacity().animation(
                                       motion::Animation::linear(1.0))});
  flag.set(true);
  step(providers, application);
  step(providers, application, 484ms);
  EXPECT_TRUE(application.last_frame().animating);
  const std::array<std::uint8_t, 4> halfway =
      pixel(providers, application, "tile");
  EXPECT_NEAR(static_cast<float>(halfway[1]), 255.0F * 0.5F, 25.0F);
  step(providers, application, 700ms);
  step(providers, application);
  EXPECT_EQ(pixel(providers, application, "tile")[1], 0);
  flag.set(false);
  step(providers, application);
  step(providers, application, 300ms);
  EXPECT_EQ(application.root().tree().departing(), 1U);
  step(providers, application, 900ms);
  step(providers, application);
  EXPECT_EQ(application.root().tree().departing(), 0U);
}

TEST_F(MotionParityRuntimeTest, ColourModifiersInACustomTransitionRecolour) {
  struct Develop {
    auto body(motion::TransitionContent content,
              motion::TransitionPhase phase) const
        -> motion::TransitionContent {
      return phase.is_identity() ? content : content.grayscale(1.0F);
    }
  };
  provider::headless::HeadlessProviderFactory providers;
  sized(providers);
  composition::Observable<bool> flag{false};
  Application application =
      start(providers, Shown{flag, motion::Transition::custom(Develop{})});
  motion::with_animation(motion::Animation::linear(1.0),
                         [&flag] { flag.set(true); });
  step(providers, application);
  step(providers, application, 484ms);
  const std::array<std::uint8_t, 4> halfway =
      pixel(providers, application, "tile");
  EXPECT_GT(halfway[1], 20);
  EXPECT_LT(halfway[0], 250);
  step(providers, application, 700ms);
  step(providers, application);
  const std::array<std::uint8_t, 4> settled =
      pixel(providers, application, "tile");
  EXPECT_EQ(settled[0], 255);
  EXPECT_EQ(settled[1], 0);
}

TEST_F(MotionParityRuntimeTest, BindingAnimationAnimatesItsWrites) {
  provider::headless::HeadlessProviderFactory providers;
  composition::Observable<bool> wide{false};
  class Host final : public composition::Component {
   public:
    explicit Host(const composition::Observable<bool>& wide) : wide_(wide) {}

    auto operator==(const Host& other) const -> bool {
      return &wide_.get() == &other.wide_.get();
    }

    auto body(composition::Context& context) const -> Element override {
      return components::VStack(
          components::ZStack()
              .frame({.width = context.read(wide_.get()) ? 300.0F : 100.0F,
                      .height = 20.0F})
              .background(theme::Colour::hex(0x204060))
              .id("bar"));
    }

   private:
    std::reference_wrapper<const composition::Observable<bool>> wide_;
  };
  Application application = start(providers, Host{wide});
  const composition::Binding<bool> plain{
      [&wide] { return wide.get(); }, [&wide](bool value) { wide.set(value); }};
  const composition::Binding<bool> animated =
      plain.animation(motion::Animation::linear(1.0));
  ASSERT_TRUE(animated.transaction().animation().has_value());
  EXPECT_FALSE(plain.transaction().animation().has_value());
  animated.set(true);
  step(providers, application);
  step(providers, application, 500ms);
  EXPECT_TRUE(application.last_frame().animating);
  EXPECT_TRUE(presentation(application, "bar")
                  .in_flight(motion::Presentation::Property::frame));
  motion::Transaction instant;
  instant.set_disables_animations(true);
  plain.transaction(instant).set(false);
  EXPECT_TRUE(plain.transaction(instant).transaction().disables_animations());
}

TEST_F(MotionParityRuntimeTest, DragUpdatesRunInAContinuousTransaction) {
  provider::headless::HeadlessProviderFactory providers;
  auto continuous = std::make_shared<std::vector<bool>>();
  Application application = start(providers, DragCard{continuous});
  const provider::PixelPoint at = centre(application, "card");
  touch_down(providers, application, at);
  for (int index = 1; index <= 4; ++index) {
    touch_move(providers, application,
               provider::PixelPoint{
                   .x = at.x, .y = at.y + (20.0F * static_cast<float>(index))});
  }
  touch_up(providers, application,
           provider::PixelPoint{.x = at.x, .y = at.y + 80.0F});
  ASSERT_FALSE(continuous->empty());
  EXPECT_TRUE(continuous->front());
}

TEST_F(MotionParityRuntimeTest, TheContentTransitionIsReadableBelow) {
  provider::headless::HeadlessProviderFactory providers;
  auto read = std::make_shared<std::optional<motion::ContentTransition>>();
  class Reader final : public composition::Component {
   public:
    explicit Reader(
        std::shared_ptr<std::optional<motion::ContentTransition>> read)
        : read_(std::move(read)) {}

    auto operator==(const Reader&) const -> bool { return true; }

    auto body(composition::Context& context) const -> Element override {
      *read_ = context.environment(
          composition::BackgroundEnvironment::content_transition);
      return components::Text("12");
    }

   private:
    std::shared_ptr<std::optional<motion::ContentTransition>> read_;
  };
  Application application =
      start(providers,
            components::VStack(Element{Reader{read}})
                .content_transition(motion::ContentTransition::numeric_text()));
  ASSERT_TRUE(read->has_value());
  EXPECT_EQ((*read)->kind(), motion::ContentTransition::Kind::numeric_text);
}

TEST_F(MotionParityRuntimeTest, ChildGeometryFollowsAnAnimatingGeometryEffect) {
  provider::headless::HeadlessProviderFactory providers;
  composition::Observable<double> shift{0.0};
  auto seen = std::make_shared<std::vector<float>>();
  class Host final : public composition::Component {
   public:
    Host(const composition::Observable<double>& shift,
         std::shared_ptr<std::vector<float>> seen)
        : shift_(shift), seen_(std::move(seen)) {}

    auto operator==(const Host& other) const -> bool {
      return &shift_.get() == &other.shift_.get();
    }

    auto body(composition::Context& context) const -> Element override {
      const std::shared_ptr<std::vector<float>> seen = seen_;
      return components::VStack(
          components::ZStack(
              components::ZStack()
                  .frame({.width = 40.0F, .height = 40.0F})
                  .on_geometry_change(
                      std::type_identity<float>{},
                      [](const composition::GeometryProxy& proxy) {
                        return proxy.frame(composition::CoordinateSpace::global)
                            .x;
                      },
                      [seen](const float& x) { seen->push_back(x); }))
              .frame({.width = 100.0F, .height = 100.0F})
              .geometry_effect(Shift{.amount = context.read(shift_.get())}));
    }

   private:
    std::reference_wrapper<const composition::Observable<double>> shift_;
    std::shared_ptr<std::vector<float>> seen_;
  };
  Application application = start(providers, Host{shift, seen});
  const std::size_t before = seen->size();
  motion::with_animation(motion::Animation::linear(1.0),
                         [&shift] { shift.set(100.0); });
  for (int index = 0; index < 8; ++index) {
    step(providers, application, 100ms);
  }
  EXPECT_GE(seen->size(), before + 6U);
  step(providers, application, 500ms);
  step(providers, application);
  ASSERT_FALSE(seen->empty());
  EXPECT_NEAR(seen->back() - seen->at(before > 0 ? before - 1 : 0), 100.0F,
              1.0F);
}

TEST_F(MotionParityRuntimeTest, GradientFillsAndGradientTextInterpolate) {
  provider::headless::HeadlessProviderFactory providers;
  sized(providers);
  composition::Observable<bool> warm{false};
  class Host final : public composition::Component {
   public:
    explicit Host(const composition::Observable<bool>& warm) : warm_(warm) {}

    auto operator==(const Host& other) const -> bool {
      return &warm_.get() == &other.warm_.get();
    }

    auto body(composition::Context& context) const -> Element override {
      const bool hot = context.read(warm_.get());
      const graphics::LinearGradient ramp{
          .gradient = graphics::Gradient{std::vector<theme::ColourValue>{
              theme::Colour::hex(hot ? 0xFF0000 : 0x0000FF),
              theme::Colour::hex(hot ? 0xFF0000 : 0x0000FF)}}};
      return components::VStack(
          components::ShapeView(theme::Rectangle())
              .fill(ramp)
              .frame({.width = 100.0F, .height = 60.0F})
              .id("swatch"),
          components::Text("Gradient").foreground_style(ramp).id("title"));
    }

   private:
    std::reference_wrapper<const composition::Observable<bool>> warm_;
  };
  Application application = start(providers, Host{warm});
  motion::with_animation(motion::Animation::linear(1.0),
                         [&warm] { warm.set(true); });
  step(providers, application);
  step(providers, application, 484ms);
  EXPECT_TRUE(
      drawn(application, "swatch").in_flight(motion::Presentation::Mix::shape));
  EXPECT_TRUE(drawn(application, "title")
                  .in_flight(motion::Presentation::Mix::foreground));
  const std::array<std::uint8_t, 4> halfway =
      pixel(providers, application, "swatch");
  EXPECT_GT(halfway[0], 60);
  EXPECT_GT(halfway[2], 60);
  step(providers, application, 700ms);
  step(providers, application);
  EXPECT_FALSE(
      drawn(application, "swatch").in_flight(motion::Presentation::Mix::shape));
  const std::array<std::uint8_t, 4> settled =
      pixel(providers, application, "swatch");
  EXPECT_EQ(settled[0], 255);
  EXPECT_EQ(settled[2], 0);
}

TEST_F(MotionParityRuntimeTest, AConfiguredTransitionSourceIsStyled) {
  provider::headless::HeadlessProviderFactory providers;
  sized(providers);
  static constexpr composition::Namespace zoom{"zoom"};
  class Host final : public composition::Component {
   public:
    auto operator==(const Host&) const -> bool { return true; }

    auto body(composition::Context& context) const -> Element override {
      return components::VStack(
          components::ZStack()
              .frame({.width = 80.0F, .height = 80.0F})
              .matched_transition_source(
                  "card", context.namespace_id(zoom),
                  [](composition::MatchedTransitionSourceConfiguration source) {
                    return source.background(theme::Colour::hex(0x00FF00))
                        .clip_shape(theme::RoundedRectangle(12.0F))
                        .shadow(theme::Colour::hex(0x000000), 6.0F, 0.0F, 3.0F);
                  })
              .id("source"));
    }
  };
  const composition::MatchedTransitionSourceConfiguration configured =
      composition::MatchedTransitionSourceConfiguration{}.shadow(4.0F);
  ASSERT_TRUE(configured.cast().has_value());
  EXPECT_FLOAT_EQ(configured.cast()->radius, 4.0F);
  Application application = start(providers, Host{});
  step(providers, application);
  const std::array<std::uint8_t, 4> centre_colour =
      pixel(providers, application, "source");
  EXPECT_EQ(centre_colour[1], 255);
  EXPECT_EQ(centre_colour[0], 0);
}

}
