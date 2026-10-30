#include <memory>
group Receiver {
entry:
    Receiver() {}
    void accept(std::shared_ptr<State> state) {
        CkPrintf("MULTIFILE PUP PASS: %d PE %d\n", state->value(), CkMyPe());
        CkExit();
    }
};
