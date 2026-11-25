#include <arisa/async/manual_executor.hpp>
#include <arisa/components/glass_effect_container.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/namespace.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/markup/markup.hpp>
#include <arisa/provider/gpu/draw_data.hpp>
#include <arisa/provider/gpu/shape_vertex.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/headless/null_gpu.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/provider/input/provider.hpp>
#include <arisa/provider/surface/surface_size.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/application_options.hpp>
#include <arisa/structure/appearance_settings_section.hpp>
#include <arisa/theme/color_scheme.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>

#include <effects_page.hpp>
#include <gallery_fixture.hpp>
#include <motion_page.hpp>
#include <presentations_page.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::runtime {

class RetainedStage final : public composition::Component {
 public:
  enum class Scene : std::uint8_t { united, tilted, faded, switched, spinning };

  static constexpr composition::StateKey<bool> apart{"retained-stage-apart",
                                                     false};
  static constexpr std::string_view toggle = "retained-stage-toggle";
  static constexpr std::string_view bubbles = "retained-stage-bubbles";
  static constexpr std::string_view control = "retained-stage-control";
  static constexpr composition::StateKey<bool> flipped{"retained-stage-flipped",
                                                       false};
  static constexpr composition::Namespace unioned{"retained-stage-union"};

  explicit RetainedStage(Scene scene) noexcept : scene_(scene) {}

