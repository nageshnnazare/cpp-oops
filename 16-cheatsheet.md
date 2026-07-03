# 16 — C++ OOP Cheatsheet

One-page recall. Read the chapters first; this is the reference card.

---

## The four pillars

<!--diagram
title: The four pillars
box[green] Encapsulation (ch 03)
  text: bundle data+methods, hide internals, protect invariants
box[teal] Abstraction (ch 07)
  text: expose **WHAT** not **HOW**; interfaces/abstract classes
box[purple] Inheritance (ch 05)
  text: is-a reuse/specialization
box[orange] Polymorphism (ch 06)
  text: one interface, many behaviors; virtual dispatch
-->
```
  ENCAPSULATION : bundle data+methods, hide internals, protect invariants (ch 03)
  ABSTRACTION   : expose WHAT not HOW; interfaces/abstract classes (ch 07)
  INHERITANCE   : is-a reuse/specialization (ch 05)
  POLYMORPHISM  : one interface, many behaviors; virtual dispatch (ch 06)
```

## Class basics

```cpp
class Foo {
public:                     // interface
    Foo(int x) : x_(x) {}   // ctor + initializer list
    int  get() const { return x_; }      // const method (non-mutating)
    void set(int x) { x_ = x; }
private:                    // internals
    int x_;                 // data member
};
// struct = class with default PUBLIC; class = default PRIVATE.
```

## Access & inheritance modes

```
  public    : everyone      | public inheritance    : is-a (use this)
  protected : + subclasses  | protected inheritance : rare
  private   : this class    | private inheritance   : "implemented-in-terms-of"
  (+ friend : granted access, not inherited/transitive)
```

## The six special members (Rule of 0/3/5)

```cpp
T();                              // default ctor
~T();                             // destructor
T(const T&);                      // copy ctor
T& operator=(const T&);           // copy assignment
T(T&&) noexcept;                  // move ctor
T& operator=(T&&) noexcept;       // move assignment

// RULE OF ZERO : own resources via RAII members; declare none. (best)
// RULE OF THREE: dtor + copy ctor + copy assign go together.
// RULE OF FIVE : + move ctor + move assign (noexcept!).
// Writing a dtor SUPPRESSES implicit moves -> then declare all five (=default).
```

## Construction / destruction order

```
  Construct: base(s) -> members (DECLARATION order) -> ctor body
  Destroy:   ctor body's counterpart -> members (REVERSE) -> base(s) (reverse)
  Virtual bases constructed FIRST, by the MOST-DERIVED class.
```

## Copy vs move

```
  copy: duplicate (deep). move: steal guts, leave source valid-empty (O(1)).
  std::move(x) = cast to rvalue (enables move overload); moves nothing itself.
  lvalue -> copy overload;  rvalue/std::move -> move overload.
```

## Polymorphism

```cpp
struct Base {
    virtual R f() ;                 // virtual -> dynamic dispatch (via vtable)
    virtual ~Base() = default;      // VIRTUAL DTOR on polymorphic bases (mandatory)
};
struct Derived : Base {
    R f() override;                 // always write 'override'
};
Base* p = new Derived();  p->f();  // Derived::f (runtime)
// final : lock a class/method. Don't call virtuals in ctors/dtors.
```

## Abstract classes & interfaces

```cpp
struct Shape {                       // abstract
    virtual double area() const = 0; // pure virtual -> must override
    virtual ~Shape() = default;
};
// Interface idiom = all pure virtual + no data. Implement several via MI.
```

## Multiple / diamond inheritance

```cpp
class D : public A, public B {};                 // multiple (best: interfaces)
class Mammal : public virtual Animal {};         // VIRTUAL inheritance ->
class Winged : public virtual Animal {};         //   ONE shared Animal (no dup)
class Bat : public Mammal, public Winged {        //   most-derived inits vbase
    Bat(...) : Animal(...) {}
};
```

## Operator overloading

```cpp
struct V {
    V& operator+=(const V&);              // member (compound assign)
    bool operator==(const V&) const = default;   // C++20 member-wise ==
    auto operator<=>(const V&) const = default;  // C++20 -> all comparisons
    friend V operator+(V a, const V& b){ return a += b; }         // symmetric
    friend std::ostream& operator<<(std::ostream&, const V&);     // hidden friend
};
// member: = [] () ->  ;  non-member/friend: + - == <<  (symmetric, conversions)
```

