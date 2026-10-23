// Phase 5.3: reference-matched receives in a loop, followed by a branch.
mainchare Main {
entry:
    Main(CkArgMsg* msg) {
        const bool early = msg->argc > 1 && msg->argv[1][0] == 'e';
        delete msg;
        CProxy_Worker worker = CProxy_Worker::ckNew(thisProxy);
        if (early) {
            worker.deliver(1, 22);
            worker.deliver(0, 20);
            worker.start();
        } else {
            worker.start();
            worker.deliver(1, 22);
            worker.deliver(0, 20);
        }
    }
    void done(int value) {
        CkPrintf("SDAG control: %d\n", value);
        CkExit();
    }
};

chare Worker {
    CProxy_Main parent;
    int total = 0;
    int step = 0;
entry:
    Worker(CProxy_Main p) : parent(p) {}
    void start() {
        total = 0;
        for (step = 0; step < 2; ++step) {
            when deliver[step](int tag, int value) {
                total += value;
            }
        }
        if (total == 42) {
            parent.done(total);
        } else {
            parent.done(-1);
        }
    }
};
