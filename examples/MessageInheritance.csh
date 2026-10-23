struct Metadata {
    int origin = 40;
};

class Extra {
protected:
    int increment = 2;
};

message Payload : public Metadata, protected Extra {
public:
    int result() const { return origin + increment; }
};

chare Worker {
entry:
    Worker() {}
    void receive(Payload* msg) {
        CkPrintf("Hello from inherited message: %d\n", msg->result());
        delete msg;
        CkExit();
    }
};

mainchare Main {
entry:
    Main(CkArgMsg* args) {
        delete args;
        CProxy_Worker worker = CProxy_Worker::ckNew();
        worker.receive(new Payload);
    }
};
