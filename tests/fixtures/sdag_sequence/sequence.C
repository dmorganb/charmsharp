#include "sequence.decl.h"

class Main : public CBase_Main {
public:
    Main(CkArgMsg* msg) {
        const bool early = msg->argc > 1 && msg->argv[1][0] == 'e';
        delete msg;
        CProxy_Worker worker = CProxy_Worker::ckNew(thisProxy);
        if (early) {
            worker.deliver(20);
            worker.confirm(21);
            worker.start();
        } else {
            worker.start();
            worker.deliver(20);
            worker.confirm(21);
        }
    }
    void done(int value) {
        CkPrintf("SDAG sequence: %d\n", value);
        CkExit();
    }
};

class Worker : public CBase_Worker {
    CProxy_Main parent;
    int total = 0;
public:
    Worker_SDAG_CODE
    Worker(CProxy_Main p) : parent(p) {}
};

#include "sequence.def.h"
