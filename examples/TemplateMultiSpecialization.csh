// Phase 6.3.4: two different specializations must register and execute.
template<typename T>
chare Worker {
entry:
    Worker(T initial, CProxy_Main caller) {
        CkPrintf("Worker value: %.2f\n", static_cast<double>(initial));
        caller.complete();
    }
};

chare Main {
    int completed = 0;
entry:
    Main(CkArgMsg* msg) {
        delete msg;
        CProxy_Worker<int>::ckNew(42, thisProxy);
        CProxy_Worker<double>::ckNew(3.5, thisProxy);
    }

    void complete() {
        if (++completed == 2) {
            CkPrintf("Both template specializations completed\n");
            CkExit();
        }
    }
};
