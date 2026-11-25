#pragma once

#include <arisa/components/vstack.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/composition/tree.hpp>
#include <arisa/layout/layout_context.hpp>
#include <arisa/layout/viewport.hpp>
#include <arisa/provider/headless/memory_fonts.hpp>
#include <arisa/provider/standard/directory_assets.hpp>
#include <arisa/text/font_collection.hpp>
#include <arisa/text/typesetter.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/shape.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

namespace arisa::layout {

class LayoutFixture {
 public:
  static constexpr Viewport handheld{
      .pixels = provider::SurfaceSize{.width = provider::SurfaceWidth{1280},
                                      .height = provider::SurfaceHeight{720}},
      .scale = provider::SurfaceScale::unit()};

  static auto docked() -> Viewport {
    return Viewport{
        .pixels =
            provider::SurfaceSize{.width = provider::SurfaceWidth{1920},
                                  .height = provider::SurfaceHeight{1080}},
        .scale = provider::SurfaceScale::create(1.5F).value()};
  }

  static auto box(std::string_view key, float width, float height)
      -> composition::Element {
    return components::VStack()
        .frame({.width = width, .height = height})
        .background(theme::ColourRole::accent, theme::Shape::rectangle())
        .id(composition::ElementKey{key});
  }

  static auto tree(composition::Element content)
      -> std::unique_ptr<composition::Tree> {
    auto created = std::make_unique<composition::Tree>(std::move(content),
                                                       [] { return true; });
    created->reconcile();
    return created;
  }

  static auto frame(const composition::Tree& tree, std::string_view key)
      -> composition::Rect {
    const auto found = tree.find(composition::ElementKey{key});
    EXPECT_TRUE(found.has_value()) << key;
    if (!found.has_value()) {
      return composition::Rect{};
    }
    return tree.node(*found)->get().frame();
  }

  static auto fonts() -> const text::FontCollection& {
    static const Fonts storage;
    return *storage.collection;
  }

  static auto typesetter() -> text::Typesetter& {
    static text::Typesetter instance =
        std::move(text::Typesetter::create(fonts()).value());
    return instance;
  }

 private:
  struct Fonts {
    provider::headless::MemoryFonts system;
    std::optional<text::FontCollection> collection;

    Fonts() {
      const provider::standard::DirectoryAssets assets{
          std::filesystem::path{ARISA_SOURCE_ASSETS}};
      auto loaded = text::FontCollection::load(assets, system);
      EXPECT_TRUE(loaded.has_value());
      collection.emplace(std::move(*loaded));
    }
  };
};

}
