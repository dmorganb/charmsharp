// Instrumentation flags are metadata; Charm# does not change scheduling.
mainchare Main {
entry:
    Main(CkArgMsg* msg) {
        delete msg;
        CProxy_Worker worker = CProxy_Worker::ckNew();
        worker.ping();
    }
};

chare Worker {
entry:
    Worker() {}
    [[charm::notrace]] void ping() { thisProxy.pong(); }
    [[charm::appwork]] void pong() { thisProxy.both(42); }
    [[charm::notrace]] [[charm::appwork]] void both(int value) {
        CkPrintf("Instrumentation attributes: %d\n", value);
        CkExit();
    }
};
