#pragma once

#include <arisa/composition/anchor.hpp>
#include <arisa/composition/anchor_source.hpp>
#include <arisa/composition/identity.hpp>
#include <arisa/composition/preference_key.hpp>
#include <arisa/composition/rect.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

namespace arisa::composition {

class Preference {
 public:
  enum class Kind : std::uint8_t {
    value,
    contribution,
    transform,
    anchored,
  };

  class Entry {
   public:
    virtual ~Entry() = default;

    virtual auto kind() const noexcept -> Kind = 0;

    virtual auto absorbs(const Entry& incoming) const -> bool = 0;
  };

  template <class T>
  class Typed : public Entry {
   public:
    virtual auto applied(std::optional<T> value, const Rect& frame) const
        -> std::optional<T> = 0;
  };

  template <class T>
  class Value final : public Typed<T> {
   public:
    Value(const PreferenceKey<T>& key, T value, Kind kind)
        : key_(key), value_(std::move(value)), kind_(kind) {}

    auto kind() const noexcept -> Kind override { return kind_; }

    auto absorbs(const Entry& incoming) const -> bool override {
      return incoming.kind() == kind_ &&
             key_.get().absorbs(value_,
                                static_cast<const Value&>(incoming).value_);
    }

    auto applied(std::optional<T> value, const Rect&) const
        -> std::optional<T> override {
      if (kind_ == Kind::contribution && value.has_value()) {
        key_.get().reduce(*value, value_);
        return value;
      }
      return value_;
    }

   private:
    std::reference_wrapper<const PreferenceKey<T>> key_;
    T value_;
    Kind kind_;
  };

  template <class T>
  class Transform final : public Typed<T> {
   public:
    using Edit = std::move_only_function<void(T&) const>;

    Transform(const PreferenceKey<T>& key, Edit edit)
        : key_(key), edit_(std::move(edit)) {}

    auto kind() const noexcept -> Kind override { return Kind::transform; }

    auto absorbs(const Entry&) const -> bool override { return false; }

    auto applied(std::optional<T> value, const Rect&) const
        -> std::optional<T> override {
      if (!value.has_value()) {
        value.emplace(key_.get().default_value());
      }
      edit_(*value);
      return value;
    }

   private:
    std::reference_wrapper<const PreferenceKey<T>> key_;
    Edit edit_;
  };

  template <class T, class Shape>
  class Anchored final : public Typed<T> {
   public:
    using Resolve = std::move_only_function<T(const Anchor<Shape>&) const>;

    Anchored(AnchorSource<Shape> source, Resolve resolve)
        : source_(source), resolve_(std::move(resolve)) {}

    auto kind() const noexcept -> Kind override { return Kind::anchored; }

    auto absorbs(const Entry&) const -> bool override { return false; }

    auto applied(std::optional<T>, const Rect& frame) const
        -> std::optional<T> override {
      return resolve_(Anchor<Shape>{source_, frame});
    }

   private:
    AnchorSource<Shape> source_;
    Resolve resolve_;
  };

  template <class T, class Shape>
  class AnchoredTransform final : public Typed<T> {
   public:
    using Edit = std::move_only_function<void(T&, const Anchor<Shape>&) const>;

    AnchoredTransform(const PreferenceKey<T>& key, AnchorSource<Shape> source,
                      Edit edit)
        : key_(key), source_(source), edit_(std::move(edit)) {}

    auto kind() const noexcept -> Kind override { return Kind::anchored; }

    auto absorbs(const Entry&) const -> bool override { return false; }

    auto applied(std::optional<T> value, const Rect& frame) const
        -> std::optional<T> override {
      if (!value.has_value()) {
        value.emplace(key_.get().default_value());
      }
      edit_(*value, Anchor<Shape>{source_, frame});
      return value;
    }

   private:
    std::reference_wrapper<const PreferenceKey<T>> key_;
    AnchorSource<Shape> source_;
    Edit edit_;
  };

  template <class T, class Shape>
  static auto anchored_transform(
      const PreferenceKey<T>& key, AnchorSource<Shape> source,
      typename AnchoredTransform<T, Shape>::Edit edit) -> Preference {
    return Preference{key.identity(),
                      std::make_unique<AnchoredTransform<T, Shape>>(
                          key, source, std::move(edit))};
  }

  template <class T>
  static auto value(const PreferenceKey<T>& key, T value) -> Preference {
    return Preference{key.identity(), std::make_unique<Value<T>>(
                                          key, std::move(value), Kind::value)};
  }

  template <class T>
  static auto contribution(const PreferenceKey<T>& key, T value) -> Preference {
    return Preference{
        key.identity(),
        std::make_unique<Value<T>>(key, std::move(value), Kind::contribution)};
  }

  template <class T>
  static auto transform(const PreferenceKey<T>& key,
                        typename Transform<T>::Edit edit) -> Preference {
    return Preference{key.identity(),
                      std::make_unique<Transform<T>>(key, std::move(edit))};
  }

  template <class T, class Shape>
  static auto anchored(const PreferenceKey<T>& key, AnchorSource<Shape> source,
                       typename Anchored<T, Shape>::Resolve resolve)
      -> Preference {
    return Preference{key.identity(), std::make_unique<Anchored<T, Shape>>(
                                          source, std::move(resolve))};
  }

  auto key() const noexcept -> Identity { return key_; }

  auto anchored() const noexcept -> bool {
    return entry_->kind() == Kind::anchored;
  }

  auto absorbs(const Preference& incoming) const -> bool {
    return incoming.key_ == key_ && entry_->absorbs(*incoming.entry_);
  }

  template <class T>
  auto applied(std::optional<T> value, const Rect& frame) const
      -> std::optional<T> {
    return static_cast<const Typed<T>&>(*entry_).applied(std::move(value),
                                                         frame);
  }

 private:
  Preference(Identity key, std::unique_ptr<Entry> entry) noexcept
      : key_(key), entry_(std::move(entry)) {}

  Identity key_;
  std::unique_ptr<Entry> entry_;
};

}
