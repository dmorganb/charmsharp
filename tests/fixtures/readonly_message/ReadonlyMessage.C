#include "ReadonlyMessage.decl.h"
class SharedConfig : public CMessage_SharedConfig {
public:
    int count = 0;
    double* values;
};
SharedConfig* configuration;

class Reader : public CBase_Reader {
public:
    Reader() {
        bool okay = configuration != nullptr && configuration->count == 3;
        if (okay)
            for (int i = 0; i < 3; ++i)
                okay = okay && configuration->values[i] == (i + 1) * 2.5;
        CkPrintf("Readonly message: %s on PE %d\n",
                 okay ? "PASS" : "FAIL", CkMyPe());
        if (!okay) CkAbort("readonly message initialization mismatch");
        CkExit();
    }
};
class Main : public CBase_Main {
public:
    Main(CkArgMsg* args) {
        delete args;
        configuration = new (3) SharedConfig;
        configuration->count = 3;
        for (int i = 0; i < 3; ++i)
            configuration->values[i] = (i + 1) * 2.5;
        CProxy_Reader::ckNew(1);
    }
};
#include "ReadonlyMessage.def.h"
