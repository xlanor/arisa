#include <arisa/components/button.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/focus_interactions.hpp>
#include <arisa/composition/pasteboard.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/transfer_item.hpp>
#include <arisa/composition/ut_type.hpp>
#include <arisa/controls/context_menu.hpp>
#include <arisa/provider/headless/headless_factory.hpp>

#include <focus_fixture.hpp>

#include <gtest/gtest.h>

#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::runtime {

using composition::Element;

class ClipboardTest : public FocusFixture {
 protected:
  class Note {
   public:
    explicit Note(std::string text) noexcept : text_(std::move(text)) {}

    static auto content_type() -> composition::UTType {
      return composition::UTType{"dev.arisa.test.note"};
    }

    auto encoded() const -> std::string { return text_; }

    static auto decoded(std::string_view text) -> std::optional<Note> {
      return Note{std::string{text}};
    }

    auto text() const noexcept -> const std::string& { return text_; }

   private:
    std::string text_;
  };

  class Board final : public composition::Component {
   public:
    static constexpr composition::StateKey<std::vector<std::string>> pasted{
        "pasted", {}};
    static constexpr composition::StateKey<int> cut_count{"cut", 0};

    auto operator==(const Board&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto received = context.state(pasted);
      const auto cuts = context.state(cut_count);
      std::vector<Element> rows;
      for (const std::string& each : received.get()) {
        rows.push_back(components::Text(each));
      }
      rows.push_back(components::Text(std::format("cut {}", cuts.get())));
      return components::VStack(
          components::Button("note", [] {})
              .frame({.width = 100.0F, .height = 50.0F})
              .id("note")
              .copyable(std::vector<Note>{Note{"hello"}})
              .cuttable(std::type_identity<Note>{},
                        [cuts] {
                          cuts.set(cuts.get() + 1);
                          return std::vector<Note>{Note{"snip"}};
                        }),
          components::VStack(std::move(rows))
              .frame({.width = 200.0F, .height = 100.0F})
              .focusable(true, composition::FocusInteractions::edit)
              .id("target")
              .paste_destination(std::type_identity<Note>{},
                                 [received](std::vector<Note> notes) {
                                   std::vector<std::string> next =
                                       received.get();
                                   for (const Note& note : notes) {
                                     next.push_back(note.text());
                                   }
                                   received.set(std::move(next));
                                 }));
    }
  };

  static void chord(provider::headless::HeadlessProviderFactory& providers,
                    Application& application, provider::Key modifier,
                    provider::Key letter) {
    provider::InputFrame down;
    down.keys.push_back(provider::KeyEvent{.key = modifier, .down = true});
    down.keys.push_back(provider::KeyEvent{.key = letter, .down = true});
    providers.input().push(down);
    provider::InputFrame up;
    up.keys.push_back(provider::KeyEvent{.key = letter, .down = false});
    up.keys.push_back(provider::KeyEvent{.key = modifier, .down = false});
    providers.input().push(up);
    frames(application, 3);
  }
};

TEST_F(ClipboardTest, KeyboardCopyAndPasteMoveTransferableItems) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, Board{});
  chord(providers, application, provider::Key::left_super, provider::Key::c);
  press(providers, application, provider::Button::dpad_down);
  chord(providers, application, provider::Key::left_ctrl, provider::Key::v);
  EXPECT_TRUE(shows(application, "hello"));
}

TEST_F(ClipboardTest, CutRunsTheActionAndFillsThePasteboard) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, Board{});
  chord(providers, application, provider::Key::left_super, provider::Key::x);
  EXPECT_TRUE(shows(application, "cut 1"));
  press(providers, application, provider::Button::dpad_down);
  chord(providers, application, provider::Key::left_super, provider::Key::v);
  EXPECT_TRUE(shows(application, "snip"));
}

TEST_F(ClipboardTest, CopyCutAndPasteCommandsRunTheirActions) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<std::vector<std::string>>();
  Application application = start(
      providers,
      components::VStack(
          components::Button("copier", [] {})
              .frame({.width = 100.0F, .height = 50.0F})
              .on_copy_command([log] {
                log->emplace_back("copy");
                return composition::TransferItem::of(std::string{"copied"});
              })
              .id("copier"),
          components::Button("cutter", [] {})
              .frame({.width = 100.0F, .height = 50.0F})
              .on_cut_command([log] {
                log->emplace_back("cut");
                return composition::TransferItem::of(std::string{"snipped"});
              })
              .id("cutter"),
          components::Button("target", [] {})
              .frame({.width = 100.0F, .height = 50.0F})
              .on_paste_command(
                  {composition::UTType::plain_text()},
                  [log](std::vector<composition::TransferItem> items) {
                    for (const composition::TransferItem& item : items) {
                      log->push_back(std::format(
                          "paste {}",
                          item.as<std::string>().value_or("nothing")));
                    }
                  })
              .id("target")));
  chord(providers, application, provider::Key::left_super, provider::Key::c);
  press(providers, application, provider::Button::dpad_down);
  press(providers, application, provider::Button::dpad_down);
  chord(providers, application, provider::Key::left_super, provider::Key::v);
  press(providers, application, provider::Button::dpad_up);
  chord(providers, application, provider::Key::left_super, provider::Key::x);
  press(providers, application, provider::Button::dpad_down);
  chord(providers, application, provider::Key::left_super, provider::Key::v);
  EXPECT_EQ(*log, (std::vector<std::string>{"copy", "paste copied", "cut",
                                            "paste snipped"}));
}

TEST_F(ClipboardTest, TheOptionsMenuOffersCopyAndPaste) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, Board{});
  press(providers, application, provider::Button::plus);
  frames(application, 10);
  EXPECT_TRUE(shows(application, "Cut"));
  EXPECT_TRUE(shows(application, "Copy"));
  press(providers, application, provider::Button::dpad_down);
  press(providers, application, provider::Button::a);
  frames(application, 10);
  press(providers, application, provider::Button::dpad_down);
  press(providers, application, provider::Button::plus);
  frames(application, 10);
  EXPECT_TRUE(shows(application, "Paste"));
  press(providers, application, provider::Button::a);
  frames(application, 10);
  EXPECT_TRUE(shows(application, "hello"));
}

TEST_F(ClipboardTest, PlainTextReachesTheSystemClipboard) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers,
      components::VStack(components::Button("text", [] {})
                             .frame({.width = 100.0F, .height = 50.0F})
                             .copyable(std::vector<std::string>{"plain words"})
                             .id("text")));
  chord(providers, application, provider::Key::left_super, provider::Key::c);
  const auto written = providers.clipboard().read_text();
  ASSERT_TRUE(written.has_value());
  EXPECT_EQ(*written, "plain words");
}

TEST_F(ClipboardTest, ExternalTextIsOfferedAsPlainText) {
  provider::headless::HeadlessProviderFactory providers;
  composition::Pasteboard board{providers.clipboard()};
  board.set_items({composition::TransferItem::of(Note{"inside"})});
  ASSERT_TRUE(providers.clipboard().write_text("outside").has_value());
  const std::vector<composition::TransferItem> items = board.items();
  ASSERT_EQ(items.size(), 1U);
  EXPECT_EQ(items.front().as<std::string>(),
            std::optional<std::string>{"outside"});
  EXPECT_FALSE(board.contains(Note::content_type()));
}

}
