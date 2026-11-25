#include <arisa/components/attributed_string.hpp>
#include <arisa/components/attributed_text_formatting.hpp>
#include <arisa/components/attributed_text_selection.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/graphics_context.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/text_glyphs.hpp>
#include <arisa/components/text_layout.hpp>
#include <arisa/components/text_layout_key.hpp>
#include <arisa/components/text_proxy.hpp>
#include <arisa/components/text_renderer.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/find_context.hpp>
#include <arisa/composition/pasteboard.hpp>
#include <arisa/composition/preference_observer.hpp>
#include <arisa/composition/search_environment.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/text_environment.hpp>
#include <arisa/composition/text_input_environment.hpp>
#include <arisa/composition/text_input_formatting_control_placement.hpp>
#include <arisa/composition/text_selectability.hpp>
#include <arisa/composition/text_selection.hpp>
#include <arisa/composition/text_selection_affinity.hpp>
#include <arisa/composition/toolbar_item.hpp>
#include <arisa/composition/transfer_item.hpp>
#include <arisa/controls/find_navigator.hpp>
#include <arisa/controls/searchable.hpp>
#include <arisa/controls/text_editing.hpp>
#include <arisa/controls/text_editing_session.hpp>
#include <arisa/controls/text_editor.hpp>
#include <arisa/controls/text_field.hpp>
#include <arisa/controls/text_field_style.hpp>
#include <arisa/controls/text_input_border_shape.hpp>
#include <arisa/controls/text_input_suggestions.hpp>
#include <arisa/controls/text_selection_host.hpp>
#include <arisa/controls/text_value_format.hpp>
#include <arisa/presentation/navigation_stack.hpp>
#include <arisa/provider/assets/asset_path.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/text/byte_count_format_style.hpp>
#include <arisa/text/date_format_style.hpp>
#include <arisa/text/locale.hpp>
#include <arisa/text/measurement.hpp>
#include <arisa/text/number_format_style.hpp>
#include <arisa/text/utf8.hpp>

#include <focus_fixture.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace arisa::runtime {

using composition::Element;
using composition::TextSelection;

class TextEditingParityTest : public FocusFixture {
 protected:
  using Build = std::function<Element(composition::Context&)>;

  struct Probe {
    std::optional<composition::StateHandle<std::string>> text;
    std::optional<composition::StateHandle<std::optional<TextSelection>>>
        selection;
    std::optional<composition::StateHandle<components::AttributedString>> rich;
    std::optional<composition::StateHandle<components::AttributedTextSelection>>
        rich_selection;
    std::optional<composition::StateHandle<bool>> flag;
    std::optional<composition::StateHandle<double>> number;
    std::optional<composition::StateHandle<std::int64_t>> bytes;
    components::Text::LayoutKey::Value layouts;
    std::optional<composition::FindContext> find;
    composition::TextSelectionAffinity affinity{
        composition::TextSelectionAffinity::automatic};
    bool selectable{true};
  };

  static auto probe() -> Probe& {
    static Probe value;
    return value;
  }

  static constexpr composition::StateKey<std::string> text_key{"parity-text",
                                                               ""};
  static inline const composition::StateKey<std::optional<TextSelection>>
      selection_key{"parity-selection", std::nullopt};
  static inline const composition::StateKey<components::AttributedString>
      rich_key{"parity-rich", components::AttributedString{"bold me"}};
  static inline const composition::StateKey<components::AttributedTextSelection>
      rich_selection_key{"parity-rich-selection",
                         components::AttributedTextSelection{}};
  static constexpr composition::StateKey<bool> flag_key{"parity-flag", false};
  static constexpr composition::StateKey<double> number_key{"parity-number",
                                                            12.5};
  static constexpr composition::StateKey<std::int64_t> bytes_key{"parity-bytes",
                                                                 2048};

  class Host final : public composition::Component {
   public:
    explicit Host(std::shared_ptr<const Build> build)
        : build_(std::move(build)) {}

    auto operator==(const Host& other) const -> bool {
      return build_ == other.build_;
    }

    auto body(composition::Context& context) const -> Element override {
      probe().text = context.state(text_key);
      probe().selection = context.state(selection_key);
      probe().rich = context.state(rich_key);
      probe().rich_selection = context.state(rich_selection_key);
      probe().flag = context.state(flag_key);
      probe().number = context.state(number_key);
      probe().bytes = context.state(bytes_key);
      return (*build_)(context);
    }

   private:
    std::shared_ptr<const Build> build_;
  };

