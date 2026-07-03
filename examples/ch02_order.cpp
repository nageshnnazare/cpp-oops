// ch02 — construction/destruction order of members
// build: clang++ -std=c++20 -Wall -Wextra ch02_order.cpp -o /tmp/d && /tmp/d
#include <cstdio>

struct A { A(){ puts("A ctor"); } ~A(){ puts("A dtor"); } };
struct B { B(){ puts("B ctor"); } ~B(){ puts("B dtor"); } };

struct C {
    A a;                       // declared first -> constructed first
    B b;                       // constructed second
    C(){ puts("C body"); }
    ~C(){ puts("C ~body"); }   // body runs first on destruction
};

int main() {
    puts("-- create C --");
    C c;
    puts("-- destroy C --");
    // members destroyed in REVERSE order (b then a) after ~body
}
