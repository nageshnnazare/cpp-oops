// ch01 — object layout: padding and member ordering
// build: clang++ -std=c++20 -Wall -Wextra ch01_layout.cpp -o /tmp/d && /tmp/d
#include <iostream>
#include <cstddef>

struct Mixed  { char c; int i; char d; };   // poor ordering -> padding
struct Packed { int i; char c; char d; };   // better ordering
struct Empty  {};
struct Poly   { virtual ~Poly() = default; };// has a vptr

int main() {
    std::cout << "sizeof(Mixed)  = " << sizeof(Mixed)  << '\n';  // usually 12
    std::cout << "sizeof(Packed) = " << sizeof(Packed) << '\n';  // usually 8
    std::cout << "sizeof(Empty)  = " << sizeof(Empty)  << '\n';  // 1
    std::cout << "sizeof(Poly)   = " << sizeof(Poly)   << '\n';  // 8 (vptr)

    std::cout << "offset i in Mixed  = " << offsetof(Mixed, i)  << '\n';
    std::cout << "offset i in Packed = " << offsetof(Packed, i) << '\n';
}
