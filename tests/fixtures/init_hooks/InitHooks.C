#include "InitHooks.decl.h"
void initializeNode() {
    CkPrintf("Hook node %d\n", CkMyNode());
}
void initializeProcessor() {
    CkPrintf("Hook processor %d\n", CkMyPe());
}
class Main : public CBase_Main {
public:
    Main(CkArgMsg* args) {
        delete args;
        CkPrintf("Hook main PE %d\n", CkMyPe());
        CkExit();
    }
};
#include "InitHooks.def.h"
