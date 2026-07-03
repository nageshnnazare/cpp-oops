// ch19 — virtual vs non-virtual destructor when deleting via a base pointer
// build: clang++ -std=c++20 -Wall -Wextra ch19_dtor_variants.cpp -o /tmp/d && /tmp/d
//
// Compile with symbols to SEE the D0/D1/D2 variants:
//   clang++ -std=c++20 -g -c ch19_dtor_variants.cpp -o /tmp/o.o
//   nm -C /tmp/o.o | grep -i '::~'      # look for [deleting], [complete], [base]
#include <cstdio>
#include <memory>

// --- correct: virtual destructor ---
struct GoodBase {
    virtual ~GoodBase() { puts("  ~GoodBase (D1 chains to base)"); }
};
struct GoodMid : GoodBase {
    ~GoodMid() override { puts("  ~GoodMid"); }
};
struct GoodDerived : GoodMid {
    ~GoodDerived() override { puts("  ~GoodDerived (runs first via D0 slot)"); }
};

// --- broken: NON-virtual destructor (shown for contrast; only ~BadBase runs) ---
struct BadBase { ~BadBase() { puts("  ~BadBase ONLY (derived dtor skipped!)"); } };
struct BadDerived : BadBase { ~BadDerived() { puts("  ~BadDerived (never called)"); } };

int main() {
    puts("delete through base* with VIRTUAL dtor (correct chain):");
    {
        GoodBase* p = new GoodDerived();
        delete p;   // D0 slot -> ~GoodDerived -> ~GoodMid -> ~GoodBase
    }

    puts("\ndelete through base* with NON-VIRTUAL dtor (BUG demo):");
    {
        // NOTE: this is UB by the standard; shown only to illustrate the failure.
        BadBase* p = new BadDerived();
        delete p;   // calls ~BadBase directly; ~BadDerived is skipped
    }
    puts("\n(See nm -C output for the [deleting]/[complete]/[base] variants.)");
}
