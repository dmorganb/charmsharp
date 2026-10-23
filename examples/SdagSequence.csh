// Phase 5.2: implicit serial runs around two separate asynchronous waits.
mainchare Main {
entry:
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

chare Worker {
    CProxy_Main parent;
    int total = 0;
entry:
    Worker(CProxy_Main p) : parent(p) {}
    void start() {
        total = 0;
        when deliver(int value) {
            total += value;
        }
        total += 1;
        when confirm(int value) {
            total += value;
        }
        parent.done(total);
    }
};
