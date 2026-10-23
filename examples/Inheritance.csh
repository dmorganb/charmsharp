struct Logging {
    int code() const { return 40; }
};

class DomainState {
protected:
    int adjustment = 2;
};

chare Worker : public Logging, protected DomainState {
entry:
    Worker() {
        CkPrintf("Hello from inherited base: %d\n", code() + adjustment);
        CkExit();
    }
};

mainchare Main {
entry:
    Main(CkArgMsg* args) {
        delete args;
        CProxy_Worker::ckNew();
    }
};
