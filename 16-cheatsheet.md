# 16 — C++ OOP Cheatsheet

A reference card. The chapters are the explanations. This file is the thing
you want next to the editor.

---

## Pillars

```
  Encapsulation   private data, invariants, small public surface          ch 03
  Abstraction     callers depend on a contract                            ch 07
  Inheritance     a base subobject; public inheritance means substitutable ch 05
  Polymorphism    virtual (open), variant (closed), template, type erasure ch 06, 14
```

## `class` / `struct` / `union`

```
  struct   default public members and bases
  class    default private members and bases
  union    at most one active non-static data member
  They are otherwise the same feature. Pick struct for passive data.
```

## Object model

```
  lifetime starts when the constructor finishes, ends when the destructor starts
  if the constructor throws: finished subobjects are destroyed, the
    object's own destructor does not run
  complete object contains base subobjects and member subobjects
  sizeof(empty class) >= 1; an empty base may occupy 0 bytes
```

## Initialization

```
  T t;          default-init          class: default ctor; int: indeterminate
  T t{};        value / list-init     int becomes 0; rejects narrowing
  T t();        most vexing parse     declares a function
  T t{a, b};    list-init             initializer_list constructor wins if viable
  vector<int> v(10, 2);               ten 2s
  vector<int> v{10, 2};               elements 10 and 2

  C++20 aggregate: no user-declared constructors (S() = default is not one),
    no private/protected data, no virtuals, no virtual/private/protected bases.
  Designated initializers are in declaration order: Point{.x = 1, .y = 2}
```

## `this`

```
  void f();            this is T*
  void f() const;      this is const T*
  void f() &;          callable on lvalues
  void f() &&;         callable on rvalues
  static void f();     no this
  explicit object parameter (C++23) cannot be virtual
```

## Access

```
  public     everyone
  protected  the class, derived classes, their friends
             through an object: that object must be the derived type
             (not an arbitrary Base&, not a sibling)
  private    the class and its friends
  per class, not per object: a member can touch another instance's privates

  friend     granted, not inherited, not transitive, not symmetric
  hidden friend   defined in the class, found by ADL only
```

## Construction / destruction order

```
  1. virtual bases, most-derived class initializes each once
  2. direct non-virtual bases, left to right
  3. members, declaration order (not initializer-list order)
  4. constructor body
  destruction is the exact reverse (virtual bases last)

  delegating ctor: the target initializes; your body runs after
  using Base::Base: does not inherit default/copy/move; no derived body;
    derived members use default member initializers
```

## Special members

```
  T();  ~T();  T(const T&);  T& operator=(const T&);
  T(T&&) noexcept;  T& operator=(T&&) noexcept;

  Rule of Zero   RAII members, declare none of the five
  Rule of Three  raw resource => dtor + copy ctor + copy assign
  Rule of Five   also move ctor + move assign, noexcept if they cannot throw

  user-declared dtor or copy  =>  implicit move is NOT declared (falls back to copy)
  user-declared move          =>  implicit copy is defined as deleted
  = default on first declaration can be trivial
  = default out of line is user-provided, not trivial

  std::move is a cast, not a move
  moved-from standard types: valid but unspecified
  unique_ptr moved-from: null
  vector growth moves your type only if the move ctor is noexcept

  C++17: prvalue initialization does not call copy/move
  return local;     allowed to elide, otherwise implicit move
  return std::move(local);   blocks NRVO, do not

  memcpy only if trivially copyable
```

## Exception safety

```
  nothrow    does not throw (dtors, noexcept move, pointer swap)
  strong     succeeds, or the object is unchanged (copy-and-swap)
  basic      invariant holds, value may differ
  dtors are implicitly noexcept; a throw during unwind calls terminate
```

## Inheritance

```
  public inheritance      is-a, implicit conversion everywhere
  protected inheritance   conversion only for derived and friends
  private inheritance     implemented-in-terms-of; not a public is-a

  derived name hides ALL base overloads of that name
  using Base::f;          puts them back in the overload set

  slicing: copying Derived into a Base value drops the derived part
  store unique_ptr<Base>, or a reference, or a variant

  final on a class or a virtual function seals it
```

