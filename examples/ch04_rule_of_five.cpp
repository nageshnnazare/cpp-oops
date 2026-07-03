// ch04 — Rule of Five: a class that manages a raw buffer correctly
// build: clang++ -std=c++20 -Wall -Wextra ch04_rule_of_five.cpp -o /tmp/d && /tmp/d
#include <algorithm>
#include <cstddef>
#include <iostream>

class Buffer {
    int* data_;
    std::size_t size_;
public:
    explicit Buffer(std::size_t n) : data_(new int[n]{}), size_(n) {
        std::cout << "ctor(" << n << ")\n";
    }
    ~Buffer() { delete[] data_; }                                  // 1

    Buffer(const Buffer& o) : data_(new int[o.size_]), size_(o.size_) {  // 2
        std::copy(o.data_, o.data_ + size_, data_);
        std::cout << "copy ctor\n";
    }
    Buffer& operator=(const Buffer& o) {                           // 3
        if (this != &o) {
            int* fresh = new int[o.size_];
            std::copy(o.data_, o.data_ + o.size_, fresh);
            delete[] data_;
            data_ = fresh; size_ = o.size_;
        }
        std::cout << "copy assign\n";
        return *this;
    }
    Buffer(Buffer&& o) noexcept : data_(o.data_), size_(o.size_) { // 4
        o.data_ = nullptr; o.size_ = 0;
        std::cout << "move ctor\n";
    }
    Buffer& operator=(Buffer&& o) noexcept {                       // 5
        if (this != &o) {
            delete[] data_;
            data_ = o.data_; size_ = o.size_;
            o.data_ = nullptr; o.size_ = 0;
        }
        std::cout << "move assign\n";
        return *this;
    }

    std::size_t size() const { return size_; }
    void set(std::size_t i, int v) { data_[i] = v; }
    int  get(std::size_t i) const { return data_[i]; }
};

int main() {
    Buffer a(3);
    a.set(0, 42);
    Buffer b = a;              // copy ctor (deep) -> independent buffer
    b.set(0, 99);
    std::cout << "a[0]=" << a.get(0) << " b[0]=" << b.get(0) << '\n'; // 42 99
    Buffer c = std::move(a);   // move ctor -> steals a's buffer
    std::cout << "c[0]=" << c.get(0) << " a.size=" << a.size() << '\n'; // 42 0
}
