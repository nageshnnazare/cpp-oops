// ch21 — RTTI: typeid, demangling, dynamic_cast (down + cross), bad_cast
// build: clang++ -std=c++20 -Wall -Wextra ch21_rtti.cpp -o /tmp/d && /tmp/d
#include <iostream>
#include <memory>
#include <string>
#include <typeinfo>
#include <typeindex>
#include <unordered_map>

#if defined(__GNUG__)     // GCC/Clang: demangle the ABI name
  #include <cxxabi.h>
  static std::string demangle(const char* n) {
      int st = 0;
      char* d = abi::__cxa_demangle(n, nullptr, nullptr, &st);
      std::string s = (st == 0 && d) ? d : n;
      std::free(d);
      return s;
  }
#else
  static std::string demangle(const char* n) { return n; }
#endif

struct Animal { virtual ~Animal() = default; };
struct Dog : Animal {};
struct Cat : Animal {};

struct Swimmer { virtual ~Swimmer() = default; };
struct Platypus : Animal, Swimmer {};       // multiple inheritance -> cross-cast

int main() {
    std::unique_ptr<Animal> a = std::make_unique<Dog>();

    std::cout << "typeid(*a).name() demangled = " << demangle(typeid(*a).name()) << '\n';

    // Downcast: checked at runtime
    std::cout << "dynamic_cast<Dog*> : " << (dynamic_cast<Dog*>(a.get()) ? "Dog" : "null") << '\n';
    std::cout << "dynamic_cast<Cat*> : " << (dynamic_cast<Cat*>(a.get()) ? "Cat" : "null") << '\n';

    // Cross-cast across multiple inheritance:
    std::unique_ptr<Animal> p = std::make_unique<Platypus>();
    Swimmer* s = dynamic_cast<Swimmer*>(p.get());   // Animal* -> Swimmer* sideways
    std::cout << "cross-cast Animal*->Swimmer* : " << (s ? "ok" : "null") << '\n';

    // Reference cast failure throws:
    try {
        Cat& bad = dynamic_cast<Cat&>(*a);          // *a is a Dog -> throws
        (void)bad;
    } catch (const std::bad_cast& e) {
        std::cout << "dynamic_cast<Cat&> threw: " << e.what() << '\n';
    }

    // type_index as a map key (type-erased registry):
    std::unordered_map<std::type_index, std::string> reg;
    reg[typeid(Dog)] = "woof"; reg[typeid(Cat)] = "meow";
    std::cout << "registry[typeid(*a)] = " << reg[typeid(*a)] << '\n';   // woof
}
