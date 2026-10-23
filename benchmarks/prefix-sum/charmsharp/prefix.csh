#include <unistd.h>

// Charm# port of charm-tutorial examples/prefix_sdag, pinned in ../charmpp.
readonly CProxy_Main mainProxy;
readonly int numElements;
readonly CProxy_Prefix prefixArray;

mainchare Main {
entry:
    Main(CkArgMsg* msg) {
        numElements = (msg->argc > 1) ? atoi(msg->argv[1]) : 8;
        delete msg;
        mainProxy = thisProxy;
        prefixArray = CProxy_Prefix::ckNew(numElements);
        prefixArray.start();
    }

    [[charm::reductiontarget]] void done() { CkExit(); }
};

array [1D] chare Prefix {
    int value, dist;
entry:
    Prefix() : value(thisIndex + 1), dist(0) {
        srand(thisIndex * 1009 + 7);
    }

    void start() {
        for (dist = 1; dist < numElements; dist <<= 1) {
            if (thisIndex + dist < numElements) {
                usleep(rand() % 5001);
                prefixArray[thisIndex + dist].passValue(dist, value);
            }
            if (thisIndex >= dist) {
                when passValue[dist](int stepDist, int val) {
                    value += val;
                }
            }
        }
        int expected = (thisIndex + 1) * (thisIndex + 2) / 2;
        CkPrintf("Prefix[%d] = %d  (expected %d)  %s\n",
                 thisIndex, value, expected,
                 (value == expected) ? "OK" : "WRONG");
        CkCallback cb(CkReductionTarget(Main, done), mainProxy);
        contribute(0, NULL, CkReduction::nop, cb);
    }
public:
    Prefix(CkMigrateMessage* m) {}
};
