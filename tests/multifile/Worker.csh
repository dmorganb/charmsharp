#include "SharedConfig.h"
chare Worker {
    SharedConfig configuration{42};
entry:
    Worker() {}
    void run(SharedValue* msg) {
        CkPrintf("MULTIFILE CHARM# PASS: %d\n", msg->value);
        CkExit();
    }
};
