#include <arisa/components/image.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/text_renderer.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/text_selectability.hpp>
#include <arisa/controls/text_selection_host.hpp>
#include <arisa/graphics/gradient.hpp>
#include <arisa/graphics/image_paint.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/provider/assets/asset_path.hpp>
#include <arisa/provider/diagnostics/provider.hpp>
#include <arisa/provider/gpu/image.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/surface/surface_scale.hpp>
#include <arisa/provider/surface/surface_size.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/material.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/typography.hpp>

#include <focus_fixture.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace arisa::runtime {

using composition::Element;

class TextureLifetimeTest : public FocusFixture {
 protected:
  static constexpr provider::SurfaceSize extent{
      .width = provider::SurfaceWidth{240},
      .height = provider::SurfaceHeight{160}};

  class Scene final : public composition::Component {
   public:
    static constexpr composition::StateKey<int> tick{"lifetime-tick", 0};

    explicit Scene(bool glass) : glass_(glass) {}

    auto operator==(const Scene&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto ticking = context.state(tick);
      ticks() = ticking;
      const theme::Colour blinking = ticking.get() % 2 == 0
                                         ? theme::Colour::hex(0xFF2020)
                                         : theme::Colour::hex(0x20FF20);
      Element ramp =
          components::ZStack()
              .frame({.width = 120.0F, .height = 64.0F})
              .background(
                  graphics::LinearGradient{
                      .gradient =
                          graphics::Gradient{std::vector<theme::ColourValue>{
                              theme::Colour::hex(0xFF8000),
                              theme::Colour::hex(0x0080FF)}},
                      .start_point = motion::UnitPoint::leading,
                      .end_point = motion::UnitPoint::trailing},
                  theme::Shape::rounded(8.0F));
      Element blinker = components::ZStack()
                            .frame({.width = 40.0F, .height = 20.0F})
                            .background(blinking);
      if (!glass_) {
        return components::VStack(std::move(ramp), std::move(blinker));
      }
      return components::ZStack(
          components::VStack(std::move(ramp), std::move(blinker)),
          components::Text(std::to_string(ticking.get()))
              .frame({.width = 100.0F, .height = 40.0F})
              .glass_effect(theme::Glass::regular(), theme::Shape::capsule()),
          components::ZStack()
              .frame({.width = 80.0F, .height = 30.0F})
              .background(theme::Material::thin)
              .offset(0.0F, 50.0F));
    }

    static auto ticks() -> std::optional<composition::StateHandle<int>>& {
      static std::optional<composition::StateHandle<int>> held;
      return held;
    }

   private:
    bool glass_{false};
  };

  class Textured final : public composition::Component {
   public:
    static constexpr composition::StateKey<int> tick{"lifetime-textured", 0};

    explicit Textured(std::shared_ptr<const provider::Image> pixels)
        : pixels_(std::move(pixels)) {}

    auto operator==(const Textured&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto ticking = context.state(tick);
      Scene::ticks() = ticking;
      return components::VStack(
          components::Image{pixels_}
              .resizable(theme::EdgeInsets::all(theme::Space{2.0F}),
                         components::Image::ResizingMode::tile)
              .frame({.width = 60.0F, .height = 30.0F}),
          components::ZStack()
              .frame({.width = 60.0F, .height = 30.0F})
              .background(graphics::ShapeStyle{graphics::ImagePaint{pixels_}},
                          theme::Shape::rectangle()),
          components::ZStack()
              .frame({.width = 40.0F, .height = 20.0F})
              .background(ticking.get() % 2 == 0
                              ? theme::Colour::hex(0xFF2020)
                              : theme::Colour::hex(0x20FF20)));
    }

   private:
    std::shared_ptr<const provider::Image> pixels_;
  };

  class Inline final : public composition::Component {
   public:
    static constexpr composition::StateKey<int> tick{"lifetime-inline", 0};

    explicit Inline(std::shared_ptr<const provider::Image> pixels)
        : pixels_(std::move(pixels)) {}

    auto operator==(const Inline&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto ticking = context.state(tick);
      Scene::ticks() = ticking;
      return components::VStack(
          (components::Text("A ") +
           components::Text(components::Image{pixels_}) +
           components::Text(" B"))
              .font(theme::Font::system(20.0F)),
          components::ZStack()
              .frame({.width = 40.0F, .height = 20.0F})
              .background(ticking.get() % 2 == 0
                              ? theme::Colour::hex(0xFF2020)
                              : theme::Colour::hex(0x20FF20)));
    }

   private:
    std::shared_ptr<const provider::Image> pixels_;
  };

  class Rendered final : public composition::Component {
   public:
    static constexpr composition::StateKey<int> tick{"lifetime-rendered", 0};

    class Lifted final : public components::TextRenderer {
     public:
      void draw(const components::Text::Layout& layout,
                components::GraphicsContext& context) const override {
        for (const components::Text::Layout::Line line : layout) {
          context.translate_by(0.0F, -1.0F);
          context.draw(line);
        }
      }
    };

    auto operator==(const Rendered&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto ticking = context.state(tick);
      Scene::ticks() = ticking;
      return components::VStack(
          components::Text("Rendered glyphs")
              .font(theme::Font::system(20.0F))
              .text_renderer(Lifted{}),
          components::Text("Selected words")
              .text_selection(composition::TextSelectability::enabled),
          components::ZStack()
              .frame({.width = 40.0F, .height = 20.0F})
              .background(ticking.get() % 2 == 0
                              ? theme::Colour::hex(0xFF2020)
                              : theme::Colour::hex(0x20FF20)));
    }
  };

