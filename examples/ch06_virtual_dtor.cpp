// ch06 — why a polymorphic base needs a virtual destructor
// build: clang++ -std=c++20 -Wall -Wextra ch06_virtual_dtor.cpp -o /tmp/d && /tmp/d
#include <cstdio>
#include <memory>

struct GoodBase {
    virtual ~GoodBase() { puts("~GoodBase"); }   // virtual -> correct cleanup
};
struct GoodDerived : GoodBase {
    ~GoodDerived() override { puts("~GoodDerived"); }
};

int main() {
    puts("-- with virtual dtor (correct) --");
    {
        std::unique_ptr<GoodBase> p = std::make_unique<GoodDerived>();
        // deleting through GoodBase* runs ~GoodDerived THEN ~GoodBase
    }
    // Output:
    //   ~GoodDerived
    //   ~GoodBase
    //
    // If GoodBase's dtor were NON-virtual, only ~GoodBase would run -> the
    // derived part would not be destroyed (UB / leak). Always make polymorphic
    // base destructors virtual.
}
