// nokeep receives a runtime-owned message; handlers must not delete it.
message Data {
    int value;
};

mainchare Main {
entry:
    Main(CkArgMsg* msg) {
        delete msg;
        CProxy_Consumer consumer = CProxy_Consumer::ckNew(thisProxy);
        Data* data = new Data;
        data->value = 42;
        consumer.consume(data);
    }
    void got(int value) {
        CProxy_Gate gate = CProxy_Gate::ckNew();
        gate.enter(value);
    }
};

group Consumer {
    CProxy_Main parent;
entry:
    Consumer(CProxy_Main p) : parent(p) {}
    [[charm::nokeep]] void consume(Data* data) {
        if (CkMyPe() == 0) parent.got(data->value);
        // The Charm++ runtime owns and releases the incoming Data message.
    }
};

nodegroup Gate {
entry:
    Gate() {}
    [[charm::exclusive]] void enter(int value) {
        CkPrintf("Ownership and exclusion: %d\n", value);
        CkExit();
    }
};
