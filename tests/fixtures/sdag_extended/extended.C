#include "extended.decl.h"

class Main : public CBase_Main {
public:
    Main(CkArgMsg* msg) {
        const bool early = msg->argc > 1 && msg->argv[1][0] == 'e';
        delete msg;
        CProxy_Worker worker = CProxy_Worker::ckNew(thisProxy);
        if (early) {
            worker.tick(1);
            worker.right(5);
            worker.indexed(1);
            worker.first(7);
            worker.left(3);
            worker.tick(0);
            worker.indexed(0);
            worker.start();
        } else {
            worker.start();
            worker.tick(1);
            worker.right(5);
            worker.indexed(1);
            worker.first(7);
            worker.left(3);
            worker.tick(0);
            worker.indexed(0);
        }
    }
    void done(int value) {
        CkPrintf("SDAG extended: %d\n", value);
        CkExit();
    }
};

class Worker : public CBase_Worker {
    CProxy_Main parent;
    int total = 0;
    int step = 0;
public:
    Worker_SDAG_CODE
    Worker(CProxy_Main p) : parent(p) {}
};

#include "extended.def.h"
