#include <arisa/components/attributed_string.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/text.hpp>
#include <arisa/composition/accessibility.hpp>
#include <arisa/composition/text_attributes.hpp>
#include <arisa/graphics/gradient.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/motion/content_transition.hpp>
#include <arisa/provider/gpu/image.hpp>
#include <arisa/text/line_height.hpp>
#include <arisa/text/line_pattern.hpp>
#include <arisa/text/localized_string_key.hpp>
#include <arisa/text/number_format_style.hpp>
#include <arisa/text/string_catalog.hpp>
#include <arisa/text/text_layout.hpp>
#include <arisa/text/typesetting_language.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/dynamic_type_size.hpp>
#include <arisa/theme/theme.hpp>
#include <arisa/theme/typography.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::components {

using namespace std::chrono_literals;

class TextParityTest : public ::testing::Test {
 protected:
  using Attributes = AttributedString::Attributes;
  using Scopes = AttributedString::Scopes;

  static auto content(const Text& text) -> const Text::Content& {
    return static_cast<const Text::Content&>(
        static_cast<const composition::Element&>(text).content());
  }

  static auto shown(const Text& text, std::string_view locale = "en")
      -> std::string {
    return std::string{content(text)
                           .resolved(composition::TextAttributes{},
                                     text::Locale::create(locale).value_or(
                                         text::Locale::undetermined()))
                           .text()};
  }

  static auto pixels() -> std::shared_ptr<const provider::Image> {
    std::vector<std::byte> bytes(8U * 4U * 4U, std::byte{200});
    auto made = provider::Image::create(8, 4, std::move(bytes));
    EXPECT_TRUE(made.has_value());
    return std::make_shared<const provider::Image>(std::move(*made));
  }

  const theme::Theme theme = theme::Theme::defaults();
};

