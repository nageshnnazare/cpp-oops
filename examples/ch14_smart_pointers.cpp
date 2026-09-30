// ch14 — unique_ptr / shared_ptr / weak_ptr ownership
// build: clang++ -std=c++20 -Wall -Wextra ch14_smart_pointers.cpp -o /tmp/d && /tmp/d
#include <iostream>
#include <memory>

struct Resource {
    int id;
    explicit Resource(int i) : id(i) { std::cout << "acquire #" << id << '\n'; }
    ~Resource() { std::cout << "release #" << id << '\n'; }
};

int main() {
    std::cout << "-- unique_ptr (sole owner) --\n";
    {
        auto up = std::make_unique<Resource>(1);
        std::cout << "using #" << up->id << '\n';
    }   // released here automatically

    std::cout << "-- shared_ptr (shared owner) --\n";
    std::weak_ptr<Resource> weak;
    {
        auto sp1 = std::make_shared<Resource>(2);
        weak = sp1;
        {
            auto sp2 = sp1;   // refcount = 2
            std::cout << "use_count = " << sp1.use_count() << '\n';   // 2
        }                     // sp2 gone -> refcount = 1 (not released)
        std::cout << "use_count = " << sp1.use_count() << '\n';       // 1
        std::cout << "weak alive? " << !weak.expired() << '\n';       // 1
    }   // sp1 gone -> released
    std::cout << "weak alive after scope? " << !weak.expired() << '\n'; // 0

    std::cout << "-- weak_ptr breaks a cycle --\n";
    struct Node {
        int id;
        std::shared_ptr<Node> next;
        std::weak_ptr<Node> prev;          // back edge does not own
        explicit Node(int i) : id(i) { std::cout << "node " << id << '\n'; }
        ~Node() { std::cout << "drop " << id << '\n'; }
    };
    {
        auto a = std::make_shared<Node>(10);
        auto b = std::make_shared<Node>(11);
        a->next = b;
        b->prev = a;                       // weak: destroying a and b is enough
    }
    std::cout << "cycle scope ended\n";
}
