#include <cstdlib>

#define DEFAULT_NUM_ELEMS 10

mainchare Main {
public:
  Main(CkMigrateMessage *m) {}
entry:
  Main(CkArgMsg *m) {
    int numElems = m->argc > 1 ? atoi(m->argv[1]) : DEFAULT_NUM_ELEMS;
    CkPrintf("reduction: %d elements, expected sum = %d\n",
             numElems, numElems*(numElems-1)/2);
    delete m;
    CProxy_Elem::ckNew(thisProxy, numElems);
  }

  [[charm::reductiontarget]] void printResult(int result) {
    CkPrintf("result = %d\n", result);
    CkExit();
  }
};

array [1D] chare Elem {
public:
  Elem(CkMigrateMessage *m) {}
entry:
  Elem(CProxy_Main mainProxy) {
    int value = thisIndex;
    CkCallback cb(CkReductionTarget(Main, printResult), mainProxy);
    contribute(sizeof(int), &value, CkReduction::sum_int, cb);
  }
};
