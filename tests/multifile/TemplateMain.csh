chare Main {
entry:
    Main(CkArgMsg* msg) {
        delete msg;
        CProxy_GenericWorker<int>::ckNew(42);
    }
};
