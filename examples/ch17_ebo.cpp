// ch17 — object layout: padding, standard-layout, EBO, [[no_unique_address]]
// build: clang++ -std=c++20 -Wall -Wextra ch17_ebo.cpp -o /tmp/d && /tmp/d
//
// Bonus: dump exact field offsets with:
//   clang++ -std=c++20 -Xclang -fdump-record-layouts -c ch17_ebo.cpp
#include <cstddef>
#include <iostream>
#include <type_traits>

struct S      { char c; double d; int i; };   // padding demo
struct S2     { double d; int i; char c; };    // reordered -> smaller

struct Empty  {};
struct AsMember { Empty e; int x; };           // empty as MEMBER -> costs space
struct AsBase : Empty { int x; };              // empty as BASE   -> EBO (0 bytes)

struct WithNUA {
    [[no_unique_address]] Empty e;             // C++20 EBO for members
    int x;
};

struct Poly { int x; virtual ~Poly() = default; };  // vptr at offset 0

int main() {
    std::cout << std::boolalpha;
    std::cout << "sizeof(S)  = " << sizeof(S)  << " (padding)\n";      // 24
    std::cout << "sizeof(S2) = " << sizeof(S2) << " (reordered)\n";    // 16

    std::cout << "sizeof(AsMember) = " << sizeof(AsMember) << '\n';    // 8
    std::cout << "sizeof(AsBase)   = " << sizeof(AsBase)   << " (EBO)\n"; // 4
    std::cout << "sizeof(WithNUA)  = " << sizeof(WithNUA)  << " (no_unique_address)\n"; // 4

    std::cout << "is_standard_layout<S>   = " << std::is_standard_layout_v<S>   << '\n'; // true
    std::cout << "is_standard_layout<Poly>= " << std::is_standard_layout_v<Poly><< '\n'; // false
    std::cout << "sizeof(Poly) = " << sizeof(Poly) << " (has vptr)\n"; // 16

    std::cout << "offsetof(S, d) = " << offsetof(S, d) << '\n';        // 8
    std::cout << "offsetof(S2,c) = " << offsetof(S2, c) << '\n';       // 12
}
