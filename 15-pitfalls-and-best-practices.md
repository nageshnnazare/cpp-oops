# 15 — Pitfalls & Best Practices

A field guide to the OOP bugs that bite everyone, and the habits that prevent
them. Most of these have appeared earlier; this is the consolidated checklist.

---

## 1. Missing virtual destructor (leak / UB)

```cpp
struct Base { ~Base(); };                 // NON-virtual dtor
struct Derived : Base { std::vector<int> big; ~Derived(); };
Base* p = new Derived();
delete p;                                 // only ~Base runs -> ~Derived skipped -> UB/leak
```

```
   RULE: a class used polymorphically (deleted via Base*) MUST have a virtual
   destructor. If a class has ANY virtual function, give it a virtual dtor.
   Alternatively: make the base dtor protected+non-virtual to FORBID
   delete-through-base (used when you never delete polymorphically).
```

---

## 2. Object slicing

```cpp
std::vector<Animal> zoo;
zoo.push_back(Dog{});          // sliced to Animal — Dog part lost
Animal a = someDog;            // sliced copy
void feed(Animal a);           // by VALUE -> slices any derived argument
feed(someDog);                 // Dog-ness gone inside feed
```

```
   Slicing happens whenever a derived object is copied into a BASE VALUE.
   FIX: pass/store by reference or pointer for polymorphism:
     void feed(const Animal& a);                    // no slice
     std::vector<std::unique_ptr<Animal>> zoo;      // no slice
```

---

## 3. Calling virtual functions in constructors/destructors

```cpp
struct Base { Base(){ setup(); } virtual void setup(); };   // calls Base::setup!
```

```
   During Base's ctor/dtor the object is (still/again) only a Base, so virtual
   calls resolve to Base's version, NOT the derived override. Avoid virtual calls
   in ctors/dtors, or use a separate init() called after construction.
```

---

## 4. Forgetting the Rule of Three/Five (shallow copy)

```cpp
class Buf { int* p_; public: Buf(int n):p_(new int[n]){} ~Buf(){ delete[] p_; } };
Buf a(10);
Buf b = a;    // default copy = shallow -> a.p_ == b.p_ -> DOUBLE FREE at scope end
```

```
   If you write a destructor that frees a resource, you MUST also handle copy
   (and move). Better: use RAII members (unique_ptr/vector) -> Rule of Zero,
   the compiler does it correctly. (Chapter 04.)
```

---

## 5. Self-assignment and self-move

```cpp
T& operator=(const T& o) {
    delete p_;                  // if &o == this, you just freed the source!
    p_ = new U(*o.p_);          // ...then read freed memory -> UB
    return *this;
}
```

```
   Guard with 'if (this != &o)' OR use copy-and-swap (chapter 04 §7), which is
   inherently self-assignment safe and exception safe.
```

---

## 6. Returning references/pointers to locals or destroyed members

```cpp
const std::string& name() const { return std::string("x"); }  // DANGLING: temp dies
int& at(int i) { int local = data_[i]; return local; }        // DANGLING: local dies
```

```
   Never return a reference/pointer to a local or a temporary. Return by value,
   or reference a member that outlives the call. Watch dangling references from
   'auto& x = obj.method_returning_temporary();'.
```

---

## 7. Inheriting to reuse (wrong is-a) & LSP violations

```cpp
class Stack : public std::vector<int> {};   // Stack is NOT a vector -> leaks API
class Square : public Rectangle {};         // breaks setWidth/setHeight contract
```

```
   Use inheritance for genuine IS-A + substitutability, not for code reuse.
   Prefer composition (chapter 10). If a subclass must throw "unsupported" or
   weaken a base guarantee, the hierarchy is wrong.
```

---

## 8. Fat interfaces & god classes

```
   * A class with 30 methods and 15 members doing unrelated jobs -> split (SRP).
   * An interface forcing implementers to stub/throw unused methods -> segregate
     (ISP). (Chapter 12.)
```

---

## 9. Overusing inheritance depth

```
   Deep hierarchies (A->B->C->D->E) are rigid and hard to reason about. Each
   level couples to all ancestors (fragile base class). Prefer shallow trees +
   composition. "Prefer flat + compose" over "deep + inherit."
```

---

## 10. Public data members / leaking internals

```cpp
class Account { public: double balance; };   // no invariant protection
```

```
   Keep data private; expose behavior. A struct of public data is fine for
   PLAIN data with no invariants, but a class with rules must guard them
   (chapter 03). Don't return non-const references to private members that let
   callers bypass your invariants.
```

