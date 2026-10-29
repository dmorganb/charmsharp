chare Worker {
entry:
    Worker() {}
    void run(SharedValue* msg) {
        CkPrintf("MULTIFILE CHARM# PASS: %d\n", msg->value);
        CkExit();
    }
};
