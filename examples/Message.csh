// Fixed-size user message exchanged between two chares.
message Payload {
public:
    int value = 0;
};

chare Worker {
entry:
    Worker() {}
    void receive(Payload* msg) {
        CkPrintf("Hello from message: %d\n", msg->value);
        delete msg;
        CkExit();
    }
};

mainchare Main {
entry:
    Main(CkArgMsg* args) {
        delete args;
        CProxy_Worker worker = CProxy_Worker::ckNew();
        auto* msg = new Payload;
        msg->value = 42;
        worker.receive(msg);
    }
};
