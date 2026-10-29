#include "Physics.h"

message SharedValue {
public:
    int value = 0;
};

mainchare Main {
entry:
    Main(CkArgMsg* msg) {
        delete msg;
        CkPrintf("PHYSICS %d\n", physics_value());
        CProxy_Worker worker = CProxy_Worker::ckNew();
        auto* msg = new SharedValue;
        msg->value = physics_value();
        worker.run(msg);
    }
};
