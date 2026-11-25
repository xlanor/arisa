#include <arisa/composition/presentation_sizing.hpp>

namespace arisa::composition {

const PresentationSizing PresentationSizing::automatic{Kind::automatic};
const PresentationSizing PresentationSizing::page{Kind::page};
const PresentationSizing PresentationSizing::form{Kind::form};
const PresentationSizing PresentationSizing::fitted{Kind::fitted};

auto PresentationSizing::proposed_size(
    const PresentationSizingRoot& root,
    const PresentationSizingContext& context) const -> ProposedViewSize {
  const Size container = context.container();
  ProposedViewSize proposal;
  switch (kind_) {
    case Kind::custom:
      proposal =
          custom_ != nullptr ? (*custom_)(root, context) : ProposedViewSize{};
      break;
    case Kind::fitted:
      proposal = ProposedViewSize::unspecified;
      break;
    case Kind::form:
      proposal = ProposedViewSize{container.width * form_width,
                                  container.height * form_height};
      break;
    case Kind::automatic:
    case Kind::page:
      proposal = ProposedViewSize{container.width * page_width,
                                  container.height * page_height};
      break;
  }
  if (fits_width_) {
    proposal.width = std::nullopt;
  }
  if (fits_height_) {
    proposal.height = std::nullopt;
  }
  return proposal;
}

}
