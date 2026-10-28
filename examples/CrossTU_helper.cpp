#include "CrossTU.decl.h"

// Charmxi's generated proxy methods are templates. A consumer TU using a
// specialization needs their definitions as well as their declarations.
#define CK_TEMPLATES_ONLY
#include "CrossTU.def.h"
#undef CK_TEMPLATES_ONLY

void launchWorker(CProxy_Main caller) {
    CProxy_Worker<int>::ckNew(42, caller);
}
