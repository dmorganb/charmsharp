mainchare Main {
    int callbacks;
entry:
    Main(CkArgMsg* msg) : callbacks(0) {
        delete msg;
        float values[3] = {1.0f, 2.0f, 3.0f};
        CProxy_Worker worker(CProxy_Worker::ckNew(thisProxy));
        worker.receive(3, values);
        worker.fixed(values);
    }

    void done(float total) {
        if (total != 6.0f) CkAbort("incorrect marshalled array sum");
        if (++callbacks == 2) {
            CkPrintf("Hello from marshalled arrays: 6\n");
            CkExit();
        }
    }
};

chare Worker {
    CProxy_Main parent;
entry:
    Worker(CProxy_Main main) : parent(main) {}

    void receive(int n, float data[n]) {
        float total = 0;
        for (int i = 0; i < n; ++i) total += data[i];
        parent.done(total);
    }

    void fixed(float data[3]) {
        parent.done(data[0] + data[1] + data[2]);
    }
};