## Relationships / design

```
  IS-A   -> inheritance (only if substitutable, LSP)
  HAS-A  -> composition (member) / aggregation (ref)   <- FAVOR THIS for reuse
  USES-A -> dependency (parameter)
  Inherit to be SUBSTITUTABLE, not to REUSE.
```

## SOLID

<!--diagram
title: SOLID
box[green] S — Single Responsibility
  text: one reason to change
box[green] O — Open/Closed
  text: extend via new code, don't edit working code
box[green] L — Liskov Substitution
  text: subtypes honor base contract
box[green] I — Interface Segregation
  text: small focused interfaces
box[green] D — Dependency Inversion
  text: depend on abstractions; inject impls
-->
```
  S single responsibility (one reason to change)
  O open/closed (extend via new code, don't edit working code)
  L liskov substitution (subtypes honor base contract)
  I interface segregation (small focused interfaces)
  D dependency inversion (depend on abstractions; inject impls)
```

## Design patterns (recognize)

<!--diagram
title: Design patterns
box[blue] Creational
  text: Factory, Builder, Singleton (sparingly), Prototype
box[teal] Structural
  text: Adapter, Decorator, Composite, Facade, Proxy, Bridge
box[purple] Behavioral
  text: Strategy, Observer, Command, State, Template Method, Visitor
box[gray] Modern alternatives
  text: lambdas/`std::function` (Strategy/Command/Observer), `std::variant`+`visit` (Visitor, closed set), DI (Singleton)
-->
```
  Creational : Factory, Builder, Singleton(sparingly), Prototype
  Structural : Adapter, Decorator, Composite, Facade, Proxy, Bridge
  Behavioral : Strategy, Observer, Command, State, Template Method, Visitor
  Modern alts: lambdas/std::function (Strategy/Command/Observer),
               std::variant+visit (Visitor, closed set), DI (Singleton)
```

## Modern C++ OOP

```cpp
std::unique_ptr<T> p = std::make_unique<T>(...);   // sole owner (DEFAULT)
std::shared_ptr<T> s = std::make_shared<T>(...);   // shared (refcount)
std::weak_ptr<T>   w = s;                           // observer (break cycles)
std::vector<std::unique_ptr<Base>> polys;           // polymorphism, no slicing/leaks

// closed set of types:
using Shape = std::variant<Circle, Square>;
std::visit([](auto& x){ return x.area(); }, shape);

// C++23 deducing this (one getter for all qualifications):
template <class Self> auto&& data(this Self&& self){ return std::forward<Self>(self).data_; }
```

## Ownership in signatures

```
  unique_ptr<T> by value  -> "I take ownership" (sink) / factory returns it
  shared_ptr<T> by value  -> "I share ownership"
  T* / T&                 -> "I just use it" (non-owning)
  const T&                -> read-only, non-owning
```

## Pitfalls checklist

<!--diagram
title: Pitfalls checklist
box[red] Check before shipping
  text: `[ ]` virtual destructor on polymorphic bases (else delete-via-base = UB)
  text: `[ ]` no object slicing → use ref/ptr/`unique_ptr`, not base values
  text: `[ ]` no virtual calls in ctors/dtors
  text: `[ ]` Rule of Zero, else full Rule of Five (`noexcept` moves)
  text: `[ ]` self-assignment safe `operator=` (copy-and-swap)
  text: `[ ]` never return refs/ptrs to locals/temporaries
  text: `[ ]` public inheritance = real is-a (LSP); else compose
  text: `[ ]` break `shared_ptr` cycles with `weak_ptr`; prefer `unique_ptr`
  text: `[ ]` private data + behavior; single-arg ctors `explicit`
  text: `[ ]` build with `-Wall -Wextra -Wnon-virtual-dtor` + sanitizers
