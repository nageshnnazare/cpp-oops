// ch00 — compile-time checks for rules the guide states.
// build: clang++ -std=c++20 -Wall -Wextra ch00_checks.cpp -o /tmp/d && /tmp/d
#include <compare>
#include <concepts>
#include <iostream>
#include <string>
#include <type_traits>

struct Agg {
    int x;
    int y = 0;
};
struct NotAgg {
    NotAgg() = default;   // user-declared: not an aggregate in C++20
    int x;
};

struct Ordered {
    int x;
    auto operator<=>(const Ordered&) const = default;   // also gives ==
};

struct HandWritten {
    int x;
    std::strong_ordering operator<=>(const HandWritten& o) const { return x <=> o.x; }
};

struct LayoutOk {
    int a;
    char b;
};
struct LayoutMixedAccess {
    int touch() const { return a + b; }
    int a = 0;
private:
    int b = 0;
};

struct Empty {};
struct EmptyAsMember { Empty e; int x; };
struct EmptyAsBase : Empty { int x; };

struct Poly { virtual ~Poly() = default; };

struct MixedAccess {
    int touch() const { return a + b; }
private:
    int a = 0;
public:
    int b = 0;
};
struct Logged {
    int x;
    ~Logged() {}
};

template <class T>
concept equality_comparable_here = requires(const T& a, const T& b) { a == b; };

static_assert(std::is_aggregate_v<Agg>);
static_assert(!std::is_aggregate_v<NotAgg>);
static_assert(equality_comparable_here<Ordered>);
static_assert(!equality_comparable_here<HandWritten>);
static_assert(std::is_standard_layout_v<LayoutOk>);
static_assert(!std::is_standard_layout_v<LayoutMixedAccess>);
static_assert(!std::is_standard_layout_v<Poly>);
static_assert(std::is_trivially_copyable_v<LayoutOk>);
static_assert(!std::is_trivially_copyable_v<std::string>);
static_assert(sizeof(Empty) >= 1);
static_assert(sizeof(EmptyAsBase) == sizeof(int));
static_assert(sizeof(EmptyAsMember) > sizeof(int));
static_assert(std::is_trivially_copyable_v<MixedAccess>);
static_assert(!std::is_standard_layout_v<MixedAccess>);
static_assert(std::is_standard_layout_v<Logged>);
static_assert(!std::is_trivially_copyable_v<Logged>);

int main() {
    std::cout << "language checks passed\n";
    std::cout << "sizeof empty-as-base " << sizeof(EmptyAsBase)
              << ", empty-as-member " << sizeof(EmptyAsMember) << '\n';
}
