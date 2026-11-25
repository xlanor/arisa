#include <focus_fixture.hpp>

#include <arisa/components/button.hpp>
#include <arisa/components/document.hpp>
#include <arisa/components/document_base_box.hpp>
#include <arisa/components/document_configuration.hpp>
#include <arisa/components/document_creation_context.hpp>
#include <arisa/components/document_creation_source.hpp>
#include <arisa/components/document_error.hpp>
#include <arisa/components/document_read_configuration.hpp>
#include <arisa/components/document_write_configuration.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/file_document_configuration.hpp>
#include <arisa/components/file_document_read_configuration.hpp>
#include <arisa/components/file_document_write_configuration.hpp>
#include <arisa/components/file_storage.hpp>
#include <arisa/components/file_wrapper.hpp>
#include <arisa/components/file_wrapper_document_reader.hpp>
#include <arisa/components/file_wrapper_document_writer.hpp>
#include <arisa/components/reference_file_document_configuration.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/url_document_configuration.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/file_url.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/composition/ut_type.hpp>
#include <arisa/controls/share_link.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/headless/memory_storage.hpp>
#include <arisa/structure/default_document_group_launch_actions.hpp>
#include <arisa/structure/document_group.hpp>
#include <arisa/structure/document_group_launch_scene.hpp>
#include <arisa/structure/document_launch_geometry_proxy.hpp>
#include <arisa/structure/document_launch_view.hpp>
#include <arisa/structure/file_browser.hpp>
#include <arisa/structure/new_document_button.hpp>

#include <gtest/gtest.h>

#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace arisa::runtime {

class DocumentGroupTest : public FocusFixture {
 protected:
  using Element = composition::Element;
  using UTType = composition::UTType;
  using DocumentError = components::DocumentError;
  using FileWrapper = components::FileWrapper;

  struct Note {
    std::string text;

    static auto readable_content_types() -> std::vector<UTType> {
      return {UTType::plain_text()};
    }

    static auto read(const components::FileDocumentReadConfiguration& read)
        -> std::expected<Note, DocumentError> {
      const std::optional<std::string> text = read.file().text();
      if (!text.has_value()) {
        return std::unexpected(DocumentError{DocumentError::Stage::decode,
                                             std::string{"not a note"}});
      }
      return Note{*text};
    }

    auto file_wrapper(const components::FileDocumentWriteConfiguration&) const
        -> std::expected<FileWrapper, DocumentError> {
      return FileWrapper::regular_file(text);
    }
  };

  class Sketch {
   public:
    using Reader = components::FileWrapperDocumentReader<std::string>;
    using Writer = components::FileWrapperDocumentWriter<std::string>;

    explicit Sketch(std::string strokes) : strokes_(std::move(strokes)) {}

    static auto readable_content_types() -> std::vector<UTType> {
      return {UTType::exported("sketch")};
    }

    static auto writable_content_types() -> std::vector<UTType> {
      return {UTType::exported("sketch")};
    }

    auto reader(const components::DocumentReadConfiguration& configuration)
        const -> Reader {
      return Reader{configuration,
                    [](const FileWrapper& wrapper)
                        -> std::expected<std::string, DocumentError> {
                      return wrapper.text().value_or(std::string{});
                    }};
    }

    auto apply(std::string snapshot, std::optional<std::string> previous)
        -> std::expected<void, DocumentError> {
      static_cast<void>(previous);
      strokes_ = std::move(snapshot);
      return {};
    }

    auto writer(const components::DocumentWriteConfiguration& configuration)
        const -> Writer {
      return Writer{
          configuration,
          [](const std::string& snapshot, const std::optional<FileWrapper>&)
              -> std::expected<FileWrapper, DocumentError> {
            return FileWrapper::regular_file(snapshot);
          }};
    }

    auto snapshot(const UTType&) const
        -> std::expected<std::string, DocumentError> {
      return strokes_;
    }

    auto strokes() const noexcept -> const std::string& { return strokes_; }

    void draw() { strokes_ += "+"; }

   private:
    std::string strokes_;
  };

  struct Board {
    using Snapshot = std::string;

    std::string cards;

    static auto readable_content_types() -> std::vector<UTType> {
      return {UTType::exported("board")};
    }

