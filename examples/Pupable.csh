#include <memory>

[[charm::pupable]]
class State : public PUP::able {
    int value_;
public:
    explicit State(int value) : value_(value) {}
    State(CkMigrateMessage* m) : PUP::able(m), value_(0) {}
    void pup(PUP::er& p) override {
        PUP::able::pup(p);
        p | value_;
    }
    int value() const { return value_; }
};

group Receiver {
entry:
    Receiver() {}
    void accept(std::shared_ptr<State> state) {
        CkPrintf("PUPable value %d PE %d\n", state->value(), CkMyPe());
        CkExit();
    }
};

chare Main {
entry:
    Main(CkArgMsg* args) {
        delete args;
        CProxy_Receiver receivers(CProxy_Receiver::ckNew());
        receivers[1].accept(std::make_shared<State>(42));
    }
};
