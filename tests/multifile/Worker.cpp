#include "Worker.h"

Worker::Worker() {}

void Worker::run() {
    CkPrintf("MULTIFILE PASS on PE %d\n", CkMyPe());
    CkExit();
}
