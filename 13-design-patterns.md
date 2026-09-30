# 13 — Design Patterns in Modern C++

A pattern is a name for a shape people keep reinventing, plus the forces that
make that shape a good idea and the forces that make it a bad one. The name
is the useful part: "this is a strategy" communicates more than "this is a
pointer to an abstract class with one method." The class diagram is not a
goal.

Modern C++ deletes some of the ceremony the Gang of Four needed in 1994.
`std::function` is a strategy. `std::variant` is a closed visitor. A
function-local static is a singleton, and chapter 11 already argued you
probably do not want one. This chapter shows the shape in C++, then the
smaller construct that often replaces it.

Prereqs: chapters 06–12.

```
   Creational     how objects come into existence
                  Factory, Abstract Factory, Builder, Prototype, Singleton
   Structural     how objects are composed
                  Adapter, Bridge, Composite, Decorator, Facade, Flyweight, Proxy
   Behavioral     how objects cooperate
                  Strategy, Observer, Command, State, Template Method,
                  Chain of Responsibility, Iterator, Visitor
```

---

## 1. Strategy

The varying algorithm is an object. The context calls it. Replacing the
object replaces the algorithm, including at run time.

```cpp
struct Compression {
    virtual std::vector<std::byte> compress(std::span<const std::byte>) const = 0;
    virtual ~Compression() = default;
};

class Archiver {
public:
    explicit Archiver(std::unique_ptr<Compression> c) : compression_(std::move(c)) {}
    void set(std::unique_ptr<Compression> c) { compression_ = std::move(c); }
    std::vector<std::byte> archive(std::span<const std::byte> in) const {
        return compression_->compress(in);
    }
private:
    std::unique_ptr<Compression> compression_;
};
```

When the strategy is a single function and you do not need a hierarchy,
`std::function` is the pattern without the base class:

```cpp
class Archiver {
public:
    using Fn = std::function<std::vector<std::byte>(std::span<const std::byte>)>;
    explicit Archiver(Fn fn) : fn_(std::move(fn)) {}
    std::vector<std::byte> archive(std::span<const std::byte> in) const { return fn_(in); }
private:
    Fn fn_;
};
```

A template parameter is the same idea with the choice fixed at compile time
and no indirect call (chapter 10). Use the virtual or `std::function` form
when the choice is runtime data. `std::function` allocates if the callable
does not fit in its small buffer, and it is not callable if it is empty.
Check that, or require the function in the constructor and never store an
empty one.

---

## 2. Observer

A subject holds a list of observers and notifies them when something
changes. The hard parts are lifetime and reentrancy, not the loop.

```cpp
struct Observer {
    virtual void on_update(int value) = 0;
    virtual ~Observer() = default;
};

class Subject {
public:
    void subscribe(Observer* o) { observers_.push_back(o); }
    void unsubscribe(Observer* o) {
        std::erase(observers_, o);             // C++20
    }
    void set_state(int v) {
        state_ = v;
        // copy: a callback may unsubscribe and mutate observers_
        auto snapshot = observers_;
        for (Observer* o : snapshot) o->on_update(state_);
    }
private:
    std::vector<Observer*> observers_;
    int state_ = 0;
};
```

`observers_` does not own the observers. Each observer must `unsubscribe` in
its destructor, or the next `set_state` is a use-after-free. A
`std::weak_ptr` list is the alternative when observers are already owned by
`shared_ptr`: lock each `weak_ptr`, skip the ones that have expired, and you
do not need an explicit unsubscribe to be memory-safe. You still want
unsubscribe so a live observer can detach.

Notifying from a snapshot matters. If `on_update` calls `unsubscribe` and
erases from the vector you are iterating, the loop is undefined. Copy the
list first, or use indices and accept that newly added observers may or may
not see this event. Document which.

Do not hold `shared_ptr<Observer>` in the subject and `shared_ptr<Subject>`
in the observer. That cycle never reaches a zero refcount (chapter 15).