    static auto read(const components::FileDocumentReadConfiguration& read)
        -> std::expected<Board, DocumentError> {
      return Board{read.file().text().value_or(std::string{})};
    }

    auto snapshot(const UTType&) const
        -> std::expected<std::string, DocumentError> {
      return cards;
    }

    static auto file_wrapper(const std::string& snapshot,
                             const components::FileDocumentWriteConfiguration&)
        -> std::expected<FileWrapper, DocumentError> {
      return FileWrapper::regular_file(snapshot);
    }
  };

  static auto launch(provider::headless::HeadlessProviderFactory& providers,
                     provider::headless::MemoryStorage& card, Element content)
      -> Application {
    auto created =
        Application::create(providers, std::move(content),
                            ApplicationOptions{.executors = std::nullopt,
                                               .fonts = {},
                                               .atlas = {},
                                               .interaction = {},
                                               .files = card});
    EXPECT_TRUE(created.has_value());
    Application application = std::move(*created);
    frames(application, 20);
    return application;
  }

  static auto path(std::string_view text) -> provider::AssetPath {
    return *provider::AssetPath::create(text);
  }

  static void enter_out(Application& application) {
    const std::string out =
        structure::FileBrowser::row_key("out").description();
    if (!node_of(application, out).has_value()) {
      activate(application, structure::FileBrowser::up_key());
    }
    if (node_of(application, out).has_value()) {
      activate(application, structure::FileBrowser::row_key("out"));
    }
  }

  static void activate(Application& application,
                       const composition::ElementKey& key) {
    ASSERT_TRUE(application.root().activate(key)) << key.description();
    frames(application, 20);
  }
};

TEST_F(DocumentGroupTest, AFileDocumentIsCreatedEditedUndoneAndSaved) {
  static std::optional<components::DocumentConfiguration> configured;
  static bool undo_present = false;
  static bool can_open = false;
  static std::optional<components::DocumentCreationSource> sourced;
  class Probe final : public composition::Component {
   public:
    auto operator==(const Probe&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      configured =
          context.environment(components::Environment::document_configuration);
      undo_present = context.environment(components::Environment::undo_manager)
                         .has_value();
      can_open = context.environment(components::Environment::open_document)
                     .available();
      return components::ZStack();
    }
  };
  provider::headless::MemoryStorage card;
  provider::headless::HeadlessProviderFactory providers;
  Application application = launch(
      providers, card,
      Element{
          structure::DocumentGroup{
              Note{"hello"},
              [](const components::FileDocumentConfiguration<Note>&
                     configuration) -> Element {
                const auto bound = configuration.document_binding();
                sourced = configuration.creation_source();
                return components::VStack(
                    components::Text(configuration.document().text).id("text"),
                    components::Button("append",
                                       [bound] {
                                         bound.update([](Note& note) {
                                           note.text += "!";
                                         });
                                       })
                        .id("append"),
                    components::Text(
                        std::string{configuration.file_url()->path()})
                        .id("where"),
                    Element{Probe{}});
              }}}
          .document_group_launch_scene(structure::DocumentGroupLaunchScene{
              "Notes", [] {
                return Element{structure::NewDocumentButton{
                                   std::nullopt, std::nullopt,
                                   components::DocumentCreationSource{"blank"}}}
                    .id("new");
              }}));
  EXPECT_TRUE(shows(application, "Notes"));
  activate(application, composition::ElementKey{"new"});
  EXPECT_TRUE(shows(application, "hello"));
  EXPECT_TRUE(shows(application, "Documents/Untitled.txt"));
  EXPECT_EQ(card.text(path("Documents/Untitled.txt")), "hello");
  ASSERT_TRUE(configured.has_value());
  EXPECT_TRUE(configured->is_editable());
  EXPECT_EQ(configured->file_url(),
            composition::FileURL::create("Documents/Untitled.txt"));
  EXPECT_TRUE(undo_present);
  EXPECT_TRUE(can_open);
  EXPECT_EQ(sourced, components::DocumentCreationSource{"blank"});
  activate(application, composition::ElementKey{"append"});
  EXPECT_TRUE(shows(application, "hello!"));
  activate(application, structure::DocumentGroup::undo_key());
  EXPECT_TRUE(shows(application, "hello"));
  activate(application, structure::DocumentGroup::redo_key());
  EXPECT_TRUE(shows(application, "hello!"));
  activate(application, structure::DocumentGroup::close_key());
  EXPECT_EQ(card.text(path("Documents/Untitled.txt")), "hello!");
  EXPECT_TRUE(shows(application, "Notes"));
  activate(application, structure::FileBrowser::row_key("Documents"));
  activate(application, structure::FileBrowser::row_key("Untitled.txt"));
  EXPECT_TRUE(shows(application, "hello!"));
  activate(application, structure::DocumentGroup::close_key());
  activate(application, composition::ElementKey{"new"});
  EXPECT_EQ(card.text(path("Documents/Untitled 2.txt")), "hello");
  EXPECT_EQ(components::DocumentContentTypes::writable<Note>(),
            Note::readable_content_types());
}

