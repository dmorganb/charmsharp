// CProxy_Worker<int> use automatically registers a concrete specialization.
template<typename T>
chare Worker {
    T value;
entry:
    Worker(T initial) : value(initial) {
        CkPrintf("Template worker: %d\n", static_cast<int>(value));
        CkExit();
    }
};

chare Main {
entry:
    Main(CkArgMsg* msg) {
        delete msg;
        CProxy_Worker<int>::ckNew(42);
    }
};