Runnable: [`examples/ch13_observer.cpp`](examples/ch13_observer.cpp).

---

## 3. Factory method, simple factory, abstract factory

These three names get collapsed into "a function that returns a base
pointer." They are different.

**Simple factory.** One function selects a concrete type. The caller does
not name the concrete class. The function is the only place that does.

```cpp
enum class Kind { Circle, Rectangle };

std::unique_ptr<Shape> make_shape(Kind kind, double a, double b) {
    switch (kind) {
    case Kind::Circle:    return std::make_unique<Circle>(a);
    case Kind::Rectangle: return std::make_unique<Rectangle>(a, b);
    }
    return nullptr;
}
```

Adding a kind edits `make_shape`. That is acceptable when the set is closed
and local. Return `unique_ptr`. Return `nullptr` or `std::expected` for an
unknown kind; do not throw a string.

**Factory method.** The factory is virtual. A base defines the call, a
derived class decides what to construct. Use it when the surrounding
algorithm is shared and the product type is the variation.

```cpp
class App {
public:
    void run() { auto doc = create_document(); doc->open(); }
    virtual ~App() = default;
protected:
    virtual std::unique_ptr<Document> create_document() const = 0;
};
class DrawingApp : public App {
    std::unique_ptr<Document> create_document() const override {
        return std::make_unique<Drawing>();
    }
};
```

**Abstract factory.** A family of products that have to match. A GUI theme
produces a button and a checkbox that belong together. The factory interface
has one method per product, and each concrete factory implements the whole
family.

```cpp
struct WidgetFactory {
    virtual std::unique_ptr<Button> make_button() const = 0;
    virtual std::unique_ptr<Checkbox> make_checkbox() const = 0;
    virtual ~WidgetFactory() = default;
};
```

If the products do not need to match, you do not have an abstract factory.
You have two simple factories. The pattern earns its keep when a mismatched
pair (a dark button and a light checkbox) is a bug the types should prevent.

---

## 4. Builder

Use a builder when a constructor would need a long parameter list of
optional pieces, and those pieces have an order or a validation step at the
end.

```cpp
class HttpRequest {
public:
    class Builder {
    public:
        Builder& url(std::string u) { req_.url_ = std::move(u); return *this; }
        Builder& method(std::string m) { req_.method_ = std::move(m); return *this; }
        Builder& header(std::string k, std::string v) {
            req_.headers_.emplace_back(std::move(k), std::move(v));
            return *this;
        }
        HttpRequest build() {
            if (req_.url_.empty()) throw std::invalid_argument("url");
            return std::move(req_);
        }
    private:
        HttpRequest req_;
    };

private:
    friend class Builder;
    std::string url_;
    std::string method_ = "GET";
    std::vector<std::pair<std::string, std::string>> headers_;
};
```

`build()` is where the invariant is checked, so a half-built request is not
observable as an `HttpRequest`. For a plain bundle of fields with no
invariant, an aggregate and designated initializers (chapter 01) replace the
builder. Do not write a builder for three required arguments.

Ref-qualify the chained setters with `&` if you want to forbid
`Builder{}.url("x").build()` from being misused through a moved-from
builder. Returning `Builder&` from a setter on a temporary is a common
fluent-interface dangling trap only if the setter returns a reference to a
member of a temporary that the caller stores. `build()` returns by value, so
the usual chain is safe. Storing `auto& b = Builder{}.url("x");` is not.

---

## 5. Prototype

A prototype builds a new object by copying an existing one, when the product
is easier to clone than to construct from parameters, or when the concrete
type is known only to the object itself.

```cpp
class Shape {
public:
    virtual std::unique_ptr<Shape> clone() const = 0;
    virtual ~Shape() = default;
protected:
    Shape() = default;
    Shape(const Shape&) = default;
};
```

