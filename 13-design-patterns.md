# 13 — Design Patterns in Modern C++

Design patterns are named, reusable solutions to recurring design problems. This
chapter surveys the most useful GoF patterns with compact, modern C++ examples
and diagrams. The goal is *recognition* (you'll see these everywhere) and
*judgment* (when each helps — and its modern alternative).

Prereqs: chapters 06–12.

<!--diagram
title: GoF pattern families
box[blue] Creational
  text: how objects are created (Factory, Builder, Singleton, Prototype)
box[teal] Structural
  text: how objects compose (Adapter, Decorator, Composite, Facade, Proxy, Bridge, Flyweight)
box[purple] Behavioral
  text: how objects interact (Strategy, Observer, Command, State, Template Method, Iterator, Visitor, …)
-->
```
   Three families (Gang of Four):
     CREATIONAL : how objects are created  (Factory, Builder, Singleton, Prototype)
     STRUCTURAL : how objects compose      (Adapter, Decorator, Composite, Facade,
                                            Proxy, Bridge, Flyweight)
     BEHAVIORAL : how objects interact     (Strategy, Observer, Command, State,
                                            Template Method, Iterator, Visitor, ...)
```

---

## 1. Strategy (behavioral) — swap an algorithm

Encapsulate interchangeable behaviors behind an interface; select at runtime.

```cpp
struct Compression {
    virtual std::vector<char> compress(const std::vector<char>&) = 0;
    virtual ~Compression() = default;
};
struct Zip  : Compression { std::vector<char> compress(const std::vector<char>&) override; };
struct Gzip : Compression { std::vector<char> compress(const std::vector<char>&) override; };

class Archiver {
    std::unique_ptr<Compression> strategy_;      // the injected strategy
public:
    explicit Archiver(std::unique_ptr<Compression> s) : strategy_(std::move(s)) {}
    void set_strategy(std::unique_ptr<Compression> s) { strategy_ = std::move(s); }
    void archive(const std::vector<char>& d) { strategy_->compress(d); }
};
```

```
   Archiver ──has──► Compression* ◄── Zip / Gzip (swap at runtime)
   Modern C++ alternative: std::function<...> as the strategy (no class needed):
     std::function<std::vector<char>(const std::vector<char>&)> strategy_;
```

---

## 2. Observer (behavioral) — publish/subscribe

Subjects notify a list of observers when they change.

```cpp
struct Observer { virtual void on_update(int value) = 0; virtual ~Observer() = default; };

class Subject {
    std::vector<Observer*> observers_;   // non-owning; or weak_ptr for safety
    int state_ = 0;
public:
    void subscribe(Observer* o)   { observers_.push_back(o); }
    void set_state(int v) {
        state_ = v;
        for (auto* o : observers_) o->on_update(state_);   // notify all
    }
};
```

```
   Subject ──notifies──► Observer, Observer, Observer ...
   Used in: GUIs (events), MVC, reactive systems.
   Pitfalls: dangling observers (unsubscribe in dtor / use weak_ptr), and
   re-entrancy (an observer modifying the list during notification).
   Modern alt: signals/slots libraries, or std::function callbacks.
```

Runnable: [`examples/ch13_observer.cpp`](examples/ch13_observer.cpp).

---

## 3. Factory Method / Simple Factory (creational)

Centralize object creation behind a function returning a base pointer.

```cpp
struct Shape { virtual double area() const = 0; virtual ~Shape() = default; };
struct Circle : Shape { /*...*/ double area() const override; };
struct Square : Shape { /*...*/ double area() const override; };

enum class Kind { Circle, Square };

std::unique_ptr<Shape> make_shape(Kind k) {     // the factory
    switch (k) {
        case Kind::Circle: return std::make_unique<Circle>();
        case Kind::Square: return std::make_unique<Square>();
    }
    return nullptr;
}
```

```
   Client asks make_shape(kind) and gets a Shape — it doesn't name concrete
   types or call 'new'. Creation logic is in ONE place (easy to change/extend).
   Returns unique_ptr -> ownership is clear, no leaks.
```

---

## 4. Builder (creational) — construct complex objects step by step

```cpp
class HttpRequest {
public:
    class Builder {
        HttpRequest req_;
    public:
        Builder& url(std::string u)    { req_.url_ = std::move(u); return *this; }
        Builder& method(std::string m) { req_.method_ = std::move(m); return *this; }
        Builder& header(std::string k, std::string v) {
            req_.headers_.emplace_back(std::move(k), std::move(v)); return *this;
        }
        HttpRequest build() { return std::move(req_); }
    };
private:
    std::string url_, method_ = "GET";
    std::vector<std::pair<std::string,std::string>> headers_;
};

// auto req = HttpRequest::Builder{}.url("x").method("POST").header("A","B").build();
```

```
   Fluent chaining (return *this) builds an object incrementally, avoiding a
   telescoping constructor with 10 parameters. Great for optional/many fields.
```

---

## 5. Singleton (creational) — one instance (use sparingly!)

```cpp
class Config {
public:
    static Config& instance() { static Config c; return c; }  // Meyers singleton
    int get(const std::string& key) const;
private:
    Config() = default;
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
};
```

```
   Thread-safe lazy init (chapter 11). BUT singletons are global state in
   disguise: they hide dependencies, hurt testability, and cause init-order
   issues. Prefer PASSING the dependency (DIP, chapter 12) over a singleton.
   Reserve for truly-single resources (logging sink) — and even then, consider DI.
```

---

## 6. Decorator (structural) — add behavior by wrapping

Wrap an object in another with the same interface, adding behavior — an
alternative to subclassing for every combination.

```cpp
struct DataSource { virtual std::string read() = 0; virtual ~DataSource() = default; };
struct FileSource : DataSource { std::string read() override { return "raw"; } };

struct Decorator : DataSource {                 // base wrapper
    std::unique_ptr<DataSource> inner_;
    explicit Decorator(std::unique_ptr<DataSource> d) : inner_(std::move(d)) {}
};
struct Encrypted : Decorator {
    using Decorator::Decorator;
    std::string read() override { return "enc(" + inner_->read() + ")"; }
};
struct Compressed : Decorator {
    using Decorator::Decorator;
    std::string read() override { return "zip(" + inner_->read() + ")"; }
};

// stack them:
// auto s = std::make_unique<Compressed>(
//              std::make_unique<Encrypted>(
//                  std::make_unique<FileSource>()));
// s->read();  ->  "zip(enc(raw))"
```

```
   FileSource   -> "raw"
   + Encrypted  -> "enc(raw)"
   + Compressed -> "zip(enc(raw))"
   Each decorator wraps the next, adding one behavior. Combine freely at runtime
   instead of creating EncryptedCompressedFileSource-style subclass explosions.
```

Runnable: [`examples/ch13_decorator.cpp`](examples/ch13_decorator.cpp).

---

## 7. Adapter (structural) — make incompatible interfaces work together

```cpp
struct Logger { virtual void log(const std::string&) = 0; virtual ~Logger()=default; };

class ThirdPartyLib {                       // has a DIFFERENT interface
public: void write_message(const char* s, int level);
};

class LibAdapter : public Logger {          // adapts ThirdPartyLib to Logger
    ThirdPartyLib& lib_;
public:
    explicit LibAdapter(ThirdPartyLib& l) : lib_(l) {}
    void log(const std::string& s) override { lib_.write_message(s.c_str(), 1); }
};
```

```
   Your code expects Logger; the library speaks write_message(). The Adapter
   TRANSLATES one interface to the other. (Like a power-plug adapter.)
```

---

## 8. Template Method (behavioral) — fixed skeleton, customizable steps

```cpp
class Game {
public:
    void play() {                 // the invariant algorithm (skeleton)
        initialize();
        start_play();
        end_play();
    }
    virtual ~Game() = default;
private:
    virtual void initialize() = 0;   // steps customized by subclasses
    virtual void start_play() = 0;
    virtual void end_play()   = 0;
};
class Chess : public Game {
    void initialize() override {} void start_play() override {} void end_play() override {}
};
```

```
   The base defines the ORDER (play()); subclasses fill in the STEPS. This is
   the NVI idiom from chapter 07: public non-virtual skeleton + private virtuals.
```

---

## 9. Command (behavioral) — actions as objects

```cpp
struct Command { virtual void execute() = 0; virtual void undo() = 0; virtual ~Command()=default; };

class AddText : public Command {
    Document& doc_; std::string text_;
public:
    AddText(Document& d, std::string t) : doc_(d), text_(std::move(t)) {}
    void execute() override { doc_.append(text_); }
    void undo()    override { doc_.remove_last(text_.size()); }
};
// std::vector<std::unique_ptr<Command>> history;  -> undo/redo, macros, queues
```

```
   Wrap an operation (+ its undo) as an object -> enables undo/redo stacks,
   queuing, logging, and replaying actions. Modern alt: std::function for simple
   commands without undo.
```

---

## 10. Pattern selector & modern alternatives

```
   Need                                  Pattern            Modern C++ alt
   -----------------------------------   ----------------   --------------------
   swap an algorithm                     Strategy           std::function / lambda
   notify many on change                 Observer           signals / callbacks
   create without naming concrete type   Factory            factory fn + unique_ptr
   build a complex object stepwise       Builder            named args / aggregates
   exactly one instance                  Singleton          dependency injection!
   add behavior by wrapping              Decorator          ranges views / wrappers
   bridge incompatible interfaces        Adapter            free-function shim
   fixed skeleton, custom steps          Template Method    NVI / std::function hooks
   actions + undo                        Command            std::function (no undo)
   closed set of types + operations      Visitor            std::variant + std::visit
```

```
   Patterns are a VOCABULARY, not a checklist. Many GoF patterns exist to work
   around missing language features; modern C++ (lambdas, std::function,
   variant, templates) often expresses them more directly. Learn them to
   COMMUNICATE and RECOGNIZE, then pick the simplest tool.
```

### Visitor vs `std::variant` (a modern note)

```cpp
// Instead of a Visitor class hierarchy for a CLOSED set of types:
using Shape = std::variant<Circle, Square, Triangle>;
double area(const Shape& s) {
    return std::visit([](const auto& x){ return x.area(); }, s);   // type-safe dispatch
}
```

```
   For a FIXED set of types, std::variant + std::visit gives compile-time-checked
   "visitor" dispatch with value semantics (no inheritance, no heap). For an OPEN
   set (plugins), stick with virtual/interfaces. See chapter 14.
```

---

## 11. Summary

<!--diagram
title: Design patterns
box[green] Key points
  text: Patterns = named solutions in 3 families: **creational**, **structural**, **behavioral**
  text: Most useful: Strategy, Observer, Factory, Builder, Decorator, Adapter, Template Method, Command, Singleton (sparingly)
  text: Modern C++ often replaces boilerplate patterns with lambdas, `std::function`, `std::variant`+`visit`, templates, ranges
  text: Use patterns to **communicate** intent & **recognize** designs — not as a mandate. Prefer the simplest construct that works
-->
```
 +------------------------------------------------------------------+
 | Patterns = named solutions in 3 families: creational, structural,|
 |   behavioral.                                                    |
 | Most useful: Strategy, Observer, Factory, Builder, Decorator,    |
 |   Adapter, Template Method, Command, Singleton (sparingly).      |
 | Modern C++ often replaces boilerplate patterns with lambdas,     |
 |   std::function, std::variant+visit, templates, ranges.          |
 | Use patterns to COMMUNICATE intent & RECOGNIZE designs — not as  |
 |   a mandate. Prefer the simplest construct that works.           |
 +------------------------------------------------------------------+
```

Next: [14-modern-cpp-oop.md](14-modern-cpp-oop.md).
