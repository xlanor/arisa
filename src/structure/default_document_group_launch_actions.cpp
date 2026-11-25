#include <arisa/structure/default_document_group_launch_actions.hpp>

#include <arisa/components/group.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/structure/document_launch_view.hpp>
#include <arisa/structure/new_document_button.hpp>

#include <memory>
#include <string>

namespace arisa::structure {

DefaultDocumentGroupLaunchActions::DefaultDocumentGroupLaunchActions()
    : Element(std::make_unique<Content>()) {}

auto DefaultDocumentGroupLaunchActions::new_key() -> composition::ElementKey {
  return composition::ElementKey{std::string{"arisa.document-launch.new"}};
}

auto DefaultDocumentGroupLaunchActions::Content::type() const noexcept
    -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto DefaultDocumentGroupLaunchActions::Content::body(
    composition::Context& context) const -> std::vector<composition::Element> {
  std::vector<composition::Element> produced;
  if (context.environment(DocumentLaunchView::creates_documents)) {
    produced.push_back(composition::Element{NewDocumentButton{}}.id(new_key()));
  }
  return produced;
}

}