Chapter 04 covers why the copy operations are protected and why the return
type is `unique_ptr<Shape>` rather than a covariant raw pointer. A registry
of prototypes (`map<string, unique_ptr<Shape>>` of templates you clone) is
the creational form. It is also a factory whose set of products can grow
without a `switch`, as long as someone registers a prototype.

---

## 6. Singleton

Chapter 11 gives the thread-safe implementation and the reasons it hurts.
The short version: a singleton hides a dependency, complicates tests, and
drags static destruction order behind it. Pass the object in. If the
resource truly is unique (one hardware device), a single instance created in
`main` and passed down is still not a globally reachable `instance()`.

---

## 7. Adapter

An adapter presents one interface in front of a type that has a different
interface. The client depends on the interface you wish you had.

```cpp
struct Logger {
    virtual void log(const std::string&) = 0;
    virtual ~Logger() = default;
};

class ThirdParty {
public:
    void write_message(const char* s, int level);
};

class ThirdPartyLogger : public Logger {
public:
    explicit ThirdPartyLogger(ThirdParty& lib) : lib_(lib) {}
    void log(const std::string& s) override { lib_.write_message(s.c_str(), 1); }
private:
    ThirdParty& lib_;
};
```

The adapter owns nothing here; it refers to the library object. If
conversion is a single function, a free function is the adapter and a class
is unnecessary. Write the class when the adapted object has to be stored and
passed around as the interface.

---

## 8. Bridge

The abstraction and the implementation vary independently, so each is its
own hierarchy, and the abstraction owns an implementation pointer. Chapter
10 has the `Shape` / `Renderer` sketch. Use a bridge when both sides have
more than one variant. One abstraction and one implementation is a member,
not a bridge.

---

## 9. Composite

A composite lets clients treat a single object and a group of objects
through the same interface. The group stores children and forwards.

```cpp
struct Graphic {
    virtual void draw() const = 0;
    virtual ~Graphic() = default;
};

class Group : public Graphic {
public:
    void add(std::unique_ptr<Graphic> g) { children_.push_back(std::move(g)); }
    void draw() const override {
        for (const auto& c : children_) c->draw();
    }
private:
    std::vector<std::unique_ptr<Graphic>> children_;
};
```

`Group` is a `Graphic`, so a group may contain groups. That is the pattern.
The ownership tree is `unique_ptr`, so there is no cycle unless you add a
raw parent pointer and someone also owns upward. A parent pointer, if you
need one, is non-owning.

Do not put `add` on `Graphic`. Leaves cannot accept children, and a pure
virtual `add` that throws is an ISP and LSP failure. `add` belongs on
`Group`. Callers who have a `Graphic&` and want to add children have already
decided they need a group; downcast at the boundary where the tree is built,
or build the tree in code that sees `Group`.

---

## 10. Decorator

A decorator wraps an object with the same interface and adds behavior. The
wrap is a runtime combination, so you do not need a class per combination.

```cpp
struct DataSource {
    virtual std::string read() = 0;
    virtual ~DataSource() = default;
};

class FileSource : public DataSource {
public:
    std::string read() override { return "raw"; }
};

class Prefix : public DataSource {
public:
    Prefix(std::string p, std::unique_ptr<DataSource> inner)
        : prefix_(std::move(p)), inner_(std::move(inner)) {}
    std::string read() override { return prefix_ + inner_->read(); }
private:
    std::string prefix_;
    std::unique_ptr<DataSource> inner_;
};
```

```cpp
auto src = std::make_unique<Prefix>(
    "zip:", std::make_unique<Prefix>("enc:", std::make_unique<FileSource>()));
// src->read() == "zip:enc:raw"
```

Each decorator owns the next. Destruction unwinds the chain. A decorator
that does not own (a reference to the inner source) must not outlive it.

Runnable: [`examples/ch13_decorator.cpp`](examples/ch13_decorator.cpp).

