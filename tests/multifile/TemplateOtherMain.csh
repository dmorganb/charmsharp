chare Main {
entry:
    Main(CkArgMsg* msg) {
        delete msg;
        CProxy_OtherWorker<long>::ckNew(43);
    }
};
