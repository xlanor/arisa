#include <arisa/structure/form.hpp>

#include <utility>

namespace arisa::structure {

Form::Form(std::vector<composition::Element> children)
    : List(ListStyle::inset_grouped(), std::move(children)) {
  edit<Content>().set_form();
}

}
