// ch14 — C++23 deducing 'this': one member for all cv/ref qualifications
// build: clang++ -std=c++23 -Wall -Wextra ch14_deducing_this.cpp -o /tmp/d && /tmp/d
#include <iostream>
#include <string>
#include <utility>
#include <type_traits>

class Widget {
    std::string data_ = "hello";
public:
    // A single accessor that returns a reference matching the caller's value
    // category / constness — replaces const&, &, and && overloads.
    template <class Self>
    auto&& data(this Self&& self) {
        return std::forward<Self>(self).data_;
    }
};

int main() {
    Widget w;
    const Widget cw;

    std::cout << w.data() << '\n';                 // hello (std::string&)
    std::cout << cw.data() << '\n';                // hello (const std::string&)
    std::string taken = std::move(w).data();       // moves out (std::string&&)
    std::cout << "taken=" << taken << " left='" << w.data() << "'\n";

    // Show that the returned ref-ness matches the caller:
    static_assert(std::is_same_v<decltype(w.data()),  std::string&>);
    static_assert(std::is_same_v<decltype(cw.data()), const std::string&>);
    static_assert(std::is_same_v<decltype(std::move(w).data()), std::string&&>);
}