---

## 11. `explicit` omitted -> surprising conversions

```cpp
class Timer { public: Timer(int seconds); };   // implicit
void wait(Timer);
wait(5);                                        // 5 silently -> Timer(5). Surprise!
```

```
   Mark single-argument constructors 'explicit' unless implicit conversion is
   genuinely wanted (chapter 02). Same for conversion operators (chapter 09).
```

---

## 12. `shared_ptr` cycles (memory leak despite RAII)

```cpp
struct Node { std::shared_ptr<Node> next; std::shared_ptr<Node> prev; };
// a->next = b; b->prev = a;  -> refcounts never reach 0 -> LEAK
```

```
   Two shared_ptrs pointing at each other keep each other alive forever.
   FIX: make one direction a weak_ptr (typically the "back"/parent pointer):
     struct Node { std::shared_ptr<Node> next; std::weak_ptr<Node> prev; };
```

---

## 13. Overusing `shared_ptr` (when `unique_ptr` suffices)

```
   shared_ptr has overhead (atomic refcount, control block) and blurs ownership
   ("who owns this?"). Default to unique_ptr (single clear owner); reach for
   shared_ptr only when ownership is genuinely shared. Pass raw T*/T& for
   non-owning use.
```

---

## 14. Best-practices checklist

```
   CLASS DESIGN
   [ ] Keep data private; expose behavior, protect invariants (ch 03).
   [ ] const-correct every non-mutating method.
   [ ] Single-arg ctors 'explicit'; single responsibility per class.
   [ ] Rule of Zero (RAII members). If not, handle all of Rule of Five.
   [ ] noexcept move operations.

   INHERITANCE / POLYMORPHISM
   [ ] Public inheritance only for real is-a + substitutability (LSP).
   [ ] Virtual destructor on polymorphic bases.
   [ ] 'override' on every override; 'final' where appropriate.
   [ ] No virtual calls in ctors/dtors.
   [ ] Prefer composition over inheritance for reuse.

   RESOURCES / MODERN
   [ ] Smart pointers, never raw new/delete; unique_ptr by default.
   [ ] Break shared_ptr cycles with weak_ptr.
   [ ] Return by value (move/RVO); sink params by value + std::move.
   [ ] Closed set of types -> std::variant; open -> virtual.
   [ ] C++20 <=> for comparisons; program to interfaces (DIP).

   TESTING / QUALITY
   [ ] Depend on abstractions so you can inject mocks (DIP).
   [ ] Build with -Wall -Wextra; run with sanitizers (asan/ubsan).
```

---

## 15. Tooling that catches OOP bugs

```
   -Wall -Wextra                 : warns on -Wreorder, hidden overrides, etc.
   -Wnon-virtual-dtor            : flags missing virtual destructors
   -fsanitize=address            : use-after-free, double-free, leaks (slicing
                                   fallout), buffer overruns
   -fsanitize=undefined          : UB (bad casts, null deref, etc.)
   clang-tidy                    : modernize-*, cppcoreguidelines-* checks
   -Weffc++                      : Meyers-style class hygiene warnings (noisy)
```

```bash
clang++ -std=c++20 -Wall -Wextra -Wnon-virtual-dtor -fsanitize=address,undefined \
        prog.cpp -o prog && ./prog
```

---

## 16. Summary

<!--diagram
title: Pitfalls & best practices
box[red] Top OOP bugs
  text: missing **virtual dtor**, **slicing**, **virtual-in-ctor**, shallow copy (rule of 3/5), self-assignment, dangling returns, wrong **is-a / LSP** breaks, `shared_ptr` cycles, missing `explicit`
box[green] Habits that prevent them
  text: private data + behavior, **const-correct**, **Rule of Zero**, virtual dtor + `override`, **composition over inheritance**, smart pointers (`unique_ptr` by default), program to interfaces, use warnings + sanitizers
-->
```
 +-------------------------------------------------------------------+
 | Top OOP bugs: missing virtual dtor, slicing, virtual-in-ctor,     |
 |   shallow copy (rule of 3/5), self-assignment, dangling returns,  |
 |   wrong is-a / LSP breaks, shared_ptr cycles, missing 'explicit'. |
 |                                                                   |
 | Habits: private data + behavior, const-correct, Rule of Zero,     |
 |   virtual dtor + override, composition over inheritance, smart    |
 |   pointers (unique by default), program to interfaces, use        |
 |   warnings + sanitizers.                                          |
 +-------------------------------------------------------------------+
```

Next: [16-cheatsheet.md](16-cheatsheet.md).
