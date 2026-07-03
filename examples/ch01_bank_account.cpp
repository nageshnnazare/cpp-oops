// ch01 — a class that owns and protects its data (encapsulation)
// build: clang++ -std=c++20 -Wall -Wextra ch01_bank_account.cpp -o /tmp/d && /tmp/d
#include <iostream>
#include <string>

class BankAccount {
public:
    BankAccount(std::string owner, double initial)
        : owner_(std::move(owner)), balance_(initial) {}

    void deposit(double amount) { if (amount > 0) balance_ += amount; }

    bool withdraw(double amount) {
        if (amount > 0 && amount <= balance_) { balance_ -= amount; return true; }
        return false;
    }

    double balance() const { return balance_; }
    const std::string& owner() const { return owner_; }

private:
    std::string owner_;
    double      balance_;
};

int main() {
    BankAccount acc("Ada", 100.0);
    acc.deposit(50);
    std::cout << acc.owner() << " has " << acc.balance() << '\n';   // Ada has 150
    std::cout << (acc.withdraw(500) ? "ok" : "declined") << '\n';   // declined
    std::cout << "balance still " << acc.balance() << '\n';         // 150
}