TEST_F(DocumentGroupTest, ADocumentClassIsMadeReadAndAutosaved) {
  static std::vector<std::optional<std::string>> sources;
  static std::vector<std::string> urls;
  provider::headless::MemoryStorage card;
  provider::headless::HeadlessProviderFactory providers;
  Application application = launch(
      providers, card,
      Element{structure::DocumentGroup{
                  true,
                  [](composition::Observable<Sketch>& sketch) -> Element {
                    return components::VStack(
                        components::Text(sketch.get().strokes()).id("strokes"),
                        components::Button("draw", [&sketch] {
                          sketch.update([](Sketch& held) { held.draw(); });
                        }).id("draw"));
                  },
                  [](const components::URLDocumentConfiguration& configuration,
                     const components::DocumentCreationContext& context)
                      -> std::expected<Sketch, DocumentError> {
                    sources.emplace_back(
                        context.creation_source().has_value()
                            ? std::optional<std::string>{std::string{
                                  context.creation_source()->id()}}
                            : std::nullopt);
                    urls.push_back(configuration.description());
                    return Sketch{"start"};
                  }}}
          .document_group_launch_scene(structure::DocumentGroupLaunchScene{
              "Sketches",
              graphics::ShapeStyle{
                  theme::ColourValue{theme::ColourRole::background}},
              [] {
                return Element{structure::NewDocumentButton{
                                   std::string{"Blank"},
                                   UTType::exported("sketch"),
                                   components::DocumentCreationSource{"blank"}}}
                    .id("new");
              }}));
  activate(application, composition::ElementKey{"new"});
  EXPECT_EQ(card.text(path("Documents/Untitled.sketch")), "start");
  ASSERT_FALSE(sources.empty());
  EXPECT_EQ(sources.front(), std::optional<std::string>{"blank"});
  EXPECT_TRUE(shows(application, "start"));
  activate(application, composition::ElementKey{"draw"});
  EXPECT_TRUE(shows(application, "start+"));
  EXPECT_EQ(card.text(path("Documents/Untitled.sketch")), "start");
  for (int step = 0; step < 8; ++step) {
    ASSERT_TRUE(
        providers.clock().advance(std::chrono::milliseconds{200}).has_value());
    frames(application, 2);
  }
  EXPECT_EQ(card.text(path("Documents/Untitled.sketch")), "start+");
  EXPECT_FALSE(urls.empty());
}

TEST_F(DocumentGroupTest, AViewerOpensReadOnly) {
  provider::headless::MemoryStorage card;
  card.add(path("old.sketch"), "old");
  provider::headless::HeadlessProviderFactory providers;
  Application application = launch(
      providers, card,
      Element{structure::DocumentGroup{
          [](composition::Observable<Sketch>& sketch) -> Element {
            return components::Text(sketch.get().strokes()).id("strokes");
          },
          [](const components::URLDocumentConfiguration&,
             const components::DocumentCreationContext&)
              -> std::expected<Sketch, DocumentError> { return Sketch{""}; }}});
  EXPECT_FALSE(shows(application,
                     std::string{structure::NewDocumentButton::default_title}));
  activate(application, structure::FileBrowser::row_key("old.sketch"));
  EXPECT_TRUE(shows(application, "old"));
  EXPECT_FALSE(
      node_of(application, structure::DocumentGroup::undo_key().description())
          .has_value());
}

