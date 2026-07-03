// ch20 — multiple inheritance: base subobjects live at different addresses,
//        pointer casts ADJUST the value, and dynamic_cast<void*> finds the top.
// build: clang++ -std=c++20 -Wall -Wextra ch20_this_adjust.cpp -o /tmp/d && /tmp/d
#include <cstdint>
#include <iostream>

struct A { int a = 1; virtual void fa() {} virtual ~A() = default; };
struct B { int b = 2; virtual void fb() {} virtual ~B() = default; };
struct C : A, B { int c = 3; void fa() override {} void fb() override {} };

static std::uintptr_t addr(const void* p) { return reinterpret_cast<std::uintptr_t>(p); }

int main() {
    C c;
    C* pc = &c;
    A* pa = pc;                 // no adjustment (A is the primary base @ offset 0)
    B* pb = pc;                 // ADJUSTED: pb = pc + sizeof(A-subobject)

    std::cout << std::hex << std::showbase;
    std::cout << "C* = " << addr(pc) << '\n';
    std::cout << "A* = " << addr(pa) << "  (delta from C = " << std::dec
              << (addr(pa) - addr(pc)) << std::hex << ")\n";
    std::cout << "B* = " << addr(pb) << "  (delta from C = " << std::dec
              << (addr(pb) - addr(pc)) << std::hex << ")\n";

    // Recover the most-derived object address from a secondary base pointer:
    void* top = dynamic_cast<void*>(pb);   // uses offset-to-top in B's vtable
    std::cout << std::dec << "\ndynamic_cast<void*>(B*) == C*? "
              << std::boolalpha << (top == static_cast<void*>(pc)) << '\n';

    // Cross-cast A* -> B* (only dynamic_cast can do this):
    B* viaCross = dynamic_cast<B*>(pa);
    std::cout << "cross-cast A*->B* == B*? " << (viaCross == pb) << '\n';
}
