chare Worker {
entry:
    Worker() {}
    void run() {
        CkPrintf("MULTIFILE CHARM# PASS\n");
        CkExit();
    }
};