TEST_F(TextParityTest, SwiftUIAttributeKeysReadAndWriteRuns) {
  AttributedString text{"Kerned raised"};
  text = text.setting<Scopes::SwiftUIAttributes::KerningAttribute>(4.0F, 0, 6);
  text = text.setting<Scopes::SwiftUIAttributes::BaselineOffsetAttribute>(
      6.0F, 7, 13);
  text = text.setting<Scopes::SwiftUIAttributes::TrackingAttribute>(1.5F, 0, 3);
  text = text.setting<Scopes::SwiftUIAttributes::BackgroundColorAttribute>(
      theme::ColourValue{theme::ColourRole::yellow}, 0, 6);
  text = text.setting<Scopes::SwiftUIAttributes::FontAttribute>(
      theme::Font::title, 7, 13);
  text = text.setting<Scopes::SwiftUIAttributes::LineHeightAttribute>(
      text::LineHeight::exact(30.0F), 0, 13);
  text = text.setting<Scopes::SwiftUIAttributes::AlignmentAttribute>(
      composition::TextAlignment::center, 0, 13);
  text = text.setting<Scopes::SwiftUIAttributes::UnderlineStyleAttribute>(
      composition::TextDecoration{.active = true,
                                  .colour = theme::ColourRole::red,
                                  .pattern = text::LinePattern::dash},
      0, 6);
  text = text.setting<Scopes::SwiftUIAttributes::StrikethroughStyleAttribute>(
      composition::TextDecoration{.active = true,
                                  .pattern = text::LinePattern::dot},
      7, 13);
  text = text.setting<Scopes::SwiftUIAttributes::AccessibilityAttribute>(
      AttributedString::Speech{
          .spells_out_characters = true,
          .heading_level = composition::AccessibilityHeadingLevel::h2},
      0, 6);
  text = text.setting<Scopes::FoundationAttributes::LinkAttribute>(
      std::string{"https://example.com"}, 7, 13);
  EXPECT_EQ(text.value<Scopes::SwiftUIAttributes::KerningAttribute>(1),
            std::optional<float>{4.0F});
  EXPECT_EQ(text.value<Scopes::SwiftUIAttributes::BaselineOffsetAttribute>(8),
            std::optional<float>{6.0F});
  EXPECT_EQ(text.value<Scopes::SwiftUIAttributes::TrackingAttribute>(1),
            std::optional<float>{1.5F});
  EXPECT_TRUE(text.value<Scopes::SwiftUIAttributes::BackgroundColorAttribute>(0)
                  .has_value());
  EXPECT_EQ(text.value<Scopes::SwiftUIAttributes::FontAttribute>(9),
            std::optional{theme::Font::title});
  EXPECT_EQ(text.value<Scopes::SwiftUIAttributes::LineHeightAttribute>(9),
            std::optional{text::LineHeight::exact(30.0F)});
  EXPECT_EQ(text.value<Scopes::SwiftUIAttributes::AlignmentAttribute>(9),
            std::optional{composition::TextAlignment::center});
  EXPECT_EQ(text.value<Scopes::SwiftUIAttributes::UnderlineStyleAttribute>(1)
                ->pattern,
            text::LinePattern::dash);
  EXPECT_EQ(
      text.value<Scopes::SwiftUIAttributes::StrikethroughStyleAttribute>(8)
          ->pattern,
      text::LinePattern::dot);
  EXPECT_TRUE(text.value<Scopes::SwiftUIAttributes::AccessibilityAttribute>(2)
                  ->spells_out_characters);
  EXPECT_EQ(text.value<Scopes::FoundationAttributes::LinkAttribute>(8),
            std::optional<std::string>{"https://example.com"});
  const text::TextRun& first = text.runs().runs().front();
  EXPECT_EQ(first.kerning, std::optional<float>{4.0F});
  EXPECT_NE(first.background, 0U);
  EXPECT_NE(first.underline_ink, 0U);
  EXPECT_EQ(first.underline_pattern, text::LinePattern::dash);
  EXPECT_EQ(Scopes::SwiftUIAttributes::KerningAttribute::name, "SwiftUI.Kern");
  EXPECT_EQ(Scopes::SwiftUIAttributes::BaselineOffsetAttribute::name,
            "SwiftUI.BaselineOffset");
  EXPECT_EQ(Scopes::SwiftUIAttributes::TrackingAttribute::name,
            "SwiftUI.Tracking");
  EXPECT_EQ(Scopes::SwiftUIAttributes::UnderlineStyleAttribute::name,
            "SwiftUI.UnderlineStyle");
  EXPECT_EQ(Scopes::swift_ui().size(), 12U);
  EXPECT_EQ(Scopes::SwiftUIAttributes::foundation(), "NSLink");
}

TEST_F(TextParityTest, DrawingOptionsReachTheTextStyleThePainterDraws) {
  using Options = text::TextLayout::DrawingOptions;
  const Text plain{"plain"};
  const Text exact =
      Text{"exact"}.drawing_options(Options::disables_subpixel_quantization);
  const auto style_of = [this](const Text& text) {
    return Text::text_style(theme, theme::Font::body,
                            text::Locale::undetermined(), std::nullopt,
                            content(text).attributes_in({}),
                            composition::LayoutDirection::left_to_right);
  };
  EXPECT_FALSE(style_of(plain).drawing.contains(
      Options::disables_subpixel_quantization));
  EXPECT_TRUE(style_of(exact).drawing.contains(
      Options::disables_subpixel_quantization));
  EXPECT_FALSE(content(plain).equals(content(
      Text{"plain"}.drawing_options(Options::disables_subpixel_quantization))));
}