TEST_F(DocumentGroupTest, AReferenceDocumentSavesItsObservableModel) {
  provider::headless::MemoryStorage card;
  provider::headless::HeadlessProviderFactory providers;
  Application application = launch(
      providers, card,
      Element{
          structure::DocumentGroup{
              [] { return Board{"a"}; },
              [](const components::ReferenceFileDocumentConfiguration<Board>&
                     configuration) -> Element {
                composition::Observable<Board>& board =
                    configuration.document();
                return components::VStack(
                    components::Text(board.get().cards).id("cards"),
                    components::Button("add", [&board] {
                      board.update([](Board& held) { held.cards += "b"; });
                    }).id("add"));
              }}}
          .document_launch_title("Boards")
          .document_launch_subtitle("Recent"));
  EXPECT_TRUE(shows(application, "Boards"));
  EXPECT_TRUE(shows(application, "Recent"));
  activate(application,
           structure::DefaultDocumentGroupLaunchActions::new_key());
  EXPECT_EQ(card.text(path("Documents/Untitled.board")), "a");
  activate(application, composition::ElementKey{"add"});
  EXPECT_TRUE(shows(application, "ab"));
  activate(application, structure::DocumentGroup::close_key());
  EXPECT_EQ(card.text(path("Documents/Untitled.board")), "ab");
  EXPECT_EQ(components::DocumentContentTypes::writable<Board>(),
            Board::readable_content_types());
}

TEST_F(DocumentGroupTest, ALaunchViewOpensPreparedDocumentsAndMeasuresItself) {
  static std::optional<structure::DocumentLaunchGeometryProxy> under;
  static std::optional<structure::DocumentLaunchGeometryProxy> over;
  provider::headless::MemoryStorage card;
  card.add(path("templates/blank.txt"), "blank");
  provider::headless::HeadlessProviderFactory providers;
  Application application = launch(
      providers, card,
      structure::DocumentLaunchView{
          "Library",
          {UTType::plain_text()},
          [] {
            return Element{structure::NewDocumentButton{
                               std::string{"Template"}, UTType::plain_text(),
                               [] {
                                 return composition::FileURL::create(
                                     "templates/blank.txt");
                               }}}
                .id("template");
          },
          [](const composition::FileURL& url) -> Element {
            return components::Text("open:" + std::string{url.path()});
          },
          structure::DocumentLaunchView::Options{
              .subtitle = "Pick",
              .background_accessory =
                  [](const structure::DocumentLaunchGeometryProxy& proxy) {
                    under = proxy;
                    return Element{components::ZStack()};
                  },
              .overlay_accessory =
                  [](const structure::DocumentLaunchGeometryProxy& proxy) {
                    over = proxy;
                    return Element{components::ZStack()};
                  }}});
  EXPECT_TRUE(shows(application, "Library"));
  EXPECT_TRUE(shows(application, "Pick"));
  ASSERT_TRUE(under.has_value() && over.has_value());
  EXPECT_GT(under->frame.width, 0.0F);
  EXPECT_EQ(over->frame, under->frame);
  activate(application, composition::ElementKey{"template"});
  EXPECT_TRUE(shows(application, "open:templates/blank.txt"));
  provider::headless::HeadlessProviderFactory styled;
  Application backed = launch(
      styled, card,
      structure::DocumentLaunchView{
          "Styled",
          {UTType::plain_text()},
          graphics::ShapeStyle{theme::ColourValue{theme::ColourRole::accent}},
          [] {
            return Element{structure::DefaultDocumentGroupLaunchActions{}};
          },
          [](const composition::FileURL&) -> Element {
            return components::Text("doc");
          }});
  EXPECT_TRUE(shows(backed, "Styled"));
  activate(backed, structure::FileBrowser::row_key("templates"));
  activate(backed, structure::FileBrowser::row_key("blank.txt"));
  EXPECT_TRUE(shows(backed, "doc"));
}

