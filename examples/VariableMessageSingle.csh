message Single {
public:
    int count = 0;
    float values[];
};
chare Worker {
    int seen = 0;
    bool okay = true;
entry:
    Worker() {}
    void receive(Single* msg) {
        okay = okay && (msg->count == 0 || msg->count == 3);
        for (int i = 0; i < msg->count; ++i)
            okay = okay && msg->values[i] == (i + 1) * 1.5f;
        ++seen;
        delete msg;
        if (seen == 2) {
            CkPrintf("Single variable message: %s on PE %d\n",
                     okay ? "PASS" : "FAIL", CkMyPe());
            if (!okay) CkAbort("single array mismatch");
            CkExit();
        }
    }
};
chare Main {
entry:
    Main(CkArgMsg* args) {
        delete args;
        CProxy_Worker worker = CProxy_Worker::ckNew(1);
        auto* zero = new (0) Single;
        zero->count = 0;
        worker.receive(zero);
        auto* three = new (3) Single;
        three->count = 3;
        for (int i = 0; i < 3; ++i) three->values[i] = (i + 1) * 1.5f;
        worker.receive(three);
    }
};
