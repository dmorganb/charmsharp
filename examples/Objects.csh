mainchare Main {
entry:
    Main(CkArgMsg* args) {
        delete args;
        CProxy_Service::ckNew();
        CProxy_NodeService::ckNew();
        CProxy_Cell::ckNew(1);
    }
};

group Service {
entry:
    Service() {}
};

nodegroup NodeService {
entry:
    NodeService() {}
};

array [1D] chare Cell {
entry:
    Cell() {
        CkPrintf("Hello from array Cell\n");
        CkExit();
    }
};
