// ch05 — construction/destruction order with inheritance
// build: clang++ -std=c++20 -Wall -Wextra ch05_ctor_order.cpp -o /tmp/d && /tmp/d
#include <cstdio>
#include <string>
#include <utility>

struct Base {
    Base(const std::string& n) { printf("Base ctor (%s)\n", n.c_str()); }
    ~Base() { puts("Base dtor"); }
};
struct Member {
    Member() { puts("Member ctor"); }
    ~Member() { puts("Member dtor"); }
};
struct Derived : Base {
    Member m;                                  // member built after Base
    Derived() : Base("from Derived") {         // must init base explicitly
        puts("Derived body");
    }
    ~Derived() { puts("Derived ~body"); }
};

int main() {
    puts("-- create Derived --");
    Derived d;
    puts("-- destroy Derived --");
    // order: Base ctor -> Member ctor -> Derived body ;
    //        Derived ~body -> Member dtor -> Base dtor
}