-->
```
  [ ] virtual destructor on polymorphic bases (else delete-via-base = UB)
  [ ] no object slicing -> use ref/ptr/unique_ptr, not base values
  [ ] no virtual calls in ctors/dtors
  [ ] Rule of Zero, else full Rule of Five (noexcept moves)
  [ ] self-assignment safe operator= (copy-and-swap)
  [ ] never return refs/ptrs to locals/temporaries
  [ ] public inheritance = real is-a (LSP); else compose
  [ ] break shared_ptr cycles with weak_ptr; prefer unique_ptr
  [ ] private data + behavior; single-arg ctors 'explicit'
  [ ] build with -Wall -Wextra -Wnon-virtual-dtor + sanitizers
```

## Expert internals (chapters 17–23)

<!--diagram
title: Expert internals
box[blue] Layout
  text: vptr @ offset 0 (Itanium); members in decl order + padding; EBO; `[[no_unique_address]]` (C++20); tail-pad reuse
box[teal] Vtable
  text: `[off-to-top][RTTI][slot0][slot1..]`; vptr → slot0. Call = load vptr; load vptr[i]; indirect call
box[purple] Virt dtor
  text: 3 variants D2/D1/D0; virtual dtor = 2 slots; `delete p` → D0 slot → derived dtor then free
box[orange] Multi inh / virtual inh
  text: N vptrs; this-adjusting thunks; vbase offsets; VTT + construction vtables
box[gray] RTTI / mangling / PTM / zero-cost / tools
  text: `type_info` @ vptr[-1]; `dynamic_cast` walks base graph; `_Z...` mangling; PTM = offset / `{ptr, adj}`; CRTP/`variant`/type-erasure; `clang++ -fdump-record-layouts`
-->
```
  LAYOUT      vptr @ offset 0 (Itanium); members in decl order + padding; EBO
              (empty base = 0 bytes); [[no_unique_address]] (C++20); tail-pad reuse.
  VTABLE      [off-to-top][RTTI][slot0][slot1..]; vptr -> slot0. Call = load vptr;
              load vptr[i]; indirect call. Slots positional; override swaps slot.
  KEY FN      first non-inline virtual anchors the vtable to one .o; else COMDAT.
              missing it -> "undefined reference to vtable".
  VIRT DTOR   3 variants D2(base)/D1(complete)/D0(deleting); virtual dtor = 2 slots;
              delete p -> D0 slot -> derived dtor then free. Non-virtual -> UB.
  MULTI INH   N vptrs; up/downcast ADJUSTS pointer value; override via 2nd base
              goes through a this-ADJUSTING THUNK. offset-to-top finds most-derived.
  VIRTUAL INH shared base offset is RUNTIME (vbase offset in vtable); VTT +
              construction vtables fix offsets mid-construction.
  RTTI        type_info @ vptr[-1] (polymorphic only). dynamic_cast walks the base
              graph (down + cross-casts); <void*> = most-derived. Fail: null / bad_cast.
  MANGLING    _Z... encodes scope+params+cv; c++filt / nm -C to read. extern "C" off.
  PTM         data member = offset; member-fn ptr = {ptr, adj} (LSB flags virtual).
  ZERO-COST   no virtual -> no vptr (C-struct cost); exceptions free until thrown;
              noexcept enables opts. CRTP/variant/type-erasure = polymorphism w/o vtable.
  TOOLS       clang++ -Xclang -fdump-record-layouts / -fdump-vtable-layouts ; c++filt.
```

## Golden rules

<!--diagram
title: Golden rules
box[green] Seven rules
  text: 1. Encapsulate: private data, public behavior, protect invariants
  text: 2. Rule of Zero: let RAII members manage resources
  text: 3. Prefer composition over inheritance; inherit for substitutability
  text: 4. Virtual destructor + `override` on every polymorphic class
  text: 5. Smart pointers, never raw `new`/`delete`; `unique_ptr` by default
  text: 6. Program to interfaces (abstractions), inject dependencies
  text: 7. Use the simplest tool: `variant`/templates/functions may beat a hierarchy
-->
```
  1. Encapsulate: private data, public behavior, protect invariants.
  2. Rule of Zero: let RAII members manage resources.
  3. Prefer composition over inheritance; inherit for substitutability.
  4. Virtual destructor + 'override' on every polymorphic class.
  5. Smart pointers, never raw new/delete; unique_ptr by default.
  6. Program to interfaces (abstractions), inject dependencies.
  7. Use the simplest tool: variant/templates/functions may beat a hierarchy.
```

---

Back to the [README](README.md).