  auto operator==(const RetainedStage&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override {
    using namespace markup;
    const auto spread = context.state(apart);
    const bool wide = spread.get();
    composition::Element button =
        Button(action([spread] {
                 with_animation(Animation::spring(),
                                [spread] { spread.set(!spread.get()); });
               }),
               id(toggle), hover_effect(HoverEffect::lift.tilt()),
               padding(theme::space::lg),
               background(theme::ColourRole::surface_raised,
                          theme::RoundedRectangle(theme::radius::card)))(
            VStack(alignment::leading, spacing(theme::space::xs))(
                Text(font::headline)("Stage"), Text("Tilt and shake")));
    if (scene_ == Scene::switched || scene_ == Scene::spinning) {
      composition::Element held =
          scene_ == Scene::switched
              ? composition::Element{Toggle("Wi-Fi", context.state(flipped))
                                         .id(control)}
              : composition::Element{ProgressView().id(control)};
      return VStack(frame({.max_width = composition::infinity,
                           .max_height = composition::infinity}),
                    padding(theme::space::lg))(
          components::Text("Above"),
          composition::Element{
              VStack(alignment::leading, spacing(theme::space::md))(
                  Text(font::headline)("Card"),
                  composition::Element{HStack(spacing(theme::space::sm))(
                      Text("Row"), std::move(held))})}
              .padding(theme::space::lg)
              .glass_effect(),
          components::Text("Below"));
    }
    if (scene_ == Scene::faded) {
      return VStack(frame({.max_width = composition::infinity,
                           .max_height = composition::infinity}),
                    spacing(theme::space::lg))(
          std::move(button),
          components::VStack(
              components::VStackOptions{.spacing = theme::space::sm},
              components::Text("Fading"), components::Text("Glass"),
              components::Text("Subtree"), components::Text("Replayed"))
              .padding(theme::space::lg)
              .glass_effect()
              .opacity(wide ? 0.2F : 1.0F));
    }
    if (scene_ == Scene::tilted) {
      return VStack(frame({.max_width = composition::infinity,
                           .max_height = composition::infinity}))(
          std::move(button));
    }
    const composition::Namespace::ID space = context.namespace_id(unioned);
    const auto bubble = [space](std::string_view label) {
      return components::Text(std::string{label})
          .padding(theme::space::md)
          .glass_effect()
          .glass_effect_union(composition::ElementKey{bubbles}, space);
    };
    return components::ZStack(
               components::VStack(
                   components::VStackOptions{.spacing = theme::space::lg},
                   std::move(button),
                   components::GlassEffectContainer(
                       theme::Space{24.0F},
                       components::HStack(
                           components::HStackOptions{
                               .spacing = theme::Space{wide ? 40.0F : 6.0F}},
                           bubble("Union"), bubble("Of"), bubble("Three")))
                       .offset(wide ? 48.0F : 0.0F, 0.0F)
                       .opacity(wide ? 0.5F : 1.0F)))
        .frame({.max_width = composition::infinity,
                .max_height = composition::infinity})
        .background(theme::ColourValue{theme::Colour::hex(0x2B5C8A)});
  }

 private:
  Scene scene_;
};

class GalleryRetainedPaintTest
    : public GalleryFixture,
      public ::testing::WithParamInterface<examples::GalleryPage> {
 protected:
  static constexpr provider::SurfaceSize size{
      .width = provider::SurfaceWidth{640},
      .height = provider::SurfaceHeight{360}};

  struct Look {
    examples::GalleryPage page{examples::GalleryPage::typography};
    theme::ColorScheme scheme{theme::ColorScheme::light};
    bool glass{false};
    std::function<composition::Element()> content{};
  };

  class Twin {
   public:
    explicit Twin(Look look) {
      retained_app_.emplace(open(retained_, retained_jobs_, look, false));
      walked_app_.emplace(open(walked_, walked_jobs_, look, true));
    }

    void step(std::optional<provider::InputFrame> input = std::nullopt) {
      if (input.has_value()) {
        retained_.input().push(*input);
        walked_.input().push(std::move(*input));
      }
      ASSERT_TRUE(retained_.clock().advance(frame_step).has_value());
      ASSERT_TRUE(walked_.clock().advance(frame_step).has_value());
      retained_jobs_.drain();
      walked_jobs_.drain();
      ASSERT_TRUE(retained_app_->frame().has_value());
      ASSERT_TRUE(walked_app_->frame().has_value());
      ++frames_;
      compare();
    }

    void tilt(provider::Stick right) {
      provider::InputFrame frame;
      frame.pads[0] = provider::PadState{
          .connected = true, .buttons = {}, .left = {}, .right = right};
      step(std::move(frame));
    }

    void idle(int count) {
      for (int index = 0; index < count; ++index) {
        step();
      }
    }

    void press(provider::Button button) {
      step(pad({button}));
      step(pad({}));
      idle(4);
    }

    void stroke(provider::PixelPoint from, float distance) {
      constexpr int moves = 8;
      step(touching(from, provider::TouchPhase::began));
      for (int index = 1; index <= moves; ++index) {
        step(touching(
            provider::PixelPoint{
                .x = from.x,
                .y = from.y - (distance * static_cast<float>(index) /
                               static_cast<float>(moves))},
            provider::TouchPhase::moved));
      }
      step(touching(provider::PixelPoint{.x = from.x, .y = from.y - distance},
                    provider::TouchPhase::ended));
      idle(14);
    }

    auto activate(std::string_view key) -> bool {
      const bool left = trigger(*retained_app_, key);
      const bool right = trigger(*walked_app_, key);
      return left && right;
    }

    void tint(theme::Colour accent) {
      for (Application* application : {&*retained_app_, &*walked_app_}) {
        ASSERT_TRUE(application->appearance().has_value());
        const appearance::AppearanceModel model = *application->appearance();
        model.update([accent](appearance::AppearanceOverrides& overrides) {
          overrides.accent = accent;
        });
      }
    }

    auto mismatched() const noexcept -> int { return mismatched_; }

    auto spliced() const noexcept -> std::uint64_t { return spliced_; }

    auto walked_spliced() const noexcept -> std::uint64_t {
      return walked_spliced_;
    }

    auto steady_recorded() const noexcept -> std::uint32_t {
      return steady_recorded_;
    }

    auto steady_spliced() const noexcept -> std::uint64_t {
      return steady_spliced_;
    }

    auto steady_median() const -> std::uint32_t {
      if (steady_samples_.empty()) {
        return 0;
      }
      std::vector<std::uint32_t> sorted = steady_samples_;
      std::ranges::sort(sorted);
      return sorted[sorted.size() / 2];
    }

    auto nodes_under(std::string_view key) -> std::size_t {
      runtime::Root& root = retained_app_->root();
      const auto found = root.tree().find(composition::ElementKey{key});
      if (!found.has_value()) {
        return 0;
      }
      const auto node = root.tree().node(*found);
      return node.has_value() ? subtree(node->get()) : 0;
    }

    auto steady_uploaded() const noexcept -> std::uint32_t {
      return steady_uploaded_;
    }

    auto first_difference() const -> const std::string& { return difference_; }

   private:
    static auto open(provider::headless::HeadlessProviderFactory& providers,
                     async::ManualExecutor& jobs, Look look, bool walked)
        -> Application {
      load_fonts(providers);
      providers.surface().resize(size);
      providers.surface().set_scale(provider::SurfaceScale::for_pixels(size));
      AppearanceOptions appearance{
          .policy = examples::GalleryScreen::policy(look.scheme),
          .storage = std::nullopt};
      if (look.glass) {
        appearance.policy.defaults.theme = "arisa-theme-glass";
      }
      auto application = Application::create(
          providers,
          look.content ? look.content()
                       : composition::Element{examples::GalleryScreen{
                             look.page, theme::Seed{.scheme = look.scheme}}},
          ApplicationOptions{
              .executors =
                  async::Executors{.background = jobs, .blocking = jobs},
              .fonts = examples::GalleryFonts::load(providers.assets()),
              .atlas = {},
              .interaction = {},
              .appearance = std::move(appearance),
              .debug_full_walk = walked});
      EXPECT_TRUE(application.has_value());
      return std::move(*application);
    }

    static auto trigger(Application& application, std::string_view key)
        -> bool {
      runtime::Root& root = application.root();
      const auto found = root.tree().find(composition::ElementKey{key});
      if (!found.has_value()) {
        return false;
      }
      const auto node = root.tree().node(*found);
      if (root.activate(*found) ||
          (node.has_value() && within(root, node->get()))) {
        return true;
      }
      std::optional<composition::NodeId> above =
          node.has_value() ? node->get().parent() : std::nullopt;
      while (above.has_value()) {
        if (root.activate(*above)) {
          return true;
        }
        const auto parent = root.tree().node(*above);
        above = parent.has_value() ? parent->get().parent() : std::nullopt;
      }
      return false;
    }

    static auto subtree(const composition::Node& node) -> std::size_t {
      std::size_t count = 1;
      for (const composition::Node& child : node.children()) {
        count += subtree(child);
      }
      return count;
    }

    static auto within(runtime::Root& root, const composition::Node& node)
        -> bool {
      return std::ranges::any_of(
          node.children(), [&root](const composition::Node& child) {
            return root.activate(child.id()) || within(root, child);
          });
    }

    void compare() {
      const FrameTimings& retained = retained_app_->last_frame();
      const FrameTimings& walked = walked_app_->last_frame();
      spliced_ += retained.spliced;
      walked_spliced_ += walked.spliced;
      const std::size_t nodes = retained_app_->root().tree().size();
      const bool settled = !retained.laid_out && retained.evaluations == 0;
      if (settled && steady_ && nodes == nodes_ && retained.presented) {
        steady_recorded_ = std::max(steady_recorded_, retained.recorded);
        steady_samples_.push_back(retained.recorded);
        steady_spliced_ += retained.spliced;
        steady_uploaded_ =
            std::max(steady_uploaded_, retained.uploaded_vertices);
      }
      steady_ = settled;
      nodes_ = nodes;
      if (retained.presented != walked.presented) {
        note(std::format("presented {} against {}", retained.presented,
                         walked.presented));
        return;
      }
      if (!retained.presented) {
        return;
      }
      const provider::headless::NullGpu::ShapeRecord seen =
          retained_.gpu().last_shapes();
      const provider::headless::NullGpu::ShapeRecord expected =
          walked_.gpu().last_shapes();
      if (seen.commands.size() != expected.commands.size()) {
        note(std::format("{} commands against {}", seen.commands.size(),
                         expected.commands.size()));
        return;
      }
      for (std::size_t index = 0; index < seen.commands.size(); ++index) {
        if (!matches(seen.commands[index], expected.commands[index])) {
          note(std::format("command {} of {} differs", index,
                           seen.commands.size()));
          return;
        }
      }
      if (seen.indices != expected.indices) {
        note(std::format("{} indices against {}", seen.indices.size(),
                         expected.indices.size()));
        return;
      }
      if (seen.vertices != expected.vertices) {
        const auto split =
            std::ranges::mismatch(seen.vertices, expected.vertices);
        const auto at = static_cast<std::size_t>(
            std::distance(seen.vertices.begin(), split.in1));
        note(std::format(
            "{} vertices against {}, first difference at {} kind {}",
            seen.vertices.size(), expected.vertices.size(), at,
            at < seen.vertices.size() ? static_cast<int>(seen.vertices[at].kind)
                                      : -1));
        return;
      }
      if (retained_.gpu().last_damage() != walked_.gpu().last_damage()) {
        note("damage differs");
      }
    }

    auto paired(provider::TextureId left, provider::TextureId right) -> bool {
      const auto forward = textures_.try_emplace(left.value(), right.value());
      const auto backward = mirrored_.try_emplace(right.value(), left.value());
      return forward.first->second == right.value() &&
             backward.first->second == left.value();
    }

    auto matches(provider::DrawCommand left, provider::DrawCommand right)
        -> bool {
      if (!paired(left.texture, right.texture) ||
          !paired(left.detail, right.detail)) {
        return false;
      }
      left.texture = right.texture;
      left.detail = right.detail;
      return left == right;
    }

    void note(std::string what) {
      ++mismatched_;
      if (difference_.empty()) {
        difference_ = std::format("frame {}: {}", frames_, what);
      }
    }

    provider::headless::HeadlessProviderFactory retained_;
    provider::headless::HeadlessProviderFactory walked_;
    async::ManualExecutor retained_jobs_;
    async::ManualExecutor walked_jobs_;
    std::optional<Application> retained_app_;
    std::optional<Application> walked_app_;
    int frames_{0};
    int mismatched_{0};
    std::uint64_t spliced_{0};
    std::uint64_t walked_spliced_{0};
    bool steady_{false};
    std::size_t nodes_{0};
    std::uint32_t steady_recorded_{0};
    std::vector<std::uint32_t> steady_samples_;
    std::uint64_t steady_spliced_{0};
    std::uint32_t steady_uploaded_{0};
    std::string difference_;
    std::map<std::uint64_t, std::uint64_t> textures_;
    std::map<std::uint64_t, std::uint64_t> mirrored_;
  };

  static constexpr provider::PixelPoint content{.x = 420.0F, .y = 250.0F};
};

TEST_P(GalleryRetainedPaintTest, RetainedPaintMatchesTheFullWalkEveryFrame) {
  Twin twin{Look{.page = GetParam()}};
  twin.idle(8);
  twin.press(provider::Button::dpad_right);
  twin.press(provider::Button::dpad_down);
  twin.press(provider::Button::dpad_down);
  twin.press(provider::Button::a);
  twin.idle(12);
  twin.press(provider::Button::dpad_right);
  twin.press(provider::Button::dpad_up);
  twin.stroke(content, 90.0F);
  twin.stroke(content, -90.0F);
  twin.press(provider::Button::y);
  twin.idle(16);
  twin.press(provider::Button::dpad_left);
  twin.press(provider::Button::dpad_left);
  twin.press(provider::Button::dpad_down);
  twin.idle(16);
  twin.press(provider::Button::dpad_up);
  twin.idle(16);
  EXPECT_EQ(twin.mismatched(), 0) << twin.first_difference();
  EXPECT_GT(twin.spliced(), 0U);
  EXPECT_EQ(twin.walked_spliced(), 0U);
}

TEST_P(GalleryRetainedPaintTest, GlassThemeAndAccentMatchTheFullWalk) {
  Twin twin{Look{
      .page = GetParam(), .scheme = theme::ColorScheme::dark, .glass = true}};
  twin.idle(8);
  twin.press(provider::Button::dpad_down);
  twin.idle(10);
  twin.press(provider::Button::dpad_right);
  twin.press(provider::Button::dpad_down);
  twin.tint(structure::AppearanceSettingsSection::accents.back().colour);
  twin.idle(12);
  twin.stroke(content, 70.0F);
  twin.press(provider::Button::dpad_left);
  twin.press(provider::Button::dpad_up);
  twin.idle(16);
  EXPECT_EQ(twin.mismatched(), 0) << twin.first_difference();
  EXPECT_GT(twin.spliced(), 0U);
}

TEST_F(GalleryRetainedPaintTest,
       DriftingWallpaperUnderTheSidebarGlassMatchesTheFullWalk) {
  Twin twin{Look{.page = examples::GalleryPage::materials, .glass = true}};
  twin.idle(120);
  twin.press(provider::Button::dpad_down);
  twin.idle(40);
  twin.press(provider::Button::dpad_up);
  twin.idle(80);
  EXPECT_EQ(twin.mismatched(), 0) << twin.first_difference();
  EXPECT_GT(twin.spliced(), 0U);
}

TEST_F(GalleryRetainedPaintTest, LongListPagingMatchesTheFullWalk) {
  Twin twin{Look{.page = examples::GalleryPage::lists, .glass = true}};
  twin.idle(8);
  twin.press(provider::Button::dpad_right);
  twin.press(provider::Button::dpad_right);
  for (int frame = 0; frame < 90; ++frame) {
    twin.step(pad({provider::Button::dpad_down}));
  }
  twin.step(pad({}));
  twin.idle(20);
  for (int page = 0; page < 4; ++page) {
    twin.press(provider::Button::zr);
    twin.idle(30);
  }
  for (int page = 0; page < 2; ++page) {
    twin.press(provider::Button::zl);
    twin.idle(30);
  }
  EXPECT_EQ(twin.mismatched(), 0) << twin.first_difference();
  EXPECT_GT(twin.spliced(), 0U);
}

TEST_F(GalleryRetainedPaintTest, SheetsOverGlassMatchTheFullWalk) {
  Twin twin{Look{.page = examples::GalleryPage::presentations, .glass = true}};
  twin.idle(8);
  ASSERT_TRUE(twin.activate(examples::PresentationsPage::sheet_button));
  twin.idle(6);
  twin.press(provider::Button::dpad_down);
  ASSERT_TRUE(twin.activate(examples::PresentationsPage::background_picker));
  twin.idle(4);
  ASSERT_TRUE(twin.activate(examples::PresentationsPage::option_key(2)));
  twin.idle(16);
  ASSERT_TRUE(twin.activate(examples::PresentationsPage::done_button));
  twin.idle(20);
  ASSERT_TRUE(twin.activate(examples::PresentationsPage::popover_button));
  twin.idle(16);
  twin.press(provider::Button::b);
  twin.idle(16);
  ASSERT_TRUE(twin.activate(examples::PresentationsPage::toast_button));
  twin.idle(30);
  EXPECT_EQ(twin.mismatched(), 0) << twin.first_difference();
  EXPECT_GT(twin.spliced(), 0U);
}

TEST_F(GalleryRetainedPaintTest, SheetPresentAndDismissOverGlassSpliceThePage) {
  Twin twin{Look{.page = examples::GalleryPage::presentations, .glass = true}};
  twin.idle(8);
  for (int round = 0; round < 2; ++round) {
    ASSERT_TRUE(twin.activate(examples::PresentationsPage::sheet_button));
    twin.idle(40);
    ASSERT_TRUE(twin.activate(examples::PresentationsPage::done_button));
    twin.idle(40);
  }
  EXPECT_EQ(twin.mismatched(), 0) << twin.first_difference();
  EXPECT_GT(twin.steady_spliced(), 0U);
  EXPECT_LE(twin.steady_recorded(), 4U);
}

TEST_F(GalleryRetainedPaintTest, ToastInAndOutMatchTheFullWalk) {
  Twin twin{Look{.page = examples::GalleryPage::presentations, .glass = true}};
  twin.idle(8);
  ASSERT_TRUE(twin.activate(examples::PresentationsPage::toast_button));
  twin.idle(180);
  ASSERT_TRUE(twin.activate(examples::PresentationsPage::toast_button));
  twin.idle(30);
  EXPECT_EQ(twin.mismatched(), 0) << twin.first_difference();
  EXPECT_GT(twin.steady_spliced(), 0U);
  EXPECT_LE(twin.steady_recorded(), 6U);
}

TEST_F(GalleryRetainedPaintTest, PageCrossFadeOnASidebarSettleMatchesTheWalk) {
  for (const bool glass : {false, true}) {
    Twin twin{Look{.page = examples::GalleryPage::materials, .glass = glass}};
    twin.idle(8);
    for (const provider::Button button :
         {provider::Button::dpad_up, provider::Button::dpad_up,
          provider::Button::dpad_down, provider::Button::dpad_down}) {
      twin.step(pad({button}));
      twin.step(pad({}));
      twin.idle(30);
    }
    EXPECT_EQ(twin.mismatched(), 0) << glass << ": " << twin.first_difference();
    EXPECT_GT(twin.steady_spliced(), 0U) << glass;
  }
}

TEST_F(GalleryRetainedPaintTest, MatchedGeometryAndMotionMatchTheFullWalk) {
  Twin effects{Look{.page = examples::GalleryPage::effects}};
  effects.idle(8);
  ASSERT_TRUE(effects.activate(examples::EffectsPage::first_card));
  effects.idle(40);
  ASSERT_TRUE(effects.activate(examples::EffectsPage::detail_card));
  effects.idle(40);
  EXPECT_EQ(effects.mismatched(), 0) << effects.first_difference();
  EXPECT_GT(effects.spliced(), 0U);
  Twin motion{Look{.page = examples::GalleryPage::motion}};
  motion.idle(8);
  ASSERT_TRUE(motion.activate(examples::MotionPage::play_button));
  motion.idle(40);
  ASSERT_TRUE(motion.activate(examples::MotionPage::toggle_button));
  motion.idle(40);
  EXPECT_EQ(motion.mismatched(), 0) << motion.first_difference();
  EXPECT_GT(motion.spliced(), 0U);
}

TEST_F(GalleryRetainedPaintTest, GlassUnionAnimationMatchesTheFullWalk) {
  for (const bool glass : {false, true}) {
    Twin twin{Look{.glass = glass, .content = [] {
                     return composition::Element{
                         RetainedStage{RetainedStage::Scene::united}};
                   }}};
    twin.idle(8);
    for (int round = 0; round < 3; ++round) {
      ASSERT_TRUE(twin.activate(RetainedStage::toggle));
      twin.idle(round == 1 ? 6 : 40);
    }
    EXPECT_EQ(twin.mismatched(), 0) << glass << ": " << twin.first_difference();
    EXPECT_GT(twin.spliced(), 0U) << glass;
  }
}

TEST_F(GalleryRetainedPaintTest, AFadingGlassSubtreeReplaysItsOpacity) {
  for (const bool glass : {false, true}) {
    Twin twin{Look{.glass = glass, .content = [] {
                     return composition::Element{
                         RetainedStage{RetainedStage::Scene::faded}};
                   }}};
    twin.idle(8);
    for (int round = 0; round < 2; ++round) {
      ASSERT_TRUE(twin.activate(RetainedStage::toggle));
      twin.idle(40);
    }
    EXPECT_EQ(twin.mismatched(), 0) << glass << ": " << twin.first_difference();
    EXPECT_GT(twin.steady_spliced(), 0U) << glass;
    EXPECT_LE(twin.steady_recorded(), 4U) << glass;
    EXPECT_EQ(twin.steady_uploaded(), 0U) << glass;
  }
}

TEST_F(GalleryRetainedPaintTest, TiltAndShakeMatchTheFullWalk) {
  for (const bool glass : {false, true}) {
    Twin twin{Look{.glass = glass, .content = [] {
                     return composition::Element{
                         RetainedStage{RetainedStage::Scene::tilted}};
                   }}};
    twin.idle(8);
    for (int step = 0; step < 12; ++step) {
      twin.tilt(provider::Stick{.x = 0.8F, .y = -0.4F});
    }
    twin.idle(30);
    twin.press(provider::Button::dpad_up);
    twin.idle(24);
    twin.press(provider::Button::dpad_right);
    twin.idle(24);
    EXPECT_EQ(twin.mismatched(), 0) << glass << ": " << twin.first_difference();
    EXPECT_GT(twin.spliced(), 0U) << glass;
  }
}

TEST_F(GalleryRetainedPaintTest, AFlippingToggleRecordsOnlyItsOwnNodes) {
  for (const bool glass : {false, true}) {
    Twin twin{Look{.glass = glass, .content = [] {
                     return composition::Element{
                         RetainedStage{RetainedStage::Scene::switched}};
                   }}};
    twin.idle(8);
    for (int round = 0; round < 3; ++round) {
      ASSERT_TRUE(twin.activate(RetainedStage::control));
      twin.idle(round == 1 ? 3 : 30);
    }
    EXPECT_EQ(twin.mismatched(), 0) << glass << ": " << twin.first_difference();
    EXPECT_GT(twin.steady_spliced(), 0U) << glass;
    EXPECT_LE(twin.steady_recorded(),
              twin.nodes_under(RetainedStage::control) + 2)
        << glass;
  }
}

TEST_F(GalleryRetainedPaintTest, ASpinnerRecordsOnlyItsOwnNodes) {
  for (const bool glass : {false, true}) {
    Twin twin{Look{.glass = glass, .content = [] {
                     return composition::Element{
                         RetainedStage{RetainedStage::Scene::spinning}};
                   }}};
    twin.idle(60);
    EXPECT_EQ(twin.mismatched(), 0) << glass << ": " << twin.first_difference();
    EXPECT_GT(twin.steady_spliced(), 0U) << glass;
    EXPECT_GT(twin.nodes_under(RetainedStage::control), 0U) << glass;
    EXPECT_LE(twin.steady_recorded(), twin.nodes_under(RetainedStage::control))
        << glass;
  }
}

TEST_F(GalleryRetainedPaintTest, ASidebarCapsuleSpringRecordsOnlyTheCapsule) {
  for (const bool glass : {false, true}) {
    Twin twin{Look{.page = examples::GalleryPage::materials, .glass = glass}};
    twin.idle(8);
    for (const provider::Button button :
         {provider::Button::dpad_up, provider::Button::dpad_up,
          provider::Button::dpad_down}) {
      twin.step(pad({button}));
      twin.step(pad({}));
      twin.idle(40);
    }
    EXPECT_EQ(twin.mismatched(), 0) << glass << ": " << twin.first_difference();
    EXPECT_GT(twin.steady_spliced(), 0U) << glass;
    EXPECT_LE(twin.steady_median(), 6U) << glass;
  }
}

TEST_F(GalleryRetainedPaintTest,
       FocusMovesOverAHeavyPageRecordOnlyTheRowsInvolved) {
  for (const bool glass : {false, true}) {
    provider::headless::HeadlessProviderFactory providers;
    AppearanceOptions appearance{
        .policy = examples::GalleryScreen::policy(theme::ColorScheme::light),
        .storage = std::nullopt};
    if (glass) {
      appearance.policy.defaults.theme = "arisa-theme-glass";
    }
    Application application =
        start_gallery(providers, examples::GalleryPage::lists,
                      theme::ColorScheme::light, size, std::move(appearance));
    run(providers, application, 30);
    providers.input().push(pad({provider::Button::dpad_right}));
    providers.input().push(pad({}));
    run(providers, application, 40);
    std::uint32_t most = 0;
    std::uint64_t spliced = 0;
    int presented = 0;
    for (int press = 0; press < 3; ++press) {
      providers.input().push(pad({provider::Button::dpad_down}));
      providers.input().push(pad({}));
      for (int frame = 0; frame < 30; ++frame) {
        run(providers, application, 1);
        const FrameTimings& timings = application.last_frame();
        if (timings.presented) {
          ++presented;
          most = std::max(most, timings.recorded);
          spliced += timings.spliced;
        }
      }
    }
    const std::size_t nodes = application.root().tree().size();
    EXPECT_GT(presented, 0) << glass;
    EXPECT_GT(spliced, 0U) << glass;
    EXPECT_LE(static_cast<std::size_t>(most) * 10, nodes)
        << glass << ": " << most << " of " << nodes << " nodes recorded";
  }
}

INSTANTIATE_TEST_SUITE_P(
    Pages, GalleryRetainedPaintTest,
    ::testing::ValuesIn(examples::GalleryPages::all.begin(),
                        examples::GalleryPages::all.end()),
    [](const ::testing::TestParamInfo<examples::GalleryPage>& shown) {
      return std::string{examples::GalleryPages::name(shown.param)};
    });

}
