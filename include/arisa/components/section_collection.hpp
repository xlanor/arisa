#pragma once

#include <arisa/components/section_configuration.hpp>
#include <arisa/composition/element.hpp>

#include <cstddef>
#include <vector>

namespace arisa::components {

class SectionCollection {
 public:
  explicit SectionCollection(composition::Element content);

  explicit SectionCollection(std::vector<composition::Element> content);

  auto size() const noexcept -> std::size_t { return sections_.size(); }

  auto empty() const noexcept -> bool { return sections_.empty(); }

  auto operator[](std::size_t index) -> SectionConfiguration& {
    return sections_[index];
  }

  auto operator[](std::size_t index) const -> const SectionConfiguration& {
    return sections_[index];
  }

  auto begin() noexcept { return sections_.begin(); }

  auto end() noexcept { return sections_.end(); }

  auto begin() const noexcept { return sections_.begin(); }

  auto end() const noexcept { return sections_.end(); }

 private:
  void flatten(composition::Element element);

  void close_loose();

  std::vector<SectionConfiguration> sections_;
  std::vector<composition::Element> loose_;
};

}
