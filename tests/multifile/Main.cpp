#include "Worker.h"

class Main : public CBase_Main {
public:
    explicit Main(CkArgMsg* msg) {
        delete msg;
        CProxy_Worker worker = CProxy_Worker::ckNew();
        worker.run();
    }
};

// Charm++ module registration must appear in exactly one translation unit.
// Both chare definitions are visible to the generated registration glue here.
#include "Simulation.def.h"