  class Reader final : public composition::Component {
   public:
    auto operator==(const Reader&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      probe().find =
          context.environment(composition::TextInputEnvironment::find_context);
      probe().affinity =
          context.environment(composition::TextEnvironment::selection_affinity);
      probe().selectable =
          context.environment(composition::TextEnvironment::selectable);
      return components::Text("read").id("reader");
    }
  };

  static auto host(Build build) -> Element {
    return Host{std::make_shared<const Build>(std::move(build))};
  }

  static void fonts(provider::headless::HeadlessProviderFactory& providers) {
    for (const std::string_view file :
         {std::string_view{"arisa/fonts/InterVariable.ttf"},
          std::string_view{"arisa/fonts/InterItalic-Regular.ttf"}}) {
      std::ifstream stream{std::filesystem::path{ARISA_SOURCE_ASSETS} / file,
                           std::ios::binary};
      const std::vector<char> raw{std::istreambuf_iterator<char>{stream},
                                  std::istreambuf_iterator<char>{}};
      std::vector<std::byte> bytes(raw.size());
      std::ranges::transform(raw, bytes.begin(),
                             [](char value) { return std::byte(value); });
      providers.assets().add(*provider::AssetPath::create(file), bytes);
    }
  }

  static auto launch(provider::headless::HeadlessProviderFactory& providers,
                     Build build) -> Application {
    fonts(providers);
    probe() = Probe{};
    return start(providers, host(std::move(build)));
  }

  static void key(provider::headless::HeadlessProviderFactory& providers,
                  Application& application, provider::Key which, bool down) {
    provider::InputFrame frame;
    frame.keys.push_back(provider::KeyEvent{.key = which, .down = down});
    providers.input().push(frame);
    frames(application, 2);
  }

  static void type(provider::headless::HeadlessProviderFactory& providers,
                   Application& application, provider::Key which) {
    key(providers, application, which, true);
    key(providers, application, which, false);
  }

  static void chord(provider::headless::HeadlessProviderFactory& providers,
                    Application& application, provider::Key held,
                    provider::Key which) {
    key(providers, application, held, true);
    type(providers, application, which);
    key(providers, application, held, false);
  }

  static auto observing(Element content) -> Element {
    return std::move(content).on_preference_change(
        components::Text::LayoutKey::key,
        [](const components::Text::LayoutKey::Value& value) {
          probe().layouts = value;
        });
  }

  struct Shout {
    int volume{0};

    friend auto operator==(const Shout&, const Shout&) -> bool = default;
  };

  class Counting final : public components::TextRenderer {
   public:
    explicit Counting(std::shared_ptr<int> drawn) : drawn_(std::move(drawn)) {}

    void draw(const components::Text::Layout& layout,
              components::GraphicsContext& context) const override {
      for (const components::Text::Layout::Line line : layout) {
        for (const components::Text::Layout::Run run : line) {
          for (const components::Text::Layout::RunSlice slice : run) {
            context.translate_by(0.0F, 1.0F);
            context.draw(slice);
            ++*drawn_;
          }
        }
      }
    }

    auto display_padding() const -> theme::EdgeInsets override {
      return theme::EdgeInsets::all(theme::Space{4.0F});
    }

   private:
    std::shared_ptr<int> drawn_;
  };
};

TEST_F(TextEditingParityTest, TextSelectionModelsInsertionsRangesAndAffinity) {
  const TextSelection caret{TextSelection::InsertionPoint{.index = 3}};
  EXPECT_TRUE(caret.is_insertion());
  EXPECT_EQ(caret.range(), (TextSelection::Range{.lower = 3, .upper = 3}));
  EXPECT_EQ(caret.affinity(), composition::TextSelectionAffinity::automatic);
  const TextSelection range{TextSelection::Range{.lower = 1, .upper = 4}};
  EXPECT_FALSE(range.is_insertion());
  EXPECT_FALSE(range.indices().is_multi_selection());
  const TextSelection many{std::vector<TextSelection::Range>{
      {.lower = 6, .upper = 8}, {.lower = 0, .upper = 2}}};
  EXPECT_TRUE(many.indices().is_multi_selection());
  EXPECT_EQ(many.indices().ranges().front().lower, 0U);
  EXPECT_EQ(TextSelection::Indices::selection({.lower = 1, .upper = 4}),
            range.indices());
  EXPECT_NE(caret, range);
  EXPECT_EQ(range.with_affinity(composition::TextSelectionAffinity::upstream)
                .affinity(),
            composition::TextSelectionAffinity::upstream);
  EXPECT_NE(composition::TextSelectionAffinity::upstream,
            composition::TextSelectionAffinity::downstream);
}