  static void fonts(provider::headless::HeadlessProviderFactory& providers) {
    const std::filesystem::path path =
        std::filesystem::path{ARISA_SOURCE_ASSETS} /
        "arisa/fonts/InterVariable.ttf";
    std::ifstream file{path, std::ios::binary};
    const std::vector<char> raw{std::istreambuf_iterator<char>{file},
                                std::istreambuf_iterator<char>{}};
    std::vector<std::byte> bytes(raw.size());
    std::ranges::transform(raw, bytes.begin(),
                           [](char value) { return std::byte(value); });
    ASSERT_FALSE(bytes.empty());
    providers.assets().add(
        *provider::AssetPath::create("arisa/fonts/InterVariable.ttf"), bytes);
  }

  static auto stripes() -> std::shared_ptr<const provider::Image> {
    std::vector<std::byte> bytes;
    for (std::uint32_t at = 0; at < 8U * 8U; ++at) {
      const std::uint8_t level = (at % 2) == 0 ? 40 : 220;
      for (const std::uint8_t channel :
           {level, std::uint8_t{90}, level, std::uint8_t{255}}) {
        bytes.push_back(static_cast<std::byte>(channel));
      }
    }
    auto made = provider::Image::create(8, 8, std::move(bytes));
    EXPECT_TRUE(made.has_value());
    return std::make_shared<const provider::Image>(std::move(*made));
  }

  static auto launch(provider::headless::HeadlessProviderFactory& providers,
                     bool glass) -> Application {
    providers.surface().resize(extent);
    providers.surface().set_scale(provider::SurfaceScale::unit());
    return start(providers, Element{Scene{glass}});
  }

  static void tick(Application& application, int frames_wanted) {
    for (int index = 1; index <= frames_wanted; ++index) {
      Scene::ticks()->set(index);
      const auto framed = application.frame();
      ASSERT_TRUE(framed.has_value())
          << "frame " << index << ": " << framed.error().explanation();
    }
  }
};

TEST_F(TextureLifetimeTest, AStaticGradientBesideARepaintingSiblingStaysLive) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = launch(providers, false);
  tick(application, 12);
  Scene::ticks().reset();
}

TEST_F(TextureLifetimeTest,
       StaticSlicedAndPaintedImagesBesideARepaintStayLive) {
  provider::headless::HeadlessProviderFactory providers;
  providers.surface().resize(extent);
  providers.surface().set_scale(provider::SurfaceScale::unit());
  Application application = start(providers, Element{Textured{stripes()}});
  tick(application, 6);
  Scene::ticks().reset();
  EXPECT_GT(providers.gpu().draws(), 0U);
}

TEST_F(TextureLifetimeTest, AStaticInlineTextImageBesideARepaintStaysLive) {
  provider::headless::HeadlessProviderFactory providers;
  fonts(providers);
  providers.surface().resize(extent);
  providers.surface().set_scale(provider::SurfaceScale::unit());
  Application application = start(providers, Element{Inline{stripes()}});
  tick(application, 6);
  Scene::ticks().reset();
  EXPECT_GT(providers.gpu().draws(), 0U);
}

TEST_F(TextureLifetimeTest, StaticRenderedTextBesideARepaintStaysLive) {
  provider::headless::HeadlessProviderFactory providers;
  fonts(providers);
  providers.surface().resize(extent);
  providers.surface().set_scale(provider::SurfaceScale::unit());
  Application application = start(providers, Element{Rendered{}});
  tick(application, 6);
  Scene::ticks().reset();
  EXPECT_GT(providers.gpu().draws(), 0U);
}

TEST_F(TextureLifetimeTest, GlassUnderRenderTargetPressureKeepsTheMainDraw) {
  for (std::size_t limit = 0; limit <= 8; ++limit) {
    provider::headless::HeadlessProviderFactory providers;
    providers.gpu().limit_render_targets(
        provider::headless::NullGpu::RenderTargetLimit{.count = limit});
    Application application = launch(providers, true);
    tick(application, 12);
    Scene::ticks().reset();
  }
}

TEST_F(TextureLifetimeTest, AnEffectWhoseTargetCannotBeCreatedDrawsWithoutIt) {
  provider::headless::HeadlessProviderFactory providers;
  providers.gpu().limit_render_targets(
      provider::headless::NullGpu::RenderTargetLimit{.count = 0});
  providers.surface().resize(extent);
  providers.surface().set_scale(provider::SurfaceScale::unit());
  Application application = start(
      providers,
      Element{components::ZStack(components::ZStack()
                                     .frame({.width = 80.0F, .height = 40.0F})
                                     .background(theme::Colour::hex(0x3060C0))
                                     .blur(4.0F))});
  for (int index = 0; index < 4; ++index) {
    ASSERT_TRUE(application.frame().has_value());
  }
  EXPECT_GT(providers.gpu().draws(), 0U);
  const auto reports = providers.diagnostics().snapshot();
  EXPECT_TRUE(
      std::ranges::any_of(reports, [](const provider::Diagnostic& each) {
        return each.message.contains("layer render target") &&
               each.message.contains("drawn directly");
      }));
}

TEST_F(TextureLifetimeTest, ARejectedBackdropPassStillDrawsTheFrame) {
  provider::headless::HeadlessProviderFactory providers;
  providers.gpu().reject_draws(
      provider::headless::NullGpu::DrawRejection{.skipped = 0, .passes = true});
  Application application = launch(providers, true);
  tick(application, 6);
  Scene::ticks().reset();
  const auto reports = providers.diagnostics().snapshot();
  EXPECT_TRUE(
      std::ranges::any_of(reports, [](const provider::Diagnostic& each) {
        return each.message.contains("pass") && each.category == "glass";
      }));
}

}
