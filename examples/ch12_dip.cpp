// ch12 — Dependency Inversion: depend on an abstraction, inject the impl
// build: clang++ -std=c++20 -Wall -Wextra ch12_dip.cpp -o /tmp/d && /tmp/d
#include <iostream>
#include <string>

struct IMessageSender {
    virtual void send(const std::string& msg) = 0;
    virtual ~IMessageSender() = default;
};

class EmailSender : public IMessageSender {
public:
    void send(const std::string& msg) override {
        std::cout << "[email] " << msg << '\n';
    }
};
class SmsSender : public IMessageSender {
public:
    void send(const std::string& msg) override {
        std::cout << "[sms] " << msg << '\n';
    }
};

// High-level policy depends on the abstraction, not a concrete sender:
class NotificationService {
    IMessageSender& sender_;
public:
    explicit NotificationService(IMessageSender& s) : sender_(s) {}
    void notify(const std::string& msg) { sender_.send(msg); }
};

int main() {
    EmailSender email;
    SmsSender   sms;

    NotificationService viaEmail(email);
    NotificationService viaSms(sms);
    viaEmail.notify("build passed");   // [email] build passed
    viaSms.notify("build passed");     // [sms] build passed
    // swapping the sender required NO change to NotificationService.
}
