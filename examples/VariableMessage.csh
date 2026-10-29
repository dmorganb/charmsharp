message DataMessage {
public:
    int count = 0;
    int fixed[2]{};
    double values[];
    int indices[];
    int fixedSum() const { return fixed[0] + fixed[1]; }
};

chare Worker {
entry:
    Worker() {}
    void receive(DataMessage* msg) {
        bool valid = msg->count == 4 && msg->fixedSum() == 9;
        for (int i = 0; i < msg->count; ++i) {
            valid = valid && msg->values[i] == (i + 1) * 0.5;
            valid = valid && msg->indices[i] == i * 10;
        }
        CkPrintf("Variable message: %s on PE %d\n", valid ? "PASS" : "FAIL", CkMyPe());
        delete msg;
        if (!valid) CkAbort("variable message mismatch");
        CkExit();
    }
};

chare Main {
entry:
    Main(CkArgMsg* args) {
        delete args;
        auto worker = CProxy_Worker::ckNew(1);
        auto* msg = new (4, 4) DataMessage;
        msg->count = 4;
        msg->fixed[0] = 4;
        msg->fixed[1] = 5;
        for (int i = 0; i < 4; ++i) {
            msg->values[i] = (i + 1) * 0.5;
            msg->indices[i] = i * 10;
        }
        worker.receive(msg);
    }
};
