#include "CrossTU_templates.h"

// A separate translation unit sees the complete chare template and its
// generated Charm++ template machinery through the shared header.
void launchWorker(CProxy_Main caller) {
    CProxy_Worker<int>::ckNew(42, caller);
}