TEST_F(TextEditingParityTest, EditingModelInsertsErasesMovesAndSelectsWords) {
  controls::TextEditing editing{
      "hello world", TextSelection{TextSelection::InsertionPoint{.index = 5}},
      false};
  editing.insert(",");
  EXPECT_EQ(editing.text(), "hello, world");
  editing.move(controls::TextEditing::Direction::forward,
               controls::TextEditing::Motion::word, true);
  EXPECT_EQ(editing.selected(), " world");
  editing.erase(controls::TextEditing::Direction::backward,
                controls::TextEditing::Motion::character);
  EXPECT_EQ(editing.text(), "hello,");
  editing.place(2);
  editing.select_word();
  EXPECT_EQ(editing.selected(), "hello");
  editing.select_all();
  EXPECT_EQ(editing.range().length(), editing.text().size());
  editing.insert("a\nb");
  EXPECT_EQ(editing.text(), "ab");
  EXPECT_EQ(controls::TextEditing::word_at("one two", 5),
            (TextSelection::Range{.lower = 4, .upper = 7}));
  EXPECT_EQ(text::Utf8::next("é", 0), 2U);
  EXPECT_EQ(text::Utf8::boundary("é", 1), 0U);
}

TEST_F(TextEditingParityTest, KeyboardTypingEditsAFocusedFieldInline) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = launch(providers, [](composition::Context&) {
    return controls::TextField(
               "Name", composition::Binding<std::string>{*probe().text},
               composition::Binding<std::optional<TextSelection>>{
                   *probe().selection})
        .id("field");
  });
  type(providers, application, provider::Key::h);
  type(providers, application, provider::Key::i);
  EXPECT_EQ(probe().text->get(), "hi");
  ASSERT_TRUE(probe().selection->get().has_value());
  EXPECT_EQ(probe().selection->get()->range().lower, 2U);
  EXPECT_TRUE(node_of(application, "text-caret").has_value());
  key(providers, application, provider::Key::left_shift, true);
  type(providers, application, provider::Key::left);
  key(providers, application, provider::Key::left_shift, false);
  EXPECT_EQ(probe().selection->get()->range(),
            (TextSelection::Range{.lower = 1, .upper = 2}));
  type(providers, application, provider::Key::backspace);
  EXPECT_EQ(probe().text->get(), "h");
  type(providers, application, provider::Key::escape);
  EXPECT_FALSE(node_of(application, "text-caret").has_value());
}

TEST_F(TextEditingParityTest, LabelledFieldAndEditorTakeSelectionBindings) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = launch(providers, [](composition::Context&) {
    return controls::TextEditor(
               composition::Binding<std::string>{*probe().text},
               composition::Binding<std::optional<TextSelection>>{
                   *probe().selection})
        .id("editor");
  });
  type(providers, application, provider::Key::a);
  type(providers, application, provider::Key::enter);
  type(providers, application, provider::Key::b);
  EXPECT_EQ(probe().text->get(), "a\nb");
  key(providers, application, provider::Key::left_ctrl, true);
  type(providers, application, provider::Key::a);
  key(providers, application, provider::Key::left_ctrl, false);
  EXPECT_EQ(probe().selection->get()->range().length(), 3U);
}

TEST_F(TextEditingParityTest, LabelledFieldTakesASelectionBinding) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = launch(providers, [](composition::Context&) {
    return controls::TextField(
               composition::Binding<std::string>{*probe().text},
               composition::Binding<std::optional<TextSelection>>{
                   *probe().selection},
               controls::TextFieldOptions{}, components::Text("Label"))
        .id("labelled");
  });
  type(providers, application, provider::Key::z);
  EXPECT_EQ(probe().text->get(), "z");
  EXPECT_EQ(probe().selection->get()->range().lower, 1U);
}

TEST_F(TextEditingParityTest, ControllerSteersTheCaretAndTheEditMenuSelects) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = launch(providers, [](composition::Context&) {
    return controls::TextField(
               "Name", composition::Binding<std::string>{*probe().text},
               composition::Binding<std::optional<TextSelection>>{
                   *probe().selection})
        .id("field");
  });
  probe().text->set("one two");
  frames(application, 2);
  press(providers, application, provider::Button::plus);
  ASSERT_TRUE(node_of(application, "edit-menu").has_value());
  const auto session = node_named(application, "field")
                           ->get()
                           .content_as<controls::TextField::Content>()
                           ->get()
                           .session();
  session->perform(controls::TextEditingSession::Action::select_all);
  frames(application, 2);
  EXPECT_EQ(probe().selection->get()->range().length(), 7U);
  press(providers, application, provider::Button::dpad_left);
  EXPECT_EQ(probe().selection->get()->range().length(), 6U);
  session->perform(controls::TextEditingSession::Action::deselect);
  frames(application, 2);
  press(providers, application, provider::Button::dpad_left);
  EXPECT_TRUE(probe().selection->get()->is_insertion());
  EXPECT_EQ(probe().selection->get()->range().lower, 5U);
}

