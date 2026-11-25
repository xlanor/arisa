#pragma once

#include <arisa/components/subviews_collection.hpp>
#include <arisa/composition/container_value_key.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/keyed_values.hpp>

#include <utility>

namespace arisa::components {

class SectionConfiguration {
 public:
  class ID {
   public:
    explicit ID(composition::ElementKey key) : key_(std::move(key)) {}

    auto key() const noexcept -> const composition::ElementKey& { return key_; }

    friend auto operator==(const ID&, const ID&) -> bool = default;

   private:
    composition::ElementKey key_;
  };

  SectionConfiguration(ID id, SubviewsCollection header,
                       SubviewsCollection footer, SubviewsCollection content,
                       SubviewsCollection actions,
                       composition::KeyedValues values)
      : id_(std::move(id)),
        header_(std::move(header)),
        footer_(std::move(footer)),
        content_(std::move(content)),
        actions_(std::move(actions)),
        values_(std::move(values)) {}

  auto id() const noexcept -> const ID& { return id_; }

  auto header() noexcept -> SubviewsCollection& { return header_; }

  auto header() const noexcept -> const SubviewsCollection& { return header_; }

  auto footer() noexcept -> SubviewsCollection& { return footer_; }

  auto footer() const noexcept -> const SubviewsCollection& { return footer_; }

  auto content() noexcept -> SubviewsCollection& { return content_; }

  auto content() const noexcept -> const SubviewsCollection& {
    return content_;
  }

  auto actions() noexcept -> SubviewsCollection& { return actions_; }

  auto actions() const noexcept -> const SubviewsCollection& {
    return actions_;
  }

  auto container_values() const noexcept -> const composition::KeyedValues& {
    return values_;
  }

  template <class T>
  auto operator[](const composition::ContainerValueKey<T>& key) const
      -> const T& {
    return values_.get(key);
  }

 private:
  ID id_;
  SubviewsCollection header_;
  SubviewsCollection footer_;
  SubviewsCollection content_;
  SubviewsCollection actions_;
  composition::KeyedValues values_;
};

}