## Virtual functions

```
  dispatch uses the dynamic type, through a pointer or reference
  one vptr per polymorphic base subobject; further virtuals add vtable slots
  override checks the signature (cv and ref qualifiers included)
  covariant return: pointer or reference to a more derived class only
    (not unique_ptr)
  default arguments bind to the STATIC type; avoid them on virtuals
  Base::f() is a direct call, no dispatch

  polymorphic delete requires virtual ~Base()
  or protected non-virtual ~Base() to forbid delete through Base*

  during Base::Base() and Base::~Base() the dynamic type is Base
  a pure virtual call in that window is undefined

  member templates and static functions cannot be virtual
  constexpr virtual is allowed since C++20; the vptr is still there
```

## Abstract classes

```
  virtual void f() = 0;     pure; class is abstract
  a pure virtual destructor still needs a definition
  a pure virtual may have a body, called as Base::f()
  interface idiom: no data, all functions pure, virtual destructor
  NVI: public non-virtual function, private virtual hook
  concepts constrain templates; they do not replace virtual dispatch
```

## Multiple and virtual inheritance

```
  each base is a subobject; a secondary-base cast adjusts the address
  non-virtual diamond: two copies of the shared base, ambiguous upcast
  virtual base: one shared subobject, found by a runtime offset
  most-derived ctor initializes virtual bases; intermediate initializers
    for those bases do not run
  dominance: an override on one path beats the virtual-base declaration
  cross-cast (sibling to sibling): dynamic_cast, not static_cast
```

## Operators

```
  must be members:  =  []  ()  ->  ->*
  usually members:  compound assignment
  usually hidden friends:  +  ==  <<   so both sides convert, ADL-only

  implement += ;  friend T operator+(T a, const T& b) { return a += b; }
  prefix  ++t   returns T&
  postfix t++   takes unused int, returns the old value by value

  defaulted operator<=> also gives defaulted == and the relational ops
  a user-written <=> does NOT give ==
  categories: strong_ordering, weak_ordering, partial_ordering (float, NaN)
  explicit operator bool  works in if/while, does not convert to int
  do not overload &&  ||  or comma
```

## Relationships

```
  is-a, substitutable          public inheritance
  owns, nested lifetime        value member or unique_ptr member
  refers, does not own         reference, raw pointer, or weak_ptr
  uses                         parameter
  reuse without is-a           composition, not public inheritance
  empty policy, zero size      [[no_unique_address]] member, or empty base
```

## Static, nested

```
  static data member: one per class, not in sizeof
  static inline (C++17): define in the header
  cross-TU init order of namespace-scope objects: unspecified
  function-local static: initialized once, thread-safe since C++11
  do not re-enter that initialization

  nested class: scoped member type, sees outer privates, no hidden outer this
```

## Smart pointers

```
  unique_ptr<T>     sole owner, move-only, default choice
  shared_ptr<T>     shared owner, atomic refcount, control block
  weak_ptr<T>       observer; lock() or expired
  T* / T&           no ownership

  make_unique / make_shared
  make_shared keeps the allocation until the last weak_ptr dies
  unique_ptr deleter is part of the type; shared_ptr deleter is type-erased
  enable_shared_from_this: only after a shared_ptr owns *this
  never shared_ptr<T>(this)     second control block, double free
  pimpl: ~T and moves defined in the .cpp, where Impl is complete
  const does not propagate through unique_ptr
```

## Polymorphism choice

```
  open set, runtime, stable interface     virtual, unique_ptr<Base>
  closed set                              std::variant + std::visit
  known at the call, zero overhead        template or CRTP
  unrelated types, value semantics        type erasure (std::function, or Concept/Model)
  one algorithm, injected                 strategy: unique_ptr or std::function
```

## SOLID, one line each