TEST_F(TextEditingParityTest, CopyCutAndPasteGoThroughTheGeneralPasteboard) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = launch(providers, [](composition::Context&) {
    return controls::TextField(
               "Name", composition::Binding<std::string>{*probe().text},
               composition::Binding<std::optional<TextSelection>>{
                   *probe().selection})
        .id("field");
  });
  probe().text->set("copy");
  probe().selection->set(TextSelection{TextSelection::Range{0, 4}});
  frames(application, 2);
  chord(providers, application, provider::Key::left_ctrl, provider::Key::x);
  EXPECT_EQ(probe().text->get(), "");
  chord(providers, application, provider::Key::left_ctrl, provider::Key::v);
  chord(providers, application, provider::Key::left_ctrl, provider::Key::v);
  EXPECT_EQ(probe().text->get(), "copycopy");
}

TEST_F(TextEditingParityTest, SelectableTextCopiesWhatTheUserSelects) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = launch(providers, [](composition::Context&) {
    return components::VStack(
        components::Text("Select me")
            .text_selection(composition::TextSelectability::enabled)
            .id("selectable"),
        Element{Reader{}}.text_selection(
            composition::TextSelectability::disabled));
  });
  EXPECT_FALSE(probe().selectable);
  chord(providers, application, provider::Key::left_ctrl, provider::Key::a);
  chord(providers, application, provider::Key::left_ctrl, provider::Key::c);
  const composition::Pasteboard board{providers.clipboard()};
  bool found = false;
  for (const composition::TransferItem& item : board.items()) {
    found = found || item.as<std::string>() == std::string{"Select me"};
  }
  EXPECT_TRUE(found);
  EXPECT_TRUE(composition::EnabledTextSelectability::allows_selection);
  EXPECT_FALSE(composition::DisabledTextSelectability::allows_selection);
  EXPECT_EQ(composition::TextSelectability::enabled,
            composition::EnabledTextSelectability{});
  press(providers, application, provider::Button::plus);
  EXPECT_TRUE(node_of(application, "selection-menu").has_value());
}

TEST_F(TextEditingParityTest, AffinityEnvironmentReachesDescendants) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = launch(providers, [](composition::Context&) {
    return Element{Reader{}}.text_selection_affinity(
        composition::TextSelectionAffinity::upstream);
  });
  EXPECT_EQ(probe().affinity, composition::TextSelectionAffinity::upstream);
  static_cast<void>(application);
}

TEST_F(TextEditingParityTest, TextPublishesItsLayoutThroughTheLayoutKey) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = launch(providers, [](composition::Context&) {
    return observing(components::VStack(
        components::Text("Two words").custom_attribute(Shout{.volume = 3}),
        components::Text("A long line that is cut short")
            .line_limit(1)
            .frame({.width = 60.0F})));
  });
  frames(application, 3);
  ASSERT_EQ(probe().layouts.size(), 2U);
  const components::Text::Layout& layout = probe().layouts[0].layout;
  EXPECT_FALSE(layout.is_truncated());
  EXPECT_TRUE(probe().layouts[1].layout.is_truncated());
  ASSERT_EQ(layout.size(), 1U);
  EXPECT_EQ(layout.start_index(), 0U);
  const components::Text::Layout::Line line = layout[0];
  EXPECT_GT(line.typographic_bounds().width, 0.0F);
  EXPECT_GT(line.typographic_bounds().ascent, 0.0F);
  EXPECT_GE(line.typographic_bounds().descent, 0.0F);
  EXPECT_GE(line.typographic_bounds().leading, 0.0F);
  EXPECT_GT(line.origin().y, 0.0F);
  EXPECT_EQ(line, layout[0]);
  ASSERT_GT(line.size(), 0U);
  const components::Text::Layout::Run run = line[0];
  EXPECT_EQ(run, line[0]);
  EXPECT_EQ(run.layout_direction(),
            composition::LayoutDirection::left_to_right);
  EXPECT_EQ(run.character_indices().size(), run.size());
  EXPECT_GT(run.typographic_bounds().rect().height, 0.0F);
  EXPECT_EQ(run.get<Shout>(), (Shout{.volume = 3}));
  const components::Text::Layout::RunSlice slice{
      run, components::Text::Layout::IndexRange{.lower = 1, .upper = 3}};
  EXPECT_EQ(slice.run(), run);
  EXPECT_EQ(slice.indices().upper, 3U);
  EXPECT_EQ(slice.size(), 2U);
  EXPECT_EQ(slice.character_indices().size(), 2U);
  EXPECT_LT(slice.typographic_bounds().width, run.typographic_bounds().width);
  EXPECT_EQ(slice.get<Shout>(), (Shout{.volume = 3}));
  EXPECT_EQ(slice, run.slice({.lower = 1, .upper = 3}));
  const components::Text::Layout::CharacterIndex index{2};
  EXPECT_EQ(index.advanced(3).value(), 5U);
  EXPECT_EQ(index.distance(components::Text::Layout::CharacterIndex{5}), 3);
  EXPECT_LT(index, index.advanced(1));
  EXPECT_EQ(components::Text::Layout::TypographicBounds{}.rect().width, 0.0F);
  EXPECT_EQ(probe().layouts[0], probe().layouts[0]);
  EXPECT_EQ(components::Text::LayoutKey::default_value().size(), 0U);
  components::Text::LayoutKey::Value merged = probe().layouts;
  components::Text::LayoutKey::reduce(merged, probe().layouts);
  EXPECT_EQ(merged.size(), 4U);
  EXPECT_GT(layout.caret(layout.character_at({.x = 1000.0F, .y = 1.0F})).x,
            0.0F);
}

