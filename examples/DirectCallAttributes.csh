// A local entry returns a C++ value. Inline entries invoke directly on
// the same PE and fall back to normal delivery for remote recipients.
group Service {
    CProxy_Main parent;
entry:
    Service(CProxy_Main p) : parent(p) {
        if (CkMyPe() == 0) parent.ready();
    }
    [[charm::local]] int twice(int value) {
        return value * 2;
    }
    [[charm::inline]] void report(int result) {
        CkPrintf("Direct-call attributes: %d\n", result);
        CkExit();
    }
};

mainchare Main {
    CProxy_Service service;
entry:
    Main(CkArgMsg* msg) {
        delete msg;
        service = CProxy_Service::ckNew(thisProxy);
    }
    void ready() {
        int answer = service.ckLocalBranch()->twice(21);
        service.report(answer);
    }
};
