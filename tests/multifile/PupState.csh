#include <memory>
[[charm::pupable]]
class State : public PUP::able {
    int value_;
public:
    explicit State(int value) : value_(value) {}
    State(CkMigrateMessage* msg) : PUP::able(msg), value_(0) {}
    void pup(PUP::er& p) override {
        PUP::able::pup(p);
        p | value_;
    }
    int value() const { return value_; }
};