---

## 11. Facade

A facade is a narrow interface in front of a cluster of types. `compile()`
on a `Toolchain` object may call the preprocessor, the compiler, and the
linker, and the caller never sees those types. There is no new mechanism
here. It is SRP at the boundary: one type whose job is "the simple way in."
Write one when the subsystem's real API is larger than the clients should
know. Do not write one that only renames a single function.

---

## 12. Flyweight

Share immutable intrinsic state among many objects. A forest of trees stores
a position per tree and a pointer to a shared `TreeModel` (mesh, texture)
owned by a factory that interns models by name.

```cpp
class TreeModel;   // heavy, immutable, shared

class Tree {
public:
    Tree(float x, float y, std::shared_ptr<const TreeModel> model)
        : x_(x), y_(y), model_(std::move(model)) {}
private:
    float x_, y_;
    std::shared_ptr<const TreeModel> model_;
};
```

`shared_ptr<const TreeModel>` is appropriate because the model really is
shared and immutable. A flyweight that is mutated through one tree is a data
race and a logic bug. If the models live for the whole process, a
non-owning pointer into a factory-owned table is enough and cheaper.

---

## 13. Proxy

A proxy has the same interface as the real object and controls access to it.
The usual kinds:

```
   lazy proxy       constructs the real object on the first call
   protection proxy checks a permission before forwarding
   remote proxy     forwards the call across a process boundary
```

```cpp
class Image {
public:
    virtual void draw() = 0;
    virtual ~Image() = default;
};

class LazyImage : public Image {
public:
    explicit LazyImage(std::string path) : path_(std::move(path)) {}
    void draw() override {
        if (!real_) real_ = std::make_unique<FileImage>(path_);
        real_->draw();
    }
private:
    std::string path_;
    std::unique_ptr<Image> real_;
};
```

`unique_ptr` is already a lazy-or-empty owner. A proxy class is justified
when the surrounding code must see an `Image&` before the file is loaded.
If the caller can hold a `unique_ptr<FileImage>` and construct it when
needed, you do not need `LazyImage`.

---

## 14. Chain of responsibility

A handler tries to process a request or passes it to the next handler. Each
handler knows one successor, not the whole chain.

```cpp
class Handler {
public:
    explicit Handler(std::unique_ptr<Handler> next = nullptr) : next_(std::move(next)) {}
    virtual ~Handler() = default;

    void handle(Request& r) {
        if (!try_handle(r) && next_) next_->handle(r);
    }
private:
    virtual bool try_handle(Request& r) = 0;
    std::unique_ptr<Handler> next_;
};
```

Build the chain from the inside out, or store handlers in a `vector` and
walk it. The vector is easier to reconfigure and does not pretend each node
owns the rest of the pipeline. Use the linked form when handlers are
genuinely nested. Stop at the first handler that returns true, and define
what happens if nobody handles the request.

---

## 15. Command

A command is an object that represents an action. Because it is an object,
you can store it, queue it, log it, and undo it.

```cpp
struct Command {
    virtual void execute() = 0;
    virtual void undo() = 0;
    virtual ~Command() = default;
};

class AddText : public Command {
public:
    AddText(Document& doc, std::string text) : doc_(doc), text_(std::move(text)) {}
    void execute() override { doc_.append(text_); }
    void undo() override { doc_.erase_suffix(text_.size()); }
private:
    Document& doc_;
    std::string text_;
};

class History {
public:
    void run(std::unique_ptr<Command> c) {
        c->execute();
        done_.push_back(std::move(c));
        undone_.clear();
    }
    void undo() {
        if (done_.empty()) return;
        done_.back()->undo();
        undone_.push_back(std::move(done_.back()));
        done_.pop_back();
    }
private:
    std::vector<std::unique_ptr<Command>> done_, undone_;
};
```

