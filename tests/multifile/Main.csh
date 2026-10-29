#include "Physics.h"

mainchare Main {
entry:
    Main(CkArgMsg* msg) {
        delete msg;
        CkPrintf("PHYSICS %d\n", physics_value());
        CProxy_Worker worker = CProxy_Worker::ckNew();
        worker.run();
    }
};
