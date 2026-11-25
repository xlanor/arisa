#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/bindable.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/composition/tree.hpp>

#include <gtest/gtest.h>

#include <format>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace arisa::composition {

class BindingTest : public ::testing::Test {
 protected:
  struct Model {
    int first{1};
    int second{2};
    std::string name{"arisa"};

    friend auto operator==(const Model&, const Model&) -> bool = default;
  };

  struct Counts {
    int first{0};
    int second{0};
    int whole{0};
    std::optional<Binding<int>> captured;
  };

  class FirstReader final : public Component {
   public:
    FirstReader(Observable<Model>* model, Counts* counts) noexcept
        : model_(model), counts_(counts) {}

    auto operator==(const FirstReader&) const -> bool = default;

    auto body(Context& context) const -> Element override {
      ++counts_->first;
      const Bindable bindable{context, *model_};
      counts_->captured = bindable[&Model::first];
      return components::Text(std::format("{}", counts_->captured->get()));
    }

   private:
    Observable<Model>* model_;
    Counts* counts_;
  };

  class SecondReader final : public Component {
   public:
    SecondReader(Observable<Model>* model, Counts* counts) noexcept
        : model_(model), counts_(counts) {}

    auto operator==(const SecondReader&) const -> bool = default;

    auto body(Context& context) const -> Element override {
      ++counts_->second;
      return components::Text(
          std::format("{}", context.read(*model_, &Model::second)));
    }

   private:
    Observable<Model>* model_;
    Counts* counts_;
  };

  class WholeReader final : public Component {
   public:
    WholeReader(Observable<Model>* model, Counts* counts) noexcept
        : model_(model), counts_(counts) {}

    auto operator==(const WholeReader&) const -> bool = default;

    auto body(Context& context) const -> Element override {
      ++counts_->whole;
      return components::Text(context.read(*model_).name);
    }

   private:
    Observable<Model>* model_;
    Counts* counts_;
  };

  class Readers final : public Component {
   public:
    Readers(Observable<Model>* model, Counts* counts) noexcept
        : model_(model), counts_(counts) {}

    auto operator==(const Readers&) const -> bool = default;

    auto body(Context&) const -> Element override {
      return components::VStack(FirstReader{model_, counts_},
                                SecondReader{model_, counts_},
                                WholeReader{model_, counts_});
    }

   private:
    Observable<Model>* model_;
    Counts* counts_;
  };

  static auto owner() -> Tree::OwnerCheck {
    return [] { return true; };
  }
};

TEST_F(BindingTest, ConstantIgnoresWritesAndComputedForwardsThem) {
  const Binding<int> fixed = Binding<int>::constant(4);
  fixed.set(9);
  EXPECT_EQ(fixed.get(), 4);
  int stored = 3;
  const Binding<int> computed{[&stored] { return stored * 2; },
                              [&stored](int value) { stored = value / 2; }};
  EXPECT_EQ(computed.get(), 6);
  computed.set(10);
  EXPECT_EQ(stored, 5);
  computed.update([](int& value) { value += 2; });
  EXPECT_EQ(stored, 6);
}

TEST_F(BindingTest, ProjectedBindingsWriteOneMember) {
  Model stored;
  const Binding<Model> whole{[&stored] { return stored; },
                             [&stored](Model value) { stored = value; }};
  const Binding<std::string> name = whole.projected(&Model::name);
  EXPECT_EQ(name.get(), "arisa");
  name.set("kumo");
  EXPECT_EQ(stored.name, "kumo");
  EXPECT_EQ(stored.first, 1);
}

TEST_F(BindingTest, ModelFieldBindingsWriteThroughTheObservable) {
  Observable<Model> model;
  int heard = 0;
  const Subscription subscription = model.subscribe([&heard] { ++heard; });
  const Binding<int> first = Bindable{model}[&Model::first];
  first.set(7);
  EXPECT_EQ(model.get().first, 7);
  EXPECT_EQ(heard, 1);
  first.set(7);
  EXPECT_EQ(heard, 1);
}

TEST_F(BindingTest, FieldWritesReevaluateOnlyReadersOfThatField) {
  Observable<Model> model;
  Counts counts;
  Tree tree{Readers{&model, &counts}, owner()};
  tree.reconcile();
  ASSERT_EQ(counts.first, 1);
  ASSERT_EQ(counts.second, 1);
  ASSERT_EQ(counts.whole, 1);
  ASSERT_TRUE(counts.captured.has_value());
  counts.captured->set(5);
  EXPECT_TRUE(tree.dirty());
  tree.reconcile();
  EXPECT_EQ(counts.first, 2);
  EXPECT_EQ(counts.second, 1);
  EXPECT_EQ(counts.whole, 2);
  model.write(&Model::second, 8);
  tree.reconcile();
  EXPECT_EQ(counts.first, 2);
  EXPECT_EQ(counts.second, 2);
  EXPECT_EQ(counts.whole, 3);
  model.update([](Model& value) { value.first = 0; });
  tree.reconcile();
  EXPECT_EQ(counts.first, 3);
  EXPECT_EQ(counts.second, 3);
  EXPECT_EQ(counts.whole, 4);
}

}