TEST_F(TextParityTest, StyledRunsResolveFontsAndGlyphSizes) {
  AttributedString text{"Title body"};
  text = text.setting<Scopes::SwiftUIAttributes::FontAttribute>(
      theme::Font::title.small_caps(), 0, 5);
  text = text + AttributedString::glyph(AttributedString::AdaptiveImageGlyph{
                    .image = pixels(),
                    .identifier = "sticker",
                    .content_description = "A sticker"});
  const text::TextStyle base =
      Text::text_style(theme, theme::Font::body, text::Locale::undetermined());
  const text::TextRuns runs = Text::styled_runs(theme, base, text);
  ASSERT_GE(runs.runs().size(), 2U);
  EXPECT_FLOAT_EQ(runs.runs().front().size->value(),
                  theme.type(theme::TextRole::title).size);
  EXPECT_EQ(runs.runs().front().small_caps, text::SmallCaps::all);
  const text::TextRun& glyph = runs.runs().back();
  ASSERT_TRUE(glyph.attachment.has_value());
  EXPECT_FLOAT_EQ(glyph.attachment->height,
                  base.size.value() * AttributedString::glyph_height);
  EXPECT_FLOAT_EQ(glyph.attachment->width, glyph.attachment->height * 2.0F);
  EXPECT_EQ(text.glyphs().front().identifier, "sticker");
  EXPECT_EQ(
      text.value<Scopes::SwiftUIAttributes::AdaptiveImageGlyphAttribute>(10)
          ->content_description,
      "A sticker");
}

TEST_F(TextParityTest, MarkdownAddsAutolinksImagesAndHardBreaks) {
  const AttributedString linked =
      AttributedString::markdown("see <https://example.com> now");
  EXPECT_EQ(linked.text(), "see https://example.com now");
  ASSERT_EQ(linked.links().size(), 1U);
  EXPECT_EQ(linked.links().front().destination, "https://example.com");
  const AttributedString imaged =
      AttributedString::markdown("a ![star](star.png) b");
  ASSERT_EQ(imaged.images().size(), 1U);
  EXPECT_EQ(imaged.images().front().name, "star.png");
  EXPECT_EQ(imaged.images().front().label, "star");
  EXPECT_TRUE(imaged.unresolved());
  const AttributedString loaded = imaged.resolving(
      [](std::string_view name) -> std::shared_ptr<const provider::Image> {
        return name == "star.png" ? pixels() : nullptr;
      });
  EXPECT_FALSE(loaded.unresolved());
  EXPECT_FLOAT_EQ(loaded.runs().runs().front().attachment->width, 8.0F);
  EXPECT_EQ(AttributedString::markdown("one  \ntwo").text(), "one\ntwo");
  EXPECT_EQ(AttributedString::markdown("one\\\ntwo").text(), "one\ntwo");
  EXPECT_EQ(AttributedString::markdown("a <not a link> b").text(),
            "a <not a link> b");
}

TEST_F(TextParityTest, ConcatenationKeepsEverySegmentsFontAndSource) {
  const Text joined = Text("Title").font(theme::Font::title) + Text(" body");
  const AttributedString& attributed = content(joined).attributed();
  ASSERT_EQ(attributed.fonts().size(), 1U);
  EXPECT_EQ(attributed.fonts().front(), theme::Font::title);
  EXPECT_FALSE(content(joined).explicit_font().has_value());
  const Text number =
      Text("Total: ") +
      Text(9.99, text::NumberFormatStyle::currency("USD")).bold();
  EXPECT_EQ(shown(number), "Total: $9.99");
  EXPECT_TRUE(content(number).localized());
  const Text dashed =
      Text("A").underline(true, text::LinePattern::dash_dot,
                          theme::ColourValue{theme::ColourRole::red}) +
      Text("B").strikethrough(true, text::LinePattern::dot);
  const AttributedString& decorated = content(dashed).attributed();
  EXPECT_EQ(decorated.runs().runs().front().underline_pattern,
            text::LinePattern::dash_dot);
  EXPECT_EQ(decorated.runs().runs().back().strikethrough_pattern,
            text::LinePattern::dot);
  const Text shaded =
      Text("G").foreground_style(graphics::LinearGradient{
          .gradient = graphics::Gradient{std::vector<theme::ColourValue>{
              theme::ColourRole::red, theme::ColourRole::blue}}}) +
      Text("h");
  EXPECT_EQ(content(shaded).attributed().shadings().size(), 1U);
}

