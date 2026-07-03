// ch13 — Decorator pattern: add behavior by wrapping
// build: clang++ -std=c++20 -Wall -Wextra ch13_decorator.cpp -o /tmp/d && /tmp/d
#include <iostream>
#include <memory>
#include <string>

struct DataSource {
    virtual std::string read() = 0;
    virtual ~DataSource() = default;
};

struct FileSource : DataSource {
    std::string read() override { return "raw"; }
};

struct Decorator : DataSource {
    std::unique_ptr<DataSource> inner_;
    explicit Decorator(std::unique_ptr<DataSource> d) : inner_(std::move(d)) {}
};

struct Encrypted : Decorator {
    using Decorator::Decorator;
    std::string read() override { return "enc(" + inner_->read() + ")"; }
};
struct Compressed : Decorator {
    using Decorator::Decorator;
    std::string read() override { return "zip(" + inner_->read() + ")"; }
};

int main() {
    auto s = std::make_unique<Compressed>(
                 std::make_unique<Encrypted>(
                     std::make_unique<FileSource>()));
    std::cout << s->read() << '\n';   // zip(enc(raw))
}
