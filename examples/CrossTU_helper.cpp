#include "CrossTU.decl.h"

// Compiled independently from the generated CrossTU.cpp.
void launchWorker(CProxy_Main caller) {
    CProxy_Worker<int>::ckNew(42, caller);
}
