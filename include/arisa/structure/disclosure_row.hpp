#pragma once

#include <arisa/components/button.hpp>
#include <arisa/composition/element.hpp>

#include <optional>
#include <string>

namespace arisa::structure {

class DisclosureRow : public components::Button {
 public:
  DisclosureRow(std::string title, Action action);

  DisclosureRow(std::string title, std::string detail, Action action);

  DisclosureRow(Action action, composition::Element label,
                std::optional<composition::Element> detail = std::nullopt);

 private:
  static auto row(composition::Element label,
                  std::optional<composition::Element> detail)
      -> composition::Element;
};

}