TEST_F(TextEditingParityTest, ACustomRendererDrawsEverySliceThroughGraphics) {
  provider::headless::HeadlessProviderFactory providers;
  static const std::shared_ptr<int> drawn = std::make_shared<int>(0);
  *drawn = 0;
  Application application = launch(providers, [](composition::Context&) {
    return components::Text("Hi").text_renderer(Counting{drawn}).id("shown");
  });
  frames(application, 3);
  EXPECT_GE(*drawn, 2);
  const Counting renderer{drawn};
  EXPECT_EQ(renderer.display_padding().top, theme::Space{4.0F});
  const components::TextProxy proxy{components::Text::Layout{}};
  EXPECT_EQ(
      renderer.size_that_fits(composition::ProposedViewSize{}, proxy).width,
      0.0F);
}

TEST_F(TextEditingParityTest,
       RichEditingFormatsTheSelectionAndTypingAttributes) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = launch(providers, [](composition::Context&) {
    return controls::TextEditor(
               composition::Binding<components::AttributedString>{
                   *probe().rich},
               composition::Binding<components::AttributedTextSelection>{
                   *probe().rich_selection})
        .id("rich");
  });
  probe().rich_selection->set(components::AttributedTextSelection{
      components::AttributedTextSelection::Range{.lower = 0, .upper = 4}});
  frames(application, 2);
  chord(providers, application, provider::Key::left_ctrl, provider::Key::b);
  const components::AttributedString styled = probe().rich->get();
  EXPECT_EQ(styled.attributes(0).weight, theme::Font::Weight::bold);
  EXPECT_FALSE(styled.attributes(5).weight.has_value());
  probe().rich_selection->set(components::AttributedTextSelection{
      components::AttributedTextSelection::InsertionPoint{.index = 4}});
  frames(application, 2);
  type(providers, application, provider::Key::x);
  EXPECT_EQ(probe().rich->get().text(), "boldx me");
  EXPECT_EQ(probe().rich->get().attributes(4).weight,
            theme::Font::Weight::bold);
}

TEST_F(TextEditingParityTest, AttributedSelectionsAndReplacementsFollowEdits) {
  using components::AttributedString;
  using components::AttributedTextSelection;
  AttributedString text{"hello world"};
  AttributedTextSelection selection{
      AttributedTextSelection::Range{.lower = 0, .upper = 5}};
  EXPECT_FALSE(selection.indices(text).is_insertion_point());
  EXPECT_EQ(selection.affinity(text),
            composition::TextSelectionAffinity::upstream);
  text.transform_attributes(selection, [](AttributedString::Attributes& held) {
    held.italic = true;
  });
  EXPECT_TRUE(text.attributes(0).italic);
  EXPECT_EQ(selection.attributes(text).size(), 1U);
  EXPECT_EQ(selection.attributes(text)
                .values<components::AttributedTextFormatting::Italic>()
                .front(),
            true);
  text.replace_selection(selection, "howdy");
  EXPECT_EQ(text.text(), "howdy world");
  EXPECT_TRUE(text.attributes(0).italic);
  EXPECT_TRUE(selection.indices(text).is_insertion_point());
  EXPECT_EQ(selection.indices(text).insertion(), 5U);
  text.replace_selection(selection, AttributedString{"!"});
  EXPECT_EQ(text.text(), "howdy! world");
  AttributedTextSelection tail{
      AttributedTextSelection::Range{.lower = 7, .upper = 12}};
  text.transform(tail, [](AttributedString& value) {
    value = value.replacing(0, 0, AttributedString{">> "});
  });
  EXPECT_EQ(tail.indices(text).range(),
            (AttributedTextSelection::Range{.lower = 10, .upper = 15}));
  EXPECT_EQ(AttributedTextSelection{}.indices(text).insertion(), 0U);
  const AttributedTextSelection typing{
      AttributedTextSelection::InsertionPoint{.index = 1},
      AttributedString::Attributes{.underline = true}};
  EXPECT_TRUE(typing.typing_attributes(text).underline);
  const AttributedTextSelection touching{
      std::vector<AttributedTextSelection::Range>{{.lower = 0, .upper = 1},
                                                  {.lower = 1, .upper = 3}}};
  EXPECT_EQ(touching.indices(text).ranges().size(), 1U);
  EXPECT_EQ(AttributedTextSelection::Indices::insertion_point(2),
            AttributedTextSelection::Indices::insertion_point(2));
  EXPECT_EQ(selection, selection);
}

