# 22 — Internals: Name Mangling, Pointers-to-Members & Linkage

Two more expert-level pieces of machinery: **name mangling** (how the linker
tells `foo(int)` from `foo(double)` and `A::f` from `B::f`) and
**pointers-to-members** (a genuinely weird type whose representation encodes
vtable indices and `this`-adjustments). Both are things senior engineers
routinely read in linker errors, `nm` output, and disassembly.

Prereq: [18-internals-vtables.md](18-internals-vtables.md), [20-internals-multiple-inheritance.md](20-internals-multiple-inheritance.md).

---

## 1. Why name mangling exists

C++ has function overloading, namespaces, classes, and templates — but the linker
only understands flat symbol names. **Name mangling** encodes the full signature
(scope, name, parameter types, cv-qualifiers) into a unique linker symbol.

```
   C++ source                         Mangled symbol (Itanium ABI)
   --------------------------------   ---------------------------------
   void foo(int)                      _Z3fooi
   void foo(double)                   _Z3food
   void foo(int, char*)               _Z3fooiPc
   namespace n { void foo(int); }     _ZN1n3fooEi
   struct A { void f() const; };      _ZNK1A1fEv     (K = const this)
   A::A(int)                          _ZN1AC1Ei      (C1 = complete ctor)
   A::~A()                            _ZN1AD1Ev      (D1 = complete dtor, ch 19)
   template T max<int>(T,T)           _Z3maxIiET_S0_S0_
```

<!--diagram
title: Anatomy of _ZNK1A3fooEi
box[blue] _Z
  text: prefix marking a mangled C++ name
box[green] N ... E
  text: a NESTED name (qualified); the block between N and E is the scope+name
box[orange] K
  text: the implicit `this` is **const** (cv-qualifier on the member)
box[teal] 1A 3foo
  text: length-prefixed identifiers: `A` (len 1), `foo` (len 3)
box[gray] i
  text: parameter list: one `int`
-->
```
   _ZNK1A3fooEi   =   A::foo(int) const
     _Z    : "this is a C++ mangled name"
     N..E  : nested-name (qualified) region
     K     : const member function (the hidden 'this' is const A*)
     1A    : identifier "A"  (the '1' is its length)
     3foo  : identifier "foo" (length 3)
     i     : parameters -> int
   Builtins have single letters: i=int, d=double, c=char, f=float, b=bool,
   v=void, x=long long, Pc = char* (P=pointer), Ri = int& (R=reference).
```

```
   extern "C" DISABLES mangling: the symbol is the bare name (so C can link it),
   which is why extern "C" functions can't be overloaded. This is how C++
   interoperates with C and stable ABIs (dlsym, plugin entry points).
```

---

## 2. Reading mangled names in practice

```
   Demangle on the command line:
     echo _ZNK1A3fooEi | c++filt         ->  A::foo(int) const
     nm -C libfoo.a                       ->  list symbols, demangled (-C)
     nm libfoo.a | c++filt                ->  same, manual demangle

   Demangle at runtime:
     #include <cxxabi.h>
     int status;
     char* dm = abi::__cxa_demangle(typeid(x).name(), nullptr, nullptr, &status);
     // use dm ...; free(dm);
```

```
   WHY YOU CARE (real scenarios):
     * "undefined reference to `A::foo(int)'" -> c++filt the mangled name in the
       error to find the exact overload/signature the linker wanted.
     * ABI breaks: change a parameter type / add const -> the mangled name
       changes -> old callers fail to link (this is how C++ detects ABI mismatch).
     * Reading disassembly / profilers: symbols appear mangled; c++filt them.
```

Runnable: [`examples/ch22_ptmf.cpp`](examples/ch22_ptmf.cpp) also prints demangled
type names.

---

## 3. Pointers to data members

A pointer-to-data-member is essentially an **offset** into an object — not an
address. You combine it with a specific object to get a real member.

```cpp
struct Point { int x; int y; };

int Point::*px = &Point::x;   // "pointer to member x": really the offset of x (0)
int Point::*py = &Point::y;   //  offset of y (4)

Point p{10, 20};
p.*px;                        // 10  (apply the offset to object p)
Point* pp = &p;
pp->*py;                      // 20  (arrow form)
```

```
   int Point::*  is NOT a normal pointer. Its VALUE is an offset:
     px  ==  offset_of(Point, x)  ==  0
     py  ==  offset_of(Point, y)  ==  4
   p.*px  means  *(int*)((char*)&p + offset)   -> reaches p.x
   A null member pointer is represented specially (often offset -1), not 0,
   because offset 0 is a valid first member.
