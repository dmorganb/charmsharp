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
        thisProxy.run();
    }

    [[charm::threaded]] void run() {
        CkPrintf("Hello from threaded entry\n");
        CkExit();
    }

    [[charm::sync]] CkArgMsg* reply() {
        return nullptr;
    }
};