TEST_F(TextEditingParityTest, FormattingDefinitionsConstrainAttributes) {
  using components::AttributedString;
  using components::AttributedTextFormatting;
  const AttributedTextFormatting::ValueConstraint<
      AttributedTextFormatting::Weight>
      weights{{std::nullopt, theme::Font::Weight::bold}, std::nullopt};
  EXPECT_EQ(weights, weights);
  const auto definition =
      AttributedTextFormatting::DefinitionBuilder::build_block(
          weights, AttributedTextFormatting::EmptyDefinition{});
  const AttributedTextFormatting::AnyDefinition erased{definition};
  AttributedString text{"heavy"};
  text = text.setting<
      AttributedString::Field<std::optional<theme::Font::Weight>,
                              &AttributedString::Attributes::weight>>(
      theme::Font::Weight::black, 0, 5);
  const AttributedString constrained =
      AttributedTextFormatting::constrained(text, erased);
  EXPECT_FALSE(constrained.attributes(0).weight.has_value());
  AttributedString::Attributes held{.weight = theme::Font::Weight::bold};
  AttributedTextFormatting::AttributeContainerProxy proxy{held};
  erased.constrain(proxy);
  EXPECT_EQ(proxy.value<AttributedTextFormatting::Weight>(),
            theme::Font::Weight::bold);
  const AttributedTextFormatting::TupleDefinition<
      AttributedTextFormatting::EmptyDefinition>
      tuple{AttributedTextFormatting::EmptyDefinition{}};
  tuple.constrain(proxy);
  static_cast<void>(AttributedTextFormatting::DefinitionBuilder::build_block());
  const AttributedTextFormatting::Transferable transfer{
      AttributedString::markdown("**bold** plain"),
      std::make_shared<const AttributedTextFormatting::AnyDefinition>(erased)};
  const std::optional<AttributedTextFormatting::Transferable> decoded =
      AttributedTextFormatting::Transferable::decoded(transfer.encoded());
  ASSERT_TRUE(decoded.has_value());
  const AttributedString restored{*decoded};
  EXPECT_EQ(restored.text(), "bold plain");
  EXPECT_EQ(AttributedTextFormatting::Transferable::transfer_representation(),
            AttributedTextFormatting::Transferable::content_type());
}

TEST_F(TextEditingParityTest, FormattingControlsFollowTheirVisibility) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = launch(providers, [](composition::Context&) {
    return controls::TextEditor(
               composition::Binding<components::AttributedString>{
                   *probe().rich})
        .text_input_formatting_control_visibility(
            probe().flag->get() ? composition::Visibility::hidden
                                : composition::Visibility::visible,
            composition::TextInputFormattingControlPlacement::Set::all)
        .attributed_text_formatting_definition(
            components::AttributedTextFormatting::EmptyDefinition{})
        .id("rich");
  });
  type(providers, application, provider::Key::a);
  EXPECT_TRUE(node_of(application, "formatting-bar").has_value());
  probe().flag->set(true);
  frames(application, 3);
  EXPECT_FALSE(node_of(application, "formatting-bar").has_value());
  using Set = composition::TextInputFormattingControlPlacement::Set;
  EXPECT_TRUE(Set::all.contains(Set::accessory_bar));
  EXPECT_TRUE(Set::default_.contains(Set::context_menu));
  EXPECT_TRUE((Set::context_menu | Set::input_assistant)
                  .contains(Set::input_assistant));
}

