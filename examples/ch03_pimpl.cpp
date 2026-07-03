// ch03 — Pimpl idiom in a single file (normally split header/.cpp)
// build: clang++ -std=c++20 -Wall -Wextra ch03_pimpl.cpp -o /tmp/d && /tmp/d
#include <iostream>
#include <memory>
#include <vector>
#include <numeric>

// ---- widget.hpp (interface only; no internals leak) ----
class Widget {
public:
    Widget();
    ~Widget();                        // defined where Impl is complete
    Widget(Widget&&) noexcept;
    Widget& operator=(Widget&&) noexcept;
    void add(int x);
    int  sum() const;
private:
    struct Impl;                      // incomplete here
    std::unique_ptr<Impl> pimpl_;
};

// ---- widget.cpp (all real internals live here) ----
struct Widget::Impl {
    std::vector<int> data;
};
Widget::Widget() : pimpl_(std::make_unique<Impl>()) {}
Widget::~Widget() = default;                       // Impl complete -> OK
Widget::Widget(Widget&&) noexcept = default;
Widget& Widget::operator=(Widget&&) noexcept = default;
void Widget::add(int x) { pimpl_->data.push_back(x); }
int  Widget::sum() const {
    return std::accumulate(pimpl_->data.begin(), pimpl_->data.end(), 0);
}

int main() {
    Widget w;
    w.add(1); w.add(2); w.add(3);
    std::cout << "sum = " << w.sum() << '\n';   // 6
}
