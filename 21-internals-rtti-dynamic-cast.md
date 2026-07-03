# 21 — Internals: RTTI, `typeid`, and the `dynamic_cast` Algorithm

**RTTI** (Run-Time Type Information) is how a running program answers "what type
is this object *really*?" It powers `typeid` and `dynamic_cast`. Experts know its
data structures (`std::type_info` and the ABI's `__class_type_info` hierarchy),
the exact algorithm `dynamic_cast` runs (including sideways cross-casts), and its
cost. This chapter covers all three.

Prereq: [18-internals-vtables.md](18-internals-vtables.md), [20-internals-multiple-inheritance.md](20-internals-multiple-inheritance.md).

---

## 1. Where RTTI lives: `vptr[-1]`

Recall the vtable layout (chapter 18): just *above* the function slots sits the
**RTTI pointer** — a pointer to the class's `std::type_info`. That single pointer
is the entry point to all run-time type queries.

```
   Object -> vptr -> vtable:
       [ offset-to-top ]
       [ RTTI ptr ] ---------> std::type_info for the dynamic class
       [ &f0 ] <- vptr
       ...
   typeid(*p) and dynamic_cast both start by reading vptr[-1] (the RTTI ptr).
   => RTTI works ONLY on POLYMORPHIC types (types with a vtable). typeid on a
      non-polymorphic expression is resolved at COMPILE time (static type).
```

---

## 2. The `type_info` and ABI class-info structures

`std::type_info` is the public face; underneath, the Itanium ABI defines a small
hierarchy of type-info classes that encode the inheritance graph so
`dynamic_cast` can walk it.

<!--diagram
title: ABI RTTI class hierarchy (in <cxxabi.h>)
box[blue] __fundamental_type_info, __pointer_type_info, ...
  text: for built-in and pointer types
box[green] std::type_info  (base)
  text: holds the mangled type name; `==` compares identity, `.name()` returns it
  box[teal] __class_type_info
    text: a class with NO bases
  box[orange] __si_class_type_info
    text: "single inheritance": one public, non-virtual base (+ pointer to it)
  box[red] __vmi_class_type_info
    text: "virtual/multiple inheritance": array of bases with offsets + flags
-->
```
   std::type_info                      (name(), operator==, hash_code())
     |
     +-- __class_type_info             (no bases)
     +-- __si_class_type_info          (Single Inheritance: 1 non-virtual public base)
     |       __base_type = &base's type_info
     +-- __vmi_class_type_info         (Virtual/Multiple Inheritance)
             __base_count
             __base_info[]  = { &base type_info, offset/flags } for each base
                              (flags say: public? virtual? and the offset or the
                               vbase-offset index to reach that base)

   These structures literally ENCODE THE CLASS GRAPH in read-only data so the
   runtime can traverse from a derived type to any base at run time.
```

```
   std::type_info key facts:
     * comparing types: use '==' (or before() for ordering) — NOT name() strings.
     * .name() is IMPLEMENTATION-DEFINED (often the mangled name; demangle with
       abi::__cxa_demangle).
     * type_info objects are unique per type per program (usually), so '==' is a
       cheap pointer compare in practice (the standard requires value equality).
     * hash_code() enables using type_index in unordered_map (type-erased maps).
```

---

## 3. `typeid` — two modes

```cpp
Base* p = get();
typeid(*p)        // POLYMORPHIC operand -> RUNTIME: reads vptr[-1] -> dynamic type
typeid(Base)      // a TYPE -> COMPILE time
typeid(p)         // p is a Base* (not dereferenced) -> static type Base*, compile time
int x;
typeid(x)         // non-polymorphic -> compile time (static type int)
```

```
   GOTCHA: typeid(*p) where p is null and *p is polymorphic THROWS
   std::bad_typeid. And typeid(*p) evaluates its operand only when needed to get
   the dynamic type (for polymorphic lvalues); for non-polymorphic operands it
   does NOT evaluate side effects.
```

Runnable: [`examples/ch21_rtti.cpp`](examples/ch21_rtti.cpp) demonstrates
`typeid`, demangling, and casts.

---

## 4. The `dynamic_cast` algorithm

`dynamic_cast<Derived*>(base_ptr)` is implemented by the ABI runtime function
`__dynamic_cast(src_ptr, src_type_info, dst_type_info, src2dst_offset)`. Here's
what it actually does:

```
   __dynamic_cast steps (conceptually):
     1. If src is null -> return null.
     2. Read the object's dynamic type via src's vptr[-1] (RTTI).
        Use offset-to-top (vptr[-2]) to find the MOST-DERIVED object address.
     3. Walk the dynamic type's RTTI base graph (__vmi/__si_class_type_info),
        computing offsets (including virtual-base offsets), searching for a
        subobject whose type_info == dst_type_info reachable via PUBLIC bases.
     4. If a unique, unambiguous public path is found -> return the adjusted
        pointer (most-derived-addr + computed offset).
     5. Otherwise -> return null (for pointers) or throw std::bad_cast (refs).
```

<!--diagram
title: dynamic_cast<Dst*>(src)
box[blue] 1. read dynamic type
  text: `src->vptr[-1]` (type_info) + `vptr[-2]` (offset-to-top) -> most-derived object
box[green] 2. search the base graph
  text: walk RTTI bases for a **public**, unambiguous subobject of type Dst
box[orange] 3a. found
  text: return most-derived addr + computed (possibly virtual) offset
box[red] 3b. not found / ambiguous
  text: pointer -> **nullptr**;  reference -> throw **std::bad_cast**
-->
```
   Downcast (Base* -> Derived*):  common case; verifies the object really is a
                                  Derived (or has one as a base) at run time.
   Cross-cast (A* -> B*) across a MULTIPLE-inheritance diamond: dynamic_cast can
   go SIDEWAYS between sibling bases of the same complete object — static_cast
   CANNOT do this. Only dynamic_cast walks the full graph from the most-derived.
   dynamic_cast<void*>(p): returns the address of the MOST-DERIVED object
                           (uses offset-to-top). Handy to get the "real" pointer.
```

```cpp
struct A { virtual ~A(); };
struct B { virtual ~B(); };
struct D : A, B {};

A* a = new D();
B* b = dynamic_cast<B*>(a);   // CROSS-CAST A->B via the shared D. Works!
                              // static_cast<B*>(a) would be ill-formed (unrelated).
void* top = dynamic_cast<void*>(a);   // address of the whole D
```

---

## 5. Cost and when to avoid it

```
   dynamic_cast cost:
     * a call to __dynamic_cast (not inlined) that WALKS the RTTI graph.
     * cost grows with hierarchy depth/width; far pricier than a virtual call.
     * downcast to a leaf is cheaper than a general cross-cast.

   Prefer, in order:
     1. A VIRTUAL FUNCTION (let the object tell you what to do) — no cast at all.
     2. A visitor / std::variant (closed set) — compile-time dispatch.
     3. dynamic_cast only when you genuinely must recover a specific derived type
        from a base reference you don't control (e.g. plugin boundaries).
   Frequent dynamic_cast in hot code is a design smell (often a missing virtual).
```

---

## 6. Disabling RTTI (`-fno-rtti`) and its consequences

```
   Compiling with -fno-rtti:
     * removes type_info emission -> smaller binaries.
     * DISABLES dynamic_cast (to polymorphic types) and typeid on polymorphic
       lvalues -> compile error / UB if used.
     * common in embedded / game engines / LLVM itself (which uses its own
       lightweight RTTI: llvm::isa<>/dyn_cast<> built on a manual 'classof' tag).

   If you find yourself wanting -fno-rtti but still needing type queries, that's
   the sign to build a lightweight tagged hierarchy (an enum 'kind' + isa<>)
   instead of relying on the language's RTTI.
```

---

## 7. `std::type_index` — using types as map keys

```cpp
#include <typeindex>
#include <unordered_map>

std::unordered_map<std::type_index, std::string> names;
names[std::type_index(typeid(int))]    = "int";
names[std::type_index(typeid(double))] = "double";
// type_index wraps type_info with copyability + hashing so it works as a key.
```

```
   This is the backbone of type-erased registries, serializers, and
   "component maps" (ECS): map from a runtime type to a handler/factory.
```

---

## 8. Summary

```
 +-------------------------------------------------------------------+
 | RTTI ptr lives at vptr[-1]; works ONLY on polymorphic types.      |
 |   typeid on non-polymorphic / a type = compile-time (static).     |
 | The ABI encodes the class graph in type_info subclasses           |
 |   (__class_/__si_class_/__vmi_class_type_info) so the runtime can |
 |   traverse bases + offsets.                                       |
 | dynamic_cast -> __dynamic_cast: find most-derived (offset-to-top),|
 |   walk PUBLIC base graph for the target subobject; adjust pointer.|
 |   Supports downcasts AND cross-casts; <void*> yields most-derived.|
 |   Fail: nullptr (ptr) / std::bad_cast (ref).                      |
 | Cost: a graph walk, not inlined -> prefer virtuals/variant.       |
 | -fno-rtti removes it (engines roll their own isa<>/dyn_cast).     |
 | type_index -> types as hash-map keys (type-erased registries).    |
 +-------------------------------------------------------------------+
```

Next: [22-internals-mangling-ptm.md](22-internals-mangling-ptm.md).