TEST_F(DocumentGroupTest, DocumentValueTypesCompareAndDescribeThemselves) {
  static_assert(components::FileDocument<Note>);
  static_assert(components::Document<Sketch>);
  static_assert(components::ReadableDocument<Sketch>);
  static_assert(components::WritableDocument<Sketch>);
  static_assert(components::DocumentReader<Sketch::Reader>);
  static_assert(components::DocumentWriter<Sketch::Writer>);
  static_assert(components::ReferenceFileDocument<Board>);
  static_assert(!components::FileDocument<Board>);
  const components::DocumentReadConfiguration read{UTType::plain_text()};
  EXPECT_EQ(read, components::DocumentReadConfiguration{UTType::plain_text()});
  EXPECT_EQ(read.content_type(), UTType::plain_text());
  EXPECT_FALSE(read.description().empty());
  const components::DocumentWriteConfiguration write{UTType::exported("x")};
  EXPECT_EQ(write.content_type(), UTType::exported("x"));
  EXPECT_FALSE(write.description().empty());
  const components::DocumentCreationSource blank{"blank"};
  EXPECT_EQ(blank, components::DocumentCreationSource{"blank"});
  EXPECT_NE(blank, components::DocumentCreationSource{"other"});
  EXPECT_EQ(blank.id(), "blank");
  const components::DocumentCreationContext created{blank};
  EXPECT_EQ(created.creation_source(), blank);
  EXPECT_EQ(created, components::DocumentCreationContext{blank});
  EXPECT_NE(created, components::DocumentCreationContext{});
  const components::URLDocumentConfiguration first{
      composition::FileURL::create("a.txt"), blank};
  const components::URLDocumentConfiguration copy = first;
  EXPECT_EQ(first, copy);
  EXPECT_NE(first, (components::URLDocumentConfiguration{
                       composition::FileURL::create("a.txt"), blank}));
  first.set_last_content_modification_date(provider::FileStamp{3});
  EXPECT_EQ(copy.last_content_modification_date(), provider::FileStamp{3});
  EXPECT_EQ(copy.creation_source(), blank);
  const components::FileDocumentReadConfiguration file_read{
      UTType::plain_text(), FileWrapper::regular_file(std::string_view{"x"})};
  EXPECT_EQ(file_read.file().text(), std::optional<std::string>{"x"});
  EXPECT_EQ(file_read.content_type(), UTType::plain_text());
  const components::FileDocumentWriteConfiguration file_write{
      UTType::plain_text(), std::nullopt};
  EXPECT_FALSE(file_write.existing_file().has_value());
  EXPECT_EQ(file_write.content_type(), UTType::plain_text());
  components::DocumentBaseBox<Note> box;
  EXPECT_EQ(box.base(), nullptr);
  box.set_base(std::make_shared<composition::Observable<Note>>(Note{"boxed"}));
  EXPECT_EQ(box.base()->get().text, "boxed");
  const DocumentError failed{DocumentError::Stage::write, std::string{"full"}};
  EXPECT_EQ(failed.explanation(), "document write failed: full");
  provider::headless::MemoryStorage card;
  const components::FileStorage storage{card};
  const FileWrapper folder = FileWrapper::directory({[] {
    FileWrapper child = FileWrapper::regular_file(std::string_view{"one"});
    child.set_preferred_filename("one.txt");
    return child;
  }()});
  ASSERT_TRUE(folder.write(storage, *composition::FileURL::create("bundle"))
                  .has_value());
  EXPECT_EQ(card.text(path("bundle/one.txt")), "one");
  const auto read_back =
      FileWrapper::read(storage, *composition::FileURL::create("bundle"));
  ASSERT_TRUE(read_back.has_value());
  EXPECT_TRUE(read_back->is_directory());
  ASSERT_EQ(read_back->file_wrappers().size(), 1U);
  EXPECT_EQ(read_back->file_wrappers().front().text(),
            std::optional<std::string>{"one"});
  const components::FileWrapperDocumentReader<std::string> reader{
      read,
      [](const FileWrapper& wrapper)
          -> std::expected<std::string, DocumentError> {
        return wrapper.text().value_or(std::string{});
      }};
  const auto read_one =
      reader.read(storage, *composition::FileURL::create("bundle/one.txt"));
  ASSERT_TRUE(read_one.has_value());
  EXPECT_EQ(*read_one, "one");
  const components::FileWrapperDocumentWriter<std::string> writer{
      write,
      [](const std::string& snapshot, const std::optional<FileWrapper>&)
          -> std::expected<FileWrapper, DocumentError> {
        return FileWrapper::regular_file(snapshot);
      }};
  ASSERT_TRUE(writer
                  .write("two", storage,
                         *composition::FileURL::create("two.txt"), std::nullopt)
                  .has_value());
  EXPECT_EQ(card.text(path("two.txt")), "two");
}

