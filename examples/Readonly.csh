// Charm++ broadcasts the value assigned in Main to all processing elements.
readonly int threshold;

chare Worker {
entry:
    Worker() {
        CkPrintf("Hello from readonly: %d\n", threshold);
        CkExit();
    }
};

mainchare Main {
entry:
    Main(CkArgMsg* args) {
        delete args;
        threshold = 42;
        CProxy_Worker::ckNew();
    }
};
