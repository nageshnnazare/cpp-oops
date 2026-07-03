// ch10 — composition (has-a) vs inheritance (is-a): Stack done right
// build: clang++ -std=c++20 -Wall -Wextra ch10_stack_composition.cpp -o /tmp/d && /tmp/d
#include <iostream>
#include <vector>

// Stack HAS-A vector (composition) -> exposes ONLY stack operations.
template <class T>
class Stack {
    std::vector<T> data_;              // hidden implementation detail
public:
    void push(const T& x) { data_.push_back(x); }
    void pop()            { data_.pop_back(); }
    const T& top() const  { return data_.back(); }
    bool empty() const    { return data_.empty(); }
    std::size_t size() const { return data_.size(); }
};

int main() {
    Stack<int> s;
    s.push(1); s.push(2); s.push(3);
    // s[0] = 9;             // COMPILE ERROR: no operator[] -> invariant safe
    // s.insert(...);        // COMPILE ERROR: no insert -> can't bypass push/pop
    std::cout << "top=" << s.top() << " size=" << s.size() << '\n';  // top=3 size=3
    s.pop();
    std::cout << "top=" << s.top() << " size=" << s.size() << '\n';  // top=2 size=2
}
