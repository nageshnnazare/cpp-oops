// ch18 — peek at a live vtable and call through it manually.
// build: clang++ -std=c++20 -Wall -Wextra ch18_vtable_dump.cpp -o /tmp/d && /tmp/d
//
// *** ABI-SPECIFIC, FOR LEARNING ONLY ***
// This relies on the Itanium C++ ABI vtable layout (GCC/Clang on Linux/macOS):
//   - the vptr is the first word of a polymorphic object
//   - the vptr points at the first virtual-function slot ("address point")
//   - slots are in declaration order (base virtuals first)
// It is technically UB by the language; do NOT do this in real code.
#include <cstdint>
#include <iostream>

struct Base {
    virtual int  f() const { return 1; }   // slot 0
    virtual int  g() const { return 2; }   // slot 1
    virtual ~Base() = default;             // dtor slots follow
};
struct Derived : Base {
    int x = 42;
    int f() const override { return 100; } // overrides slot 0
    int g() const override { return 200; } // overrides slot 1
};

using Fn = int (*)(const void*);           // signature: takes 'this'

int main() {
    Derived d;
    Base* p = &d;

    std::cout << "normal virtual calls:  f()=" << p->f()
              << " g()=" << p->g() << "\n\n";      // 100 200

    // Read the vptr (first word of the object):
    void** vtable = *reinterpret_cast<void***>(p);

    // Call slot 0 and slot 1 by hand (Itanium: vptr points at slot 0):
    Fn slot0 = reinterpret_cast<Fn>(vtable[0]);
    Fn slot1 = reinterpret_cast<Fn>(vtable[1]);
    std::cout << "manual vtable[0](this) = " << slot0(p) << '\n';   // 100
    std::cout << "manual vtable[1](this) = " << slot1(p) << '\n';   // 200

    // The RTTI pointer sits just ABOVE the address point (vptr[-1]):
    void* rtti = *reinterpret_cast<void**>(reinterpret_cast<char*>(vtable) - sizeof(void*));
    std::cout << "\nvptr[-1] (RTTI ptr) = " << rtti << " (non-null => has type_info)\n";
    std::cout << "sizeof(Base) = " << sizeof(Base) << " (just a vptr)\n";
}