```
  S  one reason to change
  O  extend by new types; virtual if open, variant if closed
  L  overrides do not strengthen preconditions or weaken postconditions
  I  do not force implementers to stub methods they do not have
  D  depend on an abstraction; inject the concrete type at the boundary
```

## Patterns, one line each

```
  Strategy          swappable algorithm
  Observer          notify listeners; weak or unsubscribe; snapshot the list
  Factory           function returning unique_ptr<Interface>
  Abstract factory  a matching family of products
  Builder           optional parts, validate in build(); else use an aggregate
  Prototype         virtual clone() const
  Adapter           the interface you want, over the type you have
  Bridge            abstraction owns an implementation pointer
  Composite         group and leaf share an interface; add() only on the group
  Decorator         wrapper with the same interface, owns the inner object
  Facade            one narrow entry to a subsystem
  Flyweight         shared immutable bulk, per-object extrinsic state
  Proxy             same interface, controls access (lazy, remote, checked)
  Command           action object; worth it when you need undo
  State             behavior swapped by replacing a state object
  Template method   non-virtual skeleton, private virtual steps (NVI)
  Visitor           operations grow, types are stable; std::visit if closed
  Singleton         avoid; pass the instance from main
```

## Itanium ABI, the short list

```
  vptr at offset 0 of a polymorphic subobject
  vtable: [offset-to-top][type_info][slot0][slot1]...
          vptr points at slot0; RTTI is vptr[-1]; offset-to-top is vptr[-2]
  virtual destructor occupies two slots: complete (D1) and deleting (D0)
  D2 is the base-object destructor (does not destroy virtual bases)
  delete through a virtual dtor calls the D0 slot of the dynamic type
  key function: first non-inline, non-pure virtual; its TU emits the vtable
  no key function => vague linkage / COMDAT
  "undefined reference to vtable" => that virtual was never defined

  multiple inheritance: one vptr per polymorphic base
  secondary-base override is reached through a this-adjusting thunk
  virtual base: vbase offset in the vtable, loaded at the call
  VTT and construction vtables exist so base ctors see the right offset

  dynamic_cast walks RTTI from the most-derived object (offset-to-top)
  pointer failure => nullptr; reference failure => std::bad_cast
  dynamic_cast<void*> => address of the most-derived object

  pointer-to-data-member: an offset (null is typically -1, not 0)
  pointer-to-member-function: {ptr, adj}, often 16 bytes
    low bit set => virtual, (ptr-1) is a vtable offset

  mangled names: c++filt, nm -C, abi::__cxa_demangle
  extern "C" suppresses mangling
```

## Layout categories

```
  trivial            special members do nothing; no user-provided ones
  trivially copyable memcpy is defined behavior
  standard-layout    no virtuals, no virtual bases, uniform access,
                     at most one class in the hierarchy has data,
                     first member not the same type as a base
  offsetof, C interop, and "first member at 0" require standard-layout
  [[no_unique_address]]  C++20 EBO for a member
  tail-padding reuse on non-standard-layout bases: do not memcpy a base subobject
```

## Exceptions, cost

```
  Itanium zero-cost model: no instructions on the path that does not throw
  tables (.eh_frame) drive unwinding; destructors run as cleanups
  a throw is expensive; a noexcept function lets the compiler skip that setup
```

## Build

```bash
clang++ -std=c++20 -Wall -Wextra -Wnon-virtual-dtor -Woverloaded-virtual \
        -fsanitize=address,undefined file.cpp -o /tmp/demo && /tmp/demo

# C++23 (deducing this):
clang++ -std=c++23 -Wall -Wextra examples/ch14_deducing_this.cpp -o /tmp/demo

./examples/build_all.sh --run
```

## Inspect

```bash
clang++ -Xclang -fdump-record-layouts -c file.cpp
clang++ -Xclang -fdump-vtable-layouts -c file.cpp
nm -C a.out | c++filt
```

Internals: [17](17-internals-object-layout.md) through
[23](23-zero-overhead-and-idioms.md).
Pitfalls with the fixes written out: [15](15-pitfalls-and-best-practices.md).
