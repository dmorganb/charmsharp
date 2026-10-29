[[charm::initnode]]
void initializeNode() {
    CkPrintf("Hook node %d\n", CkMyNode());
}
[[charm::initproc]]
void initializeProcessor() {
    CkPrintf("Hook processor %d\n", CkMyPe());
}
chare Main {
entry:
    Main(CkArgMsg* args) {
        delete args;
        CkPrintf("Hook main PE %d\n", CkMyPe());
        CkExit();
    }
};