TEST_F(TextEditingParityTest, BorderedAndSquareStylesUseTheBorderShape) {
  const controls::TextInputBorderShape capsule =
      controls::TextInputBorderShape::capsule();
  EXPECT_EQ(capsule.shape(), theme::Shape::capsule());
  EXPECT_NE(capsule, controls::TextInputBorderShape::rounded_rectangle());
  EXPECT_EQ(controls::TextInputBorderShape::automatic().kind(),
            controls::TextInputBorderShape::Kind::automatic);
  EXPECT_FLOAT_EQ(capsule.inset(2.0F).inset_amount(), 2.0F);
  EXPECT_FALSE(
      capsule.path({.x = 0, .y = 0, .width = 40, .height = 20}).is_empty());
  EXPECT_NE(controls::TextFieldStyle::bordered(),
            controls::TextFieldStyle::square_border());
  EXPECT_EQ(controls::TextFieldStyle{controls::BorderedTextFieldStyle{}},
            controls::TextFieldStyle::bordered());
  EXPECT_EQ(controls::TextFieldStyle{controls::SquareBorderTextFieldStyle{}},
            controls::TextFieldStyle::square_border());
  provider::headless::HeadlessProviderFactory providers;
  Application application = launch(providers, [](composition::Context&) {
    return components::VStack(
        controls::TextField("A",
                            composition::Binding<std::string>{*probe().text})
            .text_field_style(controls::BorderedTextFieldStyle{})
            .text_input_border_shape(controls::TextInputBorderShape::capsule())
            .id("bordered"),
        controls::TextField("B",
                            composition::Binding<std::string>{*probe().text})
            .text_field_style(controls::TextFieldStyle::square_border())
            .id("square"));
  });
  EXPECT_FLOAT_EQ(node_named(application, "bordered")->get().frame().height,
                  node_named(application, "square")->get().frame().height);
}

TEST_F(TextEditingParityTest, ValueFieldsFormatAndParseEveryFormatStyle) {
  const text::Locale english = *text::Locale::create("en-US");
  const text::ByteCountFormatStyle bytes{};
  EXPECT_EQ(bytes.parse(bytes.format(4000, english), english), 4000);
  const text::MeasurementFormatStyle measured{};
  const std::optional<text::Measurement> length = measured.parse(
      measured.format(text::Measurement{3.0, text::Measurement::Unit::meters},
                      english),
      english);
  ASSERT_TRUE(length.has_value());
  EXPECT_EQ(length->unit(), text::Measurement::Unit::meters);
  const text::DateFormatStyle dated{text::DateFormatStyle::DateStyle::numeric,
                                    text::DateFormatStyle::TimeStyle::omitted};
  const std::chrono::sys_seconds day{std::chrono::sys_days{
      std::chrono::year{2026} / std::chrono::November / 17}};
  EXPECT_EQ(dated.parse(dated.format(day, english), english), day);
  const text::NumberFormatStyle currency =
      text::NumberFormatStyle::currency("USD");
  EXPECT_DOUBLE_EQ(
      *controls::TextValueFormat::read<double>(
          currency,
          controls::TextValueFormat::describe<double>(currency, 12.5, english),
          english),
      12.5);
  provider::headless::HeadlessProviderFactory providers;
  Application application = launch(providers, [](composition::Context&) {
    return components::VStack(
        controls::TextField("Size", *probe().bytes,
                            text::ByteCountFormatStyle{})
            .id("bytes"),
        controls::TextField(*probe().number, text::NumberFormatStyle::number(),
                            controls::TextFieldOptions{},
                            components::Text("Amount"))
            .id("amount"),
        controls::TextField(
            "Doubled", *probe().number,
            controls::Formatter<double>{
                [](const double& value) { return std::to_string(value * 2.0); },
                [](std::string_view text) -> std::optional<double> {
                  return std::stod(std::string{text}) / 2.0;
                }})
            .id("doubled"),
        controls::TextField(
            *probe().number,
            controls::Formatter<double>{
                [](const double& value) { return std::to_string(value); },
                [](std::string_view) -> std::optional<double> {
                  return std::nullopt;
                }},
            controls::TextFieldOptions{}, components::Text("Raw"))
            .id("raw"));
  });
  chord(providers, application, provider::Key::left_ctrl, provider::Key::a);
  type(providers, application, provider::Key::digit5);
  EXPECT_EQ(probe().bytes->get(), 5);
  EXPECT_TRUE(node_of(application, "doubled").has_value());
}

TEST_F(TextEditingParityTest, SuggestionsShowWhileFocusedAndCompletionsFill) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = launch(providers, [](composition::Context&) {
    return controls::TextField("City",
                               composition::Binding<std::string>{*probe().text})
        .text_input_suggestions(
            std::vector<std::string>{"Lisbon", "Lima"},
            [](const std::string& city) {
              return components::Text(city).text_input_completion(city).id(
                  city);
            })
        .id("city");
  });
  EXPECT_TRUE(node_of(application, "text-input-suggestions").has_value());
  press(providers, application, provider::Button::dpad_down);
  EXPECT_EQ(focused_key(application), "arisa.completion.Lisbon");
  press(providers, application, provider::Button::dpad_down);
  press(providers, application, provider::Button::a);
  EXPECT_EQ(probe().text->get(), "Lima");
}