TEST_F(TextParityTest, TextComparesByContent) {
  EXPECT_EQ(Text("a"), Text("a"));
  EXPECT_FALSE(Text("a") == Text("b"));
  EXPECT_FALSE(Text("a") == Text("a").bold());
  EXPECT_EQ(Text("a").font(theme::Font::title),
            Text("a").font(theme::Font::title));
}

TEST_F(TextParityTest, InlineImagesBecomeGlyphsOrAttachments) {
  const Text symbol =
      Text("A ") + Text(Image{Image::SystemName{"star.fill"}}) + Text(" B");
  EXPECT_NE(content(symbol).attributed().text().find("A "), std::string::npos);
  EXPECT_GT(content(symbol).attributed().text().size(), 4U);
  const Text picture{Image{pixels(), 2.0F}};
  ASSERT_EQ(content(picture).attributed().images().size(), 1U);
  EXPECT_FLOAT_EQ(content(picture).attributed().images().front().width(), 4.0F);
  EXPECT_FLOAT_EQ(
      content(picture).attributed().runs().runs().front().attachment->height,
      2.0F);
  const Text named{Image{"photo"}};
  EXPECT_TRUE(content(named).attributed().unresolved());
  EXPECT_TRUE(content(named).refreshes());
}

TEST_F(TextParityTest, InterpolationEmbedsTextImagesAndFormats) {
  const text::LocalizedStringKey key{
      "Pay {} {} now", Text("dear").bold(),
      text::LocalizedStringKey::formatted(
          5.5, text::NumberFormatStyle::currency("EUR"))};
  const Text text{key};
  EXPECT_EQ(shown(text), "Pay dear €5.50 now");
  const AttributedString& resolved = content(text).resolved(
      composition::TextAttributes{}, *text::Locale::create("en"));
  EXPECT_EQ(resolved.attributes(5).weight,
            std::optional{theme::Font::Weight::bold});
  const text::LocalizedStringKey imaged{"Go {}", Text(Image{pixels(), 1.0F})};
  EXPECT_EQ(
      content(Text{imaged})
          .resolved(composition::TextAttributes{}, *text::Locale::create("en"))
          .images()
          .size(),
      1U);
  EXPECT_EQ(
      text::StringCatalog{}.resolve(
          text::LocalizedStringKey{
              "Name {}", Text::accessibility_name(theme::ColourRole::red)},
          *text::Locale::create("en")),
      "Name red");
  EXPECT_EQ(Text::colour_name(theme::Colour::hex(0x1060E0)), "blue");
  EXPECT_EQ(Text::colour_name(theme::Colour::hex(0x101010)), "black");
  EXPECT_EQ(Text::colour_name(theme::Colour::hex(0x0A2A0A)), "dark green");
  const text::LocalizedStringKey attributed{
      "Say {}", AttributedString{"hi", Attributes{.italic = true}}};
  EXPECT_EQ(shown(Text{attributed}), "Say hi");
}

