struct DomainState {
    explicit DomainState(int value) : offset(value) {}
    int offset;
};

mainchare Main : public DomainState {
    int first;
    int second;
entry:
    Main(CkArgMsg* msg)
        : DomainState(40), first{1}, second((1 + 1)) {
        delete msg;
        CkPrintf("Hello from constructor initializers: %d\n",
                 offset + first + second);
        CkExit();
    }
};