TEST_F(TextEditingParityTest, SuggestionBuildersTakeContentAndIdentifiedData) {
  provider::headless::HeadlessProviderFactory providers;
  struct City {
    int id;
    std::string name;
  };
  Application application = launch(providers, [](composition::Context&) {
    return components::VStack(
        controls::TextField("One",
                            composition::Binding<std::string>{*probe().text})
            .text_input_suggestions([] {
              return components::Text("Only").text_input_completion("Only");
            })
            .id("one"),
        controls::TextField("Two",
                            composition::Binding<std::string>{*probe().text})
            .text_input_suggestions(
                std::vector<City>{{.id = 1, .name = "Oslo"}},
                [](const City& city) { return city.id; },
                [](const City& city) { return components::Text(city.name); })
            .id("two"));
  });
  EXPECT_TRUE(shows(application, "Only"));
}

TEST_F(TextEditingParityTest, FindNavigatorFindsAndReplacesInAnEditor) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = launch(providers, [](composition::Context&) {
    return components::VStack(
               controls::TextEditor(
                   composition::Binding<std::string>{*probe().text},
                   composition::Binding<std::optional<TextSelection>>{
                       *probe().selection})
                   .id("editor"),
               Element{Reader{}})
        .find_navigator(composition::Binding<bool>{*probe().flag});
  });
  probe().text->set("cat bat cat");
  probe().flag->set(true);
  frames(application, 3);
  ASSERT_TRUE(node_of(application, "find-navigator").has_value());
  providers.text_input().push(std::string{"cat"});
  ASSERT_TRUE(
      application.root().activate(composition::ElementKey{"find-field"}));
  frames(application, 6);
  EXPECT_EQ(shows(application, "1 of 2"), true);
  ASSERT_TRUE(probe().find.has_value());
  EXPECT_FALSE(probe().find->supports_replace);
  EXPECT_EQ(controls::FindNavigator::matches("cat bat CAT", "cat").size(), 2U);
  EXPECT_EQ(controls::FindNavigator::status(0, 2), "1 of 2");
  ASSERT_TRUE(
      application.root().activate(composition::ElementKey{"find-next"}));
  frames(application, 2);
  ASSERT_TRUE(probe().selection->get().has_value());
  ASSERT_TRUE(
      application.root().activate(composition::ElementKey{"find-done"}));
  frames(application, 2);
  EXPECT_FALSE(probe().flag->get());
}

TEST_F(TextEditingParityTest, FindAndReplaceCanBeDisabled) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = launch(providers, [](composition::Context&) {
    return controls::TextEditor(
               composition::Binding<std::string>{*probe().text})
        .replace_disabled(false)
        .find_disabled(probe().number->get() > 100.0)
        .find_navigator(composition::Binding<bool>{*probe().flag})
        .id("editor");
  });
  probe().flag->set(true);
  frames(application, 3);
  EXPECT_TRUE(node_of(application, "replace-field").has_value());
  probe().number->set(500.0);
  frames(application, 3);
  EXPECT_FALSE(node_of(application, "find-navigator").has_value());
}

TEST_F(TextEditingParityTest, SearchSelectionBindsTheSearchFieldSelection) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = launch(providers, [](composition::Context&) {
    return components::Text("results")
        .searchable(composition::Binding<std::string>{*probe().text})
        .search_selection(composition::Binding<std::optional<TextSelection>>{
            *probe().selection});
  });
  type(providers, application, provider::Key::q);
  ASSERT_TRUE(probe().selection->get().has_value());
  EXPECT_EQ(probe().selection->get()->range().lower, 1U);
}

TEST_F(TextEditingParityTest, KeyboardToolbarItemsShowWhileEditing) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = launch(providers, [](composition::Context&) {
    return presentation::NavigationStack([] {
      return Element{
          controls::TextField("Note",
                              composition::Binding<std::string>{*probe().text})
              .toolbar(composition::ToolbarItem(
                  composition::ToolbarItemPlacement::keyboard,
                  components::Text("Done").id("keyboard-done")))};
    });
  });
  EXPECT_FALSE(node_of(application, "keyboard-done").has_value());
  type(providers, application, provider::Key::a);
  frames(application, 3);
  EXPECT_TRUE(node_of(application, "keyboard-done").has_value());
  type(providers, application, provider::Key::escape);
  frames(application, 3);
  EXPECT_FALSE(node_of(application, "keyboard-done").has_value());
}

}
