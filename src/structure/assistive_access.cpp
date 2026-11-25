#include <arisa/structure/assistive_access.hpp>

#include <arisa/components/environment.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/context.hpp>

#include <string>
#include <utility>
#include <vector>

namespace arisa::structure {

auto AssistiveAccess::content() const -> composition::Element {
  if (content_ == nullptr || !*content_) {
    return components::Group();
  }
  return (*content_)();
}

auto AssistiveAccess::standard_key() -> composition::ElementKey {
  return composition::ElementKey{
      std::string{"arisa.assistive-access.standard"}};
}

auto AssistiveAccess::scene_key() -> composition::ElementKey {
  return composition::ElementKey{std::string{"arisa.assistive-access.scene"}};
}

auto AssistiveAccess::Host::body(composition::Context& context) const
    -> composition::Element {
  const bool assistive = context.environment(
      components::Environment::accessibility_assistive_access_enabled);
  composition::Element standard = composition::PlaceholderContentView{source_};
  if (assistive) {
    standard =
        std::move(standard).hidden().disabled(true).accessibility_hidden(true);
  }
  std::vector<composition::Element> layers;
  layers.push_back(std::move(standard).id(standard_key()));
  if (assistive) {
    layers.push_back(scene_.content().id(scene_key()));
  }
  return components::ZStack(components::ZStackOptions{}, std::move(layers));
}

}
