message Two {
public:
    int doublesCount = 0;
    int intsCount = 0;
    double doubles[];
    int ints[];
};
chare Worker {
entry:
    Worker() {}
    void receive(Two* msg) {
        bool okay = msg->doublesCount == 2 && msg->intsCount == 5;
        for (int i = 0; i < msg->doublesCount; ++i)
            okay = okay && msg->doubles[i] == 10.0 + i;
        for (int i = 0; i < msg->intsCount; ++i)
            okay = okay && msg->ints[i] == 20 + i;
        CkPrintf("Unequal variable message: %s on PE %d\n",
                 okay ? "PASS" : "FAIL", CkMyPe());
        delete msg;
        if (!okay) CkAbort("unequal allocation mismatch");
        CkExit();
    }
};
chare Main {
entry:
    Main(CkArgMsg* args) {
        delete args;
        CProxy_Worker worker = CProxy_Worker::ckNew(1);
        auto* msg = new (2, 5) Two;
        msg->doublesCount = 2;
        msg->intsCount = 5;
        for (int i = 0; i < 2; ++i) msg->doubles[i] = 10.0 + i;
        for (int i = 0; i < 5; ++i) msg->ints[i] = 20 + i;
        worker.receive(msg);
    }
};
