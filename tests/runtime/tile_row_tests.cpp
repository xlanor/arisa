#include "focus_fixture.hpp"

#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/modifiers.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/provider/assets/asset_path.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/structure/tile_row.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/typography.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::runtime {

class TileRowTest : public FocusFixture {
 protected:
  using Row = structure::TileRow;
  using Button = provider::Button;

  static auto row(std::shared_ptr<std::vector<std::size_t>> chosen,
                  Row::Options options = {},
                  std::vector<std::string> hints = {}) -> composition::Element {
    std::vector<Row::Tile> tiles;
    const std::vector<std::string> titles{"Remote Play", "Cloud", "Couch"};
    for (std::size_t index = 0; index < titles.size(); ++index) {
      tiles.push_back(Row::Tile{
          .title = titles[index],
          .system_image = "sports_esports",
          .hint = hints.empty() ? "About " + titles[index] : hints[index],
          .action = [chosen, index] { chosen->push_back(index); }});
    }
    return Row(std::move(tiles), options);
  }

  static auto hint(Application& application) -> std::string {
    const auto node = node_named(application, Row::hint_key);
    if (!node.has_value()) {
      return {};
    }
    std::vector<std::string> found;
    const auto text = node->get().content_as<components::Text::Content>();
    if (text.has_value()) {
      return std::string{text->get().text()};
    }
    for (const composition::Node& child : node->get().children()) {
      if (const auto inner = child.content_as<components::Text::Content>()) {
        return std::string{inner->get().text()};
      }
    }
    return {};
  }

  static void fonts(provider::headless::HeadlessProviderFactory& providers) {
    const std::string file = "arisa/fonts/InterVariable.ttf";
    std::ifstream stream{std::filesystem::path{ARISA_SOURCE_ASSETS} / file,
                         std::ios::binary};
    const std::vector<char> raw{std::istreambuf_iterator<char>{stream},
                                std::istreambuf_iterator<char>{}};
    std::vector<std::byte> bytes(raw.size());
    std::ranges::transform(raw, bytes.begin(),
                           [](char value) { return std::byte(value); });
    ASSERT_FALSE(bytes.empty());
    providers.assets().add(*provider::AssetPath::create(file), bytes);
  }

  static auto filled(const composition::Node& node, theme::ColourRole role)
      -> bool {
    const auto& background = node.modifiers().background;
    if (background.has_value() &&
        background->fill == graphics::ShapeStyle{role}) {
      return true;
    }
    return std::ranges::any_of(
        node.children(),
        [role](const composition::Node& child) { return filled(child, role); });
  }

  static auto title(const composition::Node& node, std::string_view text)
      -> std::optional<composition::Rect> {
    if (const auto written = node.content_as<components::Text::Content>();
        written.has_value() && written->get().text() == text) {
      return node.frame();
    }
    for (const composition::Node& child : node.children()) {
      if (auto found = title(child, text)) {
        return found;
      }
    }
    return std::nullopt;
  }

  provider::headless::HeadlessProviderFactory providers_;
};

TEST_F(TileRowTest, TheHintFollowsTheFocusedTile) {
  auto chosen = std::make_shared<std::vector<std::size_t>>();
  Application application = start(providers_, row(chosen));
  EXPECT_EQ(focused_key(application), Row::tile_key(0));
  EXPECT_EQ(hint(application), "About Remote Play");
  press(providers_, application, Button::dpad_right);
  EXPECT_EQ(focused_key(application), Row::tile_key(1));
  EXPECT_EQ(hint(application), "About Cloud");
  press(providers_, application, Button::dpad_right);
  EXPECT_EQ(hint(application), "About Couch");
  press(providers_, application, Button::a);
  ASSERT_EQ(chosen->size(), 1U);
  EXPECT_EQ(chosen->front(), 2U);
}

TEST_F(TileRowTest, TheHintKeepsItsHeightAndDefaultFocusPicksATile) {
  fonts(providers_);
  auto chosen = std::make_shared<std::vector<std::size_t>>();
  Application application = start(
      providers_,
      row(chosen,
          Row::Options{.tile_width = 168.0F,
                       .tile_height = 112.0F,
                       .default_focus = 1},
          {"", "Stream from a console on your network or over the internet",
           "Short"}));
  EXPECT_EQ(focused_key(application), Row::tile_key(1));
  const auto shown = node_named(application, Row::hint_key);
  ASSERT_TRUE(shown.has_value());
  const float tall = shown->get().frame().height;
  EXPECT_GT(tall, 20.0F);
  press(providers_, application, Button::dpad_left);
  EXPECT_EQ(focused_key(application), Row::tile_key(0));
  EXPECT_EQ(hint(application), "");
  const auto empty = node_named(application, Row::hint_key);
  ASSERT_TRUE(empty.has_value());
  EXPECT_FLOAT_EQ(empty->get().frame().height, tall);
  const auto first = node_named(application, Row::tile_key(0));
  const auto second = node_named(application, Row::tile_key(1));
  ASSERT_TRUE(first.has_value() && second.has_value());
  EXPECT_NEAR(second->get().frame().x - first->get().frame().x,
              168.0F + Row::tile_spacing, 0.5F);
  EXPECT_LT(first->get().frame().y + first->get().frame().height,
            empty->get().frame().y);
}

TEST_F(TileRowTest, AProminentTileTakesTheAccentAndLabelsWrapToTheirLineCount) {
  fonts(providers_);
  std::vector<Row::Tile> tiles;
  tiles.push_back(Row::Tile{.title = "Remote Play over the internet",
                            .system_image = "sports_esports",
                            .hint = "Recommended",
                            .action = [] {},
                            .prominent = true});
  tiles.push_back(Row::Tile{.title = "Couch",
                            .system_image = "sports_esports",
                            .hint = "Local",
                            .action = [] {},
                            .prominent = false});
  Application application = start(
      providers_, Row(std::move(tiles),
                      Row::Options{.tile_width = 108.0F,
                                   .tile_height = 108.0F,
                                   .default_focus = std::nullopt,
                                   .label_font = theme::Font::system(12.5F),
                                   .label_lines = 2}));
  const auto first = node_named(application, Row::tile_key(0));
  const auto second = node_named(application, Row::tile_key(1));
  ASSERT_TRUE(first.has_value() && second.has_value());
  EXPECT_TRUE(filled(first->get(), theme::ColourRole::accent));
  EXPECT_FALSE(filled(second->get(), theme::ColourRole::accent));
  EXPECT_TRUE(filled(second->get(), theme::ColourRole::secondary_system_fill));
  EXPECT_NEAR(first->get().frame().width, 108.0F, 0.5F);
  const auto wrapped = title(first->get(), "Remote Play over the internet");
  const auto single = title(second->get(), "Couch");
  ASSERT_TRUE(wrapped.has_value() && single.has_value());
  EXPECT_GT(wrapped->height, single->height * 1.5F);
  EXPECT_LE(wrapped->width, 108.0F);
}
}
