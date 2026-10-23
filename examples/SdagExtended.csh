// Phase 5.5: four SDAG control constructs, both early and late message arrival.
mainchare Main {
entry:
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

chare Worker {
    CProxy_Main parent;
    int total = 0;
    int step = 0;
entry:
    Worker(CProxy_Main p) : parent(p) {}
    void start() {
        while (step < 2) {
            when tick[step](int tag) {
                total += tag + 1;
                ++step;
            }
        }
        overlap {
            when left(int x) { total += x; }
            when right(int y) { total += y; }
        }
        case {
            when first(int x) { total += x; }
            when second(int y) { total += y; }
        }
        forall [step] (0:1, 1) {
            when indexed[step](int index) { total += index + 2; }
        }
        parent.done(total);
    }
};
