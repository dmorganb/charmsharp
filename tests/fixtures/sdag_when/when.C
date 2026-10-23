#include "sdagwait.decl.h"
#include <cstring>

class Main : public CBase_Main {
public:
    Main(CkArgMsg* msg) {
        const bool early = msg->argc > 1 && std::strcmp(msg->argv[1], "early") == 0;
        delete msg;
        CProxy_Worker worker = CProxy_Worker::ckNew(thisProxy);
        if (early) {
            worker.deliver(42);
            worker.start();
        } else {
            worker.start();
            worker.deliver(42);
        }
    }
    void done(int value) {
        CkPrintf("SDAG result: %d\n", value);
        CkExit();
    }
};

class Worker : public CBase_Worker {
    CProxy_Main parent;
public:
    Worker_SDAG_CODE
    Worker(CProxy_Main p) : parent(p) {}
};

#include "sdagwait.def.h"