```

---

## 4. Pointers to member FUNCTIONS (the weird one)

A pointer-to-member-function must work for both **non-virtual** and **virtual**
functions, and must carry the `this`-adjustment needed under multiple
inheritance. The Itanium ABI represents it as a **two-word struct**: `{ ptr,
adj }`.

<!--diagram
title: Pointer-to-member-function representation (Itanium)
box[blue] field 1: ptr
  text: if **even** -> a normal function address (non-virtual); if **odd** -> `1 + vtable byte-offset` (virtual)
box[green] field 2: adj
  text: the `this`-pointer adjustment (bytes to add to `this` before the call — for MI)
-->
```
   struct MemberFnPtr {
       fnptr_or_vtblindex  ptr;   // LSB encodes virtual-ness (Itanium trick):
                                  //   ptr & 1 == 0 -> ptr is the function address
                                  //   ptr & 1 == 1 -> (ptr-1) is a byte offset
                                  //                   into the vtable (virtual!)
       ptrdiff_t           adj;   // add this to 'this' before calling (MI/vbase)
   };

   Calling  (obj.*pmf)(args):
     this' = (char*)&obj + pmf.adj          ; apply adjustment
     if (pmf.ptr & 1)                       ; virtual?
         fn = *(vtable_of(this') + pmf.ptr - 1)   ; look up in vtable
     else
         fn = pmf.ptr                       ; direct address
     call fn(this', args)
```

```
   Consequences experts know:
     * sizeof(pointer-to-member-function) is often 16 bytes (two words), NOT 8.
       It is NOT a plain code pointer — don't reinterpret_cast it to void*.
     * A member-function pointer can transparently refer to a VIRTUAL function;
       calling through it still dispatches correctly (the LSB/vtable trick).
     * The 'adj' field is why member pointers survive multiple inheritance:
       the correct 'this' offset is baked into the pointer value.
     * std::function / std::mem_fn wrap all this so you rarely touch it raw.
   (MSVC uses a DIFFERENT, variable-size representation depending on the
    inheritance model — another reason member-fn pointers aren't portable bits.)
```

```cpp
struct Base { virtual int f(); int g(); };
int (Base::*pf)() = &Base::f;    // virtual   -> {ptr = 1+vtbl_offset, adj = 0}
int (Base::*pg)() = &Base::g;    // non-virt  -> {ptr = &Base::g,      adj = 0}
Base b;
(b.*pf)();   // dispatches virtually through b's vtable
(b.*pg)();   // direct call
```

Runnable: [`examples/ch22_ptmf.cpp`](examples/ch22_ptmf.cpp).

---

## 5. Linkage & the ODR at the symbol level

Tying chapters together — mangling is what makes the **One Definition Rule** and
**vague linkage** (chapter 18 §6) enforceable at link time:

```
   * Each entity gets a mangled symbol. The linker requires exactly ONE
     definition of each strong symbol (ODR) -> "multiple definition" errors.
   * inline functions, templates, and vtables use VAGUE LINKAGE: emitted as
     WEAK/COMDAT symbols in every TU that needs them; the linker keeps one and
     discards the rest (so header-defined inline/template code doesn't collide).
   * 'static' / anonymous-namespace entities get INTERNAL linkage -> not mangled
     into external symbols; invisible across TUs (good for TU-private helpers).
   * Templates are instantiated (and mangled with their args, e.g. _Z3maxIiE...)
     on demand; identical instantiations across TUs merge via COMDAT.
```

<!--diagram
title: Linkage kinds and the linker
box[green] External + strong (normal functions)
  text: exactly ONE definition allowed (ODR); duplicate -> "multiple definition"
box[orange] Vague / weak / COMDAT (inline, templates, vtables)
  text: may appear in many TUs; linker keeps one, drops the rest
box[gray] Internal (static / anonymous namespace)
  text: private to its TU; never collides across TUs
-->
```
   This is the machinery behind the everyday rules:
     * "define non-inline functions in ONE .cpp" (strong symbol, ODR)
     * "inline / templates can live in headers" (vague linkage merges them)
     * "anchor the vtable with an out-of-line virtual" (key function, ch 18)
```

---

## 6. Summary

```
 +-------------------------------------------------------------------+
 | NAME MANGLING encodes scope+name+param types+cv into a unique     |
 |   linker symbol (Itanium: _Z..., N..E nested, K const, builtins   |
 |   as letters). Demangle with c++filt / nm -C / __cxa_demangle.    |
 |   Enables overloading & ABI checks; extern "C" turns it off.      |
 | POINTER-TO-DATA-MEMBER = an OFFSET (combine with an object).      |
 | POINTER-TO-MEMBER-FUNCTION = a 2-word struct {ptr, adj}: LSB of   |
 |   ptr flags virtual (vtable offset) vs direct; adj fixes 'this'   |
 |   for MI. Often 16 bytes; NOT a plain code pointer.               |
 | Linkage: strong/external (ODR, one def), vague/COMDAT (inline,    |
 |   templates, vtables — deduped), internal (static/anon ns).       |
 +-------------------------------------------------------------------+
```

Next: [23-zero-overhead-and-idioms.md](23-zero-overhead-and-idioms.md).
