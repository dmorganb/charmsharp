mainchare Main {
entry:
    Main(CkArgMsg* args) {
        delete args;
        CProxy_Worker::ckNew();
    }
};

chare Worker {
entry:
    Worker() {
        CkPrintf("Hello from Charm#\n");
        CkExit();
    }
};
