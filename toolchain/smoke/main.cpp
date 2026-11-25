#include <switch.h>
#include <deko3d.hpp>

#include <coroutine>
#include <cstdio>
#include <expected>
#include <flat_map>
#include <functional>
#include <generator>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace smoke {

class Counter {
public:
    explicit Counter(int start) : value_(start) {}

    template <typename Self>
    auto&& advanced(this Self&& self, int by)
    {
        self.value_ += by;
        return std::forward<Self>(self);
    }

    [[nodiscard]] int value() const { return value_; }

private:
    int value_;
};

enum class ParseError { empty };

[[nodiscard]] std::expected<int, ParseError> parse(std::string_view text)
{
    if (text.empty())
        return std::unexpected(ParseError::empty);
    return static_cast<int>(text.size());
}

std::generator<int> squares(int count)
{
    for (int i : std::views::iota(0, count))
        co_yield i * i;
}

}

int main()
{
    consoleInit(nullptr);

    std::flat_map<std::string, int> table{{"a", 1}, {"b", 2}};
    std::move_only_function<int()> sum = [owned = std::vector{1, 2, 3}] {
        return owned[0] + owned[1] + owned[2];
    };
    auto counter = smoke::Counter{1}.advanced(2);
    auto parsed = smoke::parse("four");
    auto total = std::ranges::fold_left(smoke::squares(4), 0, std::plus{});
    auto vector = smoke::squares(3) | std::ranges::to<std::vector>();

    std::printf("%zu %d %d %d %d %zu\n", table.size(), sum(), counter.value(),
        parsed.value_or(-1), total, vector.size());

    consoleExit(nullptr);
    return 0;
}