TEST_F(DocumentGroupTest, FileDialogsHandleSeveralFiles) {
  static std::vector<std::string> imported;
  static std::vector<std::string> exported;
  static std::vector<std::string> moved;
  static std::vector<std::string> items;
  static std::string single;
  static constexpr composition::StateKey<int> stage{"documents.stage", 0};
  static std::optional<composition::StateHandle<int>> stepping;
  class Host final : public composition::Component {
   public:
    auto operator==(const Host&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      stepping = context.state(stage);
      const int at = stepping->get();
      const auto record = [](std::vector<std::string>& into) {
        return [&into](structure::FileBrowser::Results results) {
          if (results.has_value()) {
            for (const composition::FileURL& url : *results) {
              into.emplace_back(url.path());
            }
          }
        };
      };
      Element shown = components::Text("host");
      if (at == 0) {
        return std::move(shown).file_importer(
            composition::Binding<bool>::constant(true), {UTType::plain_text()},
            true, record(imported));
      }
      if (at == 1) {
        return std::move(shown).file_exporter(
            composition::Binding<bool>::constant(true),
            std::vector<Note>{Note{"a"}, Note{"b"}}, UTType::plain_text(),
            record(exported));
      }
      if (at == 2) {
        return std::move(shown).file_mover(
            composition::Binding<bool>::constant(true),
            std::vector<composition::FileURL>{
                *composition::FileURL::create("in/a.txt"),
                *composition::FileURL::create("in/b.txt")},
            record(moved));
      }
      if (at == 3) {
        return std::move(shown).file_exporter(
            composition::Binding<bool>::constant(true),
            std::vector<std::string>{"x", "y"},
            std::vector<UTType>{UTType::plain_text()}, record(items));
      }
      return std::move(shown).file_exporter(
          composition::Binding<bool>::constant(true),
          std::optional<std::string>{"z"},
          std::vector<UTType>{UTType::plain_text()}, "single.txt",
          [](structure::FileBrowser::Result result) {
            if (result.has_value()) {
              single = std::string{result->path()};
            }
          });
    }
  };
  provider::headless::MemoryStorage card;
  card.add(path("in/a.txt"), "a");
  card.add(path("in/b.txt"), "b");
  card.add(path("out/keep.txt"), "k");
  provider::headless::HeadlessProviderFactory providers;
  Application application = launch(providers, card, Element{Host{}});
  activate(application, structure::FileBrowser::row_key("in"));
  activate(application, structure::FileBrowser::row_key("a.txt"));
  activate(application, structure::FileBrowser::row_key("b.txt"));
  activate(application, structure::FileBrowser::action_key());
  EXPECT_EQ(imported, (std::vector<std::string>{"in/a.txt", "in/b.txt"}));
  stepping->set(1);
  frames(application, 20);
  enter_out(application);
  activate(application, structure::FileBrowser::action_key());
  EXPECT_EQ(exported, (std::vector<std::string>{"out/Untitled.txt",
                                                "out/Untitled 2.txt"}));
  EXPECT_EQ(card.text(path("out/Untitled 2.txt")), "b");
  stepping->set(2);
  frames(application, 20);
  enter_out(application);
  activate(application, structure::FileBrowser::action_key());
  EXPECT_EQ(moved, (std::vector<std::string>{"out/a.txt", "out/b.txt"}));
  stepping->set(3);
  frames(application, 20);
  enter_out(application);
  activate(application, structure::FileBrowser::action_key());
  EXPECT_EQ(items.size(), 2U);
  stepping->set(4);
  frames(application, 20);
  enter_out(application);
  activate(application, structure::FileBrowser::action_key());
  EXPECT_EQ(single, "out/single.txt");
  EXPECT_EQ(card.text(path("out/single.txt")), "z");
}

TEST_F(DocumentGroupTest, NavigationDocumentsShareFromTheTitleMenu) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers,
      components::VStack(
          components::Text("body").navigation_title("Doc").navigation_document(
              std::string{"notes"}),
          components::Text("previewed")
              .navigation_document(std::string{"notes"},
                                   components::SharePreview{"Notes"})));
  EXPECT_TRUE(shows(application, "body"));
  EXPECT_TRUE(shows(application, "previewed"));
}

}
