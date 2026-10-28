// The template definition and explicit registration live in this unit.
// Its CProxy_Worker<int> use lives in a separate ordinary C++ source file.
void launchWorker(CProxy_Main caller);

chare Main {
entry:
    Main(CkArgMsg* msg) {
        delete msg;
        launchWorker(thisProxy);
    }

    void complete() { CkExit(); }
};

template<typename T>
chare Worker {
entry:
    Worker(T initial, CProxy_Main caller) {
        CkPrintf("Cross TU worker: %d\n", static_cast<int>(initial));
        caller.complete();
    }
};

// A cross-TU proxy usage is not visible to Charm#'s single-source tokenizer.
template chare Worker<int>;