TEST_F(TextParityTest, LiveTextReportsItsNextChange) {
  using Date = Text::Date;
  const Date now{std::chrono::seconds{1778307200}};
  const Text timer = Text::timer_interval({.start = now, .end = now + 100s});
  EXPECT_TRUE(content(timer).live());
  ASSERT_TRUE(content(timer).next(now).has_value());
  EXPECT_EQ(*content(timer).next(now) - now, 1s);
  const Text paused =
      Text::timer_interval({.start = now, .end = now + 100s}, now + 10s, true);
  EXPECT_FALSE(content(paused).next(now + 20s).has_value());
  const Text relative{now - 3h, Text::DateStyle::relative};
  EXPECT_TRUE(content(relative).live());
  EXPECT_EQ(*content(relative).next(now) - now, 60s);
  const Text offset{now - std::chrono::days{45}, Text::DateStyle::offset};
  EXPECT_TRUE(content(offset).live());
  const Text clock{now - 152s, Text::DateStyle::timer};
  EXPECT_EQ(*content(clock).next(now) - now, 1s);
  const Text range{DateInterval{.start = now, .end = now + 1h}};
  EXPECT_FALSE(content(range).live());
  EXPECT_FALSE(shown(range).empty());
  const Text interpolated{text::LocalizedStringKey{
      "Ends in {}", Text::timer_argument({.start = now, .end = now + 90s})}};
  EXPECT_TRUE(content(interpolated).live());
  const Text dated{text::LocalizedStringKey{
      "Seen {}", Text::argument(now - 2h, Text::DateStyle::relative)}};
  EXPECT_TRUE(content(dated).live());
  const Text sourced{TimeDataSource<Date>::current_date(),
                     SystemFormatStyle::DateReference{now - 1h}};
  EXPECT_TRUE(content(sourced).live());
  const Text keyed{text::LocalizedStringKey{
      "Ago {}", Text::argument(TimeDataSource<Date>::current_date(),
                               SystemFormatStyle::DateReference{now - 1h})}};
  EXPECT_TRUE(content(keyed).live());
  const Text joined =
      Text("Left ") + Text::timer_interval({.start = now, .end = now + 100s});
  EXPECT_TRUE(content(joined).live());
}

TEST_F(TextParityTest, FontFormsResolveToConcreteMetrics) {
  const theme::Font::Context context = theme.font_context();
  const theme::Font::Resolved body = theme::Font::body.resolve(context);
  EXPECT_FLOAT_EQ(body.point_size, theme.type(theme::TextRole::body).size);
  EXPECT_FALSE(body.is_bold);
  const theme::Font styled =
      theme::Font::system(theme::TextRole::title, theme::Font::Design::serif,
                          theme::Font::Weight::bold)
          .small_caps()
          .monospaced()
          .italic(true);
  const theme::Font::Resolved resolved = styled.resolve(context);
  EXPECT_TRUE(resolved.is_bold);
  EXPECT_TRUE(resolved.is_italic);
  EXPECT_TRUE(resolved.is_monospaced);
  EXPECT_TRUE(resolved.is_small_caps);
  EXPECT_TRUE(resolved.is_lowercase_small_caps);
  EXPECT_TRUE(resolved.is_uppercase_small_caps);
  EXPECT_EQ(resolved.weight, theme::Font::Weight::bold);
  EXPECT_EQ(resolved.face.typeface, text::Typeface::monospaced);
  EXPECT_EQ(resolved.leading, theme::Font::Leading::standard);
  const theme::Font loose =
      theme::Font::body.leading(theme::Font::Leading::loose).point_size(30.0F);
  EXPECT_FLOAT_EQ(loose.resolve(context).point_size, 30.0F);
  EXPECT_EQ(loose.resolve(context).leading, theme::Font::Leading::loose);
  EXPECT_FALSE(theme::Font::body.lowercase_small_caps(false)
                   .resolve(context)
                   .is_lowercase_small_caps);
  EXPECT_TRUE(theme::Font::body.uppercase_small_caps()
                  .resolve(context)
                  .is_uppercase_small_caps);
  EXPECT_FALSE(theme::Font::body.bold(true).bold(false).weight().has_value());
  EXPECT_FALSE(theme::Font::body.monospaced(true)
                   .monospaced(false)
                   .design()
                   .has_value());
  EXPECT_TRUE(theme::Font::body.monospaced_digit().is_monospaced_digit());
  const theme::Theme large = theme.sized(theme::DynamicTypeSize::xxx_large);
  const theme::Font fixed =
      theme::Font::custom("Avenir", theme::Font::FixedSize{20.0F});
  EXPECT_FLOAT_EQ(fixed.resolve(large.font_context()).point_size, 20.0F);
  EXPECT_GT(theme::Font::custom("Avenir", 20.0F)
                .resolve(large.font_context())
                .point_size,
            20.0F);
  const theme::Font platform{text::FontName{"Avenir"}, 18.0F};
  EXPECT_EQ(platform.name(), std::optional{text::FontName{"Avenir"}});
  EXPECT_FLOAT_EQ(platform.resolve(large.font_context()).point_size, 18.0F);
  EXPECT_FLOAT_EQ(theme::Font::Width{0.1F}.value(), 0.1F);
  EXPECT_FLOAT_EQ(theme::Font::Width::condensed.value(), -0.2F);
  EXPECT_FLOAT_EQ(theme::Font::Width::condensed.stretch(),
                  theme::Font::condensed_width);
  EXPECT_GT(theme::Font::Width{0.1F}.stretch(), 1.0F);
  EXPECT_NE(theme::Font::Leading::tight, theme::Font::Leading::loose);
  EXPECT_EQ(
      theme::Font::body.width(theme::Font::Width{0.1F}).resolve(context).width,
      theme::Font::Width{0.1F});
}

