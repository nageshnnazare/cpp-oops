// ch11 — static members, static method, hidden-friend operator
// build: clang++ -std=c++20 -Wall -Wextra ch11_static_friend.cpp -o /tmp/d && /tmp/d
#include <iostream>

class Entity {
    static inline int next_id_ = 1;    // one shared counter (C++17 inline static)
    int id_;
public:
    Entity() : id_(next_id_++) {}
    int id() const { return id_; }
    static int created() { return next_id_ - 1; }   // static: no 'this'

    // hidden friend: found by ADL, can read privates:
    friend bool operator==(const Entity& a, const Entity& b) { return a.id_ == b.id_; }
    friend std::ostream& operator<<(std::ostream& os, const Entity& e) {
        return os << "Entity#" << e.id_;
    }
};

int main() {
    Entity a, b, c;
    std::cout << a << ' ' << b << ' ' << c << '\n';       // Entity#1 Entity#2 Entity#3
    std::cout << "created: " << Entity::created() << '\n'; // 3
    std::cout << std::boolalpha << (a == a) << ' ' << (a == b) << '\n'; // true false
}