The command refers to the document; it must not outlive it. If commands are
queued onto another thread, the document reference is a race unless the
queue is the only caller. A command that does not need undo is a
`std::function<void()>`. Use the virtual command when undo, redo, or
serialization of the action matters.

---

## 16. State

An object changes behavior by changing the state object it holds, instead of
by a `switch` on an enum in every method.

```cpp
class Connection {
public:
    void open();
    void send(std::string_view);
    void close();
private:
    struct State {
        virtual void open(Connection&) = 0;
        virtual void send(Connection&, std::string_view) = 0;
        virtual void close(Connection&) = 0;
        virtual ~State() = default;
    };
    struct Closed : State { /* open() moves the connection to Connected */ };
    struct Connected : State { /* send() is legal; close() moves to Closed */ };

    std::unique_ptr<State> state_ = std::make_unique<Closed>();
    friend struct Closed;
    friend struct Connected;
};
```

Transitions happen by replacing `state_`. Illegal operations live in the
state that rejects them, so `Connected::open` can refuse a second open
without a flag on `Connection`.

For a small fixed set of states, an enum and a `switch`, or a
`std::variant` of state types, is less machinery and the compiler checks
exhaustiveness. Use the virtual state pattern when states are numerous or
supplied from outside. Do not create a class hierarchy to replace a
three-way enum.

---

## 17. Template method

The base defines the steps in order. Derived classes override the steps.
Chapter 07's non-virtual interface is this pattern with the public function
non-virtual and the hooks private:

```cpp
void Game::play() {        // non-virtual
    initialize();
    start_play();
    end_play();
}
```

The order is the base's invariant. Derived classes do not reorder it, because
they cannot override `play`. That is the difference between a template method
and a pile of virtual functions the caller has to remember to sequence.

---

## 18. Iterator

An iterator walks a structure without exposing the structure. C++ already
has this as a core abstraction: `begin` / `end`, `operator++`, `operator*`,
and the range-for loop. A hand-rolled iterator type is justified when you
are writing a container. It is not justified to walk a `vector` you could
return a `span` into.

When you do write one, match the standard's expectations or generic code
will not work: the right nested `iterator_category` (or, in C++20, the right
concepts), `value_type`, and a real end sentinel. Chapter 11's nested
`Iterator` is the minimal shape. Prefer `std::ranges` algorithms over a
custom iterator that only exists to be passed to your own for-loop.

---

## 19. Visitor and `std::visit`

Classic visitor: the element hierarchy is stable, and the set of operations
grows. Each element has an `accept(Visitor&)` that calls the visitor's
overload for that element. Adding an operation is a new visitor. Adding an
element touches every visitor. That trade is the opposite of a virtual
function, where adding an element is a new class and adding an operation
touches the base.

```cpp
struct Circle;
struct Square;
struct Visitor {
    virtual void visit(Circle&) = 0;
    virtual void visit(Square&) = 0;
    virtual ~Visitor() = default;
};
struct Shape {
    virtual void accept(Visitor&) = 0;
    virtual ~Shape() = default;
};
```

For a closed set, `std::variant` and `std::visit` are this double dispatch
without the `accept` boilerplate, and a missed alternative does not compile:

```cpp
using Shape = std::variant<Circle, Square>;

struct Area {
    double operator()(const Circle& c) const { return 3.141592653589793 * c.r * c.r; }
    double operator()(const Square& s) const { return s.side * s.side; }
};

double area(const Shape& s) { return std::visit(Area{}, s); }
```

The overload set can be a generic lambda when every alternative has
`area()`:

```cpp
double area(const Shape& s) {
    return std::visit([](const auto& x) { return x.area(); }, s);
}
```

Use the classic visitor when the element types are an open polymorphic
hierarchy you do not control and the operations are external. Use
`std::visit` when you own the set of types. Chapter 14 goes further on
`variant`.

---

## 20. Choosing, and not choosing