TEST_F(TextParityTest, FontLeadingAndCapitalsReachTheTextStyle) {
  const text::TextStyle tight = Text::text_style(
      theme, theme::Font::body.leading(theme::Font::Leading::tight),
      text::Locale::undetermined());
  EXPECT_FLOAT_EQ(tight.line_spacing, -theme::Font::leading_step);
  const text::TextStyle sized = Text::text_style(
      theme, theme::Font::system(20.0F).leading(theme::Font::Leading::loose),
      text::Locale::undetermined());
  EXPECT_FLOAT_EQ(sized.line_spacing, 0.0F);
  EXPECT_EQ(Text::text_style(theme, theme::Font::body.lowercase_small_caps(),
                             text::Locale::undetermined())
                .small_caps,
            text::SmallCaps::lowercase);
  EXPECT_TRUE(Text::text_style(theme, theme::Font::body.monospaced_digit(),
                               text::Locale::undetermined())
                  .tabular_digits);
}

TEST_F(TextParityTest, ViewAttributesReachTheTextStyle) {
  composition::TextAttributes attributes;
  attributes.line_height = text::LineHeight::multiple(1.5F);
  attributes.typesetting_language = text::Locale::create("ja");
  attributes.writing_direction =
      composition::WritingDirectionStrategy::layout_based;
  attributes.alignment_strategy =
      composition::TextAlignmentStrategy::writing_direction_based;
  attributes.multiline_alignment = composition::TextAlignment::trailing;
  attributes.underline = composition::TextDecoration{
      .active = true, .pattern = text::LinePattern::dash};
  const text::TextStyle style = Text::text_style(
      theme, theme::Font::body, text::Locale::undetermined(), std::nullopt,
      attributes, composition::LayoutDirection::right_to_left);
  EXPECT_EQ(style.line_rule, std::optional{text::LineHeight::multiple(1.5F)});
  EXPECT_EQ(style.language, text::Locale::create("ja"));
  EXPECT_EQ(style.direction, text::WritingDirection::right_to_left);
  EXPECT_EQ(style.alignment, text::TextAlignment::opposite);
  EXPECT_EQ(style.underline_pattern, text::LinePattern::dash);
  attributes.alignment_strategy =
      composition::TextAlignmentStrategy::layout_based;
  attributes.multiline_alignment = composition::TextAlignment::leading;
  EXPECT_EQ(
      Text::text_style(theme, theme::Font::body, text::Locale::undetermined(),
                       std::nullopt, attributes,
                       composition::LayoutDirection::right_to_left)
          .alignment,
      text::TextAlignment::right);
  EXPECT_EQ(composition::text_alignment_cases.size(), 3U);
  EXPECT_NE(composition::TextAlignmentStrategy::layout_based,
            composition::TextAlignmentStrategy::default_);
  const composition::TextDecoration ns =
      composition::TextDecoration::ns_underline_style(0x0201U);
  EXPECT_TRUE(ns.active);
  EXPECT_EQ(ns.pattern, text::LinePattern::dash);
  EXPECT_EQ(composition::TextDecoration{}, composition::TextDecoration{});
  const Text language = Text("x").typesetting_language(
      text::TypesettingLanguage::explicit_(*text::Locale::create("ja")));
  EXPECT_EQ(content(language)
                .attributes_in(composition::TextAttributes{})
                .typesetting_language,
            text::Locale::create("ja"));
  const Text varied = Text("x").text_variant(
      composition::TextVariantPreference::size_dependent);
  EXPECT_EQ(
      content(varied).attributes_in(composition::TextAttributes{}).variant,
      composition::TextVariantPreference::size_dependent);
  const Text typed = Text("x").accessibility_text_content_type(
      composition::AccessibilityTextContentType::source_code);
  EXPECT_FALSE(typed == Text("x"));
}

