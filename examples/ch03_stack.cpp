// ch03 — encapsulated Stack: internals hidden, invariants enforced
// build: clang++ -std=c++20 -Wall -Wextra ch03_stack.cpp -o /tmp/d && /tmp/d
#include <vector>
#include <stdexcept>
#include <iostream>

template <class T>
class Stack {
    std::vector<T> data_;
public:
    bool empty() const { return data_.empty(); }
    std::size_t size() const { return data_.size(); }
    void push(T value) { data_.push_back(std::move(value)); }

    T pop() {
        if (data_.empty()) throw std::out_of_range("pop from empty stack");
        T top = std::move(data_.back());
        data_.pop_back();
        return top;
    }
    const T& top() const {
        if (data_.empty()) throw std::out_of_range("top of empty stack");
        return data_.back();
    }
};

int main() {
    Stack<int> s;
    s.push(1); s.push(2); s.push(3);
    std::cout << s.top() << '\n';   // 3
    std::cout << s.pop() << '\n';   // 3
    std::cout << s.size() << '\n';  // 2
    try { Stack<int>{}.pop(); } catch (const std::exception& e) {
        std::cout << "caught: " << e.what() << '\n';
    }
}
