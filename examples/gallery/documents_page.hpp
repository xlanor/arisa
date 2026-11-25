#pragma once

#include <arisa/components/document_error.hpp>
#include <arisa/components/file_document_read_configuration.hpp>
#include <arisa/components/file_document_write_configuration.hpp>
#include <arisa/components/file_wrapper.hpp>
#include <arisa/composition/app_storage.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/lazy_state.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/ut_type.hpp>

#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::examples {

class DocumentsPage final : public composition::Component {
 public:
  class Opener;
  class Demos;
  class Note;

  static constexpr std::string_view close_button = "documents-close";

  explicit DocumentsPage(composition::StateHandle<bool> shown) noexcept
      : shown_(std::move(shown)) {}

  auto operator==(const DocumentsPage& other) const -> bool {
    return shown_.identity() == other.shown_.identity();
  }

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  composition::StateHandle<bool> shown_;
};

class DocumentsPage::Note {
 public:
  explicit Note(std::string text = {}) : text_(std::move(text)) {}

  static auto readable_content_types() -> std::vector<composition::UTType>;

  static auto read(const components::FileDocumentReadConfiguration& read)
      -> std::expected<Note, components::DocumentError>;

  auto file_wrapper(const components::FileDocumentWriteConfiguration&) const
      -> std::expected<components::FileWrapper, components::DocumentError>;

  auto text() const noexcept -> const std::string& { return text_; }

  void set_text(std::string text) { text_ = std::move(text); }

  friend auto operator==(const Note&, const Note&) -> bool = default;

 private:
  std::string text_;
};

class DocumentsPage::Demos final : public composition::Component {
 public:
  static constexpr composition::StateKey<bool> importing{"documents-importing",
                                                         false};
  static constexpr composition::StateKey<bool> exporting{"documents-exporting",
                                                         false};
  static constexpr composition::StateKey<std::string> picked{"documents-picked",
                                                             std::string{}};
  static constexpr composition::StateKey<int> received{"documents-received", 0};
  static constexpr composition::StateKey<bool> overlays_hidden{
      "documents-overlays", false};
  static constexpr composition::LazyState<int> seeded{[] { return 7; }};
  static constexpr std::string_view import_button = "documents-import";
  static constexpr std::string_view export_button = "documents-export";
  static constexpr std::string_view tick_button = "documents-tick";
  static constexpr std::string_view overlays_toggle = "documents-overlays";
  static constexpr std::string_view notes_scene = "documents-notes";

  auto operator==(const Demos&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  static auto card(std::string_view title, composition::Element content)
      -> composition::Element;

  static auto notes() -> composition::Element;

  static auto files(composition::Context& context) -> composition::Element;

  static auto flow(composition::Context& context) -> composition::Element;

  static auto overlays(composition::Context& context) -> composition::Element;

  static auto ticks() -> composition::Observable<int>&;
};

class DocumentsPage::Opener final : public composition::Component {
 public:
  static constexpr composition::StateKey<bool> shown_key{"documents-shown",
                                                         false};
  static constexpr std::string_view button = "documents-open";

  auto operator==(const Opener&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;
};

}
