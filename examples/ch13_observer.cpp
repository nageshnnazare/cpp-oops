// ch13 — Observer pattern (publish/subscribe)
// build: clang++ -std=c++20 -Wall -Wextra ch13_observer.cpp -o /tmp/d && /tmp/d
#include <iostream>
#include <string>
#include <vector>

struct Observer {
    virtual void on_update(int value) = 0;
    virtual ~Observer() = default;
};

class Subject {
    std::vector<Observer*> observers_;   // non-owning
    int state_ = 0;
public:
    void subscribe(Observer* o) { observers_.push_back(o); }
    void set_state(int v) {
        state_ = v;
        for (auto* o : observers_) o->on_update(state_);
    }
};

struct Display : Observer {
    std::string name;
    explicit Display(std::string n) : name(std::move(n)) {}
    void on_update(int value) override {
        std::cout << name << " sees " << value << '\n';
    }
};

int main() {
    Subject temperature;
    Display a("PhoneApp"), b("Dashboard");
    temperature.subscribe(&a);
    temperature.subscribe(&b);

    temperature.set_state(21);   // both observers notified
    temperature.set_state(25);
}