```
   need                                      prefer
   ---------------------------------------   ---------------------------------
   swap an algorithm at run time             strategy, often std::function
   notify a set of listeners                 observer, with a lifetime story
   construct without naming the concrete     simple factory returning unique_ptr
   a matching family of products             abstract factory
   many optional construction steps          builder, or an aggregate
   copy an object of unknown dynamic type    virtual clone
   one process-lifetime instance             an object in main, passed down
   a type that speaks the wrong interface    adapter (or a free function)
   single object and a group, same API       composite, add() only on the group
   wrap behavior in combinations             decorator
   a simple door on a large subsystem        facade
   shared immutable bulk data                flyweight
   delay or gate access                      proxy
   a pipeline of optional handlers           chain, often a vector of handlers
   undoable actions                          command
   behavior that changes by mode             state, or an enum if the set is small
   a fixed algorithm, variable steps         template method / NVI
   an operation over a closed set of types   std::visit
```

If the description of the pattern is longer than the code it replaces, write
the code. Patterns are a vocabulary for designs you already have, not a
checklist to implement before the problem shows up.

---

## 21. Exercises

1. A subject stores `shared_ptr<Observer>` and each observer stores
   `shared_ptr<Subject>`. Why does neither destructor run?
2. Why does `add()` not belong on the `Graphic` base of a composite?
3. You have three document kinds, all in this binary, and two operations,
   `export_pdf` and `spellcheck`, that will keep growing. Which pattern fits
   better than a virtual function per operation, and what is the modern
   spelling?
4. `Builder::build()` throws if `url` is empty. Why is that check in
   `build()` rather than in the `HttpRequest` constructor that `Builder`
   bypasses by being a friend?

### Answers

1. Each `shared_ptr` keeps the other's refcount above zero. It is a cycle.
   One side must be a `weak_ptr` or a raw observer pointer with an explicit
   unsubscribe.
2. Leaves have no children. A pure virtual `add` forces them to throw or
   no-op, which breaks substitutability. Callers with a `Graphic&` should
   draw, not restructure the tree.
3. Visitor. The element set is stable and closed; the operations grow. In
   modern C++ that is `using Doc = std::variant<...>`, and each operation is
   a `std::visit`. A new operation is a new visitor and does not edit
   `Doc`. A new document kind edits every visitor, which is the trade you
   accepted.
4. It can be in both. The important part is that no `HttpRequest` becomes
   observable with an empty url. `build()` is the function that produces one.
   If `HttpRequest`'s constructors are private and `build()` is the only
   caller, the check has one site. A public constructor that also checks is
   better if any other friend appears later. One gate is the point.

---

## 22. Summary

<!--diagram
title: Design patterns
box[green] Key points
  text: A pattern is a name for a recurring shape. Prefer the smallest C++ feature that has that shape
  text: Strategy is often std::function. Observer needs a lifetime rule and a snapshot during notify. Factories return unique_ptr
  text: Composite puts add() on the group, not on the leaf interface. Decorator wraps by ownership. Command is for undo, not for every callback
  text: Visitor fits a stable set of types and a growing set of operations. std::visit is that pattern for a closed set
  text: Singleton, facade-of-one-function, and a class hierarchy for a three-value enum are the usual overreaches
-->
```
 +------------------------------------------------------------------+
 | Name the shape, then use the smallest feature that implements it.|
 | Strategy: unique_ptr to an interface, or std::function.          |
 | Observer: non-owning or weak, unsubscribe, snapshot on notify.   |
 | Factories return unique_ptr. Abstract factory = a matching family.|
 | Builder when construction has optional parts and a final check.  |
 | Composite: children only on the group. Decorator owns the inner. |
 | Command when you need undo. State when modes are heavy; else enum.|
 | Visitor / std::visit when operations grow faster than types.     |
 +------------------------------------------------------------------+
```

Next: [14-modern-cpp-oop.md](14-modern-cpp-oop.md).