TEST_F(TextParityTest, SizeDependentDatesOfferShorterVariants) {
  const Text dated{std::chrono::sys_seconds{std::chrono::seconds{1778307200}},
                   Text::DateStyle::date};
  composition::TextAttributes attributes;
  attributes.variant = composition::TextVariantPreference::size_dependent;
  const std::vector<std::string> variants =
      content(dated).variants(attributes, *text::Locale::create("en"));
  ASSERT_GE(variants.size(), 3U);
  EXPECT_GT(variants.front().size(), variants.back().size());
  EXPECT_TRUE(
      content(dated)
          .variants(composition::TextAttributes{}, *text::Locale::create("en"))
          .empty());
  const std::optional<std::size_t> picked =
      Text::variant_for(variants, 60.0F, [](const std::string& variant) {
        return static_cast<float>(variant.size()) * 7.0F;
      });
  ASSERT_TRUE(picked.has_value());
  EXPECT_LE(variants[*picked].size() * 7U, 60U);
}

TEST_F(TextParityTest, ContentSizeCategoriesMirrorDynamicType) {
  EXPECT_EQ(theme::ContentSizeCategories::all_cases.size(), 12U);
  EXPECT_LT(theme::ContentSizeCategory::small,
            theme::ContentSizeCategory::large);
  EXPECT_EQ(theme::ContentSizeCategories::size(
                theme::ContentSizeCategory::accessibility_medium),
            theme::DynamicTypeSize::accessibility1);
  EXPECT_EQ(
      theme::ContentSizeCategories::category(theme::DynamicTypeSize::x_large),
      theme::ContentSizeCategory::extra_large);
  EXPECT_TRUE(theme::ContentSizeCategories::is_accessibility_category(
      theme::ContentSizeCategory::accessibility_large));
  EXPECT_FALSE(theme::ContentSizeCategories::is_accessibility_category(
      theme::ContentSizeCategory::extra_extra_extra_large));
}

TEST_F(TextParityTest, ResolvedColoursExposeLinearComponents) {
  const theme::Colour mid{.red = 0.5F, .green = 0.0F, .blue = 1.0F};
  EXPECT_NEAR(mid.linear_red(), 0.2140F, 0.0005F);
  EXPECT_FLOAT_EQ(mid.linear_green(), 0.0F);
  EXPECT_FLOAT_EQ(mid.linear_blue(), 1.0F);
  EXPECT_NEAR(theme::Colour::linearised(0.02F), 0.02F / 12.92F, 1e-6F);
}

TEST_F(TextParityTest, NumericTextTransitionsCarryTheirValue) {
  const motion::ContentTransition counted =
      motion::ContentTransition::numeric_text_value(3.0);
  EXPECT_EQ(counted.kind(), motion::ContentTransition::Kind::numeric_text);
  EXPECT_EQ(counted.value(), std::optional<double>{3.0});
  EXPECT_FALSE(motion::ContentTransition::numeric_text().value().has_value());
}

}
