#include <cstring>

// Phase 5.1: an implicit SDAG receive and one suspension point.
mainchare Main {
entry:
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

chare Worker {
    CProxy_Main parent;
entry:
    Worker(CProxy_Main p) : parent(p) {}

    void start() {
        when deliver(int value) {
            parent.done(value);
        }
    }
};
